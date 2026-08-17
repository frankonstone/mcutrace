#include <mcutrace/producer_importers.hpp>
#include <mcutest/mcutest.hpp>

#include <string>

TEST(producer_importers, imports_mcutest_results) {
    const mcutrace::ArtifactInput input{
        .path = "/tmp/tests.json",
        .base_directory = "/work/project",
        .content = R"({"format":"mcutest-results","version":1,"tests":[{"name":"math.adds","status":"passed"},{"name":"math.fails","status":"failed"}]})",
    };
    const auto result = mcutrace::import_producer_artifact(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->format.producer, std::string("mcutest"));
    ASSERT_EQ(result->nodes.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->nodes[0].kind, mcutrace::NodeKind::test);
    ASSERT_EQ(result->nodes[0].evidence_state, mcutrace::EvidenceState::passed);
    ASSERT_EQ(result->nodes[1].evidence_state, mcutrace::EvidenceState::failed);
}

TEST(producer_importers, imports_mcucov_report) {
    const mcutrace::ArtifactInput input{
        .path = "/tmp/coverage.json",
        .base_directory = "/work/project",
        .content = R"({"format":"mcucov-report","version":1,"modules":[{"id":1,"path":"src/foo.cpp","variant":"host","skipped":[{"state":"not-instrumented","severity":"warning","line":12,"column":3,"detail":"unsupported construct"}]}]})",
    };
    const auto result = mcutrace::import_producer_artifact(input, "mcucov");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->format.schema, std::string("mcucov-report"));
    ASSERT_EQ(result->nodes.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->edges[0].type.kind, mcutrace::RelationshipKind::covers);
    ASSERT_EQ(result->diagnostics.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->diagnostics[0].source->path, std::string("/work/project/src/foo.cpp"));
}

TEST(producer_importers, streams_mcucov_reports_larger_than_dom_string_capacity) {
    std::string content = R"({"format":"mcucov-report","version":1,"modules":[{"path":"src/foo.cpp","variant":"host","probes":[{"name":")";
    content.append(70000, 'x');
    content += R"("}],"skipped":[]}]})";

    const mcutrace::ArtifactInput input{
        .path = "/tmp/large-coverage.json",
        .base_directory = "/work/project",
        .content = content,
    };
    const auto result = mcutrace::import_producer_artifact(input, "mcucov");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->nodes.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->nodes[0].kind, mcutrace::NodeKind::coverage);
    ASSERT_EQ(result->nodes[1].id, std::string("source:/work/project/src/foo.cpp"));
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(1));
}

TEST(producer_importers, imports_mcucheck_results) {
    const mcutrace::ArtifactInput input{
        .path = "/tmp/check.json",
        .base_directory = "/work/project",
        .content = R"({"format":"mcucheck-results","version":1,"diagnostics":[{"rule_id":"AUTOSAR-A1","message":"bad thing","severity":"warning","state":"violation","id":"abc123","location":{"path":"src/foo.cpp","line":7,"column":2}}]})",
    };
    const auto result = mcutrace::import_producer_artifact(input);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->format.producer, std::string("mcucheck"));
    ASSERT_EQ(result->nodes.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->nodes[0].kind, mcutrace::NodeKind::finding);
    ASSERT_EQ(result->edges.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->edges[0].type.kind, mcutrace::RelationshipKind::reports);
    ASSERT_EQ(result->nodes[0].source->line, static_cast<std::uint32_t>(7));
}

TEST(producer_importers, rejects_unsupported_version) {
    const mcutrace::ArtifactInput input{
        .path = "coverage.json",
        .content = R"({"format":"mcucov-report","version":2,"modules":[]})",
    };
    const auto result = mcutrace::import_producer_artifact(input);
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, mcutrace::ImportErrorCode::unsupported_version);
}

TEST(producer_importers, rejects_wrong_explicit_importer) {
    const mcutrace::ArtifactInput input{
        .path = "check.json",
        .content = R"({"format":"mcucheck-results","version":1,"diagnostics":[]})",
    };
    const auto result = mcutrace::import_producer_artifact(input, "mcucov");
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, mcutrace::ImportErrorCode::unrecognized_format);
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::StdoutOutput> runner;
    return mcutest::run_with_gtest_compat(argc, argv, runner);
}
