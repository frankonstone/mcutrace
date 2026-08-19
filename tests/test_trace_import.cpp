#include <mcutrace/trace_import.hpp>
#include <mcutest/mcutest.hpp>

#include <string>

TEST(trace_import, imports_sidecar_relationships, "REQ-0079", "REQ-0081", "REQ-0032") {
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

TEST(trace_import, imports_explicit_sidecar_nodes, "REQ-0081", "REQ-0084") {
    const mcutrace::ArtifactInput input{
        .path = "/tmp/build-links.json",
        .content = R"({"format":"mcutrace-links","version":1,"nodes":[{"id":"artifact:ci:macos","kind":"artifact","label":"macOS CI"}],"links":[{"source":"artifact:ci:macos","target":"REQ-0077","type":"verifies"}]})",
    };

    const auto result = mcutrace::import_trace_artifact(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->nodes.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->nodes[0].id, std::string("artifact:ci:macos"));
    ASSERT_EQ(result->nodes[0].kind, mcutrace::NodeKind::artifact);
    ASSERT_EQ(result->nodes[0].label, std::string("macOS CI"));
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->edges[0].source_id, std::string("artifact:ci:macos"));
    ASSERT_EQ(result->edges[0].target_id, std::string("REQ-0077"));
}

TEST(trace_import, canonicalizes_relative_source_sidecar_ids, "REQ-0043", "REQ-0044", "REQ-0081") {
    const mcutrace::ArtifactInput input{
        .path = "/work/project/dogfood/links.json",
        .base_directory = "/work/project",
        .content = R"({"format":"mcutrace-links","version":1,"nodes":[{"id":"source:src/foo.cpp","kind":"source","label":"src/foo.cpp"}],"links":[{"source":"source:src/foo.cpp","target":"REQ-0001","type":"implements"}]})",
    };

    const auto result = mcutrace::import_trace_artifact(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->nodes.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->nodes[0].id, std::string("source:/work/project/src/foo.cpp"));
    ASSERT_TRUE(result->nodes[0].source.has_value());
    ASSERT_EQ(result->nodes[0].source->path, std::string("/work/project/src/foo.cpp"));
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->edges[0].source_id, std::string("source:/work/project/src/foo.cpp"));
}

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
    return mcutest::run_with_gtest_compat(argc, argv, runner);
}
