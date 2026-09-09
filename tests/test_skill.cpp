#include <mcujson/mcujson.hpp>
#include <mcutest/mcutest.hpp>

#include "test_runner.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace {

using SkillJson = mcujson::BasicJson<512U, 40000U>;

const std::filesystem::path kSkillDirectory = MCUTRACE_SKILL_DIRECTORY;

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

bool contains(std::string_view text, std::string_view value) {
    return text.find(value) != std::string_view::npos;
}

std::size_t line_count(std::string_view text) {
    return static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n')) + 1U;
}

}  // namespace

TEST(skill, has_portable_metadata_and_mcp_dependency,
     "REQ-0162", "REQ-0163", "REQ-0164", "REQ-0176", "REQ-0177") {
    const auto skill = read_file(kSkillDirectory / "SKILL.md");
    const auto metadata = read_file(kSkillDirectory / "agents" / "openai.yaml");

    ASSERT_TRUE(skill.starts_with("---\nname: mcutrace\n"));
    ASSERT_TRUE(contains(skill, "description:"));
    ASSERT_TRUE(contains(skill, "Create a new mcutrace project"));
    ASSERT_TRUE(contains(skill, "add mcutrace to an existing project"));
    ASSERT_TRUE(contains(skill, "Do not select it for unrelated project work"));
    ASSERT_TRUE(contains(skill, "an accessible read-only mcutrace MCP server"));
    ASSERT_TRUE(line_count(skill) < static_cast<std::size_t>(200));
    ASSERT_EQ(skill.find("TODO"), std::string::npos);
    ASSERT_EQ(skill.find("[PLACEHOLDER]"), std::string::npos);

    ASSERT_TRUE(contains(metadata, "display_name: \"mcutrace\""));
    ASSERT_TRUE(contains(metadata, "allow_implicit_invocation: true"));
    ASSERT_TRUE(contains(metadata, "type: \"mcp\""));
    ASSERT_TRUE(contains(metadata, "value: \"mcutrace\""));
    ASSERT_TRUE(contains(metadata, "provided by mcutrace-mcp"));
}

TEST(skill, creates_new_projects_and_integrates_existing_projects,
     "REQ-0163", "REQ-0164", "REQ-0173", "REQ-0175", "REQ-0223", "REQ-0224") {
    const auto skill = read_file(kSkillDirectory / "SKILL.md");
    const auto setup = read_file(kSkillDirectory / "references" / "project-setup.md");

    ASSERT_TRUE(contains(skill, "references/project-setup.md"));
    ASSERT_TRUE(contains(skill, "An absent MCP connection is expected during"));
    ASSERT_TRUE(contains(skill, "Do not turn setup into permission to download software"));
    ASSERT_TRUE(contains(setup, "## Create a new project"));
    ASSERT_TRUE(contains(setup, "## Add mcutrace to an existing project"));
    ASSERT_TRUE(contains(setup, "requirements = [\"requirements.md\"]"));
    ASSERT_TRUE(contains(setup, "only files that exist"));
    ASSERT_TRUE(contains(setup, "Do not fabricate evidence files"));
    ASSERT_TRUE(contains(setup, "Preserve an existing skill"));
    ASSERT_TRUE(contains(setup, "mcutrace --config mcutrace.toml validate"));
    ASSERT_TRUE(contains(setup, "mcutrace://rules"));
    ASSERT_TRUE(contains(setup, "exact remaining action"));
}

TEST(skill, orients_and_routes_queries_without_rebuilding_semantics,
     "REQ-0165", "REQ-0166", "REQ-0167", "REQ-0168", "REQ-0169") {
    const auto skill = read_file(kSkillDirectory / "SKILL.md");

    for (const std::string_view required : {
             "mcutrace://rules", "mcutrace://project", "mcutrace://schema",
             "project_overview", "search", "requirement", "requirement_context",
             "change_impact", "plan", "validate", "analysis_revision",
             "continuation cursors", "filtered, complete, or truncated"}) {
        ASSERT_TRUE(contains(skill, required));
    }
    ASSERT_TRUE(contains(skill, "Do not reconstruct those semantics"));
    ASSERT_TRUE(contains(skill, "smallest useful query"));
    ASSERT_TRUE(contains(skill, "do not merge old and"));
    ASSERT_TRUE(contains(skill, "new snapshots into one conclusion"));
}

TEST(skill, preserves_semantics_conventions_and_authorization,
     "REQ-0170", "REQ-0171", "REQ-0172", "REQ-0173", "REQ-0174") {
    const auto skill = read_file(kSkillDirectory / "SKILL.md");

    ASSERT_TRUE(contains(skill, "absent evidence distinct from evidence that ran and failed"));
    ASSERT_TRUE(contains(skill, "prerequisites distinct from dependents"));
    ASSERT_TRUE(contains(skill, "plan links and plan status as planning metadata"));
    ASSERT_TRUE(contains(skill, "not infer links from proximity or prose"));
    ASSERT_TRUE(contains(skill, "Follow the project's existing structure"));
    ASSERT_TRUE(contains(skill, "MCP calls are read-only and grant no permission"));
    ASSERT_TRUE(contains(skill, "which builds, tests, coverage runs, static analysis"));
    ASSERT_TRUE(contains(skill, "validation query reads current\nartifacts; it does not execute"));
}

TEST(skill, gives_actionable_degraded_behavior, "REQ-0175") {
    const auto skill = read_file(kSkillDirectory / "SKILL.md");

    ASSERT_TRUE(contains(skill, "project cannot load"));
    ASSERT_TRUE(contains(skill, "server is unavailable"));
    ASSERT_TRUE(contains(skill, "needed capability"));
    ASSERT_TRUE(contains(skill, "is absent"));
    ASSERT_TRUE(contains(skill, "actionable connection"));
    ASSERT_TRUE(contains(skill, "Do not replace missing MCP context"));
}

TEST(skill, provides_repeatable_behavioral_scenarios,
     "REQ-0163", "REQ-0164", "REQ-0166", "REQ-0167", "REQ-0168", "REQ-0169",
     "REQ-0170", "REQ-0171", "REQ-0172", "REQ-0173", "REQ-0174", "REQ-0175",
     "REQ-0177") {
    const auto evals_text = read_file(kSkillDirectory / "evals" / "evals.json");
    auto parsed = SkillJson::parse(evals_text);
    ASSERT_TRUE(parsed.has_value());

    const auto& root = *parsed;
    ASSERT_TRUE(root["skill_name"].is_string());
    ASSERT_EQ(root["skill_name"].get<std::string>(), std::string("mcutrace"));
    ASSERT_TRUE(root["evals"].is_array());
    ASSERT_EQ(root["evals"].size(), static_cast<std::size_t>(13));

    for (std::size_t index = 0; index < root["evals"].size(); ++index) {
        const auto scenario = root["evals"][index];
        ASSERT_TRUE(scenario.is_object());
        ASSERT_TRUE(scenario["prompt"].is_string());
        ASSERT_TRUE(scenario["expected_output"].is_string());
        ASSERT_TRUE(scenario["files"].is_array());
        ASSERT_TRUE(scenario["expectations"].is_array());
        ASSERT_TRUE(scenario["expectations"].size() >= static_cast<std::size_t>(1));
    }

    ASSERT_TRUE(contains(evals_text, "search result is truncated"));
    ASSERT_TRUE(contains(evals_text, "revision a81"));
    ASSERT_TRUE(contains(evals_text, "no coverage artifact"));
    ASSERT_TRUE(contains(evals_text, "MCP server is unavailable"));
    ASSERT_TRUE(contains(evals_text, "happens to contain mcutrace.toml"));
    ASSERT_TRUE(contains(evals_text, "Create a new mcutrace project"));
    ASSERT_TRUE(contains(evals_text, "Add mcutrace to this existing JavaScript project"));
    ASSERT_TRUE(contains(evals_text, "acceptance-criteria sections"));
    ASSERT_TRUE(contains(evals_text, "does not advertise change_impact"));
    ASSERT_TRUE(contains(evals_text, "project.configuration"));
    ASSERT_TRUE(contains(evals_text, "loaded partially"));
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutrace::test::run(argc, argv, runner);
}
