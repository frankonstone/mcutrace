#include <mcutrace/model.hpp>

#include <algorithm>
#include <utility>

namespace mcutrace {
namespace {

bool location_less(const std::optional<SourceLocation>& lhs,
                   const std::optional<SourceLocation>& rhs) noexcept {
    if (!lhs.has_value()) {
        return rhs.has_value();
    }
    if (!rhs.has_value()) {
        return false;
    }
    if (lhs->path != rhs->path) {
        return lhs->path < rhs->path;
    }
    if (lhs->line != rhs->line) {
        return lhs->line < rhs->line;
    }
    return lhs->column < rhs->column;
}

bool edge_less(const Edge& lhs, const Edge& rhs) noexcept {
    if (lhs.source_id != rhs.source_id) {
        return lhs.source_id < rhs.source_id;
    }
    if (lhs.target_id != rhs.target_id) {
        return lhs.target_id < rhs.target_id;
    }
    if (lhs.type.display_name() != rhs.type.display_name()) {
        return lhs.type.display_name() < rhs.type.display_name();
    }
    if (lhs.provenance.importer != rhs.provenance.importer) {
        return lhs.provenance.importer < rhs.provenance.importer;
    }
    if (lhs.provenance.artifact != rhs.provenance.artifact) {
        return lhs.provenance.artifact < rhs.provenance.artifact;
    }
    if (lhs.provenance.source != rhs.provenance.source) {
        return location_less(lhs.provenance.source, rhs.provenance.source);
    }
    return location_less(lhs.source, rhs.source);
}

}  // namespace

std::string_view severity_name(Severity severity) noexcept {
    switch (severity) {
    case Severity::note:
        return "note";
    case Severity::warning:
        return "warning";
    case Severity::error:
        return "error";
    }
    return "error";
}

std::string_view error_code_name(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::invalid_node_id:
        return "invalid_node_id";
    case ErrorCode::duplicate_node:
        return "duplicate_node";
    case ErrorCode::invalid_edge:
        return "invalid_edge";
    case ErrorCode::duplicate_edge:
        return "duplicate_edge";
    case ErrorCode::invalid_requirement_id:
        return "invalid_requirement_id";
    case ErrorCode::requirement_id_space_exhausted:
        return "requirement_id_space_exhausted";
    }
    return "invalid_node_id";
}

std::string_view node_kind_name(NodeKind kind) noexcept {
    switch (kind) {
    case NodeKind::requirement:
        return "requirement";
    case NodeKind::source:
        return "source";
    case NodeKind::implementation:
        return "implementation";
    case NodeKind::test:
        return "test";
    case NodeKind::coverage:
        return "coverage";
    case NodeKind::finding:
        return "finding";
    case NodeKind::artifact:
        return "artifact";
    }
    return "artifact";
}

std::string_view evidence_state_name(EvidenceState state) noexcept {
    switch (state) {
    case EvidenceState::unknown:
        return "unknown";
    case EvidenceState::passed:
        return "passed";
    case EvidenceState::failed:
        return "failed";
    }
    return "unknown";
}

std::string_view relationship_kind_name(RelationshipKind kind) noexcept {
    switch (kind) {
    case RelationshipKind::satisfies:
        return "satisfies";
    case RelationshipKind::verifies:
        return "verifies";
    case RelationshipKind::implements:
        return "implements";
    case RelationshipKind::covers:
        return "covers";
    case RelationshipKind::reports:
        return "reports";
    case RelationshipKind::relates:
        return "relates";
    case RelationshipKind::custom:
        return "custom";
    }
    return "relates";
}

RelationshipType RelationshipType::known(RelationshipKind kind) {
    return RelationshipType{.kind = kind, .name = {}};
}

RelationshipType RelationshipType::custom(std::string name) {
    return RelationshipType{.kind = RelationshipKind::custom, .name = std::move(name)};
}

std::string_view RelationshipType::display_name() const noexcept {
    if (kind == RelationshipKind::custom && !name.empty()) {
        return name;
    }
    return relationship_kind_name(kind);
}

// @req REQ-0020 REQ-0046
std::expected<void, Error> Graph::add_node(Node node) {
    if (node.id.empty()) {
        return std::unexpected(Error{
            .code = ErrorCode::invalid_node_id,
            .detail = "traceability node id must not be empty",
            .source = node.source,
        });
    }

    const auto position = std::lower_bound(
        nodes_.begin(), nodes_.end(), node.id,
        [](const Node& current, std::string_view id) { return current.id < id; });
    if (position != nodes_.end() && position->id == node.id) {
        if (*position == node) {
            return {};
        }
        return std::unexpected(Error{
            .code = ErrorCode::duplicate_node,
            .detail = "conflicting node identity: " + node.id,
            .source = node.source,
        });
    }
    nodes_.insert(position, std::move(node));
    return {};
}

// @req REQ-0027 REQ-0028 REQ-0029 REQ-0030 REQ-0031 REQ-0032
std::expected<void, Error> Graph::add_edge(Edge edge) {
    if (edge.source_id.empty() || edge.target_id.empty() ||
        (edge.type.kind == RelationshipKind::custom && edge.type.name.empty())) {
        return std::unexpected(Error{
            .code = ErrorCode::invalid_edge,
            .detail = "traceability edge requires source, target, and relationship type",
            .source = edge.source,
        });
    }

    const auto position = std::lower_bound(edges_.begin(), edges_.end(), edge, edge_less);
    if (position != edges_.end() && !edge_less(edge, *position) && !edge_less(*position, edge)) {
        if (*position == edge) {
            return {};
        }
        return std::unexpected(Error{
            .code = ErrorCode::duplicate_edge,
            .detail = "conflicting duplicate traceability edge",
            .source = edge.source,
        });
    }
    edges_.insert(position, std::move(edge));
    return {};
}

const Node* Graph::find_node(std::string_view id) const noexcept {
    const auto position = std::lower_bound(
        nodes_.begin(), nodes_.end(), id,
        [](const Node& current, std::string_view value) { return current.id < value; });
    if (position == nodes_.end() || position->id != id) {
        return nullptr;
    }
    return &*position;
}

}  // namespace mcutrace
