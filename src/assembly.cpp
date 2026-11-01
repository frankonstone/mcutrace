#include <mcutrace/assembly.hpp>

#include <algorithm>
#include <string_view>
#include <tuple>

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
    return std::tie(lhs->path, lhs->line, lhs->column) <
           std::tie(rhs->path, rhs->line, rhs->column);
}

// @req REQ-0020
void canonicalize_node(Node& node) {
    if (node.kind != NodeKind::source) {
        return;
    }

    constexpr std::string_view prefix = "source:";
    if (node.id.starts_with(prefix)) {
        const auto path = node.id.substr(prefix.size());
        node.label = path;
        node.source = SourceLocation{.path = path};
        return;
    }

    if (node.source.has_value()) {
        node.source->line = 0;
        node.source->column = 0;
    }
}

bool node_less(const Node& lhs, const Node& rhs) noexcept {
    if (lhs.id != rhs.id) {
        return lhs.id < rhs.id;
    }
    if (lhs.kind != rhs.kind) {
        return lhs.kind < rhs.kind;
    }
    if (lhs.label != rhs.label) {
        return lhs.label < rhs.label;
    }
    if (lhs.evidence_state != rhs.evidence_state) {
        return lhs.evidence_state < rhs.evidence_state;
    }
    if (lhs.finding_state != rhs.finding_state) {
        return lhs.finding_state < rhs.finding_state;
    }
    if (lhs.body != rhs.body) {
        return lhs.body < rhs.body;
    }
    if (lhs.status != rhs.status) {
        return lhs.status < rhs.status;
    }
    if (lhs.expected_evidence != rhs.expected_evidence) {
        return lhs.expected_evidence < rhs.expected_evidence;
    }
    return location_less(lhs.source, rhs.source);
}

bool artifact_less(const GenericArtifact& lhs, const GenericArtifact& rhs) noexcept {
    if (lhs.id != rhs.id) {
        return lhs.id < rhs.id;
    }
    if (lhs.type != rhs.type) {
        return lhs.type < rhs.type;
    }
    if (lhs.media_type != rhs.media_type) {
        return lhs.media_type < rhs.media_type;
    }
    if (lhs.payload != rhs.payload) {
        return lhs.payload < rhs.payload;
    }
    return location_less(lhs.source, rhs.source);
}

bool diagnostic_less(const Diagnostic& lhs, const Diagnostic& rhs) noexcept {
    if (lhs.code != rhs.code) {
        return lhs.code < rhs.code;
    }
    if (lhs.severity != rhs.severity) {
        return lhs.severity < rhs.severity;
    }
    if (lhs.message != rhs.message) {
        return lhs.message < rhs.message;
    }
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

std::vector<Node> collect_nodes(std::span<const Requirement> requirements,
                                std::span<const ImportFragment> fragments,
                                std::span<const PlanItem> plans) {
    std::vector<Node> nodes;
    nodes.reserve(requirements.size() + plans.size());
    for (const auto& requirement : requirements) {
        nodes.push_back(requirement.as_node());
    }
    for (const auto& fragment : fragments) {
        nodes.insert(nodes.end(), fragment.nodes.begin(), fragment.nodes.end());
    }
    for (const auto& plan : plans) {
        nodes.push_back(plan.as_node());
    }
    for (auto& node : nodes) {
        canonicalize_node(node);
    }
    std::sort(nodes.begin(), nodes.end(), node_less);
    return nodes;
}

void collect_semantic_edges(std::vector<Edge>& edges,
                            std::span<const Requirement> requirements,
                            std::span<const PlanItem> plans) {
    for (const auto& requirement : requirements) {
        for (const auto& dependency : requirement.dependencies) {
            edges.push_back(Edge{
                .source_id = requirement.id,
                .target_id = dependency.id,
                .type = RelationshipType::known(RelationshipKind::depends_on),
                .provenance = Provenance{
                    .importer = "requirements",
                    .artifact = requirement.source.path,
                    .source = dependency.source,
                    .scope = {},
                    .symbol = {},
                },
                .source = dependency.source,
            });
        }
    }
    for (const auto& plan : plans) {
        for (const auto& link : plan.requirement_links) {
            edges.push_back(Edge{
                .source_id = plan.id,
                .target_id = link.id,
                .type = RelationshipType::known(RelationshipKind::plans),
                .provenance = Provenance{
                    .importer = "implementation-plans",
                    .artifact = plan.source.path,
                    .source = link.source,
                    .scope = {},
                    .symbol = {},
                },
                .source = link.source,
            });
        }
    }
}

// @req REQ-0046
void insert_nodes(Graph& graph,
                  std::vector<Diagnostic>& diagnostics,
                  std::span<const Node> nodes) {
    for (const auto& node : nodes) {
        const Node* existing = graph.find_node(node.id);
        if (existing != nullptr) {
            if (*existing != node) {
                diagnostics.push_back(duplicate_node_diagnostic(node));
            }
            continue;
        }
        const auto added = graph.add_node(node);
        if (!added) {
            diagnostics.push_back(Diagnostic{
                .code = "mcutrace.node_error",
                .severity = Severity::error,
                .message = added.error().detail,
                .source = added.error().source,
            });
        }
    }
}

void collect_fragment_data(TraceResult& result,
                           std::vector<Edge>& edges,
                           std::span<const ImportFragment> fragments) {
    for (const auto& fragment : fragments) {
        edges.insert(edges.end(), fragment.edges.begin(), fragment.edges.end());
        result.artifacts.insert(result.artifacts.end(),
                                fragment.artifacts.begin(), fragment.artifacts.end());
        result.diagnostics.insert(result.diagnostics.end(),
                                  fragment.diagnostics.begin(), fragment.diagnostics.end());
    }
}

void insert_edges(Graph& graph,
                  std::vector<Diagnostic>& diagnostics,
                  std::span<const Edge> edges) {
    for (const auto& edge : edges) {
        const auto added = graph.add_edge(edge);
        if (!added) {
            diagnostics.push_back(Diagnostic{
                .code = "mcutrace.edge_error",
                .severity = Severity::error,
                .message = added.error().detail,
                .source = added.error().source,
            });
        }
    }
}

void deduplicate_artifacts_and_diagnostics(TraceResult& result) {
    std::sort(result.artifacts.begin(), result.artifacts.end(), artifact_less);
    result.artifacts.erase(std::unique(result.artifacts.begin(), result.artifacts.end()),
                           result.artifacts.end());

    std::sort(result.diagnostics.begin(), result.diagnostics.end(), diagnostic_less);
    result.diagnostics.erase(std::unique(result.diagnostics.begin(), result.diagnostics.end()),
                             result.diagnostics.end());
}

// @req REQ-0192 REQ-0227
void reconcile_test_sources(std::vector<Node>& nodes, std::vector<Diagnostic>& diagnostics) {
    std::vector<Node> result;
    std::vector<std::string> handled;
    for (const auto& node : nodes) {
        if (!node.id.starts_with("test:tcl:")) {
            result.push_back(node);
            continue;
        }
        if (std::find(handled.begin(), handled.end(), node.id) != handled.end())
            continue;
        handled.push_back(node.id);
        const Node* declaration = nullptr;
        const Node* execution = nullptr;
        bool conflict = false;
        for (const auto& candidate : nodes) {
            if (candidate.id != node.id)
                continue;
            if (candidate.status == "declared") {
                if (declaration && *declaration != candidate)
                    conflict = true;
                declaration = &candidate;
            } else if (!execution)
                execution = &candidate;
            else if (*execution != candidate)
                conflict = true;
        }
        auto checked = execution ? *execution : node;
        std::string issue;
        if (conflict)
            issue = "conflicting_results";
        else if (!declaration)
            issue = "undeclared";
        else if (!execution)
            issue = "not_executed";
        else if (execution->source_digest.empty())
            issue = "missing_digest";
        else if (execution->source_digest != declaration->source_digest)
            issue = "stale";
        else if (execution->evidence_state == EvidenceState::unknown)
            issue = "incomplete";
        if (!issue.empty()) {
            checked.evidence_state = EvidenceState::unknown;
            checked.status = issue;
            if (conflict && declaration)
                checked = *declaration;
            checked.evidence_state = EvidenceState::unknown;
            checked.status = issue;
            checked.evidence_detail =
                issue + (!conflict && execution ? " (recorded " + execution->status + ")" : "");
            if (conflict && !declaration)
                checked.source_digest.clear();
            const bool warning = issue == "not_executed" || issue == "incomplete";
            diagnostics.push_back(
                Diagnostic{.code = "test_source." + issue,
                           .severity = warning ? Severity::warning : Severity::error,
                           .message = "Tcl test cannot verify current source: " + node.label +
                                      " (" + issue + ")",
                           .source = node.source});
        }
        result.push_back(std::move(checked));
    }
    nodes = std::move(result);
}

}  // namespace

// @req REQ-0005 REQ-0019 REQ-0027 REQ-0029 REQ-0031 REQ-0130 REQ-0137
TraceResult assemble_trace(std::span<const Requirement> requirements,
                           std::span<const ImportFragment> fragments,
                           std::span<const PlanItem> plans,
                           std::span<const Diagnostic> diagnostics) {
    TraceResult result;
    result.diagnostics.insert(result.diagnostics.end(), diagnostics.begin(), diagnostics.end());
    auto nodes = collect_nodes(requirements, fragments, plans);
    reconcile_test_sources(nodes, result.diagnostics);
    insert_nodes(result.graph, result.diagnostics, nodes);

    std::vector<Edge> edges;
    collect_semantic_edges(edges, requirements, plans);
    collect_fragment_data(result, edges, fragments);
    insert_edges(result.graph, result.diagnostics, edges);
    deduplicate_artifacts_and_diagnostics(result);
    return result;
}

}  // namespace mcutrace
