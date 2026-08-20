#include <mcutrace/output.hpp>

#include <mcujson/mcujson.hpp>

#include <algorithm>
#include <array>
#include <sstream>
#include <string_view>

namespace mcutrace {
namespace {

bool has_requirement_evidence(const TraceResult& trace, const Node& requirement) {
    if (requirement.expected_evidence.has_value() && *requirement.expected_evidence == 0U) {
        return true;
    }
    for (const auto& edge : trace.graph.edges()) {
        if (edge.source_id == requirement.id || edge.target_id == requirement.id) {
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

std::size_t diagnostic_count(const ValidationResult& validation, Severity severity) {
    return static_cast<std::size_t>(std::count_if(
        validation.diagnostics.begin(), validation.diagnostics.end(),
        [severity](const Diagnostic& diagnostic) { return diagnostic.severity == severity; }));
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
    return report;
}

std::optional<RequirementTraceReport>
build_requirement_trace_report(const TraceResult& trace, const std::string_view requirement_id) {
    const Node* requirement = trace.graph.find_node(requirement_id);
    if (requirement == nullptr || requirement->kind != NodeKind::requirement) {
        return std::nullopt;
    }

    RequirementTraceReport report{.requirement = *requirement};
    std::vector<std::string> source_paths;
    for (const auto& edge : trace.graph.edges()) {
        std::string_view other_id;
        if (edge.source_id == requirement_id) {
            other_id = edge.target_id;
        } else if (edge.target_id == requirement_id) {
            other_id = edge.source_id;
        } else {
            continue;
        }
        const Node* node = trace.graph.find_node(other_id);
        if (node == nullptr) {
            continue;
        }
        append_related_node(report, *node);
        append_source_path(source_paths, *node);
    }

    for (const auto& implementation : report.implementations) {
        append_source_path(source_paths, implementation);
    }
    for (const auto& path : source_paths) {
        const Node* source = trace.graph.find_node("source:" + path);
        if (source != nullptr && source->kind == NodeKind::source) {
            append_node_once(report.sources, *source);
        }
    }
    for (const auto& node : trace.graph.nodes()) {
        if (!node.source || !has_source_path(source_paths, node.source->path)) {
            continue;
        }
        if (node.kind == NodeKind::coverage || node.kind == NodeKind::finding) {
            append_related_node(report, node);
        }
    }
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
           << "validation errors: " << report.summary.validation_errors << '\n'
           << "validation warnings: " << report.summary.validation_warnings << '\n';
    if (!report.untraced_requirements.empty()) {
        output << "untraced requirements:\n";
        for (const auto& id : report.untraced_requirements) {
            output << "  " << id << '\n';
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
    return output.str();
}

// @req REQ-0055 REQ-0057 REQ-0058 REQ-0061 REQ-0087 REQ-0097
std::expected<std::string, OutputError>
render_json_report(const TraceResult& trace, const ValidationResult& validation) {
    const TraceReport report = build_report(trace, validation);

    std::vector<char> buffer(4096U + trace.graph.nodes().size() * 384U +
                             trace.graph.edges().size() * 1024U +
                             validation.diagnostics.size() * 384U);
    mcujson::JsonWriter writer(std::span<char>(buffer.data(), buffer.size()));
    writer.object([&](mcujson::JsonWriter::Object& root) {
        root("schema_version", kOutputSchemaVersion);
        root.object("summary", [&](mcujson::JsonWriter::Object& summary) {
            summary("requirements", report.summary.requirements)
                ("trace_nodes", report.summary.trace_nodes)
                ("relationships", report.summary.relationships)
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
                    object("severity", severity_name(diagnostic.severity))
                        ("code", diagnostic.code)
                        ("message", diagnostic.message);
                    if (diagnostic.source) {
                        object.object("source", [&](mcujson::JsonWriter::Object& source) {
                            source("path", diagnostic.source->path)
                                ("line", diagnostic.source->line)
                                ("column", diagnostic.source->column);
                            if (diagnostic.source->end_line != 0U) {
                                source("end_line", diagnostic.source->end_line);
                            }
                            if (diagnostic.source->end_column != 0U) {
                                source("end_column", diagnostic.source->end_column);
                            }
                        });
                    }
                });
            }
        });
        root.array("untraced_requirements", [&](mcujson::JsonWriter::Array& requirements) {
            for (const auto& id : report.untraced_requirements) {
                requirements(id);
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
                                   report.findings.size();
    std::vector<char> buffer(2048U + node_count * 384U);
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

}  // namespace mcutrace
