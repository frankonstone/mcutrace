#include <mcutrace/assembly.hpp>

#include <algorithm>
#include <string_view>
#include <tuple>
#include <utility>

namespace mcutrace {
namespace {

bool location_less(const std::optional<SourceLocation>& lhs,
                   const std::optional<SourceLocation>& rhs) noexcept {
    if (!lhs.has_value()) return rhs.has_value();
    if (!rhs.has_value()) return false;
    return std::tie(lhs->path, lhs->line, lhs->column) <
           std::tie(rhs->path, rhs->line, rhs->column);
}

void canonicalize_node(Node& node) {
    if (node.kind != NodeKind::source) return;

    constexpr std::string_view prefix = "source:";
    if (node.id.starts_with(prefix)) {
        node.source = SourceLocation{.path = node.id.substr(prefix.size())};
        return;
    }

    if (node.source.has_value()) {
        node.source->line = 0;
        node.source->column = 0;
    }
}

bool node_less(const Node& lhs, const Node& rhs) noexcept {
    if (lhs.id != rhs.id) return lhs.id < rhs.id;
    if (lhs.kind != rhs.kind) return lhs.kind < rhs.kind;
    if (lhs.label != rhs.label) return lhs.label < rhs.label;
    if (lhs.evidence_state != rhs.evidence_state) return lhs.evidence_state < rhs.evidence_state;
    if (lhs.expected_evidence != rhs.expected_evidence) return lhs.expected_evidence < rhs.expected_evidence;
    return location_less(lhs.source, rhs.source);
}

bool artifact_less(const GenericArtifact& lhs, const GenericArtifact& rhs) noexcept {
    if (lhs.id != rhs.id) return lhs.id < rhs.id;
    if (lhs.type != rhs.type) return lhs.type < rhs.type;
    if (lhs.media_type != rhs.media_type) return lhs.media_type < rhs.media_type;
    if (lhs.payload != rhs.payload) return lhs.payload < rhs.payload;
    return location_less(lhs.source, rhs.source);
}

bool diagnostic_less(const Diagnostic& lhs, const Diagnostic& rhs) noexcept {
    if (lhs.code != rhs.code) return lhs.code < rhs.code;
    if (lhs.severity != rhs.severity) return lhs.severity < rhs.severity;
    if (lhs.message != rhs.message) return lhs.message < rhs.message;
    return location_less(lhs.source, rhs.source);
}

Diagnostic duplicate_node_diagnostic(const Node& node) {
    return Diagnostic{
        .code = "mcutrace.duplicate_node",
        .severity = Severity::error,
        .message = "conflicting duplicate node identity: " + node.id,
        .source = node.source,
    };
}

}  // namespace

TraceResult assemble_trace(std::span<const Requirement> requirements,
                           std::span<const ImportFragment> fragments) {
    TraceResult result;

    std::vector<Node> nodes;
    nodes.reserve(requirements.size());
    for (const auto& requirement : requirements) {
        nodes.push_back(requirement.as_node());
    }

    std::size_t edge_count = 0;
    std::size_t artifact_count = 0;
    std::size_t diagnostic_count = 0;
    for (const auto& fragment : fragments) {
        nodes.insert(nodes.end(), fragment.nodes.begin(), fragment.nodes.end());
        edge_count += fragment.edges.size();
        artifact_count += fragment.artifacts.size();
        diagnostic_count += fragment.diagnostics.size();
    }

    for (auto& node : nodes) canonicalize_node(node);
    std::sort(nodes.begin(), nodes.end(), node_less);
    for (const auto& node : nodes) {
        const Node* existing = result.graph.find_node(node.id);
        if (existing != nullptr) {
            if (*existing != node) {
                result.diagnostics.push_back(duplicate_node_diagnostic(node));
            }
            continue;
        }
        const auto added = result.graph.add_node(node);
        if (!added) {
            result.diagnostics.push_back(Diagnostic{
                .code = "mcutrace.node_error",
                .severity = Severity::error,
                .message = added.error().detail,
                .source = added.error().source,
            });
        }
    }

    std::vector<Edge> edges;
    edges.reserve(edge_count);
    result.artifacts.reserve(artifact_count);
    result.diagnostics.reserve(result.diagnostics.size() + diagnostic_count);

    for (const auto& fragment : fragments) {
        edges.insert(edges.end(), fragment.edges.begin(), fragment.edges.end());
        result.artifacts.insert(result.artifacts.end(),
                                fragment.artifacts.begin(), fragment.artifacts.end());
        result.diagnostics.insert(result.diagnostics.end(),
                                  fragment.diagnostics.begin(), fragment.diagnostics.end());
    }

    for (const auto& edge : edges) {
        const auto added = result.graph.add_edge(edge);
        if (!added) {
            result.diagnostics.push_back(Diagnostic{
                .code = "mcutrace.edge_error",
                .severity = Severity::error,
                .message = added.error().detail,
                .source = added.error().source,
            });
        }
    }

    std::sort(result.artifacts.begin(), result.artifacts.end(), artifact_less);
    result.artifacts.erase(std::unique(result.artifacts.begin(), result.artifacts.end()),
                           result.artifacts.end());

    std::sort(result.diagnostics.begin(), result.diagnostics.end(), diagnostic_less);
    result.diagnostics.erase(std::unique(result.diagnostics.begin(), result.diagnostics.end()),
                             result.diagnostics.end());

    return result;
}

}  // namespace mcutrace
