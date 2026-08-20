#include <mcutrace/assembly.hpp>
#include <mcutest/mcutest.hpp>

#include "test_runner.hpp"

#include <array>
#include <string>

namespace {

mcutrace::Requirement requirement(std::string id, std::string title, std::uint32_t line) {
    return mcutrace::Requirement{
        .id = std::move(id),
        .title = std::move(title),
        .body = {},
        .source = mcutrace::SourceLocation{.path = "requirements.md", .line = line, .column = 1},
        .heading_level = 3,
    };
}

mcutrace::Edge verifies(std::string source, std::string target, std::string artifact) {
    return mcutrace::Edge{
        .source_id = std::move(source),
        .target_id = std::move(target),
        .type = mcutrace::RelationshipType::known(mcutrace::RelationshipKind::verifies),
        .provenance = mcutrace::Provenance{
            .importer = "example",
            .artifact = std::move(artifact),
        },
    };
}

}  // namespace

TEST(assembly, merges_requirements_and_imported_nodes, "REQ-0001", "REQ-0019", "REQ-0021") {
    const std::array requirements{
        requirement("REQ-0002", "Second", 8),
        requirement("REQ-0001", "First", 3),
    };
    mcutrace::ImportFragment fragment;
    fragment.nodes.push_back(mcutrace::Node{
        .id = "test:alpha",
        .kind = mcutrace::NodeKind::test,
        .label = "alpha",
    });

    const std::array fragments{fragment};
    const auto result = mcutrace::assemble_trace(requirements, fragments);

    ASSERT_EQ(result.graph.nodes().size(), static_cast<std::size_t>(3));
    ASSERT_EQ(result.graph.nodes()[0].id, std::string("REQ-0001"));
    ASSERT_EQ(result.graph.nodes()[1].id, std::string("REQ-0002"));
    ASSERT_EQ(result.graph.nodes()[2].id, std::string("test:alpha"));
}

TEST(assembly, preserves_multiple_provenance_sources_for_same_relationship, "REQ-0029", "REQ-0031") {
    mcutrace::ImportFragment first;
    first.edges.push_back(verifies("test:alpha", "REQ-0001", "a.json"));
    mcutrace::ImportFragment second;
    second.edges.push_back(verifies("test:alpha", "REQ-0001", "b.json"));

    const std::array fragments{second, first};
    const auto result = mcutrace::assemble_trace({}, fragments);

    ASSERT_EQ(result.graph.edges().size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result.graph.edges()[0].provenance.artifact, std::string("a.json"));
    ASSERT_EQ(result.graph.edges()[1].provenance.artifact, std::string("b.json"));
}

TEST(assembly, keeps_dangling_edges_for_validation, "REQ-0045", "REQ-0082") {
    mcutrace::ImportFragment fragment;
    fragment.edges.push_back(verifies("test:missing", "REQ-9999", "results.json"));

    const std::array fragments{fragment};
    const auto result = mcutrace::assemble_trace({}, fragments);

    ASSERT_EQ(result.graph.nodes().size(), static_cast<std::size_t>(0));
    ASSERT_EQ(result.graph.edges().size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result.graph.edges().front().target_id, std::string("REQ-9999"));
}

TEST(assembly, canonicalizes_file_level_source_node_locations, "REQ-0020", "REQ-0043") {
    mcutrace::ImportFragment source_fragment;
    source_fragment.nodes.push_back(mcutrace::Node{
        .id = "source:/work/src/main.cpp",
        .kind = mcutrace::NodeKind::source,
        .label = "src/main.cpp",
        .source = mcutrace::SourceLocation{.path = "/work/links.json"},
    });
    mcutrace::ImportFragment coverage;
    coverage.nodes.push_back(mcutrace::Node{
        .id = "source:/work/src/main.cpp",
        .kind = mcutrace::NodeKind::source,
        .label = "/work/src/main.cpp",
        .source = mcutrace::SourceLocation{.path = "/work/src/main.cpp", .line = 42, .column = 7},
    });

    const std::array fragments{source_fragment, coverage};
    const auto result = mcutrace::assemble_trace({}, fragments);

    ASSERT_EQ(result.graph.nodes().size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result.diagnostics.size(), static_cast<std::size_t>(0));
    ASSERT_EQ(result.graph.nodes().front().label, std::string("/work/src/main.cpp"));
    ASSERT_TRUE(result.graph.nodes().front().source.has_value());
    ASSERT_EQ(result.graph.nodes().front().source->path, std::string("/work/src/main.cpp"));
    ASSERT_EQ(result.graph.nodes().front().source->line, static_cast<std::uint32_t>(0));
    ASSERT_EQ(result.graph.nodes().front().source->column, static_cast<std::uint32_t>(0));
}

TEST(assembly, diagnoses_conflicting_duplicate_nodes_deterministically, "REQ-0005", "REQ-0046") {
    mcutrace::ImportFragment first;
    first.nodes.push_back(mcutrace::Node{
        .id = "artifact:build",
        .kind = mcutrace::NodeKind::artifact,
        .label = "zeta",
    });
    mcutrace::ImportFragment second;
    second.nodes.push_back(mcutrace::Node{
        .id = "artifact:build",
        .kind = mcutrace::NodeKind::artifact,
        .label = "alpha",
    });

    const std::array forward{first, second};
    const std::array reverse{second, first};
    const auto a = mcutrace::assemble_trace({}, forward);
    const auto b = mcutrace::assemble_trace({}, reverse);

    ASSERT_EQ(a.graph.nodes().size(), static_cast<std::size_t>(1));
    ASSERT_EQ(b.graph.nodes().size(), static_cast<std::size_t>(1));
    ASSERT_EQ(a.graph.nodes().front().label, std::string("alpha"));
    ASSERT_EQ(b.graph.nodes().front().label, std::string("alpha"));
    ASSERT_EQ(a.diagnostics.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(b.diagnostics.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(a.diagnostics.front().code, std::string("mcutrace.duplicate_node"));
}

TEST(assembly, carries_importer_artifacts_and_diagnostics_deterministically, "REQ-0005", "REQ-0026", "REQ-0036") {
    mcutrace::ImportFragment first;
    first.artifacts.push_back(mcutrace::preserve_json_artifact("b", "raw", "{\"b\":1}"));
    first.diagnostics.push_back(mcutrace::Diagnostic{
        .code = "z.warning",
        .severity = mcutrace::Severity::warning,
        .message = "later",
    });

    mcutrace::ImportFragment second;
    second.artifacts.push_back(mcutrace::preserve_json_artifact("a", "raw", "{\"a\":1}"));
    second.diagnostics.push_back(mcutrace::Diagnostic{
        .code = "a.note",
        .severity = mcutrace::Severity::note,
        .message = "earlier",
    });

    const std::array fragments{first, second};
    const auto result = mcutrace::assemble_trace({}, fragments);

    ASSERT_EQ(result.artifacts.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result.artifacts[0].id, std::string("a"));
    ASSERT_EQ(result.artifacts[1].id, std::string("b"));
    ASSERT_EQ(result.diagnostics.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result.diagnostics[0].code, std::string("a.note"));
    ASSERT_EQ(result.diagnostics[1].code, std::string("z.warning"));
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutrace::test::run(argc, argv, runner);
}
