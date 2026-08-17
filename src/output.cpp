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

std::size_t diagnostic_count(const ValidationResult& validation, Severity severity) {
    return static_cast<std::size_t>(std::count_if(
        validation.diagnostics.begin(), validation.diagnostics.end(),
        [severity](const Diagnostic& diagnostic) { return diagnostic.severity == severity; }));
}

}  // namespace

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
    return output.str();
}

std::expected<std::string, OutputError>
render_json_report(const TraceResult& trace, const ValidationResult& validation) {
    const TraceReport report = build_report(trace, validation);

    std::vector<char> buffer(4096U + trace.graph.nodes().size() * 384U +
                             trace.graph.edges().size() * 512U +
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
                    object("id", node.id)
                        ("kind", node_kind_name(node.kind))
                        ("label", node.label);
                    if (!node.finding_state.empty()) {
                        object("finding_state", node.finding_state);
                    }
                    if (node.source) {
                        object.object("source", [&](mcujson::JsonWriter::Object& source) {
                            source("path", node.source->path)
                                ("line", node.source->line)
                                ("column", node.source->column);
                        });
                    }
                });
            }
        });
        root.array("relationships", [&](mcujson::JsonWriter::Array& relationships) {
            for (const auto& edge : trace.graph.edges()) {
                relationships.object([&](mcujson::JsonWriter::Object& object) {
                    object("source", edge.source_id)
                        ("target", edge.target_id)
                        ("type", edge.type.display_name());
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

}  // namespace mcutrace