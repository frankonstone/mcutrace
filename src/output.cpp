#include <mcutrace/output.hpp>

#include <mcujson/mcujson.hpp>

#include <algorithm>
#include <array>
#include <sstream>
#include <string_view>
#include <utility>

namespace mcutrace {
namespace {

bool has_requirement_evidence(const TraceResult& trace, const Node& requirement) {
    if (requirement.expected_evidence.has_value() && *requirement.expected_evidence == 0U) {
        return true;
    }
    for (const auto& edge : trace.graph.edges()) {
        if ((edge.source_id == requirement.id || edge.target_id == requirement.id) &&
            edge.type.kind != RelationshipKind::depends_on &&
            edge.type.kind != RelationshipKind::plans) {
            return true;
        }
    }
    return false;
}

void append_node_once(std::vector<Node>& nodes, const Node& node) {
    const auto existing = std::find_if(nodes.begin(), nodes.end(), [&node](const Node& candidate) {
        return candidate.id == node.id;
    });
    if (existing == nodes.end()) {
        nodes.push_back(node);
    }
}

bool has_source_path(const std::vector<std::string>& paths, std::string_view path) {
    return std::find(paths.begin(), paths.end(), path) != paths.end();
}

void append_source_path(std::vector<std::string>& paths, const Node& node) {
    if (node.source && !has_source_path(paths, node.source->path)) {
        paths.push_back(node.source->path);
    }
}

void append_related_node(RequirementTraceReport& report, const Node& node) {
    switch (node.kind) {
    case NodeKind::plan:
        append_node_once(report.plans, node);
        break;
    case NodeKind::implementation:
        append_node_once(report.implementations, node);
        break;
    case NodeKind::source:
        append_node_once(report.sources, node);
        break;
    case NodeKind::test:
        append_node_once(report.tests, node);
        break;
    case NodeKind::coverage:
        append_node_once(report.coverage, node);
        break;
    case NodeKind::finding:
        append_node_once(report.findings, node);
        break;
    case NodeKind::artifact:
        append_node_once(report.builds, node);
        break;
    case NodeKind::requirement:
        break;
    }
}

std::string node_location(const Node& node) {
    if (!node.source) {
        return {};
    }
    std::string result = node.source->path;
    if (node.source->line != 0U) {
        result += ':' + std::to_string(node.source->line);
    }
    return result;
}

void append_node_group(std::ostringstream& output,
                       std::string_view heading,
                       const std::vector<Node>& nodes) {
    if (nodes.empty()) {
        return;
    }
    output << heading << ":\n";
    for (const auto& node : nodes) {
        output << "  " << node.label;
        const std::string location = node_location(node);
        if (!location.empty()) {
            output << " (" << location << ')';
        }
        if (node.evidence_state != EvidenceState::unknown) {
            output << " [" << evidence_state_name(node.evidence_state) << ']';
        }
        if (!node.evidence_detail.empty()) {
            output << " [" << node.evidence_detail << ']';
        }
        if (!node.finding_state.empty()) {
            output << " [" << node.finding_state << ']';
        }
        if (!node.status.empty()) {
            output << " [" << node.status << ']';
        }
        output << '\n';
    }
}

template <typename Object>
void write_node(Object& object, const Node& node) {
    object("id", node.id)
        ("kind", node_kind_name(node.kind))
        ("label", node.label)
        ("evidence_state", evidence_state_name(node.evidence_state));
    if (!node.evidence_detail.empty()) {
        object("evidence_detail", node.evidence_detail);
    }
    if (!node.finding_state.empty()) {
        object("finding_state", node.finding_state);
    }
    if (!node.body.empty()) {
        object("body", node.body);
    }
    if (!node.status.empty()) {
        object("status", node.status);
    }
    const auto expected = node.expected_evidence.value_or(
        node.kind == NodeKind::requirement ? evidence_mask(EvidenceExpectation::test) : 0U);
    if (node.kind == NodeKind::requirement) {
        object.array("expected_evidence", [&](mcujson::JsonWriter::Array& values) {
            for (const auto expectation : {
                    std::pair{EvidenceExpectation::test, std::string_view{"test"}},
                    std::pair{EvidenceExpectation::implementation,
                              std::string_view{"implementation"}},
                    std::pair{EvidenceExpectation::coverage, std::string_view{"coverage"}},
                    std::pair{EvidenceExpectation::build, std::string_view{"build"}}}) {
                if ((expected & evidence_mask(expectation.first)) != 0U) {
                    values(expectation.second);
                }
            }
        });
    }
    if (node.source) {
        object.object("source", [&](mcujson::JsonWriter::Object& source) {
            source("path", node.source->path)
                ("line", node.source->line)
                ("column", node.source->column);
            if (node.source->end_line != 0U) {
                source("end_line", node.source->end_line);
            }
            if (node.source->end_column != 0U) {
                source("end_column", node.source->end_column);
            }
        });
    }
}

std::size_t node_buffer_size(const Node& node) {
    return 512U + node.id.size() + node.label.size() + node.body.size() +
           node.evidence_detail.size() + node.finding_state.size() + node.status.size() +
           (node.source ? node.source->path.size() : 0U);
}

std::size_t nodes_buffer_size(std::span<const Node> nodes) {
    std::size_t result = 0U;
    for (const auto& node : nodes) {
        result += node_buffer_size(node);
    }
    return result;
}

std::size_t diagnostic_count(const ValidationResult& validation, Severity severity) {
    return static_cast<std::size_t>(std::count_if(
        validation.diagnostics.begin(), validation.diagnostics.end(),
        [severity](const Diagnostic& diagnostic) { return diagnostic.severity == severity; }));
}

std::optional<std::string_view> related_node_id(const Edge& edge,
                                                std::string_view requirement_id) {
    if (edge.source_id == requirement_id) {
        return edge.target_id;
    }
    if (edge.target_id == requirement_id) {
        return edge.source_id;
    }
    return std::nullopt;
}

void append_directly_related_nodes(const TraceResult& trace,
                                   std::string_view requirement_id,
                                   RequirementTraceReport& report,
                                   std::vector<std::string>& source_paths) {
    for (const auto& edge : trace.graph.edges()) {
        if (edge.type.kind == RelationshipKind::depends_on) {
            if (edge.source_id == requirement_id) {
                const Node* prerequisite = trace.graph.find_node(edge.target_id);
                if (prerequisite != nullptr) {
                    append_node_once(report.prerequisites, *prerequisite);
                } else {
                    report.unresolved_dependencies.push_back(edge);
                }
            } else if (edge.target_id == requirement_id) {
                const Node* dependent = trace.graph.find_node(edge.source_id);
                if (dependent != nullptr) {
                    append_node_once(report.dependents, *dependent);
                }
            }
            continue;
        }
        if (edge.type.kind == RelationshipKind::plans) {
            if (edge.target_id == requirement_id) {
                const Node* plan = trace.graph.find_node(edge.source_id);
                if (plan != nullptr) {
                    append_node_once(report.plans, *plan);
                }
            }
            continue;
        }
        const auto other_id = related_node_id(edge, requirement_id);
        if (!other_id) {
            continue;
        }
        const Node* node = trace.graph.find_node(*other_id);
        if (node != nullptr) {
            append_related_node(report, *node);
            append_source_path(source_paths, *node);
        }
    }
}

void append_prerequisite_diagnostics(RequirementTraceReport& report,
                                     const ValidationResult& validation) {
    for (const auto& prerequisite : report.prerequisites) {
        for (const auto& diagnostic : validation.diagnostics) {
            if (diagnostic.message.find(prerequisite.id) != std::string::npos &&
                std::find(report.prerequisite_diagnostics.begin(),
                          report.prerequisite_diagnostics.end(), diagnostic) ==
                    report.prerequisite_diagnostics.end()) {
                report.prerequisite_diagnostics.push_back(diagnostic);
            }
        }
    }
}

template <typename Object>
void write_diagnostic(Object& object, const Diagnostic& diagnostic) {
    object("severity", severity_name(diagnostic.severity))
        ("code", diagnostic.code)
        ("message", diagnostic.message);
    if (diagnostic.source) {
        object.object("source", [&](mcujson::JsonWriter::Object& source) {
            source("path", diagnostic.source->path)
                ("line", diagnostic.source->line)
                ("column", diagnostic.source->column);
        });
    }
}

template <typename Object>
void write_edge(Object& object, const Edge& edge) {
    object("source", edge.source_id)
        ("target", edge.target_id)
        ("type", edge.type.display_name());
    if (edge.source) {
        object.object("location", [&](mcujson::JsonWriter::Object& source) {
            source("path", edge.source->path)
                ("line", edge.source->line)
                ("column", edge.source->column);
        });
    }
}

void append_implementation_source_paths(const RequirementTraceReport& report,
                                        std::vector<std::string>& source_paths) {
    for (const auto& implementation : report.implementations) {
        append_source_path(source_paths, implementation);
    }
}

void append_source_nodes(const TraceResult& trace,
                         const std::vector<std::string>& source_paths,
                         RequirementTraceReport& report) {
    for (const auto& path : source_paths) {
        const Node* source = trace.graph.find_node("source:" + path);
        if (source != nullptr && source->kind == NodeKind::source) {
            append_node_once(report.sources, *source);
        }
    }
}

void append_source_evidence(const TraceResult& trace,
                            const std::vector<std::string>& source_paths,
                            RequirementTraceReport& report) {
    for (const auto& node : trace.graph.nodes()) {
        if (!node.source || !has_source_path(source_paths, node.source->path)) {
            continue;
        }
        if (node.kind == NodeKind::coverage || node.kind == NodeKind::finding) {
            append_related_node(report, node);
        }
    }
}

}  // namespace

// @req REQ-0059 REQ-0060
TraceReport build_report(const TraceResult& trace, const ValidationResult& validation) {
    TraceReport report;
    report.summary.trace_nodes = trace.graph.nodes().size();
    report.summary.relationships = trace.graph.edges().size();
    report.summary.validation_errors = diagnostic_count(validation, Severity::error);
    report.summary.validation_warnings = diagnostic_count(validation, Severity::warning);

    for (const auto& node : trace.graph.nodes()) {
        if (node.kind != NodeKind::requirement) {
            continue;
        }
        ++report.summary.requirements;
        if (!has_requirement_evidence(trace, node)) {
            report.untraced_requirements.push_back(node.id);
        }
    }
    for (const auto& node : trace.graph.nodes()) {
        if (node.kind != NodeKind::plan) {
            continue;
        }
        ++report.summary.plan_items;
        if (node.status != "done") {
            ++report.summary.outstanding_plan_items;
            report.outstanding_plans.push_back(node);
        }
    }
    return report;
}

std::optional<RequirementTraceReport>
build_requirement_trace_report(const TraceResult& trace, const std::string_view requirement_id) {
    const Node* requirement = trace.graph.find_node(requirement_id);
    if (requirement == nullptr || requirement->kind != NodeKind::requirement) {
        return std::nullopt;
    }

    RequirementTraceReport report;
    report.requirement = *requirement;
    std::vector<std::string> source_paths;
    append_directly_related_nodes(trace, requirement_id, report, source_paths);
    append_implementation_source_paths(report, source_paths);
    append_source_nodes(trace, source_paths, report);
    append_source_evidence(trace, source_paths, report);
    return report;
}

// @req REQ-0133 REQ-0137
std::optional<RequirementTraceReport>
build_requirement_trace_report(const TraceResult& trace,
                               const ValidationResult& validation,
                               const std::string_view requirement_id) {
    auto report = build_requirement_trace_report(trace, requirement_id);
    if (report) {
        append_prerequisite_diagnostics(*report, validation);
    }
    return report;
}

// @req REQ-0134
std::optional<RequirementContextReport>
build_requirement_context_report(const TraceResult& trace,
                                 const ValidationResult& validation,
                                 const std::string_view requirement_id) {
    const Node* requested = trace.graph.find_node(requirement_id);
    if (requested == nullptr || requested->kind != NodeKind::requirement) {
        return std::nullopt;
    }
    RequirementContextReport report;
    report.requested = *requested;
    std::vector<std::string> pending{std::string{requirement_id}};
    std::vector<std::string> visited;
    while (!pending.empty()) {
        const std::string current = std::move(pending.back());
        pending.pop_back();
        if (std::find(visited.begin(), visited.end(), current) != visited.end()) {
            continue;
        }
        visited.push_back(current);
        const Node* node = trace.graph.find_node(current);
        if (node != nullptr && node->kind == NodeKind::requirement) {
            append_node_once(report.requirements, *node);
        }
        for (const auto& edge : trace.graph.edges()) {
            if (edge.type.kind != RelationshipKind::depends_on || edge.source_id != current) {
                continue;
            }
            report.dependencies.push_back(edge);
            const Node* target = trace.graph.find_node(edge.target_id);
            if (target == nullptr) {
                report.unresolved_dependencies.push_back(edge);
            } else {
                pending.push_back(edge.target_id);
            }
        }
    }
    std::sort(report.requirements.begin(), report.requirements.end(),
              [](const Node& lhs, const Node& rhs) { return lhs.id < rhs.id; });
    for (const auto& diagnostic : validation.diagnostics) {
        const bool relevant = std::any_of(
            report.requirements.begin(), report.requirements.end(),
            [&diagnostic](const Node& node) {
                return diagnostic.message.find(node.id) != std::string::npos ||
                       (diagnostic.source && node.source &&
                        diagnostic.source->path == node.source->path &&
                        diagnostic.source->line == node.source->line);
            });
        if (relevant) {
            report.diagnostics.push_back(diagnostic);
        }
    }
    for (const auto& edge : trace.graph.edges()) {
        if (edge.type.kind == RelationshipKind::depends_on ||
            edge.type.kind == RelationshipKind::plans) {
            continue;
        }
        const bool touches_context = std::any_of(
            report.requirements.begin(), report.requirements.end(),
            [&edge](const Node& node) {
                return edge.source_id == node.id || edge.target_id == node.id;
            });
        if (!touches_context) {
            continue;
        }
        report.evidence_relationships.push_back(edge);
        for (const auto id : {std::string_view{edge.source_id}, std::string_view{edge.target_id}}) {
            const Node* evidence = trace.graph.find_node(id);
            if (evidence != nullptr && evidence->kind != NodeKind::requirement) {
                append_node_once(report.evidence, *evidence);
            }
        }
    }
    std::sort(report.evidence.begin(), report.evidence.end(),
              [](const Node& lhs, const Node& rhs) { return lhs.id < rhs.id; });
    return report;
}

// @req REQ-0056 REQ-0059 REQ-0060 REQ-0097
std::string render_text_report(const TraceResult& trace, const ValidationResult& validation) {
    const TraceReport report = build_report(trace, validation);
    std::ostringstream output;
    output << "mcutrace summary\n"
           << "requirements: " << report.summary.requirements << '\n'
           << "trace nodes: " << report.summary.trace_nodes << '\n'
           << "relationships: " << report.summary.relationships << '\n'
           << "plan items: " << report.summary.plan_items << '\n'
           << "outstanding plan items: " << report.summary.outstanding_plan_items << '\n'
           << "validation errors: " << report.summary.validation_errors << '\n'
           << "validation warnings: " << report.summary.validation_warnings << '\n';
    if (!report.untraced_requirements.empty()) {
        output << "untraced requirements:\n";
        for (const auto& id : report.untraced_requirements) {
            output << "  " << id << '\n';
        }
    }
    if (!report.outstanding_plans.empty()) {
        output << "outstanding plan items:\n";
        for (const auto& plan : report.outstanding_plans) {
            output << "  " << plan.id << " [" << plan.status << "]";
            if (!plan.label.empty()) {
                output << " — " << plan.label;
            }
            output << '\n';
        }
    }
    bool wrote_heading = false;
    for (const auto& edge : trace.graph.edges()) {
        if (edge.type.kind != RelationshipKind::implements) {
            continue;
        }
        const auto* implementation = trace.graph.find_node(edge.source_id);
        if (implementation == nullptr || implementation->kind != NodeKind::implementation) {
            continue;
        }
        if (!wrote_heading) {
            output << "implementation links:\n";
            wrote_heading = true;
        }
        output << "  " << edge.target_id << " <- " << implementation->label;
        if (implementation->source) {
            output << " (" << implementation->source->path;
            if (implementation->source->line != 0U) {
                output << ':' << implementation->source->line;
            }
            output << ')';
        }
        output << '\n';
    }
    return output.str();
}

std::string render_requirement_text_report(const RequirementTraceReport& report) {
    std::ostringstream output;
    output << report.requirement.id;
    if (!report.requirement.label.empty()) {
        output << " — " << report.requirement.label;
    }
    output << '\n';
    append_node_group(output, "implementations", report.implementations);
    append_node_group(output, "sources", report.sources);
    append_node_group(output, "tests", report.tests);
    append_node_group(output, "coverage", report.coverage);
    append_node_group(output, "build evidence", report.builds);
    append_node_group(output, "static analysis", report.findings);
    append_node_group(output, "Depends on", report.prerequisites);
    append_node_group(output, "Required by", report.dependents);
    append_node_group(output, "implementation plans", report.plans);
    if (!report.unresolved_dependencies.empty()) {
        output << "Depends on (unresolved):\n";
        for (const auto& edge : report.unresolved_dependencies) {
            output << "  " << edge.target_id << '\n';
        }
    }
    if (!report.prerequisite_diagnostics.empty()) {
        output << "prerequisite problems:\n";
        for (const auto& diagnostic : report.prerequisite_diagnostics) {
            output << "  " << diagnostic.code << ": " << diagnostic.message << '\n';
        }
    }
    return output.str();
}

// @req REQ-0055 REQ-0057 REQ-0058 REQ-0061 REQ-0087 REQ-0097
std::expected<std::string, OutputError>
render_json_report(const TraceResult& trace, const ValidationResult& validation) {
    const TraceReport report = build_report(trace, validation);

    std::vector<char> buffer(4096U + nodes_buffer_size(trace.graph.nodes()) +
                             trace.graph.edges().size() * 1024U +
                             validation.diagnostics.size() * 384U);
    mcujson::JsonWriter writer(std::span<char>(buffer.data(), buffer.size()));
    writer.object([&](mcujson::JsonWriter::Object& root) {
        root("schema_version", kOutputSchemaVersion);
        root.object("summary", [&](mcujson::JsonWriter::Object& summary) {
            summary("requirements", report.summary.requirements)
                ("trace_nodes", report.summary.trace_nodes)
                ("relationships", report.summary.relationships)
                ("plan_items", report.summary.plan_items)
                ("outstanding_plan_items", report.summary.outstanding_plan_items)
                ("validation_errors", report.summary.validation_errors)
                ("validation_warnings", report.summary.validation_warnings);
        });
        root.array("nodes", [&](mcujson::JsonWriter::Array& nodes) {
            for (const auto& node : trace.graph.nodes()) {
                nodes.object([&](mcujson::JsonWriter::Object& object) {
                    write_node(object, node);
                });
            }
        });
        root.array("relationships", [&](mcujson::JsonWriter::Array& relationships) {
            for (const auto& edge : trace.graph.edges()) {
                relationships.object([&](mcujson::JsonWriter::Object& object) {
                    object("source", edge.source_id)
                        ("target", edge.target_id)
                        ("type", edge.type.display_name());
                    object.object("provenance", [&](mcujson::JsonWriter::Object& provenance) {
                        provenance("importer", edge.provenance.importer)
                            ("artifact", edge.provenance.artifact);
                        if (!edge.provenance.scope.empty()) {
                            provenance("scope", edge.provenance.scope);
                        }
                        if (!edge.provenance.symbol.empty()) {
                            provenance("symbol", edge.provenance.symbol);
                        }
                        if (edge.provenance.source) {
                            provenance.object("source", [&](mcujson::JsonWriter::Object& source) {
                                source("path", edge.provenance.source->path)
                                    ("line", edge.provenance.source->line)
                                    ("column", edge.provenance.source->column);
                            });
                        }
                    });
                });
            }
        });
        root.array("diagnostics", [&](mcujson::JsonWriter::Array& diagnostics) {
            for (const auto& diagnostic : validation.diagnostics) {
                diagnostics.object([&](mcujson::JsonWriter::Object& object) {
                    write_diagnostic(object, diagnostic);
                });
            }
        });
        root.array("untraced_requirements", [&](mcujson::JsonWriter::Array& requirements) {
            for (const auto& id : report.untraced_requirements) {
                requirements(id);
            }
        });
        root.array("outstanding_plans", [&](mcujson::JsonWriter::Array& plans) {
            for (const auto& plan : report.outstanding_plans) {
                plans.object([&](mcujson::JsonWriter::Object& object) {
                    write_node(object, plan);
                });
            }
        });
    });

    const auto size = writer.finish();
    if (!size) {
        return std::unexpected(OutputError{
            .code = OutputErrorCode::serialize_failed,
            .detail = "failed to serialize mcutrace JSON output",
        });
    }
    return std::string(buffer.data(), *size);
}

std::expected<std::string, OutputError>
render_requirement_json_report(const RequirementTraceReport& report) {
    const std::size_t node_count = 1U + report.implementations.size() + report.sources.size() +
                                   report.tests.size() + report.coverage.size() +
                                   report.builds.size() +
                                   report.findings.size() + report.prerequisites.size() +
                                   report.dependents.size() + report.plans.size();
    std::size_t buffer_size = 2048U + node_count * 384U + node_buffer_size(report.requirement);
    for (const auto* nodes : {&report.implementations, &report.sources, &report.tests,
                             &report.coverage, &report.builds, &report.findings,
                             &report.prerequisites, &report.dependents, &report.plans}) {
        buffer_size += nodes_buffer_size(*nodes);
    }
    std::vector<char> buffer(buffer_size);
    mcujson::JsonWriter writer(std::span<char>(buffer.data(), buffer.size()));
    writer.object([&](mcujson::JsonWriter::Object& root) {
        root("schema_version", kOutputSchemaVersion);
        root.object("requirement", [&](mcujson::JsonWriter::Object& requirement) {
            write_node(requirement, report.requirement);
        });
        const auto write_nodes = [&root](std::string_view name, const std::vector<Node>& nodes) {
            root.array(name, [&](mcujson::JsonWriter::Array& array) {
                for (const auto& node : nodes) {
                    array.object([&](mcujson::JsonWriter::Object& object) {
                        write_node(object, node);
                    });
                }
            });
        };
        write_nodes("implementations", report.implementations);
        write_nodes("sources", report.sources);
        write_nodes("tests", report.tests);
        write_nodes("coverage", report.coverage);
        write_nodes("builds", report.builds);
        write_nodes("findings", report.findings);
        write_nodes("prerequisites", report.prerequisites);
        write_nodes("dependents", report.dependents);
        write_nodes("plans", report.plans);
        root.array("unresolved_dependencies", [&](mcujson::JsonWriter::Array& array) {
            for (const auto& edge : report.unresolved_dependencies) {
                array.object([&](mcujson::JsonWriter::Object& object) { write_edge(object, edge); });
            }
        });
        root.array("prerequisite_diagnostics", [&](mcujson::JsonWriter::Array& array) {
            for (const auto& diagnostic : report.prerequisite_diagnostics) {
                array.object([&](mcujson::JsonWriter::Object& object) {
                    write_diagnostic(object, diagnostic);
                });
            }
        });
    });
    const auto size = writer.finish();
    if (!size) {
        return std::unexpected(OutputError{
            .code = OutputErrorCode::serialize_failed,
            .detail = "failed to serialize mcutrace requirement JSON output",
        });
    }
    return std::string(buffer.data(), *size);
}

std::string render_requirement_context_text_report(const RequirementContextReport& report) {
    std::ostringstream output;
    output << "requirement context for " << report.requested.id << '\n';
    for (const auto& requirement : report.requirements) {
        output << '\n' << requirement.id;
        if (!requirement.label.empty()) {
            output << " — " << requirement.label;
        }
        const auto location = node_location(requirement);
        if (!location.empty()) {
            output << " (" << location << ')';
        }
        output << '\n';
        if (!requirement.body.empty()) {
            output << requirement.body << '\n';
        }
        for (const auto& evidence : report.evidence) {
            const bool linked = std::any_of(
                report.evidence_relationships.begin(), report.evidence_relationships.end(),
                [&requirement, &evidence](const Edge& edge) {
                    return (edge.source_id == requirement.id && edge.target_id == evidence.id) ||
                           (edge.target_id == requirement.id && edge.source_id == evidence.id);
                });
            if (linked) {
                output << "  evidence: " << evidence.id << " — " << evidence.label << '\n';
            }
        }
        for (const auto& edge : report.dependencies) {
            if (edge.source_id == requirement.id) {
                output << "  depends on: " << edge.target_id << '\n';
            }
        }
    }
    if (!report.diagnostics.empty()) {
        output << "\ncontext diagnostics:\n";
        for (const auto& diagnostic : report.diagnostics) {
            output << "  " << diagnostic.code << ": " << diagnostic.message << '\n';
        }
    }
    return output.str();
}

std::expected<std::string, OutputError>
render_requirement_context_json_report(const RequirementContextReport& report) {
    std::vector<char> buffer(4096U + nodes_buffer_size(report.requirements) +
                             nodes_buffer_size(report.evidence) +
                             report.dependencies.size() * 384U +
                             report.diagnostics.size() * 384U);
    mcujson::JsonWriter writer(std::span<char>(buffer.data(), buffer.size()));
    writer.object([&](mcujson::JsonWriter::Object& root) {
        root("schema_version", kOutputSchemaVersion);
        root.object("requested", [&](mcujson::JsonWriter::Object& object) {
            write_node(object, report.requested);
        });
        root.array("requirements", [&](mcujson::JsonWriter::Array& array) {
            for (const auto& requirement : report.requirements) {
                array.object([&](mcujson::JsonWriter::Object& object) {
                    write_node(object, requirement);
                });
            }
        });
        root.array("dependencies", [&](mcujson::JsonWriter::Array& array) {
            for (const auto& edge : report.dependencies) {
                array.object([&](mcujson::JsonWriter::Object& object) { write_edge(object, edge); });
            }
        });
        root.array("unresolved_dependencies", [&](mcujson::JsonWriter::Array& array) {
            for (const auto& edge : report.unresolved_dependencies) {
                array.object([&](mcujson::JsonWriter::Object& object) { write_edge(object, edge); });
            }
        });
        root.array("evidence", [&](mcujson::JsonWriter::Array& array) {
            for (const auto& evidence : report.evidence) {
                array.object([&](mcujson::JsonWriter::Object& object) {
                    write_node(object, evidence);
                });
            }
        });
        root.array("evidence_relationships", [&](mcujson::JsonWriter::Array& array) {
            for (const auto& edge : report.evidence_relationships) {
                array.object([&](mcujson::JsonWriter::Object& object) { write_edge(object, edge); });
            }
        });
        root.array("diagnostics", [&](mcujson::JsonWriter::Array& array) {
            for (const auto& diagnostic : report.diagnostics) {
                array.object([&](mcujson::JsonWriter::Object& object) {
                    write_diagnostic(object, diagnostic);
                });
            }
        });
    });
    const auto size = writer.finish();
    if (!size) {
        return std::unexpected(OutputError{
            .code = OutputErrorCode::serialize_failed,
            .detail = "failed to serialize requirement context JSON output",
        });
    }
    return std::string(buffer.data(), *size);
}

}  // namespace mcutrace
