#include <mcutrace/model.hpp>
#include <mcutest/mcutest.hpp>

#include <string>

TEST(model, exposes_stable_node_kind_and_severity_names) {
    ASSERT_EQ(std::string(mcutrace::node_kind_name(mcutrace::NodeKind::requirement)),
              std::string("requirement"));
    ASSERT_EQ(std::string(mcutrace::node_kind_name(mcutrace::NodeKind::test)),
              std::string("test"));
    ASSERT_EQ(std::string(mcutrace::severity_name(mcutrace::Severity::warning)),
              std::string("warning"));
}

TEST(model, preserves_known_and_custom_relationship_types) {
    const auto verifies =
        mcutrace::RelationshipType::known(mcutrace::RelationshipKind::verifies);
    const auto custom = mcutrace::RelationshipType::custom("derived-from");

    ASSERT_EQ(std::string(verifies.display_name()), std::string("verifies"));
    ASSERT_EQ(std::string(custom.display_name()), std::string("derived-from"));
    ASSERT_EQ(custom.kind, mcutrace::RelationshipKind::custom);
}

TEST(model, stores_nodes_in_deterministic_identity_order) {
    mcutrace::Graph graph;
    ASSERT_TRUE(graph.add_node({.id = "TEST-0002", .kind = mcutrace::NodeKind::test}).has_value());
    ASSERT_TRUE(graph.add_node({.id = "REQ-0001", .kind = mcutrace::NodeKind::requirement}).has_value());
    ASSERT_TRUE(graph.add_node({.id = "SRC-main", .kind = mcutrace::NodeKind::source}).has_value());

    ASSERT_EQ(graph.nodes().size(), static_cast<std::size_t>(3));
    ASSERT_EQ(graph.nodes()[0].id, std::string("REQ-0001"));
    ASSERT_EQ(graph.nodes()[1].id, std::string("SRC-main"));
    ASSERT_EQ(graph.nodes()[2].id, std::string("TEST-0002"));
    ASSERT_TRUE(graph.find_node("SRC-main") != nullptr);
    ASSERT_TRUE(graph.find_node("missing") == nullptr);
}

TEST(model, accepts_identical_node_reinsertion_and_rejects_conflicts) {
    mcutrace::Graph graph;
    const mcutrace::Node requirement{
        .id = "REQ-0001",
        .kind = mcutrace::NodeKind::requirement,
        .label = "First requirement",
        .source = mcutrace::SourceLocation{.path = "requirements.md", .line = 10, .column = 1},
    };

    ASSERT_TRUE(graph.add_node(requirement).has_value());
    ASSERT_TRUE(graph.add_node(requirement).has_value());

    const auto conflict = graph.add_node({
        .id = "REQ-0001",
        .kind = mcutrace::NodeKind::test,
        .label = "Conflicting identity",
    });
    ASSERT_FALSE(conflict.has_value());
    ASSERT_EQ(conflict.error().code, mcutrace::ErrorCode::duplicate_node);
    ASSERT_EQ(graph.nodes().size(), static_cast<std::size_t>(1));
}

TEST(model, rejects_empty_node_identity_with_source_context) {
    mcutrace::Graph graph;
    const auto result = graph.add_node({
        .id = "",
        .kind = mcutrace::NodeKind::artifact,
        .source = mcutrace::SourceLocation{.path = "input.json", .line = 4, .column = 2},
    });

    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, mcutrace::ErrorCode::invalid_node_id);
    ASSERT_TRUE(result.error().source.has_value());
    ASSERT_EQ(result.error().source->path, std::string("input.json"));
    ASSERT_EQ(result.error().source->line, static_cast<std::uint32_t>(4));
}

TEST(model, preserves_multiple_edge_provenance_sources) {
    mcutrace::Graph graph;
    const auto type = mcutrace::RelationshipType::known(mcutrace::RelationshipKind::verifies);

    ASSERT_TRUE(graph.add_edge({
        .source_id = "TEST-0001",
        .target_id = "REQ-0001",
        .type = type,
        .provenance = {.importer = "mcutest", .artifact = "unit.json"},
    }).has_value());
    ASSERT_TRUE(graph.add_edge({
        .source_id = "TEST-0001",
        .target_id = "REQ-0001",
        .type = type,
        .provenance = {.importer = "external", .artifact = "system.json"},
    }).has_value());

    ASSERT_EQ(graph.edges().size(), static_cast<std::size_t>(2));
    ASSERT_EQ(graph.edges()[0].provenance.importer, std::string("external"));
    ASSERT_EQ(graph.edges()[1].provenance.importer, std::string("mcutest"));
}

TEST(model, preserves_dangling_edges_for_later_validation) {
    mcutrace::Graph graph;
    const auto result = graph.add_edge({
        .source_id = "UNKNOWN-TEST",
        .target_id = "REQ-9999",
        .type = mcutrace::RelationshipType::custom("evidence-for"),
        .provenance = {
            .importer = "third-party",
            .artifact = "trace.json",
            .source = mcutrace::SourceLocation{.path = "trace.json", .line = 7, .column = 1},
        },
        .source = mcutrace::SourceLocation{.path = "trace.json", .line = 7, .column = 5},
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(graph.edges().size(), static_cast<std::size_t>(1));
    ASSERT_EQ(std::string(graph.edges()[0].type.display_name()), std::string("evidence-for"));
    ASSERT_EQ(graph.edges()[0].source->line, static_cast<std::uint32_t>(7));
}

TEST(model, rejects_invalid_edges_without_throwing) {
    mcutrace::Graph graph;
    const auto result = graph.add_edge({
        .source_id = "REQ-0001",
        .target_id = "",
        .type = mcutrace::RelationshipType::known(mcutrace::RelationshipKind::relates),
        .provenance = {.importer = "test", .artifact = "fixture"},
    });

    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().code, mcutrace::ErrorCode::invalid_edge);
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::StdoutOutput> runner;
    return mcutest::run_with_gtest_compat(argc, argv, runner);
}
