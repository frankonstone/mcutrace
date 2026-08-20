#include <mcutrace/trace_import.hpp>
#include <mcutest/mcutest.hpp>

#include "test_runner.hpp"

#include <string>

TEST(trace_import, augments_mcutest_requirement_links, "REQ-0037", "REQ-0079", "REQ-0080") {
    const mcutrace::ArtifactInput input{
        .path = "/tmp/tests.json",
        .content = R"({"format":"mcutest-results","version":1,"tests":[{"name":"math.adds","status":"passed","requirements":["REQ-0001","REQ-0002"]}]})",
    };

    const auto result = mcutrace::import_trace_artifact(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->edges[0].source_id, std::string("test:mcutest:math.adds"));
    ASSERT_EQ(result->edges[0].target_id, std::string("REQ-0001"));
    ASSERT_EQ(result->edges[0].type.kind, mcutrace::RelationshipKind::verifies);
}

TEST(trace_import, augments_mcucov_requirement_links, "REQ-0038", "REQ-0079", "REQ-0080") {
    const mcutrace::ArtifactInput input{
        .path = "/tmp/coverage.json",
        .base_directory = "/work/project",
        .content = R"({"format":"mcucov-report","version":1,"modules":[{"path":"src/foo.cpp","variant":"host","requirements":["REQ-0003"]}]})",
    };

    const auto result = mcutrace::import_trace_artifact(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->edges[1].target_id, std::string("REQ-0003"));
    ASSERT_EQ(result->edges[1].type.kind, mcutrace::RelationshipKind::covers);
}

TEST(trace_import, augments_mcucheck_requirement_links_and_preserves_state, "REQ-0039", "REQ-0079", "REQ-0080", "REQ-0087") {
    const mcutrace::ArtifactInput input{
        .path = "/tmp/check.json",
        .base_directory = "/work/project",
        .content = R"({"format":"mcucheck-results","version":1,"diagnostics":[{"rule_id":"A1","message":"bad","state":"informational","id":"abc","location":{"path":"src/foo.cpp","line":7,"column":2},"requirements":["REQ-0004"]}]})",
    };

    const auto result = mcutrace::import_trace_artifact(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->nodes.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->nodes[0].id, std::string("finding:mcucheck:abc"));
    ASSERT_EQ(result->nodes[0].finding_state, std::string("informational"));
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->edges[1].source_id, std::string("finding:mcucheck:abc"));
    ASSERT_EQ(result->edges[1].target_id, std::string("REQ-0004"));
    ASSERT_EQ(result->edges[1].type.kind, mcutrace::RelationshipKind::reports);
}

TEST(trace_import, diagnoses_invalid_requirement_reference, "REQ-0080") {
    const mcutrace::ArtifactInput input{
        .path = "/tmp/tests.json",
        .content = R"({"format":"mcutest-results","version":1,"tests":[{"name":"math.adds","status":"passed","requirements":["REQ-1"]}]})",
    };

    const auto result = mcutrace::import_trace_artifact(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(0));
    ASSERT_EQ(result->diagnostics.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->diagnostics[0].code, std::string("import.requirements.invalid_id"));
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutrace::test::run(argc, argv, runner);
}
