#include <mcutrace/output.hpp>

#include <mcujson/mcujson.hpp>

#include <algorithm>
#include <sstream>
#include <string_view>

namespace mcutrace {
namespace {

constexpr std::uint8_t kDefaultEvidenceMask =
    evidence_mask(EvidenceExpectation::test) |
    evidence_mask(EvidenceExpectation::implementation);

std::size_t diagnostic_count(const ValidationResult& validation, Severity severity) {
    return static_cast<std::size_t>(std::count_if(
        validation.diagnostics.begin(), validation.diagnostics.end(),
        [severity](const Diagnostic& diagnostic) { return diagnostic.severity == severity; }));
}

std::size_t connected_count(const Graph& graph, std::string_view id, NodeKind kind) {
    std::size_t count = 0;
    for (const auto& edge : graph.edges()) {
        std::string_view other;
        if (edge.source_id == id) {
            other = edge.target_id;
        } else if (edge.target_id == id) {
            other = edge.source_id;
        } else {
            continue;
        }
        const auto* node = graph.find_node(other);
        if (node != nullptr && node->kind == kind) {
            ++count;
        }
    }
    return count;
}

bool has_failed_test(const Graph& graph, std::string_view id) {
    for (const auto& edge : graph.edges()) {
        std::string_view other;
        if (edge.source_id == id) {
            other = edge.target_id;
        } else if (edge.target_id == id) {
            other = edge.source_id;
        } else {
            continue;
        }
        const auto* node = graph.find_node(other);
        if (node != nullptr && node->kind == NodeKind::test &&
            node->evidence_state == EvidenceState::failed) {
            return true;
        }
    }
    return false;
}

bool expects(const Node& requirement, EvidenceExpectation expectation) noexcept {
    const auto mask = requirement.expected_evidence.value_or(kDefaultEvidenceMask);
    return (mask & evidence_mask(expectation)) != 0U;
}

void append_missing(std::vector<std::string>& missing,
                    bool expected,
                    std::size_t count,
                    std::string_view name) {
    if (expected && count == 0U) {
        missing.emplace_back(name);
    }
}

RequirementReportEntry build_requirement_entry(const Graph& graph, const Node& requirement) {
    RequirementReportEntry entry{
        .id = requirement.id,
        .title = requirement.label,
        .status = RequirementReportStatus::complete,
        .evidence = EvidenceCounts{
            .implementation = connected_count(graph, requirement.id, NodeKind::source),
            .test = connected_count(graph, requirement.id, NodeKind::test),
            .coverage = connected_count(graph, requirement.id, NodeKind::coverage),
            .build = connected_count(graph, requirement.id, NodeKind::artifact),
        },
        .missing_evidence = {},
    };

    append_missing(entry.missing_evidence,
                   expects(requirement, EvidenceExpectation::implementation),
                   entry.evidence.implementation, "implementation");
    append_missing(entry.missing_evidence,
                   expects(requirement, EvidenceExpectation::test),
                   entry.evidence.test, "test");
    append_missing(entry.missing_evidence,
                   expects(requirement, EvidenceExpectation::coverage),
                   entry.evidence.coverage, "coverage");
    append_missing(entry.missing_evidence,
                   expects(requirement, EvidenceExpectation::build),
                   entry.evidence.build, "build");

    if (has_failed_test(graph, requirement.id)) {
        entry.status = RequirementReportStatus::failed;
    } else if (!entry.missing_evidence.empty()) {
        entry.status = RequirementReportStatus::incomplete;
    }
    return entry;
}

void update_requirement_summary(TraceReport& report, const RequirementReportEntry& entry) {
    ++report.summary.requirements;
    switch (entry.status) {
    case RequirementReportStatus::complete:
        ++report.summary.complete_requirements;
        break;
    case RequirementReportStatus::incomplete:
        ++report.summary.incomplete_requirements;
        break;
    case RequirementReportStatus::failed:
        ++report.summary.failed_requirements;
        break;
    }
}

std::string_view status_marker(RequirementReportStatus status) noexcept {
    switch (status) {
    case RequirementReportStatus::complete:
        return "OK";
    case RequirementReportStatus::incomplete:
        return "MISSING";
    case RequirementReportStatus::failed:
        return "FAIL";
    }
    return "MISSING";
}

void render_missing(std::ostringstream& output, const RequirementReportEntry& requirement) {
    if (requirement.missing_evidence.empty()) {
        return;
    }
    output << "  missing: ";
    for (std::size_t index = 0; index < requirement.missing_evidence.size(); ++index) {
        if (index != 0U) {
            output << ", ";
        }
        output << requirement.missing_evidence[index];
    }
    output << '\n';
}

void render_requirement(std::ostringstream& output, const RequirementReportEntry& requirement) {
    output << '[' << status_marker(requirement.status) << "] "
           << requirement.id << ' ' << requirement.title
           << " | impl=" << requirement.evidence.implementation
           << " test=" << requirement.evidence.test
           << " cov=" << requirement.evidence.coverage
           << " build=" << requirement.evidence.build << '\n';
    render_missing(output, requirement);
}

void render_diagnostics(std::ostringstream& output, const ValidationResult& validation) {
    if (validation.diagnostics.empty()) {
        return;
    }
    output << "\ndiagnostics\n";
    for (const auto& diagnostic : validation.diagnostics) {
        output << '[' << severity_name(diagnostic.severity) << "] "
               << diagnostic.code << ": " << diagnostic.message;
        if (diagnostic.source) {
            output << " (" << diagnostic.source->path;
            if (diagnostic.source->line != 0U) {
                output << ':' << diagnostic.source->line;
            }
            output << ')';
        }
        output << '\n';
    }
}

void write_evidence(mcujson::JsonWriter::Object& object, const EvidenceCounts& evidence) {
    object.object("evidence", [&](mcujson::JsonWriter::Object& values) {
        values("implementation", evidence.implementation)
            ("test", evidence.test)
            ("coverage", evidence.coverage)
            ("build", evidence.build);
    });
}

void write_requirement(mcujson::JsonWriter::Array& requirements,
                       const RequirementReportEntry& requirement) {
    requirements.object([&](mcujson::JsonWriter::Object& object) {
        object("id", requirement.id)
            ("title", requirement.title)
            ("status", requirement_report_status_name(requirement.status));
        write_evidence(object, requirement.evidence);
        object.array("missing_evidence", [&](mcujson::JsonWriter::Array& missing) {
            for (const auto& evidence : requirement.missing_evidence) {
                missing(evidence);
            }
        });
    });
}

}  // namespace

std::string_view requirement_report_status_name(RequirementReportStatus status) noexcept {
    switch (status) {
    case RequirementReportStatus::complete:
        return "complete";
    case RequirementReportStatus::incomplete:
        return "incomplete";
    case RequirementReportStatus::failed:
        return "failed";
    }
    return "incomplete";
}

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
        auto entry = build_requirement_entry(trace.graph, node);
        update_requirement_summary(report, entry);
        if (!entry.missing_evidence.empty()) {
            report.untraced_requirements.push_back(entry.id);
        }
        report.requirements.push_back(std::move(entry));
    }
    return report;
}

std::string render_text_report(const TraceResult& trace, const ValidationResult& validation) {
    const TraceReport report = build_report(trace, validation);
    std::ostringstream output;
    output << "mcutrace traceability report\n"
           << "status: " << (validation.failed ? "FAIL" : "PASS") << '\n'
           << "requirements: " << report.summary.requirements
           << " total | " << report.summary.complete_requirements << " complete | "
           << report.summary.incomplete_requirements << " incomplete | "
           << report.summary.failed_requirements << " failed\n"
           << "graph: " << report.summary.trace_nodes << " nodes | "
           << report.summary.relationships << " relationships\n"
           << "validation: " << report.summary.validation_errors << " errors | "
           << report.summary.validation_warnings << " warnings\n\n"
           << "requirements\n";

    for (const auto& requirement : report.requirements) {
        render_requirement(output, requirement);
    }
    render_diagnostics(output, validation);
    return output.str();
}

std::expected<std::string, OutputError>
render_json_report(const TraceResult& trace, const ValidationResult& validation) {
    const TraceReport report = build_report(trace, validation);

    std::vector<char> buffer(8192U + trace.graph.nodes().size() * 512U +
                             trace.graph.edges().size() * 512U +
                             validation.diagnostics.size() * 512U +
                             report.requirements.size() * 512U);
    mcujson::JsonWriter writer(std::span<char>(buffer.data(), buffer.size()));
    writer.object([&](mcujson::JsonWriter::Object& root) {
        root("schema_version", kOutputSchemaVersion)
            ("status", validation.failed ? "fail" : "pass");
        root.object("summary", [&](mcujson::JsonWriter::Object& summary) {
            summary("requirements", report.summary.requirements)
                ("complete_requirements", report.summary.complete_requirements)
                ("incomplete_requirements", report.summary.incomplete_requirements)
                ("failed_requirements", report.summary.failed_requirements)
                ("trace_nodes", report.summary.trace_nodes)
                ("relationships", report.summary.relationships)
                ("validation_errors", report.summary.validation_errors)
                ("validation_warnings", report.summary.validation_warnings);
        });
        root.array("requirements", [&](mcujson::JsonWriter::Array& requirements) {
            for (const auto& requirement : report.requirements) {
                write_requirement(requirements, requirement);
            }
        });
        root.array("nodes", [&](mcujson::JsonWriter::Array& nodes) {
            for (const auto& node : trace.graph.nodes()) {
                nodes.object([&](mcujson::JsonWriter::Object& object) {
                    object("id", node.id)
                        ("kind", node_kind_name(node.kind))
                        ("label", node.label)
                        ("evidence_state", evidence_state_name(node.evidence_state));
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
