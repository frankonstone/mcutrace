#include "test_runner.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mcutest/mcutest.hpp>
#include <mcutrace/assembly.hpp>
#include <mcutrace/cli.hpp>
#include <mcutrace/config.hpp>
#include <mcutrace/lsp.hpp>
#include <mcutrace/mcp.hpp>
#include <mcutrace/project.hpp>
#include <mcutrace/source_annotations.hpp>
#include <mcutrace/trace_import.hpp>
#include <mcutrace/validation.hpp>
#include <sstream>

using namespace mcutrace;
namespace {
const std::string source = "# @req-file REQ-0001\nputs done\n";
ArtifactInput input(std::string text) {
    return {.path = "/work/test.tcl", .base_directory = "/work", .content = std::move(text)};
}
TraceResult trace(std::string status = "passed", std::string digest = {}, bool execute = true) {
    auto declaration = import_test_annotations(input(source));
    const std::array<RequirementDocument, 1> docs{
        {{"/work/req.md", "# REQ-0001 Retention @evidence(test)\n"}}};
    auto requirements = parse_requirements(docs);
    std::vector<ImportFragment> fragments{*declaration};
    if (execute) {
        if (digest.empty())
            digest = source_digest(source);
        auto artifact = import_trace_artifact(
            {.path = "/work/results.json",
             .base_directory = "/work",
             .content =
                 "{\"format\":\"mcutest-results\",\"version\":1,\"tests\":[{\"name\":\"retention\",\"file\":"
                 "\"test.tcl\",\"language\":\"tcl\",\"outcome\":\"completed\",\"checks\":1,\"status\":\"" +
                 status + "\",\"source_digest\":\"" + digest + "\"}]}"},
            "mcutest");
        if (artifact)
            fragments.push_back(*artifact);
    }
    return assemble_trace(requirements.requirements, fragments);
}
bool has(const std::vector<Diagnostic> &diagnostics, std::string_view code) {
    return std::any_of(diagnostics.begin(), diagnostics.end(), [&](const auto &d) { return d.code == code; });
}
struct Workspace {
    std::filesystem::path root = std::filesystem::temp_directory_path() / "mcutrace-tcl-tests";
    Workspace() { std::filesystem::create_directories(root); }
    ~Workspace() {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }
    void write(std::string_view file, std::string_view text) {
        std::ofstream out{root / file, std::ios::binary};
        out << text;
    }
};
} // namespace
TEST(tcl, recognizes_script_comments_without_treating_data_as_claims, "REQ-0192", "REQ-0193", "REQ-0225") {
    ASSERT_EQ(source_language_for_path("path/file.tcl"), SourceLanguage::tcl);
    ASSERT_EQ(source_language_for_path("path/file.TCL"), SourceLanguage::unsupported);
    const auto result = import_source_annotations(input(R"TCL(
set text "# @req-file REQ-0002"
set block {
# @req-file REQ-0003
}
puts x # @req-file REQ-0004
set value [list {
# @req-file REQ-0005
}]
proc later {} {
# @req-file REQ-0006
}
set variable ${a{b}
set expanded {*}{a b}
set array "$data([list "]"])"
# @req-file REQ-0001
puts done; # @req-file REQ-0007
)TCL"));
    ASSERT_TRUE(result);
    ASSERT_EQ(result->edges.size(), 2U);
    ASSERT_EQ(result->edges[0].target_id, "REQ-0001");
    ASSERT_EQ(result->edges[1].target_id, "REQ-0007");
    ASSERT_EQ(result->edges[0].type.kind, RelationshipKind::implements);
}
TEST(tcl, unsupported_declaration_targets_and_malformed_input_are_diagnosed, "REQ-0192", "REQ-0227") {
    auto unsupported = import_test_annotations(input("# @req REQ-0001\nproc f {} {}\n"));
    ASSERT_TRUE(unsupported);
    ASSERT_TRUE(has(unsupported->diagnostics, "source.annotation.unsupported_target"));
    ASSERT_TRUE(unsupported->edges.empty());
    auto invalid = import_test_annotations(input("# @req-file REQ-0001\nset x {unfinished\n"));
    ASSERT_TRUE(invalid);
    ASSERT_TRUE(has(invalid->diagnostics, "source.annotation.invalid_tcl"));
    ASSERT_TRUE(invalid->edges.empty());
    ASSERT_FALSE(import_test_annotations(
        {.path = "test.py", .base_directory = "/work", .content = "# @req-file REQ-0001"}));
}
TEST(tcl, escapes_continuations_and_nested_substitutions_preserve_comment_boundaries, "REQ-0192",
     "REQ-0227") {
    auto result =
        import_test_annotations(input("set text \\\n# @req-file REQ-0002\nset block {escaped \\} brace}\n"
                                      "set value [\n# @req-file REQ-0003\nlist x]\n# @req-file REQ-0001\n"));
    ASSERT_TRUE(result);
    ASSERT_EQ(result->edges.size(), 1U);
    ASSERT_EQ(result->edges[0].target_id, "REQ-0001");
    std::string deep;
    for (unsigned n = 0; n < 150; ++n)
        deep += "$a(";
    auto nested = import_source_annotations(input(deep));
    ASSERT_TRUE(has(nested->diagnostics, "source.annotation.invalid_tcl"));
}
TEST(tcl, test_declarations_never_create_implementation_claims, "REQ-0090", "REQ-0093", "REQ-0226") {
    auto declaration = import_test_annotations(input(source));
    ASSERT_TRUE(declaration);
    ASSERT_EQ(declaration->nodes.size(), 1U);
    ASSERT_EQ(declaration->nodes[0].kind, NodeKind::test);
    ASSERT_EQ(declaration->nodes[0].evidence_state, EvidenceState::unknown);
    ASSERT_EQ(declaration->edges[0].type.kind, RelationshipKind::verifies);
    ASSERT_EQ(declaration->edges[0].provenance.importer, "test-annotations");
    auto missing = trace("passed", {}, false);
    ASSERT_TRUE(has(missing.diagnostics, "test_source.not_executed"));
    ASSERT_EQ(missing.graph.find_node("test:tcl:/work/test.tcl")->evidence_state, EvidenceState::unknown);
}
TEST(tcl, matching_results_join_declarations_independently_of_artifact_location, "REQ-0192", "REQ-0227") {
    auto result = trace();
    ASSERT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(result.graph.find_node("test:tcl:/work/test.tcl")->evidence_state, EvidenceState::passed);
    ASSERT_TRUE(validate_trace(result).diagnostics.empty());
    ASSERT_EQ(source_digest(""), "fnv1a64:cbf29ce484222325");
    ASSERT_EQ(source_digest("a"), "fnv1a64:af63dc4c8601ec8c");
}
TEST(tcl, stale_missing_skipped_and_incomplete_results_cannot_pass, "REQ-0192", "REQ-0227") {
    auto stale = trace("passed", source_digest(source + "\n"));
    ASSERT_TRUE(has(stale.diagnostics, "test_source.stale"));
    ASSERT_EQ(stale.graph.find_node("test:tcl:/work/test.tcl")->evidence_state, EvidenceState::unknown);
    for (const char *status : {"skipped", "incomplete"}) {
        auto result = trace(status);
        ASSERT_EQ(result.graph.find_node("test:tcl:/work/test.tcl")->evidence_state, EvidenceState::unknown);
        ASSERT_TRUE(has(result.diagnostics, "test_source.incomplete"));
    }
    auto failed = trace("failed");
    ASSERT_TRUE(has(validate_trace(failed).diagnostics, "validation.failed_test_evidence"));
}
TEST(tcl, conflicting_and_undeclared_executions_are_rejected, "REQ-0192", "REQ-0227") {
    auto declaration = import_test_annotations(input(source));
    auto run = import_trace_artifact(
        {.path = "result.json",
         .base_directory = "/work",
         .content =
             "{\"format\":\"mcutest-results\",\"version\":1,\"tests\":[{\"name\":\"x\",\"file\":\"test.tcl\","
             "\"language\":\"tcl\",\"outcome\":\"completed\",\"checks\":1,\"status\":\"passed\"}]}"},
        "mcutest");
    ASSERT_TRUE(run);
    const std::array<ImportFragment, 1> unbound{*run};
    auto orphan = assemble_trace({}, unbound);
    ASSERT_TRUE(has(orphan.diagnostics, "test_source.undeclared"));
    const std::array<ImportFragment, 2> missing{*declaration, *run};
    ASSERT_TRUE(has(assemble_trace({}, missing).diagnostics, "test_source.missing_digest"));
    run->nodes[0].source_digest = source_digest(source);
    auto second = *run;
    second.nodes[0].evidence_state = EvidenceState::failed;
    second.nodes[0].status = "failed";
    const std::array<ImportFragment, 3> conflict{*declaration, *run, second};
    auto combined = assemble_trace({}, conflict);
    ASSERT_TRUE(has(combined.diagnostics, "test_source.conflicting_results"));
    ASSERT_EQ(combined.graph.find_node("test:tcl:/work/test.tcl")->evidence_state, EvidenceState::unknown);
}
TEST(tcl, configuration_and_cli_accept_test_source_inputs, "REQ-0093") {
    auto config = parse_project_config("[project]\nroot=\"/work\"\ntest_sources=[\"test.tcl\"]\n",
                                       "/work/mcutrace.toml");
    ASSERT_TRUE(config);
    ASSERT_EQ(config->test_source_files.size(), 1U);
    ASSERT_EQ(config->test_source_files[0], "/work/test.tcl");
    ASSERT_FALSE(parse_project_config("[project]\ntest_sources=42\n"));
    const char *argv[]{"mcutrace", "validate", "--test-source", "test.tcl"};
    auto cli = parse_cli(4, argv);
    ASSERT_TRUE(cli);
    ASSERT_EQ(cli->test_source_files.size(), 1U);
}
TEST(tcl, project_and_mcp_reload_changed_test_files, "REQ-0192", "REQ-0093") {
    Workspace workspace;
    workspace.write("req.md", "# REQ-0001 Retention @evidence(test)\n");
    workspace.write("test.tcl", source);
    workspace.write("mcutrace.toml", "[project]\nrequirements=[\"req.md\"]\ntest_sources=[\"test.tcl\"]\n");
    const auto path = (workspace.root / "mcutrace.toml").generic_string();
    auto first = load_project_snapshot(path);
    ASSERT_TRUE(first);
    ASSERT_TRUE(has(first->trace.diagnostics, "test_source.not_executed"));
    ASSERT_TRUE(std::any_of(first->inputs.begin(), first->inputs.end(),
                            [](const auto &v) { return v.kind == "test_source"; }));
    workspace.write("test.tcl", source + "\n");
    auto second = load_project_snapshot(path);
    ASSERT_TRUE(second);
    ASSERT_NE(first->revision, second->revision);
    McpServer server(path);
    static_cast<void>(server.handle(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"test","version":"1"}}})"));
    const auto messages = server.handle(
        R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"requirement","arguments":{"id":"REQ-0001"}}})");
    ASSERT_FALSE(messages.empty());
    ASSERT_NE(messages[0].find("test:tcl:"), std::string::npos);
    std::ostringstream captured, errors;
    auto *previous = std::cout.rdbuf(captured.rdbuf());
    auto *previous_error = std::cerr.rdbuf(errors.rdbuf());
    CliOptions options;
    options.config_path = path;
    const auto status = run_cli(options);
    std::cout.rdbuf(previous);
    std::cerr.rdbuf(previous_error);
    ASSERT_EQ(status, 0);
    ASSERT_NE(errors.str().find("not_executed"), std::string::npos);
}

TEST(tcl, passing_status_without_completed_checked_execution_is_rejected, "REQ-0227") {
    auto declaration = import_test_annotations(input(source));
    for (const char *fields :
         {"", "\"checks\":0,\"outcome\":\"completed\",", "\"checks\":1,\"outcome\":\"limit\","}) {
        auto run = import_trace_artifact(
            {.path = "result.json",
             .base_directory = "/work",
             .content = "{\"format\":\"mcutest-results\",\"version\":1,\"tests\":[{" + std::string(fields) +
                        "\"name\":\"x\",\"file\":\"test.tcl\",\"language\":\"tcl\",\"status\":\"passed\","
                        "\"source_digest\":\"" +
                        source_digest(source) + "\"}]}"},
            "mcutest");
        ASSERT_TRUE(run);
        const std::array<ImportFragment, 2> fragments{*declaration, *run};
        auto result = assemble_trace({}, fragments);
        ASSERT_TRUE(has(result.diagnostics, "import.mcutest.incomplete_tcl_result"));
        ASSERT_EQ(result.graph.find_node("test:tcl:/work/test.tcl")->evidence_state, EvidenceState::unknown);
    }
}
TEST(tcl, lsp_uses_unsaved_tcl_text_for_freshness_and_annotation_validation, "REQ-0226", "REQ-0227") {
    Workspace workspace;
    workspace.write("req.md", "# REQ-0001 Retention @evidence(test)\n");
    workspace.write("test.tcl", source);
    workspace.write("result.json", "{\"format\":\"mcutest-results\",\"version\":1,\"tests\":[{\"name\":\"x\","
                                   "\"file\":\"test.tcl\",\"language\":\"tcl\",\"status\":\"passed\","
                                   "\"outcome\":\"completed\",\"checks\":1,\"source_digest\":\"" +
                                       source_digest(source) + "\"}]}");
    workspace.write("mcutrace.toml", "[project]\nrequirements=[\"req.md\"]\ntest_sources=[\"test.tcl\"]\n[["
                                     "artifacts]]\npath=\"result.json\"\nimporter=\"mcutest\"\n");
    LanguageServer server;
    const auto initial = server.handle(
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"rootUri\":\"file://" +
        workspace.root.generic_string() + "\",\"initializationOptions\":{\"configPath\":\"" +
        (workspace.root / "mcutrace.toml").generic_string() + "\"}}}");
    std::string initial_text;
    for (const auto &message : initial)
        initial_text += message;
    ASSERT_EQ(initial_text.find("test_source.stale"), std::string::npos);
    const auto changed = server.handle(
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/"
        "didOpen\",\"params\":{\"textDocument\":{\"uri\":\"file://" +
        (workspace.root / "test.tcl").generic_string() +
        "\",\"languageId\":\"tcl\",\"version\":1,\"text\":\"# @req-file REQ-0001\\nputs changed\\n\"}}}");
    std::string changed_text;
    for (const auto &message : changed)
        changed_text += message;
    ASSERT_NE(changed_text.find("test_source.stale"), std::string::npos);
}

TEST(tcl, namespace_separators_and_delimited_word_boundaries_follow_tcl_syntax, "REQ-0225") {
    auto result = import_test_annotations(input("set x $name:tail(open\n# @req-file REQ-0001\nclose)\n"));
    ASSERT_TRUE(result);
    ASSERT_EQ(result->edges.size(), 1U);
    for (const char *text :
         {"# @req-file REQ-0001\nset x {closed}extra\n", "# @req-file REQ-0001\nset x \"closed\"extra\n"}) {
        auto invalid = import_test_annotations(input(text));
        ASSERT_TRUE(invalid);
        ASSERT_TRUE(has(invalid->diagnostics, "source.annotation.invalid_tcl"));
        ASSERT_TRUE(invalid->edges.empty());
    }
}
int main(int argc, char *argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutrace::test::run(argc, argv, runner);
}
