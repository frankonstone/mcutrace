#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <mcutrace/error.hpp>

namespace mcutrace {

enum class NodeKind : std::uint8_t {
    requirement,
    source,
    test,
    coverage,
    finding,
    artifact,
};

[[nodiscard]] std::string_view node_kind_name(NodeKind kind) noexcept;

enum class EvidenceState : std::uint8_t {
    unknown,
    passed,
    failed,
};

[[nodiscard]] std::string_view evidence_state_name(EvidenceState state) noexcept;

struct Node final {
    std::string id;
    NodeKind kind = NodeKind::artifact;
    std::string label;
    EvidenceState evidence_state = EvidenceState::unknown;
    std::optional<SourceLocation> source;

    friend bool operator==(const Node&, const Node&) = default;
};

enum class RelationshipKind : std::uint8_t {
    satisfies,
    verifies,
    implements,
    covers,
    reports,
    relates,
    custom,
};

[[nodiscard]] std::string_view relationship_kind_name(RelationshipKind kind) noexcept;

struct RelationshipType final {
    RelationshipKind kind = RelationshipKind::relates;
    std::string name;

    [[nodiscard]] static RelationshipType known(RelationshipKind kind);
    [[nodiscard]] static RelationshipType custom(std::string name);
    [[nodiscard]] std::string_view display_name() const noexcept;

    friend bool operator==(const RelationshipType&, const RelationshipType&) = default;
};

struct Provenance final {
    std::string importer;
    std::string artifact;
    std::optional<SourceLocation> source;

    friend bool operator==(const Provenance&, const Provenance&) = default;
};

struct Edge final {
    std::string source_id;
    std::string target_id;
    RelationshipType type;
    Provenance provenance;
    std::optional<SourceLocation> source;

    friend bool operator==(const Edge&, const Edge&) = default;
};

class Graph final {
  public:
    [[nodiscard]] std::expected<void, Error> add_node(Node node);
    [[nodiscard]] std::expected<void, Error> add_edge(Edge edge);

    [[nodiscard]] const std::vector<Node>& nodes() const noexcept { return nodes_; }
    [[nodiscard]] const std::vector<Edge>& edges() const noexcept { return edges_; }
    [[nodiscard]] const Node* find_node(std::string_view id) const noexcept;

  private:
    std::vector<Node> nodes_;
    std::vector<Edge> edges_;
};

}  // namespace mcutrace
