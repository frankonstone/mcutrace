#include <mcutrace/mcp.hpp>

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

TemporaryWorkspace create_workspace() {
    std::error_code error;
    auto root = std::filesystem::temp_directory_path(error) / "mcutrace-mcp-tests";
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    return TemporaryWorkspace{.root = std::move(root)};
}

std::string joined(const std::vector<std::string>& messages) {
    std::string result;
    for (const auto& message : messages) {
        result += message;
        result += '\n';
    }
    return result;
}

std::string request(mcutrace::McpServer& server, std::string_view message) {
    return joined(server.handle(message));
}

std::string tool_call(mcutrace::McpServer& server,
                      int id,
                      std::string_view name,
                      std::string_view arguments = "{}") {
    return request(server,
        "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) +
        ",\"method\":\"tools/call\",\"params\":{\"name\":\"" +
        std::string(name) + "\",\"arguments\":" + std::string(arguments) + "}}");
}

std::filesystem::path setup_workspace(const TemporaryWorkspace& workspace) {
    const auto config = workspace.root / "mcutrace.toml";
    static_cast<void>(write_file(config,
        "[project]\n"
        "root = \".\"\n"
        "requirements = [\"requirements.md\"]\n"
        "plans = [\"plan.md\"]\n"
        "sources = [\"component.cpp\"]\n"
        "\n"
        "[validation]\n"
        "fail_at_or_above = \"error\"\n"));
    static_cast<void>(write_file(workspace.root / "requirements.md",
        "### REQ-0138 Root capability @evidence(none)\n"
        "The root capability is exposed to agents.\n"
        "\n"
        "### REQ-0139 Dependent capability @evidence(none) @depends(REQ-0138)\n"
        "The dependent capability supplies transitive context.\n"));
    static_cast<void>(write_file(workspace.root / "plan.md",
        "## PLN-0014 Implement dependent capability\n"
        "@link(REQ-0139)\n"
        "@status(planned)\n"
        "\n"
        "The plan remains outstanding.\n"));
    static_cast<void>(write_file(workspace.root / "component.cpp",
        "// @req REQ-0138\n"
        "void component() {}\n"));
    return config;
}

void initialize(mcutrace::McpServer& server) {
    const auto output = request(server,
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"test","version":"1"}}})");
    ASSERT_NE(output.find("\"protocolVersion\":\"2025-11-25\""), std::string::npos);
    ASSERT_NE(output.find("\"tools\""), std::string::npos);
    ASSERT_NE(output.find("\"resources\""), std::string::npos);
    ASSERT_TRUE(request(server,
        R"({"jsonrpc":"2.0","method":"notifications/initialized","params":{}})").empty());
}

TEST(mcp, exposes_read_only_core_backed_tools_and_resources,
     "REQ-0143", "REQ-0144", "REQ-0145", "REQ-0146",
     "REQ-0147", "REQ-0148", "REQ-0149", "REQ-0150", "REQ-0151", "REQ-0152",
     "REQ-0153", "REQ-0154", "REQ-0155", "REQ-0158") {
    const auto workspace = create_workspace();
    const auto config = setup_workspace(workspace);
    mcutrace::McpServer server(config.generic_string());

    const auto premature = request(server,
        R"({"jsonrpc":"2.0","id":0,"method":"tools/list","params":{}})");
    ASSERT_NE(premature.find("server has not been initialized"), std::string::npos);
    initialize(server);

    const auto tools = request(server,
        R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})");
    ASSERT_NE(tools.find("\"name\":\"project_overview\""), std::string::npos);
    ASSERT_NE(tools.find("\"name\":\"change_impact\""), std::string::npos);
    ASSERT_NE(tools.find("\"inputSchema\""), std::string::npos);
    ASSERT_NE(tools.find("\"outputSchema\""), std::string::npos);
    ASSERT_LT(tools.find("\"name\":\"project_overview\""),
              tools.find("\"name\":\"search\""));

    const auto overview = tool_call(server, 3, "project_overview");
    ASSERT_NE(overview.find("\"structuredContent\""), std::string::npos);
    ASSERT_NE(overview.find("\"schema_version\":1"), std::string::npos);
    ASSERT_NE(overview.find("\"analysis_revision\""), std::string::npos);
    ASSERT_NE(overview.find("\"outstanding_plan_items\":1"), std::string::npos);
    ASSERT_NE(overview.find("PLN-0014"), std::string::npos);

    const auto search = tool_call(server, 4, "search",
        R"({"query":"dependent","kind":"requirement","limit":1})");
    ASSERT_NE(search.find("REQ-0139"), std::string::npos);
    ASSERT_NE(search.find("\"complete\":true"), std::string::npos);
    ASSERT_EQ(search.find("Root capability"), std::string::npos);
    const auto bounded = tool_call(server, 40, "search", R"({"limit":1})");
    ASSERT_NE(bounded.find("\"truncated\":true"), std::string::npos);
    ASSERT_NE(bounded.find("\"next_cursor\":\"1\""), std::string::npos);
    const auto continued = tool_call(server, 41, "search", R"({"limit":1,"cursor":"1"})");
    ASSERT_NE(continued.find("\"offset\":1"), std::string::npos);
    ASSERT_NE(continued.find("\"returned\":1"), std::string::npos);

    const auto requirement = tool_call(server, 5, "requirement", R"({"id":"REQ-0139"})");
    ASSERT_NE(requirement.find("Dependent capability"), std::string::npos);
    ASSERT_NE(requirement.find("\"prerequisites\""), std::string::npos);
    ASSERT_NE(requirement.find("REQ-0138"), std::string::npos);
    ASSERT_NE(requirement.find("PLN-0014"), std::string::npos);

    const auto context = tool_call(server, 6, "requirement_context",
                                   R"({"id":"REQ-0139"})");
    ASSERT_NE(context.find("Root capability"), std::string::npos);
    ASSERT_NE(context.find("depends_on"), std::string::npos);

    const auto impact = tool_call(server, 7, "change_impact", R"({"id":"REQ-0138"})");
    ASSERT_NE(impact.find("\"dependents\""), std::string::npos);
    ASSERT_NE(impact.find("REQ-0139"), std::string::npos);
    ASSERT_NE(impact.find("\"complete\":true"), std::string::npos);

    const auto plan = tool_call(server, 8, "plan", R"({"id":"PLN-0014"})");
    ASSERT_NE(plan.find("\"status\":\"planned\""), std::string::npos);
    ASSERT_NE(plan.find("\"plan_is_evidence\":false"), std::string::npos);
    ASSERT_NE(plan.find("REQ-0139"), std::string::npos);

    const auto validation = tool_call(server, 9, "validate", R"({"ids":["REQ-0139"]})");
    ASSERT_NE(validation.find("\"scope\":\"filtered\""), std::string::npos);
    ASSERT_NE(validation.find("\"complete\":true"), std::string::npos);
    const auto rejected_path = tool_call(server, 10, "validate",
                                         R"({"source_paths":["../secret.txt"]})");
    ASSERT_NE(rejected_path.find("\"isError\":true"), std::string::npos);
    ASSERT_NE(rejected_path.find("not a configured project input"), std::string::npos);

    const auto resources = request(server,
        R"({"jsonrpc":"2.0","id":11,"method":"resources/list","params":{}})");
    ASSERT_NE(resources.find("mcutrace://rules"), std::string::npos);
    ASSERT_NE(resources.find("mcutrace://schema"), std::string::npos);
    ASSERT_NE(resources.find("mcutrace://project"), std::string::npos);
    const auto rules = request(server,
        R"({"jsonrpc":"2.0","id":12,"method":"resources/read","params":{"uri":"mcutrace://rules"}})");
    ASSERT_NE(rules.find("Plan links and statuses represent work"), std::string::npos);
    const auto schema = request(server,
        R"({"jsonrpc":"2.0","id":13,"method":"resources/read","params":{"uri":"mcutrace://schema"}})");
    ASSERT_NE(schema.find("evidence_expectations"), std::string::npos);
    const auto project = request(server,
        R"({"jsonrpc":"2.0","id":14,"method":"resources/read","params":{"uri":"mcutrace://project"}})");
    ASSERT_NE(project.find("analysis_revision"), std::string::npos);

    const auto unknown = tool_call(server, 15, "does_not_exist");
    ASSERT_NE(unknown.find("mcp.unknown_tool"), std::string::npos);
    const auto unknown_requirement = tool_call(server, 151, "requirement",
                                               R"({"id":"REQ-9999"})");
    ASSERT_NE(unknown_requirement.find("mcutrace.unknown_requirement"), std::string::npos);
    ASSERT_TRUE(request(server,
        R"({"jsonrpc":"2.0","method":"notifications/cancelled","params":{"requestId":15}})").empty());
    const auto recovered = request(server,
        R"({"jsonrpc":"2.0","id":16,"method":"ping","params":{}})");
    ASSERT_NE(recovered.find("\"result\":{}"), std::string::npos);
}

TEST(mcp, refreshes_changed_inputs_and_keeps_revision_stable_otherwise,
     "REQ-0139", "REQ-0142", "REQ-0145", "REQ-0146", "REQ-0157", "REQ-0159") {
    const auto workspace = create_workspace();
    const auto config = setup_workspace(workspace);
    mcutrace::McpServer server(config.generic_string());
    initialize(server);

    const auto first = tool_call(server, 2, "project_overview");
    const auto second = tool_call(server, 3, "project_overview");
    ASSERT_EQ(first.substr(first.find("analysis_revision"), 64),
              second.substr(second.find("analysis_revision"), 64));

    ASSERT_TRUE(write_file(workspace.root / "requirements.md",
        "### REQ-0138 Root capability @evidence(none)\n"
        "The root capability is exposed to agents.\n"
        "\n"
        "### REQ-0139 Dependent capability @evidence(none) @depends(REQ-0138)\n"
        "The dependent capability supplies transitive context.\n"
        "\n"
        "### REQ-0140 Newly added capability @evidence(none)\n"
        "The new requirement must appear after refresh.\n"));
    const auto changed = tool_call(server, 4, "project_overview");
    ASSERT_NE(first.substr(first.find("analysis_revision"), 64),
              changed.substr(changed.find("analysis_revision"), 64));
    const auto search = tool_call(server, 5, "search", R"({"query":"Newly added"})");
    ASSERT_NE(search.find("REQ-0140"), std::string::npos);
}

TEST(mcp, handles_stdio_protocol_and_recovers_after_malformed_input,
     "REQ-0138", "REQ-0140", "REQ-0141", "REQ-0156", "REQ-0159", "REQ-0160") {
    const auto workspace = create_workspace();
    const auto config = setup_workspace(workspace);
    std::istringstream input(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25"}})"
        "\n{not-json}\n"
        R"({"jsonrpc":"2.0","id":2,"method":"resources/list","params":{}})"
        "\n");
    std::ostringstream output;
    std::ostringstream errors;
    ASSERT_EQ(mcutrace::run_mcp_server(input, output, errors, config.generic_string()), 0);
    ASSERT_NE(output.str().find("\"id\":1"), std::string::npos);
    ASSERT_NE(output.str().find("\"code\":-32700"), std::string::npos);
    ASSERT_NE(output.str().find("mcutrace://rules"), std::string::npos);
    ASSERT_TRUE(errors.str().empty());
}

TEST(mcp, rejects_unsupported_protocol_revision_without_terminating,
     "REQ-0141", "REQ-0156") {
    const auto workspace = create_workspace();
    const auto config = setup_workspace(workspace);
    mcutrace::McpServer server(config.generic_string());
    const auto rejected = request(server,
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"1999-01-01"}})");
    ASSERT_NE(rejected.find("unsupported MCP protocol revision"), std::string::npos);
    const auto accepted = request(server,
        R"({"jsonrpc":"2.0","id":2,"method":"initialize","params":{"protocolVersion":"2025-06-18"}})");
    ASSERT_NE(accepted.find("\"protocolVersion\":\"2025-06-18\""), std::string::npos);
}

TEST(mcp, reports_project_loading_failures_and_recovers_after_fix,
     "REQ-0142", "REQ-0156", "REQ-0157", "REQ-0159") {
    const auto workspace = create_workspace();
    const auto config = workspace.root / "mcutrace.toml";
    ASSERT_TRUE(write_file(config, "[project\nroot = \".\"\n"));
    mcutrace::McpServer server(config.generic_string());
    initialize(server);

    const auto invalid = tool_call(server, 2, "project_overview");
    ASSERT_NE(invalid.find("\"isError\":true"), std::string::npos);
    ASSERT_NE(invalid.find("project.configuration"), std::string::npos);

    static_cast<void>(setup_workspace(workspace));
    const auto recovered = tool_call(server, 3, "project_overview");
    ASSERT_NE(recovered.find("\"isError\":false"), std::string::npos);
    ASSERT_NE(recovered.find("\"requirements\":2"), std::string::npos);

    ASSERT_TRUE(write_file(workspace.root / "requirements.md",
        "### REQ-0138 Valid requirement @evidence(none)\n"
        "This valid requirement remains queryable.\n"
        "\n"
        "### REQ-0139 Partially invalid requirement @evidence(none) @depends(REQ-bad)\n"
        "The malformed dependency produces a diagnostic.\n"));
    const auto partial = tool_call(server, 4, "project_overview");
    ASSERT_NE(partial.find("\"isError\":false"), std::string::npos);
    ASSERT_NE(partial.find("MTR-REQ-INVALID-DEPENDENCY"), std::string::npos);
    const auto unaffected = tool_call(server, 5, "search", R"({"query":"Valid requirement"})");
    ASSERT_NE(unaffected.find("REQ-0138"), std::string::npos);
}

TEST(mcp, terminates_cycle_safe_prerequisite_and_impact_queries,
     "REQ-0148", "REQ-0149", "REQ-0155", "REQ-0159") {
    const auto workspace = create_workspace();
    const auto config = setup_workspace(workspace);
    ASSERT_TRUE(write_file(workspace.root / "requirements.md",
        "### REQ-0138 First cycle member @evidence(none) @depends(REQ-0139)\n"
        "The first member depends on the second.\n"
        "\n"
        "### REQ-0139 Second cycle member @evidence(none) @depends(REQ-0138)\n"
        "The second member depends on the first.\n"));
    mcutrace::McpServer server(config.generic_string());
    initialize(server);

    const auto context = tool_call(server, 2, "requirement_context",
                                   R"({"id":"REQ-0138"})");
    ASSERT_NE(context.find("REQ-0139"), std::string::npos);
    ASSERT_LT(context.size(), std::size_t{50000});
    const auto impact = tool_call(server, 3, "change_impact", R"({"id":"REQ-0138"})");
    ASSERT_NE(impact.find("REQ-0139"), std::string::npos);
    ASSERT_NE(impact.find("\"complete\":true"), std::string::npos);
    ASSERT_LT(impact.size(), std::size_t{50000});
}

}  // namespace

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutrace::test::run(argc, argv, runner);
}
