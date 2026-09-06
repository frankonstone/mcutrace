#include <mcutrace/lsp.hpp>

#include <mcutest/mcutest.hpp>

#include "test_runner.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

struct TemporaryWorkspace final {
    std::filesystem::path root;

    ~TemporaryWorkspace() {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }
};

bool write_file(const std::filesystem::path& path, std::string_view content) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }
    std::ofstream stream(path, std::ios::binary);
    stream << content;
    return static_cast<bool>(stream);
}

std::string uri(const std::filesystem::path& path) {
    return "file://" + path.generic_string();
}

std::size_t occurrences(std::string_view text, std::string_view value) {
    std::size_t count = 0;
    std::size_t offset = 0;
    while ((offset = text.find(value, offset)) != std::string_view::npos) {
        ++count;
        offset += value.size();
    }
    return count;
}

std::string joined(const std::vector<std::string>& messages) {
    std::string result;
    for (const auto& message : messages) {
        result += message;
        result += '\n';
    }
    return result;
}

std::string frame(std::string_view message) {
    return "Content-Length: " + std::to_string(message.size()) + "\r\n\r\n" + std::string(message);
}

TemporaryWorkspace create_workspace() {
    std::error_code error;
    auto root = std::filesystem::temp_directory_path(error) / "mcutrace-lsp-tests";
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    return TemporaryWorkspace{.root = std::move(root)};
}

TEST(lsp, serves_core_backed_editor_features_and_live_diagnostics,
     "REQ-0103", "REQ-0104", "REQ-0106", "REQ-0108", "REQ-0109", "REQ-0112",
     "REQ-0113", "REQ-0114", "REQ-0115", "REQ-0116", "REQ-0117", "REQ-0118",
     "REQ-0119", "REQ-0120") {
    const auto workspace = create_workspace();
    const auto config = workspace.root / "mcutrace.toml";
    const auto requirements = workspace.root / "docs/requirements.md";
    const auto source = workspace.root / "src/component.cpp";
    constexpr std::string_view requirement_text =
        "### REQ-0103 First definition\n"
        "The server shall use the shared parser.\n"
        "\n"
        "### REQ-0103 Second definition\n"
        "This is intentionally duplicated.\n"
        "\n"
        "### REQ-0104 Existing requirement\n"
        "This prevents a conflicting rename.\n";
    constexpr std::string_view config_text =
        "[project]\n"
        "root = \".\"\n"
        "requirements = [\"docs/requirements.md\"]\n"
        "sources = [\"src/component.cpp\"]\n"
        "\n"
        "[[artifacts]]\n"
        "path = \"artifacts/tests.json\"\n"
        "importer = \"mcutest\"\n"
        "\n"
        "[[artifacts]]\n"
        "path = \"artifacts/coverage.json\"\n"
        "importer = \"mcucov\"\n"
        "\n"
        "[[artifacts]]\n"
        "path = \"artifacts/mcucheck.json\"\n"
        "importer = \"mcucheck\"\n"
        "\n"
        "[validation.missing_test]\n"
        "enabled = false\n"
        "\n"
        "[validation.missing_coverage]\n"
        "enabled = false\n";
    ASSERT_TRUE(write_file(config, config_text));
    ASSERT_TRUE(write_file(requirements, requirement_text));
    ASSERT_TRUE(write_file(source, "// @req REQ-0103\nvoid component() {}\n"));
    ASSERT_TRUE(write_file(workspace.root / "artifacts/tests.json",
                           R"({"format":"mcutest-results","version":1,"tests":[{"name":"component.works","status":"passed","requirements":["REQ-0103"]},{"name":"component.fails","status":"failed","requirements":["REQ-0103"]}]})"));
    ASSERT_TRUE(write_file(workspace.root / "artifacts/coverage.json",
                           R"({"format":"mcucov-report","version":1,"modules":[{"path":"src/component.cpp","variant":"host","requirements":["REQ-0103"],"probes":[{"covered":true},{"covered":true},{"covered":false}]}]})"));
    ASSERT_TRUE(write_file(workspace.root / "artifacts/mcucheck.json",
                           R"({"format":"mcucheck-results","version":1,"diagnostics":[{"rule_id":"AUTOSAR-A1","message":"bad thing","state":"violation","id":"component","location":{"path":"src/component.cpp","line":2,"column":1,"end_line":2,"end_column":5},"requirements":["REQ-0103"]}]})"));

    mcutrace::LanguageServer server;
    const auto initialized = server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{"
        "\"rootUri\":\"" + uri(workspace.root) + "\",\"initializationOptions\":{"
        "\"configPath\":\"" + config.generic_string() + "\"}}}");
    const auto initialize_output = joined(initialized);
    ASSERT_NE(initialize_output.find("\"hoverProvider\":true"), std::string::npos);
    ASSERT_NE(initialize_output.find("\"definitionProvider\":true"), std::string::npos);
    ASSERT_EQ(occurrences(initialize_output, "MTR-REQ-DUPLICATE-ID"), std::size_t{2});
    ASSERT_NE(initialize_output.find("relatedInformation"), std::string::npos);
    ASSERT_NE(initialize_output.find("static-analysis finding: AUTOSAR-A1: bad thing"),
              std::string::npos);
    const auto trace_notification = initialize_output.find("\"method\":\"mcutrace/publishTrace\"");
    ASSERT_NE(trace_notification, std::string::npos);
    ASSERT_EQ(initialize_output.substr(0, trace_notification).find(
                  "static-analysis finding: AUTOSAR-A1: bad thing"),
              std::string::npos);
    ASSERT_NE(initialize_output.find("\"headingLevel\":3", trace_notification),
              std::string::npos);
    ASSERT_NE(initialize_output.find("\"tests\":[", trace_notification),
              std::string::npos);
    ASSERT_NE(initialize_output.find("\"end\":{\"line\":1,\"character\":4}"),
              std::string::npos);

    const auto definition = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/definition\",\"params\":{"
        "\"textDocument\":{\"uri\":\"" + uri(requirements) + "\"},"
        "\"position\":{\"line\":0,\"character\":6}}}"));
    ASSERT_EQ(occurrences(definition, "component.cpp"), std::size_t{0});
    ASSERT_EQ(occurrences(definition, "requirements.md"), std::size_t{2});

    const auto hover = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/hover\",\"params\":{"
        "\"textDocument\":{\"uri\":\"" + uri(requirements) + "\"},"
        "\"position\":{\"line\":0,\"character\":6}}}"));
    ASSERT_NE(hover.find("First definition"), std::string::npos);
    ASSERT_NE(hover.find("**Implementations**"), std::string::npos);
    ASSERT_NE(hover.find("function component"), std::string::npos);
    ASSERT_NE(hover.find("**Tests**"), std::string::npos);
    ASSERT_NE(hover.find("component.works"), std::string::npos);
    ASSERT_NE(hover.find("✅ passed"), std::string::npos);
    ASSERT_NE(hover.find("❌ failed"), std::string::npos);
    ASSERT_NE(hover.find("**Coverage**"), std::string::npos);
    const auto source_link = "[src/component.cpp](" + uri(source) + ")";
    ASSERT_NE(hover.find(source_link), std::string::npos);
    ASSERT_NE(hover.find(source_link + " — 2/3 probes covered"), std::string::npos);
    ASSERT_NE(hover.find("**Static analysis findings**"), std::string::npos);
    ASSERT_NE(hover.find("AUTOSAR-A1: bad thing"), std::string::npos);
    ASSERT_NE(hover.find("❌ violation"), std::string::npos);

    const auto references = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"textDocument/references\",\"params\":{"
        "\"textDocument\":{\"uri\":\"" + uri(requirements) + "\"},"
        "\"position\":{\"line\":0,\"character\":6}}}"));
    ASSERT_EQ(occurrences(references, "requirements.md"), std::size_t{2});
    ASSERT_EQ(occurrences(references, "component.cpp"), std::size_t{1});

    const auto implementations = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"textDocument/implementation\",\"params\":{"
        "\"textDocument\":{\"uri\":\"" + uri(requirements) + "\"},"
        "\"position\":{\"line\":0,\"character\":6}}}"));
    ASSERT_NE(implementations.find("component.cpp"), std::string::npos);

    const auto symbols = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"textDocument/documentSymbol\",\"params\":{"
        "\"textDocument\":{\"uri\":\"" + uri(requirements) + "\"}}}"));
    ASSERT_EQ(occurrences(symbols, "First definition"), std::size_t{1});
    ASSERT_EQ(occurrences(symbols, "Second definition"), std::size_t{1});

    const auto completion = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"textDocument/completion\",\"params\":{}}"));
    ASSERT_NE(completion.find("@evidence(test)"), std::string::npos);
    ASSERT_NE(completion.find("REQ-0103"), std::string::npos);

    const auto lenses = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"textDocument/codeLens\",\"params\":{"
        "\"textDocument\":{\"uri\":\"" + uri(requirements) + "\"}}}"));
    ASSERT_NE(lenses.find("mcutrace.showTrace"), std::string::npos);
    ASSERT_NE(lenses.find("1 implementations"), std::string::npos);

    const auto renamed = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"textDocument/rename\",\"params\":{"
        "\"textDocument\":{\"uri\":\"" + uri(requirements) + "\"},"
        "\"position\":{\"line\":0,\"character\":6},\"newName\":\"REQ-0105\"}}"));
    ASSERT_NE(renamed.find("\"newText\":\"REQ-0105\""), std::string::npos);
    ASSERT_NE(renamed.find("component.cpp"), std::string::npos);

    const auto rejected_rename = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":10,\"method\":\"textDocument/rename\",\"params\":{"
        "\"textDocument\":{\"uri\":\"" + uri(requirements) + "\"},"
        "\"position\":{\"line\":0,\"character\":6},\"newName\":\"REQ-0104\"}}"));
    ASSERT_NE(rejected_rename.find("already exists"), std::string::npos);

    const auto changed = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{"
        "\"textDocument\":{\"uri\":\"" + uri(requirements) + "\",\"version\":2},"
        "\"contentChanges\":[{\"text\":\"### REQ-0103 First definition\\nThe server shall use the shared parser.\\n\"}]}}"));
    ASSERT_NE(changed.find("\"diagnostics\":[]"), std::string::npos);

    ASSERT_TRUE(write_file(source, "// @req REQ-0103\n"));
    const auto refreshed = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"method\":\"workspace/didChangeWatchedFiles\",\"params\":{\"changes\":[]}}"));
    ASSERT_NE(refreshed.find("source.annotation.missing_target"), std::string::npos);
    const auto refreshed_trace = refreshed.find("\"method\":\"mcutrace/publishTrace\"");
    ASSERT_NE(refreshed_trace, std::string::npos);
    ASSERT_EQ(refreshed.substr(0, refreshed_trace).find("source.annotation.missing_target"),
              std::string::npos);
}

TEST(lsp, reloads_configured_documents_on_refresh_and_file_changes,
     "REQ-0105", "REQ-0106", "REQ-0126") {
    constexpr std::string_view refreshes[] = {
        R"({"jsonrpc":"2.0","method":"workspace/didChangeWatchedFiles","params":{"changes":[]}})",
        R"({"jsonrpc":"2.0","id":2,"method":"workspace/executeCommand","params":{"command":"mcutrace.refresh"}})",
        R"({"jsonrpc":"2.0","method":"workspace/didChangeConfiguration","params":{"settings":{}}})",
    };
    for (const auto refresh : refreshes) {
        const auto workspace = create_workspace();
        const auto config = workspace.root / "custom.toml";
        const auto original = workspace.root / "original.md";
        const auto added = workspace.root / "added.md";
        ASSERT_TRUE(write_file(config,
            "[project]\nrequirements = [\"original.md\"]\n"));
        ASSERT_TRUE(write_file(original, "### REQ-0105 Original requirement\n"));
        ASSERT_TRUE(write_file(added, "### REQ-0126 Added requirement\n"));

        mcutrace::LanguageServer server;
        const auto initialized = joined(server.handle(
            "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{"
            "\"rootUri\":\"" + uri(workspace.root) + "\",\"initializationOptions\":{"
            "\"configPath\":\"" + config.generic_string() + "\"}}}"));
        ASSERT_NE(initialized.find("Original requirement"), std::string::npos);
        ASSERT_EQ(initialized.find("Added requirement"), std::string::npos);
        static_cast<void>(server.handle(
            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
            "\"textDocument\":{\"uri\":\"" + uri(added) + "\",\"version\":1,"
            "\"text\":\"### REQ-0126 Unsaved added requirement\\n\"}}}"));

        ASSERT_TRUE(write_file(config,
            "[project]\nrequirements = [\"original.md\", \"added.md\"]\n"));
        const auto refreshed = joined(server.handle(refresh));
        ASSERT_NE(refreshed.find("\"method\":\"mcutrace/publishTrace\""), std::string::npos);
        ASSERT_NE(refreshed.find("Original requirement"), std::string::npos);
        ASSERT_NE(refreshed.find("Unsaved added requirement"), std::string::npos);
        ASSERT_NE(refreshed.find(uri(added)), std::string::npos);

        ASSERT_TRUE(write_file(config,
            "[project]\nrequirements = [\"added.md\"]\n"));
        const auto removed = joined(server.handle(refresh));
        ASSERT_EQ(removed.find("Original requirement"), std::string::npos);
        ASSERT_NE(removed.find("Unsaved added requirement"), std::string::npos);
    }
}

TEST(lsp, discovers_wildcard_inputs_on_watched_file_changes,
     "REQ-0105", "REQ-0125", "REQ-0126") {
    const auto workspace = create_workspace();
    ASSERT_TRUE(write_file(workspace.root / "mcutrace.toml",
        "[project]\nrequirements = [\"docs/*.md\"]\n"));
    ASSERT_TRUE(write_file(workspace.root / "docs/original.md",
        "### REQ-0105 Original requirement\n"));
    mcutrace::LanguageServer server;
    static_cast<void>(server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{"
        "\"rootUri\":\"" + uri(workspace.root) + "\"}}"));

    const auto added = workspace.root / "docs/added.md";
    ASSERT_TRUE(write_file(added, "### REQ-0126 Added requirement\n"));
    const auto created = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"method\":\"workspace/didChangeWatchedFiles\",\"params\":{"
        "\"changes\":[{\"uri\":\"" + uri(added) + "\",\"type\":1}]}}"));
    ASSERT_NE(created.find("Added requirement"), std::string::npos);
    ASSERT_NE(created.find("Original requirement"), std::string::npos);

    std::error_code error;
    ASSERT_TRUE(std::filesystem::remove(added, error));
    const auto removed = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"method\":\"workspace/didChangeWatchedFiles\",\"params\":{"
        "\"changes\":[{\"uri\":\"" + uri(added) + "\",\"type\":3}]}}"));
    ASSERT_EQ(removed.find("Added requirement"), std::string::npos);
    ASSERT_NE(removed.find("Original requirement"), std::string::npos);
    ASSERT_EQ(removed.find("lsp.requirement_input"), std::string::npos);
}

TEST(lsp, uses_content_length_framing, "REQ-0103") {
    std::istringstream input(frame("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}") +
                             frame("{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\",\"params\":{}}") +
                             frame("{\"jsonrpc\":\"2.0\",\"method\":\"exit\",\"params\":{}}"));
    std::ostringstream output;

    ASSERT_EQ(mcutrace::run_language_server(input, output), 0);
    ASSERT_NE(output.str().find("Content-Length:"), std::string::npos);
    ASSERT_NE(output.str().find("\"id\":1"), std::string::npos);
    ASSERT_NE(output.str().find("\"id\":2"), std::string::npos);
}

TEST(lsp, rejects_invalid_transport_headers_and_unknown_requests, "REQ-0103", "REQ-0105") {
    std::istringstream invalid_input("Content-Length: not-a-number\r\n\r\n");
    std::ostringstream output;
    ASSERT_EQ(mcutrace::run_language_server(invalid_input, output), 1);
    ASSERT_TRUE(output.str().empty());

    mcutrace::LanguageServer server;
    const auto unknown = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"mcutrace/notImplemented\",\"params\":{}}"));
    ASSERT_NE(unknown.find("method not found"), std::string::npos);

    const auto missing_method = joined(server.handle("{\"jsonrpc\":\"2.0\",\"id\":2}"));
    ASSERT_NE(missing_method.find("method is required"), std::string::npos);
}

TEST(lsp, exposes_live_dependencies_and_implementation_plans,
     "REQ-0084", "REQ-0129", "REQ-0135", "REQ-0136", "REQ-0137") {
    const auto workspace = create_workspace();
    const auto config = workspace.root / "mcutrace.toml";
    const auto requirements = workspace.root / "requirements.md";
    const auto plans = workspace.root / "implementation-plan.md";
    ASSERT_TRUE(write_file(config,
        "[project]\nrequirements = [\"requirements.md\"]\n"
        "plans = [\"implementation-plan.md\"]\n"));
    ASSERT_TRUE(write_file(requirements,
        "## REQ-0001 Consumer\n"
        "Uses @depends(REQ-0002) here and @evidence(none).\n"
        "## REQ-0002 Foundation\n"
        "Body ends with @evidence(none)\n"));
    ASSERT_TRUE(write_file(plans,
        "## PLN-0001 Implement consumer\n"
        "@link(REQ-0001, REQ-9999)\n"
        "@status(in_progress)\n"));

    mcutrace::LanguageServer server;
    const auto initialized = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{"
        "\"rootUri\":\"" + uri(workspace.root) + "\"}}"));
    ASSERT_NE(initialized.find("\"prerequisites\":[{\"id\":\"REQ-0002\""),
              std::string::npos);
    ASSERT_NE(initialized.find("\"plans\":[{\"id\":\"PLN-0001\""),
              std::string::npos);
    ASSERT_NE(initialized.find("\"kind\":\"plan\""), std::string::npos);
    ASSERT_NE(initialized.find("\"status\":\"in_progress\""), std::string::npos);
    ASSERT_NE(initialized.find("\"links\":[{\"id\":\"REQ-0001\",\"title\":\"Consumer\""),
              std::string::npos);
    ASSERT_NE(initialized.find("\"id\":\"REQ-9999\",\"location\":"),
              std::string::npos);

    const auto definition = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/definition\",\"params\":{"
        "\"textDocument\":{\"uri\":\"" + uri(requirements) + "\"},"
        "\"position\":{\"line\":1,\"character\":16}}}"));
    ASSERT_NE(definition.find("requirements.md"), std::string::npos);
    ASSERT_NE(definition.find("\"line\":2"), std::string::npos);

    const auto completion = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/completion\",\"params\":{}}"));
    ASSERT_NE(completion.find("@depends(REQ-NNNN)"), std::string::npos);

    const auto renamed = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"textDocument/rename\",\"params\":{"
        "\"textDocument\":{\"uri\":\"" + uri(requirements) + "\"},"
        "\"position\":{\"line\":0,\"character\":6},\"newName\":\"REQ-0003\"}}"));
    ASSERT_NE(renamed.find("implementation-plan.md"), std::string::npos);

    const auto changed = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"" + uri(plans) + "\",\"version\":2,"
        "\"text\":\"## PLN-0001 Implement consumer\\n@link(REQ-0001)\\n@status(done)\\n\"}}}"));
    ASSERT_NE(changed.find("\"status\":\"done\""), std::string::npos);
}

TEST(lsp, isolates_configuration_failures_and_handles_safe_editor_requests,
     "REQ-0105", "REQ-0107", "REQ-0110", "REQ-0111", "REQ-0121", "REQ-0122", "REQ-0123") {
    mcutrace::LanguageServer server;
    const auto initialized = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{"
        "\"initializationOptions\":{\"configPath\":\"/definitely/missing/mcutrace.toml\"}}}"));
    ASSERT_NE(initialized.find("cannot read mcutrace configuration"), std::string::npos);
    ASSERT_NE(initialized.find("\"id\":1"), std::string::npos);

    const auto code_actions = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/codeAction\",\"params\":{}}"));
    ASSERT_NE(code_actions.find("\"result\":[]"), std::string::npos);

    const auto refreshed = joined(server.handle(
        "{\"jsonrpc\":\"2.0\",\"method\":\"workspace/didChangeWatchedFiles\",\"params\":{\"changes\":[]}}"));
    ASSERT_NE(refreshed.find("cannot read mcutrace configuration"), std::string::npos);
    ASSERT_NE(refreshed.find("\"method\":\"mcutrace/publishTrace\""), std::string::npos);
    ASSERT_NE(refreshed.find("\"documents\":[]"), std::string::npos);
}

}  // namespace

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutrace::test::run(argc, argv, runner);
}
