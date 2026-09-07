#include <mcutest/mcutest.hpp>

#include "test_runner.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace {

const std::filesystem::path kRepositoryRoot = MCUTRACE_REPOSITORY_ROOT;

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

bool contains(std::string_view text, std::string_view value) {
    return text.find(value) != std::string_view::npos;
}

}  // namespace

TEST(vscode_agent, declares_supported_skill_provider_and_settings,
     "REQ-0178", "REQ-0179", "REQ-0180", "REQ-0181") {
    const auto manifest = read_file(kRepositoryRoot / "editors" / "vscode" / "package.json");

    ASSERT_TRUE(contains(manifest, R"("vscode": "^1.109.0")"));
    ASSERT_TRUE(contains(manifest, R"("chatSkills")"));
    ASSERT_TRUE(contains(manifest, R"("./skills/mcutrace/SKILL.md")"));
    ASSERT_TRUE(contains(manifest, R"("mcpServerDefinitionProviders")"));
    ASSERT_TRUE(contains(manifest, R"("id": "mcutrace.agent.mcp")"));
    ASSERT_TRUE(contains(manifest, R"("mcutrace.showAgentIntegrationStatus")"));

    for (const std::string_view setting : {
             "mcutrace.agent.enabled", "mcutrace.agent.skill.enabled",
             "mcutrace.agent.mcp.enabled", "mcutrace.agent.mcp.path",
             "mcutrace.agent.mcp.arguments", "mcutrace.agent.mcp.configPath"}) {
        ASSERT_TRUE(contains(manifest, setting));
    }
}

TEST(vscode_agent, implements_isolated_live_and_safe_folder_definitions,
     "REQ-0180", "REQ-0182", "REQ-0183", "REQ-0184", "REQ-0185",
     "REQ-0186", "REQ-0187", "REQ-0188", "REQ-0189") {
    const auto integration =
        read_file(kRepositoryRoot / "editors" / "vscode" / "agent-integration.js");

    for (const std::string_view behavior : {
             "registerMcpServerDefinitionProvider", "McpStdioServerDefinition",
             "getConfiguration('mcutrace', folder.uri)", "${workspaceFolder}",
             "mcutrace.toml", "--config", "cwd", "workspace.isTrusted",
             "onDidChangeConfiguration", "onDidChangeWorkspaceFolders",
             "onDidGrantWorkspaceTrust", "mcutrace.agent.skillAvailable",
             "mcutrace.agent.mcpAvailable", "mcutrace Agent Integration"}) {
        ASSERT_TRUE(contains(integration, behavior));
    }
    const auto activation_start = integration.find("async function activateAgentIntegration");
    ASSERT_TRUE(activation_start != std::string::npos);
    const std::string_view automatic_integration(integration.data() + activation_start,
                                                 integration.size() - activation_start);
    ASSERT_FALSE(contains(automatic_integration, "child_process"));
    ASSERT_FALSE(contains(automatic_integration, "workspace.fs.writeFile"));
    ASSERT_FALSE(contains(automatic_integration, ".vscode/mcp.json"));
}

TEST(vscode_agent, packages_and_verifies_cross_platform_payload,
     "REQ-0179", "REQ-0190", "REQ-0191") {
    const auto staging =
        read_file(kRepositoryRoot / "editors" / "vscode" / "scripts" / "stage-server.js");
    const auto verification =
        read_file(kRepositoryRoot / "editors" / "vscode" / "scripts" / "verify-vsix.cmake");
    const auto workflow = read_file(kRepositoryRoot / ".github" / "workflows" / "ci.yml");
    const auto fixture_config =
        read_file(kRepositoryRoot / "tests" / "fixtures" / "mcp-project" / "mcutrace.toml");
    const auto fixture_source =
        read_file(kRepositoryRoot / "tests" / "fixtures" / "mcp-project" / "source.cpp");

    ASSERT_TRUE(contains(staging, "mcutrace-lsp"));
    ASSERT_TRUE(contains(staging, "mcutrace-mcp"));
    ASSERT_TRUE(contains(staging, "integrations', 'skills', 'mcutrace"));
    ASSERT_TRUE(contains(staging, "platform === 'win32'"));
    ASSERT_TRUE(contains(verification, "extension/skills/mcutrace/SKILL.md"));
    ASSERT_TRUE(contains(verification, "extension/bin/mcutrace-mcp"));
    ASSERT_TRUE(contains(workflow, "darwin-arm64"));
    ASSERT_TRUE(contains(workflow, "linux-x64"));
    ASSERT_TRUE(contains(workflow, "win32-x64"));
    ASSERT_TRUE(contains(workflow, "verify-vsix.cmake"));
    ASSERT_TRUE(contains(fixture_config, R"(sources = ["source.cpp"])"));
    ASSERT_TRUE(contains(fixture_source, "@req REQ-0139"));
}

TEST(vscode_agent, sets_up_codex_skill_and_mcp_in_selected_trusted_workspace,
     "REQ-0221", "REQ-0222") {
    const auto manifest = read_file(kRepositoryRoot / "editors" / "vscode" / "package.json");
    const auto integration =
        read_file(kRepositoryRoot / "editors" / "vscode" / "agent-integration.js");
    const auto tests = read_file(
        kRepositoryRoot / "editors" / "vscode" / "test" / "agent-integration.test.js");

    ASSERT_TRUE(contains(manifest, R"("mcutrace.setupCodex")"));
    ASSERT_TRUE(contains(integration, "'.agents', 'skills', 'mcutrace'"));
    ASSERT_TRUE(contains(integration, "'.codex', 'config.toml'"));
    ASSERT_TRUE(contains(integration, "mcp_servers.mcutrace"));
    ASSERT_TRUE(contains(integration, "globalStorageUri"));
    ASSERT_TRUE(contains(integration, "showWorkspaceFolderPick"));
    ASSERT_TRUE(contains(integration, "workspace.isTrusted === false"));
    ASSERT_TRUE(contains(integration, "workspace.fs.copy(source, target, { overwrite: false })"));
    ASSERT_TRUE(contains(integration, "workspace.fs.copy(source, target, { overwrite: true })"));
    ASSERT_TRUE(contains(tests, "without overwriting an existing workspace skill"));
    ASSERT_TRUE(contains(tests, "without replacing user-owned entries"));
    ASSERT_TRUE(contains(tests, "installs both skill and MCP configuration"));
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutrace::test::run(argc, argv, runner);
}
