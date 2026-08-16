#include <mcutrace/trace_import.hpp>
#include <mcutest/mcutest.hpp>

#include <string>

TEST(trace_import, imports_sidecar_relationships) {
    const mcutrace::ArtifactInput input{
        .path = "/tmp/links.json",
        .content = R"({"format":"mcutrace-links","version":1,"links":[{"source":"test:mcutest:math.adds","target":"REQ-0001","type":"verifies"},{"source":"source:/work/project/src/foo.cpp","target":"REQ-0001","type":"implements"},{"source":"finding:mcucheck:abc","target":"REQ-0002","type":"vendor-risk"}]})",
    };

    const auto result = mcutrace::import_trace_artifact(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->format.schema, std::string("mcutrace-links"));
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(3));
    ASSERT_EQ(result->edges[0].type.kind, mcutrace::RelationshipKind::verifies);
    ASSERT_EQ(result->edges[1].type.kind, mcutrace::RelationshipKind::implements);
    ASSERT_EQ(result->edges[2].type.kind, mcutrace::RelationshipKind::custom);
    ASSERT_EQ(result->edges[2].type.name, std::string("vendor-risk"));
}

TEST(trace_import, augments_mcutest_requirement_links) {
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

TEST(trace_import, augments_mcucov_requirement_links) {
    const mcutrace::ArtifactInput input{
        .path = "/tmp/coverage.json",
        .base_directory = "/work/project",
        .content = R"({"format":"mcucov-report","version":1,"modules":[{"path":"src/foo.cpp","variant":"host","requirements":["REQ-0003"]}]})",
    };

    const auto result = mcutrace::import_trace_artifact(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(3));
    ASSERT_EQ(result->edges[1].target_id, std::string("REQ-0003"));
    ASSERT_EQ(result->edges[1].type.kind, mcutrace::RelationshipKind::covers);
    ASSERT_EQ(result->edges[2].source_id, std::string("source:/work/project/src/foo.cpp"));
    ASSERT_EQ(result->edges[2].type.kind, mcutrace::RelationshipKind::implements);
}

TEST(trace_import, augments_mcucheck_requirement_links) {
    const mcutrace::ArtifactInput input{
        .path = "/tmp/check.json",
        .base_directory = "/work/project",
        .content = R"({"format":"mcucheck-results","version":1,"diagnostics":[{"rule_id":"A1","message":"bad","id":"abc","location":{"path":"src/foo.cpp","line":7,"column":2},"requirements":["REQ-0004"]}]})",
    };

    const auto result = mcutrace::import_trace_artifact(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->edges[1].source_id, std::string("finding:mcucheck:abc"));
    ASSERT_EQ(result->edges[1].target_id, std::string("REQ-0004"));
    ASSERT_EQ(result->edges[1].type.kind, mcutrace::RelationshipKind::reports);
}

TEST(trace_import, diagnoses_invalid_requirement_reference) {
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
    mcutest::Runner<mcutest::StdoutOutput> runner;
    return mcutest::run_with_gtest_compat(argc, argv, runner);
}
