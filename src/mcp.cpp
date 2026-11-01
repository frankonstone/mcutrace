#include <mcutrace/mcp.hpp>

#include <mcutrace/output.hpp>
#include <mcutrace/project.hpp>

#include <mcujson/mcujson.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <istream>
#include <map>
#include <optional>
#include <ostream>
#include <queue>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mcutrace {
namespace {

using McpJson = mcujson::BasicJson<8192U, 60000U>;

class JsonObject final {
  public:
    void member(std::string_view key, std::string value) {
        if (!first_) {
            value_ += ',';
        }
        first_ = false;
        value_ += quote(key);
        value_ += ':';
        value_ += value;
    }

    void string(std::string_view key, std::string_view value) {
        member(key, quote(value));
    }

    void boolean(std::string_view key, bool value) {
        member(key, value ? "true" : "false");
    }

    void number(std::string_view key, std::size_t value) {
        member(key, std::to_string(value));
    }

    [[nodiscard]] std::string finish() && {
        return '{' + std::move(value_) + '}';
    }

    [[nodiscard]] static std::string quote(std::string_view value) {
        constexpr std::string_view digits = "0123456789abcdef";
        std::string result;
        result.reserve(value.size() + 2U);
        result.push_back('"');
        for (const unsigned char character : value) {
            switch (character) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (character < 0x20U) {
                    result += "\\u00";
                    result.push_back(digits[character >> 4U]);
                    result.push_back(digits[character & 0x0fU]);
                } else {
                    result.push_back(static_cast<char>(character));
                }
                break;
            }
        }
        result.push_back('"');
        return result;
    }

  private:
    std::string value_;
    bool first_ = true;
};

class JsonArray final {
  public:
    void value(std::string value) {
        if (!first_) {
            value_ += ',';
        }
        first_ = false;
        value_ += value;
    }

    void string(std::string_view value) { this->value(JsonObject::quote(value)); }

    [[nodiscard]] std::string finish() && {
        return '[' + std::move(value_) + ']';
    }

  private:
    std::string value_;
    bool first_ = true;
};

std::optional<std::string> string_value(const mcujson::JsonRef value) {
    return value.is_string() ? std::optional<std::string>(value.get<std::string>())
                             : std::nullopt;
}

std::string source_json(const SourceLocation& source) {
    JsonObject object;
    object.string("path", source.path);
    object.number("line", source.line);
    object.number("column", source.column);
    object.number("end_line", source.end_line);
    object.number("end_column", source.end_column);
    return std::move(object).finish();
}

std::string expected_evidence_json(const std::optional<std::uint8_t> mask) {
    JsonArray values;
    if (!mask) {
        return std::move(values).finish();
    }
    constexpr EvidenceExpectation expectations[] = {
        EvidenceExpectation::test,
        EvidenceExpectation::implementation,
        EvidenceExpectation::coverage,
        EvidenceExpectation::build,
    };
    constexpr std::string_view names[] = {"test", "implementation", "coverage", "build"};
    for (std::size_t index = 0; index < std::size(expectations); ++index) {
        if ((*mask & evidence_mask(expectations[index])) != 0U) {
            values.string(names[index]);
        }
    }
    return std::move(values).finish();
}

std::string node_json(const Node& node, bool include_body = true) {
    JsonObject object;
    object.string("id", node.id);
    object.string("kind", node_kind_name(node.kind));
    object.string("title", node.label);
    object.string("evidence_state", evidence_state_name(node.evidence_state));
    if (!node.source_digest.empty()) {
        object.string("source_digest", node.source_digest);
    }
    if (!node.evidence_detail.empty()) {
        object.string("evidence_detail", node.evidence_detail);
    }
    if (!node.finding_state.empty()) {
        object.string("finding_state", node.finding_state);
    }
    if (!node.status.empty()) {
        object.string("status", node.status);
    }
    if (include_body) {
        object.string("body", node.body);
    }
    object.member("expected_evidence", expected_evidence_json(node.expected_evidence));
    if (node.source) {
        object.member("source", source_json(*node.source));
    }
    return std::move(object).finish();
}

std::string edge_json(const Edge& edge) {
    JsonObject object;
    object.string("source", edge.source_id);
    object.string("target", edge.target_id);
    object.string("type", edge.type.display_name());
    JsonObject provenance;
    provenance.string("importer", edge.provenance.importer);
    provenance.string("artifact", edge.provenance.artifact);
    if (!edge.provenance.scope.empty()) {
        provenance.string("scope", edge.provenance.scope);
    }
    if (!edge.provenance.symbol.empty()) {
        provenance.string("symbol", edge.provenance.symbol);
    }
    if (edge.provenance.source) {
        provenance.member("source", source_json(*edge.provenance.source));
    }
    object.member("provenance", std::move(provenance).finish());
    if (edge.source) {
        object.member("location", source_json(*edge.source));
    }
    return std::move(object).finish();
}

std::string diagnostic_json(const Diagnostic& diagnostic) {
    JsonObject object;
    object.string("severity", severity_name(diagnostic.severity));
    object.string("code", diagnostic.code);
    object.string("message", diagnostic.message);
    if (diagnostic.source) {
        object.member("source", source_json(*diagnostic.source));
    }
    return std::move(object).finish();
}

std::string diagnostics_json(const std::vector<Diagnostic>& diagnostics) {
    JsonArray values;
    for (const auto& diagnostic : diagnostics) {
        values.value(diagnostic_json(diagnostic));
    }
    return std::move(values).finish();
}

std::vector<Diagnostic> related_diagnostics(const ProjectSnapshot& snapshot,
                                            const std::set<std::string>& identifiers) {
    std::vector<Diagnostic> result;
    for (const auto& diagnostic : snapshot.validation.diagnostics) {
        const bool related = std::any_of(identifiers.begin(), identifiers.end(),
            [&](const std::string& identifier) {
                return diagnostic.message.find(identifier) != std::string::npos;
            });
        if (related) {
            result.push_back(diagnostic);
        }
    }
    return result;
}

std::string envelope(const ProjectSnapshot& snapshot,
                     std::string data,
                     const std::vector<Diagnostic>& diagnostics) {
    JsonObject object;
    object.number("schema_version", kMcpSchemaVersion);
    object.string("analysis_revision", snapshot.revision);
    object.member("data", std::move(data));
    object.member("diagnostics", diagnostics_json(diagnostics));
    return std::move(object).finish();
}

std::string tool_success(std::string structured) {
    JsonObject text;
    text.string("type", "text");
    text.string("text", structured);
    JsonArray content;
    content.value(std::move(text).finish());
    JsonObject result;
    result.member("content", std::move(content).finish());
    result.member("structuredContent", std::move(structured));
    result.boolean("isError", false);
    return std::move(result).finish();
}

std::string tool_failure(std::string_view code, std::string_view detail) {
    JsonObject error;
    error.string("code", code);
    error.string("message", detail);
    const std::string error_text = std::move(error).finish();
    JsonObject text;
    text.string("type", "text");
    text.string("text", error_text);
    JsonArray content;
    content.value(std::move(text).finish());
    JsonObject result;
    result.member("content", std::move(content).finish());
    result.boolean("isError", true);
    return std::move(result).finish();
}

std::string response(std::string_view id, std::string result) {
    JsonObject object;
    object.string("jsonrpc", "2.0");
    object.member("id", id.empty() ? "null" : std::string(id));
    object.member("result", std::move(result));
    return std::move(object).finish();
}

std::string error_response(std::string_view id, int code, std::string_view message) {
    JsonObject error;
    error.member("code", std::to_string(code));
    error.string("message", message);
    JsonObject object;
    object.string("jsonrpc", "2.0");
    object.member("id", id.empty() ? "null" : std::string(id));
    object.member("error", std::move(error).finish());
    return std::move(object).finish();
}

std::string base_output_schema(std::string_view data_schema) {
    JsonObject properties;
    properties.member("schema_version", R"({"type":"integer","const":1})");
    properties.member("analysis_revision", R"({"type":"string"})");
    properties.member("data", std::string(data_schema));
    properties.member("diagnostics", R"({"type":"array","items":{"type":"object"}})");
    JsonObject schema;
    schema.string("type", "object");
    schema.member("properties", std::move(properties).finish());
    schema.member("required", R"(["schema_version","analysis_revision","data","diagnostics"])");
    schema.boolean("additionalProperties", false);
    return std::move(schema).finish();
}

std::string tool_definition(std::string_view name,
                            std::string_view title,
                            std::string_view description,
                            std::string_view input_schema,
                            std::string_view data_schema) {
    JsonObject tool;
    tool.string("name", name);
    tool.string("title", title);
    tool.string("description", description);
    tool.member("inputSchema", std::string(input_schema));
    tool.member("outputSchema", base_output_schema(data_schema));
    return std::move(tool).finish();
}

std::string tools_catalog() {
    constexpr std::string_view empty_input =
        R"({"type":"object","properties":{},"additionalProperties":false})";
    constexpr std::string_view id_input =
        R"({"type":"object","properties":{"id":{"type":"string","pattern":"^(REQ|PLN)-[0-9]{4}$"}},"required":["id"],"additionalProperties":false})";
    constexpr std::string_view search_input =
        R"({"type":"object","properties":{"query":{"type":"string"},"kind":{"type":"string","enum":["requirement","plan"]},"status":{"type":"string"},"evidence_state":{"type":"string","enum":["unknown","passed","failed"]},"diagnostic_severity":{"type":"string","enum":["note","warning","error"]},"diagnostic_code":{"type":"string"},"limit":{"type":"integer","minimum":1,"maximum":100},"cursor":{"type":"string"}},"additionalProperties":false})";
    constexpr std::string_view validate_input =
        R"({"type":"object","properties":{"ids":{"type":"array","items":{"type":"string"}},"source_paths":{"type":"array","items":{"type":"string"}},"severity":{"type":"string","enum":["note","warning","error"]},"code":{"type":"string"},"limit":{"type":"integer","minimum":1,"maximum":100},"cursor":{"type":"string"}},"additionalProperties":false})";
    constexpr std::string_view object_data = R"({"type":"object"})";
    JsonArray tools;
    tools.value(tool_definition("project_overview", "Project overview",
        "Start here when no identifier is known. Returns project inputs, graph counts, validation summary, untraced requirements, and outstanding plans.",
        empty_input, object_data));
    tools.value(tool_definition("search", "Search trace entities",
        "Find requirements and plan items by identifier or text, with optional status, evidence, and diagnostic filters. Returns bounded summaries; use requirement or plan for full content.",
        search_input, object_data));
    tools.value(tool_definition("requirement", "Requirement detail",
        "Return one requirement with direct dependencies, dependents, plans, implementation and verification evidence, provenance, locations, and diagnostics.",
        id_input, object_data));
    tools.value(tool_definition("requirement_context", "Requirement prerequisite context",
        "Return one requirement and its cycle-safe transitive prerequisite context with evidence, relationships, provenance, and diagnostics.",
        id_input, object_data));
    tools.value(tool_definition("change_impact", "Requirement change impact",
        "Return cycle-safe direct and transitive dependents and the dependency paths that make them potentially affected by a requirement change.",
        id_input, object_data));
    tools.value(tool_definition("plan", "Plan item detail",
        "Return one implementation plan item, its status, full body, explicit requirement links, locations, and diagnostics. Plan state is not evidence.",
        id_input, object_data));
    tools.value(tool_definition("validate", "Validate traceability",
        "Return project diagnostics, optionally filtered by trace IDs, configured paths, severity, or code. Results explicitly report filtering and truncation.",
        validate_input, object_data));
    JsonObject result;
    result.member("tools", std::move(tools).finish());
    return std::move(result).finish();
}

std::string project_data(const ProjectSnapshot& snapshot) {
    const TraceReport report = build_report(snapshot.trace, snapshot.validation);
    JsonObject summary;
    summary.number("requirements", report.summary.requirements);
    summary.number("trace_nodes", report.summary.trace_nodes);
    summary.number("relationships", report.summary.relationships);
    summary.number("plan_items", report.summary.plan_items);
    summary.number("outstanding_plan_items", report.summary.outstanding_plan_items);
    summary.number("validation_errors", report.summary.validation_errors);
    summary.number("validation_warnings", report.summary.validation_warnings);

    JsonArray inputs;
    for (const auto& input : snapshot.inputs) {
        JsonObject value;
        value.string("kind", input.kind);
        value.string("path", input.path);
        inputs.value(std::move(value).finish());
    }
    JsonArray untraced;
    for (const auto& identifier : report.untraced_requirements) {
        untraced.string(identifier);
    }
    JsonArray plans;
    for (const auto& plan : report.outstanding_plans) {
        plans.value(node_json(plan, false));
    }
    JsonObject data;
    data.string("config_path", snapshot.config_path);
    data.string("project_root", snapshot.config.root);
    data.member("inputs", std::move(inputs).finish());
    data.member("summary", std::move(summary).finish());
    data.member("untraced_requirements", std::move(untraced).finish());
    data.member("outstanding_plans", std::move(plans).finish());
    data.boolean("complete", true);
    return std::move(data).finish();
}

std::optional<std::size_t> positive_size(const mcujson::JsonRef value,
                                         std::size_t maximum) {
    if (!value.is_number()) {
        return std::nullopt;
    }
    const auto number = value.get<long long>();
    if (number <= 0 || number > static_cast<long long>(maximum)) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(number);
}

std::optional<std::size_t> cursor_offset(const mcujson::JsonRef value) {
    if (!value.valid()) {
        return std::size_t{0};
    }
    const auto cursor = string_value(value);
    if (!cursor || cursor->empty()) {
        return std::nullopt;
    }
    std::size_t result = 0;
    const auto parsed = std::from_chars(cursor->data(), cursor->data() + cursor->size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != cursor->data() + cursor->size()) {
        return std::nullopt;
    }
    return result;
}

std::string lower(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

bool diagnostic_matches_node(const Diagnostic& diagnostic, const Node& node) {
    if (diagnostic.message.find(node.id) != std::string::npos) {
        return true;
    }
    return diagnostic.source && node.source && diagnostic.source->path == node.source->path &&
           (diagnostic.source->line == 0U || diagnostic.source->line == node.source->line);
}

struct SearchOptions final {
    std::string query;
    std::string kind;
    std::string status;
    std::string evidence_state;
    std::string diagnostic_severity;
    std::string diagnostic_code;
    std::size_t limit = 20U;
    std::size_t offset = 0U;
};

std::optional<std::string> optional_string_argument(const mcujson::JsonRef arguments,
                                                    std::string_view name,
                                                    std::string& failure) {
    const auto value = arguments[name];
    if (!value.valid()) {
        return std::string{};
    }
    const auto parsed = string_value(value);
    if (!parsed) {
        failure = std::string(name) + " must be a string";
    }
    return parsed;
}

bool valid_search_enums(const SearchOptions& options, std::string& failure) {
    if (!options.kind.empty() && options.kind != "requirement" && options.kind != "plan") {
        failure = "kind must be requirement or plan";
        return false;
    }
    if (!options.evidence_state.empty() && options.evidence_state != "unknown" &&
        options.evidence_state != "passed" && options.evidence_state != "failed") {
        failure = "evidence_state must be unknown, passed, or failed";
        return false;
    }
    if (!options.diagnostic_severity.empty() && options.diagnostic_severity != "note" &&
        options.diagnostic_severity != "warning" &&
        options.diagnostic_severity != "error") {
        failure = "diagnostic_severity must be note, warning, or error";
        return false;
    }
    return true;
}

std::optional<SearchOptions> parse_search_options(const mcujson::JsonRef arguments,
                                                  std::string& failure) {
    SearchOptions options;
    auto query = optional_string_argument(arguments, "query", failure);
    auto kind = optional_string_argument(arguments, "kind", failure);
    auto status = optional_string_argument(arguments, "status", failure);
    auto evidence = optional_string_argument(arguments, "evidence_state", failure);
    auto severity = optional_string_argument(arguments, "diagnostic_severity", failure);
    auto code = optional_string_argument(arguments, "diagnostic_code", failure);
    if (!query || !kind || !status || !evidence || !severity || !code) {
        return std::nullopt;
    }
    options.query = lower(*query);
    options.kind = std::move(*kind);
    options.status = std::move(*status);
    options.evidence_state = std::move(*evidence);
    options.diagnostic_severity = std::move(*severity);
    options.diagnostic_code = std::move(*code);
    if (!valid_search_enums(options, failure)) {
        return std::nullopt;
    }
    if (arguments["limit"].valid()) {
        const auto limit = positive_size(arguments["limit"], 100U);
        if (!limit) {
            failure = "limit must be an integer between 1 and 100";
            return std::nullopt;
        }
        options.limit = *limit;
    }
    const auto offset = cursor_offset(arguments["cursor"]);
    if (!offset) {
        failure = "cursor must be a non-negative decimal offset";
        return std::nullopt;
    }
    options.offset = *offset;
    return options;
}

bool matches_search_diagnostic(const ProjectSnapshot& snapshot,
                               const Node& node,
                               const SearchOptions& options) {
    if (options.diagnostic_severity.empty() && options.diagnostic_code.empty()) {
        return true;
    }
    return std::any_of(snapshot.validation.diagnostics.begin(),
                       snapshot.validation.diagnostics.end(),
        [&](const Diagnostic& diagnostic) {
            return diagnostic_matches_node(diagnostic, node) &&
                (options.diagnostic_severity.empty() ||
                 severity_name(diagnostic.severity) == options.diagnostic_severity) &&
                (options.diagnostic_code.empty() || diagnostic.code == options.diagnostic_code);
        });
}

bool matches_search(const ProjectSnapshot& snapshot,
                    const Node& node,
                    const SearchOptions& options) {
    if (node.kind != NodeKind::requirement && node.kind != NodeKind::plan) {
        return false;
    }
    if (!options.kind.empty() && node_kind_name(node.kind) != options.kind) {
        return false;
    }
    if (!options.status.empty() && node.status != options.status) {
        return false;
    }
    if (!options.evidence_state.empty() &&
        evidence_state_name(node.evidence_state) != options.evidence_state) {
        return false;
    }
    if (!options.query.empty()) {
        const std::string haystack = lower(node.id + "\n" + node.label + "\n" + node.body);
        if (haystack.find(options.query) == std::string::npos) {
            return false;
        }
    }
    return matches_search_diagnostic(snapshot, node, options);
}

std::string search_node_json(const ProjectSnapshot& snapshot, const Node& node) {
    JsonObject value;
    value.string("id", node.id);
    value.string("kind", node_kind_name(node.kind));
    value.string("title", node.label);
    value.string("evidence_state", evidence_state_name(node.evidence_state));
    if (!node.status.empty()) {
        value.string("status", node.status);
    }
    if (node.source) {
        value.member("source", source_json(*node.source));
    }
    const auto issue_count = static_cast<std::size_t>(std::count_if(
        snapshot.validation.diagnostics.begin(), snapshot.validation.diagnostics.end(),
        [&](const Diagnostic& diagnostic) { return diagnostic_matches_node(diagnostic, node); }));
    value.number("diagnostic_count", issue_count);
    return std::move(value).finish();
}

std::string search_result(const ProjectSnapshot& snapshot,
                          const mcujson::JsonRef arguments,
                          std::string& failure) {
    const auto options = parse_search_options(arguments, failure);
    if (!options) {
        return {};
    }
    std::vector<const Node*> matches;
    for (const auto& node : snapshot.trace.graph.nodes()) {
        if (matches_search(snapshot, node, *options)) {
            matches.push_back(&node);
        }
    }

    if (options->offset > matches.size()) {
        failure = "cursor is beyond the result set";
        return {};
    }
    const std::size_t end = std::min(matches.size(), options->offset + options->limit);
    JsonArray values;
    for (std::size_t index = options->offset; index < end; ++index) {
        values.value(search_node_json(snapshot, *matches[index]));
    }
    JsonObject data;
    data.member("matches", std::move(values).finish());
    data.number("total", matches.size());
    data.number("offset", options->offset);
    data.number("returned", end - options->offset);
    const bool truncated = end < matches.size();
    data.boolean("truncated", truncated);
    data.boolean("complete", !truncated);
    if (truncated) {
        data.string("next_cursor", std::to_string(end));
    }
    return std::move(data).finish();
}

std::string change_impact_data(const ProjectSnapshot& snapshot,
                               std::string_view identifier,
                               std::set<std::string>& identifiers,
                               std::string& failure) {
    const Node* requested = snapshot.trace.graph.find_node(identifier);
    if (requested == nullptr || requested->kind != NodeKind::requirement) {
        failure = "unknown requirement: " + std::string(identifier);
        return {};
    }
    identifiers.insert(requested->id);
    std::queue<std::string> pending;
    pending.push(requested->id);
    std::vector<const Node*> dependents;
    std::vector<const Edge*> dependencies;
    std::vector<const Edge*> unresolved;
    while (!pending.empty()) {
        const std::string current = std::move(pending.front());
        pending.pop();
        for (const auto& edge : snapshot.trace.graph.edges()) {
            if (edge.type.kind != RelationshipKind::depends_on || edge.target_id != current) {
                continue;
            }
            const Node* dependent = snapshot.trace.graph.find_node(edge.source_id);
            if (dependent == nullptr || dependent->kind != NodeKind::requirement) {
                unresolved.push_back(&edge);
                continue;
            }
            dependencies.push_back(&edge);
            if (identifiers.insert(dependent->id).second) {
                dependents.push_back(dependent);
                pending.push(dependent->id);
            }
        }
    }
    std::sort(dependents.begin(), dependents.end(), [](const Node* left, const Node* right) {
        return left->id < right->id;
    });

    JsonArray nodes;
    for (const auto* node : dependents) {
        nodes.value(node_json(*node));
    }
    JsonArray edges;
    for (const auto* edge : dependencies) {
        edges.value(edge_json(*edge));
    }
    JsonArray missing;
    for (const auto* edge : unresolved) {
        missing.value(edge_json(*edge));
    }
    JsonObject data;
    data.member("requested", node_json(*requested));
    data.member("dependents", std::move(nodes).finish());
    data.member("dependencies", std::move(edges).finish());
    data.member("unresolved_dependencies", std::move(missing).finish());
    data.boolean("complete", true);
    return std::move(data).finish();
}

std::string plan_data(const ProjectSnapshot& snapshot,
                      std::string_view identifier,
                      std::set<std::string>& identifiers,
                      std::string& failure) {
    const Node* plan = snapshot.trace.graph.find_node(identifier);
    if (plan == nullptr || plan->kind != NodeKind::plan) {
        failure = "unknown plan item: " + std::string(identifier);
        return {};
    }
    identifiers.insert(plan->id);
    JsonArray requirements;
    JsonArray links;
    JsonArray unresolved;
    for (const auto& edge : snapshot.trace.graph.edges()) {
        if (edge.type.kind != RelationshipKind::plans || edge.source_id != plan->id) {
            continue;
        }
        links.value(edge_json(edge));
        const Node* requirement = snapshot.trace.graph.find_node(edge.target_id);
        if (requirement == nullptr || requirement->kind != NodeKind::requirement) {
            unresolved.value(edge_json(edge));
            continue;
        }
        identifiers.insert(requirement->id);
        requirements.value(node_json(*requirement, false));
    }
    JsonObject data;
    data.member("plan", node_json(*plan));
    data.member("requirements", std::move(requirements).finish());
    data.member("links", std::move(links).finish());
    data.member("unresolved_links", std::move(unresolved).finish());
    data.boolean("plan_is_evidence", false);
    return std::move(data).finish();
}

std::optional<std::vector<std::string>> string_array_argument(
    const mcujson::JsonRef arguments,
    std::string_view name,
    std::string& failure) {
    const auto value = arguments[name];
    if (!value.valid()) {
        return std::vector<std::string>{};
    }
    if (!value.is_array()) {
        failure = std::string(name) + " must be an array of strings";
        return std::nullopt;
    }
    std::vector<std::string> result;
    for (const auto entry : value) {
        const auto text = string_value(entry);
        if (!text) {
            failure = std::string(name) + " must contain only strings";
            return std::nullopt;
        }
        result.push_back(*text);
    }
    return result;
}

std::string normalized_filter_path(std::string_view value, std::string_view root) {
    std::filesystem::path path(value);
    if (path.is_relative()) {
        path = std::filesystem::path(root) / path;
    }
    return path.lexically_normal().generic_string();
}

bool configured_path(const ProjectSnapshot& snapshot, std::string_view path) {
    return std::any_of(snapshot.inputs.begin(), snapshot.inputs.end(),
        [&](const ProjectInput& input) { return input.kind == "source" && input.path == path; });
}

struct ValidationOptions final {
    std::vector<std::string> identifiers;
    std::vector<std::string> paths;
    std::string severity;
    std::string code;
    std::size_t limit = 100U;
    std::size_t offset = 0U;
};

bool validate_filter_entities(const ProjectSnapshot& snapshot,
                              ValidationOptions& options,
                              std::string& failure) {
    for (const auto& identifier : options.identifiers) {
        if (snapshot.trace.graph.find_node(identifier) == nullptr) {
            failure = "unknown trace identifier: " + identifier;
            return false;
        }
    }
    for (auto& path : options.paths) {
        path = normalized_filter_path(path, snapshot.config.root);
        if (!configured_path(snapshot, path)) {
            failure = "source path is not a configured project input: " + path;
            return false;
        }
    }
    return true;
}

std::optional<ValidationOptions> parse_validation_options(
    const ProjectSnapshot& snapshot,
    const mcujson::JsonRef arguments,
    std::string& failure) {
    ValidationOptions options;
    auto identifiers = string_array_argument(arguments, "ids", failure);
    auto paths = string_array_argument(arguments, "source_paths", failure);
    auto severity = optional_string_argument(arguments, "severity", failure);
    auto code = optional_string_argument(arguments, "code", failure);
    if (!identifiers || !paths || !severity || !code) {
        return std::nullopt;
    }
    options.identifiers = std::move(*identifiers);
    options.paths = std::move(*paths);
    options.severity = std::move(*severity);
    options.code = std::move(*code);
    if (!options.severity.empty() && options.severity != "note" &&
        options.severity != "warning" && options.severity != "error") {
        failure = "severity must be note, warning, or error";
        return std::nullopt;
    }
    if (!validate_filter_entities(snapshot, options, failure)) {
        return std::nullopt;
    }
    if (arguments["limit"].valid()) {
        const auto limit = positive_size(arguments["limit"], 100U);
        if (!limit) {
            failure = "limit must be an integer between 1 and 100";
            return std::nullopt;
        }
        options.limit = *limit;
    }
    const auto offset = cursor_offset(arguments["cursor"]);
    if (!offset) {
        failure = "cursor must be a non-negative decimal offset";
        return std::nullopt;
    }
    options.offset = *offset;
    return options;
}

bool matches_validation_filter(const Diagnostic& diagnostic,
                               const ValidationOptions& options) {
    const bool id_match = options.identifiers.empty() ||
        std::any_of(options.identifiers.begin(), options.identifiers.end(),
            [&](const std::string& identifier) {
                return diagnostic.message.find(identifier) != std::string::npos;
            });
    const bool path_match = options.paths.empty() || (diagnostic.source &&
        std::find(options.paths.begin(), options.paths.end(), diagnostic.source->path) !=
            options.paths.end());
    return id_match && path_match &&
        (options.severity.empty() || severity_name(diagnostic.severity) == options.severity) &&
        (options.code.empty() || diagnostic.code == options.code);
}

std::string validation_data(const ProjectSnapshot& snapshot,
                            const mcujson::JsonRef arguments,
                            std::vector<Diagnostic>& selected,
                            std::string& failure) {
    const auto options = parse_validation_options(snapshot, arguments, failure);
    if (!options) {
        return {};
    }

    std::vector<Diagnostic> filtered;
    for (const auto& diagnostic : snapshot.validation.diagnostics) {
        if (matches_validation_filter(diagnostic, *options)) {
            filtered.push_back(diagnostic);
        }
    }
    if (options->offset > filtered.size()) {
        failure = "cursor is beyond the diagnostic result set";
        return {};
    }
    const std::size_t end = std::min(filtered.size(), options->offset + options->limit);
    selected.assign(filtered.begin() + static_cast<std::ptrdiff_t>(options->offset),
                    filtered.begin() + static_cast<std::ptrdiff_t>(end));
    const bool truncated = end < filtered.size();
    const bool has_filters = !options->identifiers.empty() || !options->paths.empty() ||
        !options->severity.empty() || !options->code.empty();
    JsonObject data;
    data.string("scope", has_filters ? "filtered" : "project");
    data.boolean("filtered", has_filters);
    data.boolean("validation_failed", snapshot.validation.failed);
    data.number("total", filtered.size());
    data.number("offset", options->offset);
    data.number("returned", selected.size());
    data.boolean("truncated", truncated);
    data.boolean("complete", !truncated);
    if (truncated) {
        data.string("next_cursor", std::to_string(end));
    }
    data.member("diagnostics", diagnostics_json(selected));
    return std::move(data).finish();
}

std::string rules_resource() {
    constexpr std::string_view local_annotation = "@req";
    constexpr std::string_view file_annotation = "@req-file";
    return std::string(R"(# mcutrace rules

mcutrace aggregates and validates traceability data; it does not run build, test,
coverage, or static-analysis producers.

- Requirements are Markdown heading blocks identified by canonical `REQ-NNNN` IDs.
- `@evidence(test, implementation, coverage, build)` declares evidence expectations;
  `@evidence(none)` explicitly declares no expectation.
- `@depends(REQ-NNNN, ...)` creates directed `depends_on` relationships from the
  dependent requirement to its prerequisites. Ordinary ID mentions create no link.
- Dependency evidence is independent: prerequisites neither satisfy nor copy evidence.
- Source `// )") +
           std::string(local_annotation) +
           R"(` annotations attach implementation claims to declarations;
  `// )" + std::string(file_annotation) +
           R"(` is the explicit file-scoped fallback.
- Tcl `.tcl` outer-script `# @req-file` comments are supported.
- `test_sources` / `--test-source` declare Tcl verification intent; matching
  mcutest `language: tcl` results require a matching `source_digest` to verify.
- CMake `# @req` annotations create build verification links.
- Plan headings use canonical `PLN-NNNN` IDs with explicit `@link(REQ-NNNN, ...)`
  and one status: planned, in_progress, blocked, or done.
- Plan links and statuses represent work, never implementation or verification evidence.
- Missing evidence, failed evidence, unresolved relationships, findings, and analysis
  failures are distinct states and must not be reported as interchangeable.
- Canonical IDs, relationship direction, provenance, source locations, diagnostics,
  and analysis revision should be retained when using MCP results.
)";
}

std::string schema_resource() {
    return R"({"schema_version":1,"protocol_version":"2025-11-25","result_envelope":{"required":["schema_version","analysis_revision","data","diagnostics"]},"node_kinds":["requirement","plan","source","implementation","test","coverage","finding","artifact"],"relationship_kinds":["satisfies","verifies","implements","covers","reports","depends_on","plans","relates","custom"],"evidence_states":["unknown","passed","failed"],"evidence_expectations":["test","implementation","coverage","build"],"plan_statuses":["planned","in_progress","blocked","done"],"tools":["project_overview","search","requirement","requirement_context","change_impact","plan","validate"]})";
}

std::string resources_catalog() {
    JsonArray resources;
    const auto add = [&](std::string_view uri, std::string_view name,
                         std::string_view description, std::string_view media_type) {
        JsonObject resource;
        resource.string("uri", uri);
        resource.string("name", name);
        resource.string("description", description);
        resource.string("mimeType", media_type);
        resources.value(std::move(resource).finish());
    };
    add("mcutrace://rules", "mcutrace rules",
        "Annotations, relationship semantics, evidence rules, and usage boundaries supported by this server.",
        "text/markdown");
    add("mcutrace://schema", "mcutrace MCP schema",
        "MCP result envelope version and mcutrace entity vocabulary.", "application/json");
    add("mcutrace://project", "mcutrace project",
        "Selected configuration, resolved inputs, current analysis revision, and validation summary.",
        "application/json");
    JsonObject result;
    result.member("resources", std::move(resources).finish());
    return std::move(result).finish();
}

std::string resource_contents(std::string_view uri,
                              std::string_view media_type,
                              std::string_view content) {
    JsonObject item;
    item.string("uri", uri);
    item.string("mimeType", media_type);
    item.string("text", content);
    JsonArray contents;
    contents.value(std::move(item).finish());
    JsonObject result;
    result.member("contents", std::move(contents).finish());
    return std::move(result).finish();
}

bool known_tool_name(std::string_view name) {
    constexpr std::string_view names[] = {
        "project_overview", "search", "requirement", "requirement_context",
        "change_impact", "plan", "validate",
    };
    return std::find(std::begin(names), std::end(names), name) != std::end(names);
}

std::string requirement_tool(const ProjectSnapshot& project,
                             std::string_view identifier) {
    const auto report = build_requirement_trace_report(project.trace, project.validation,
                                                       identifier);
    if (!report) {
        return tool_failure("mcutrace.unknown_requirement",
                            "unknown requirement: " + std::string(identifier));
    }
    const auto data = render_requirement_json_report(*report);
    if (!data) {
        return tool_failure("mcutrace.serialize", data.error().detail);
    }
    return tool_success(envelope(project, *data,
        related_diagnostics(project, {std::string(identifier)})));
}

std::string requirement_context_tool(const ProjectSnapshot& project,
                                     std::string_view identifier) {
    const auto report = build_requirement_context_report(project.trace, project.validation,
                                                         identifier);
    if (!report) {
        return tool_failure("mcutrace.unknown_requirement",
                            "unknown requirement: " + std::string(identifier));
    }
    const auto data = render_requirement_context_json_report(*report);
    if (!data) {
        return tool_failure("mcutrace.serialize", data.error().detail);
    }
    std::set<std::string> identifiers;
    for (const auto& requirement : report->requirements) {
        identifiers.insert(requirement.id);
    }
    return tool_success(envelope(project, *data, related_diagnostics(project, identifiers)));
}

std::string change_impact_tool(const ProjectSnapshot& project,
                               std::string_view identifier) {
    std::set<std::string> identifiers;
    std::string failure;
    const std::string data = change_impact_data(project, identifier, identifiers, failure);
    return failure.empty()
        ? tool_success(envelope(project, data, related_diagnostics(project, identifiers)))
        : tool_failure("mcutrace.unknown_requirement", failure);
}

std::string plan_tool(const ProjectSnapshot& project, std::string_view identifier) {
    std::set<std::string> identifiers;
    std::string failure;
    const std::string data = plan_data(project, identifier, identifiers, failure);
    return failure.empty()
        ? tool_success(envelope(project, data, related_diagnostics(project, identifiers)))
        : tool_failure("mcutrace.unknown_plan", failure);
}

std::string validation_tool(const ProjectSnapshot& project,
                            const mcujson::JsonRef arguments) {
    std::vector<Diagnostic> diagnostics;
    std::string failure;
    const std::string data = validation_data(project, arguments, diagnostics, failure);
    return failure.empty()
        ? tool_success(envelope(project, data, diagnostics))
        : tool_failure("mcp.invalid_arguments", failure);
}

std::string identifier_tool(const ProjectSnapshot& project,
                            std::string_view name,
                            const mcujson::JsonRef arguments) {
    const auto identifier = string_value(arguments["id"]);
    if (!identifier) {
        return tool_failure("mcp.invalid_arguments", "tool requires a string id");
    }
    if (name == "requirement") {
        return requirement_tool(project, *identifier);
    }
    if (name == "requirement_context") {
        return requirement_context_tool(project, *identifier);
    }
    if (name == "change_impact") {
        return change_impact_tool(project, *identifier);
    }
    return plan_tool(project, *identifier);
}

std::string loaded_tool(const ProjectSnapshot& project,
                        std::string_view name,
                        const mcujson::JsonRef arguments) {
    if (name == "project_overview") {
        return tool_success(envelope(project, project_data(project),
                                     project.validation.diagnostics));
    }
    if (name == "search") {
        std::string failure;
        const std::string data = search_result(project, arguments, failure);
        return failure.empty()
            ? tool_success(envelope(project, data, {}))
            : tool_failure("mcp.invalid_arguments", failure);
    }
    if (name == "validate") {
        return validation_tool(project, arguments);
    }
    return identifier_tool(project, name, arguments);
}

}  // namespace

struct McpServer::State final {
    explicit State(std::string path) : config_path(std::move(path)) {}

    std::string config_path;
    bool initialized = false;

    [[nodiscard]] std::expected<ProjectSnapshot, ProjectLoadError> snapshot() const {
        return load_project_snapshot(config_path);
    }

    [[nodiscard]] std::string initialize(const mcujson::JsonRef params,
                                         std::string_view id) {
        const auto requested = string_value(params["protocolVersion"]);
        if (!requested || (*requested != kMcpProtocolVersion && *requested != "2025-06-18")) {
            return error_response(id, -32602, "unsupported MCP protocol revision");
        }
        initialized = true;
        JsonObject tools;
        tools.boolean("listChanged", false);
        JsonObject resources;
        resources.boolean("subscribe", false);
        resources.boolean("listChanged", false);
        JsonObject capabilities;
        capabilities.member("tools", std::move(tools).finish());
        capabilities.member("resources", std::move(resources).finish());
        JsonObject server_info;
        server_info.string("name", "mcutrace");
        server_info.string("version", "0.1.0");
        JsonObject result;
        result.string("protocolVersion", *requested);
        result.member("capabilities", std::move(capabilities).finish());
        result.member("serverInfo", std::move(server_info).finish());
        result.string("instructions",
            "Read-only mcutrace access. Start with project_overview or search, then use identifier-specific tools. Plan state is not evidence.");
        return response(id, std::move(result).finish());
    }

    [[nodiscard]] std::string call_tool(const mcujson::JsonRef params) const {
        const auto name = string_value(params["name"]);
        if (!name) {
            return tool_failure("mcp.invalid_arguments", "tools/call requires a string name");
        }
        if (!known_tool_name(*name)) {
            return tool_failure("mcp.unknown_tool", "unknown tool: " + *name);
        }
        const auto arguments = params["arguments"];
        if (arguments.valid() && !arguments.is_object()) {
            return tool_failure("mcp.invalid_arguments", "tools/call arguments must be an object");
        }
        auto loaded = snapshot();
        if (!loaded) {
            return tool_failure(loaded.error().code, loaded.error().detail);
        }
        const mcujson::JsonRef args = arguments.valid() ? arguments : mcujson::JsonRef{};
        return loaded_tool(*loaded, *name, args);
    }

    [[nodiscard]] std::string read_resource(const mcujson::JsonRef params,
                                            std::string_view id) const {
        const auto uri = string_value(params["uri"]);
        if (!uri) {
            return error_response(id, -32602, "resources/read requires a string uri");
        }
        if (*uri == "mcutrace://rules") {
            return response(id, resource_contents(*uri, "text/markdown", rules_resource()));
        }
        if (*uri == "mcutrace://schema") {
            return response(id, resource_contents(*uri, "application/json", schema_resource()));
        }
        if (*uri == "mcutrace://project") {
            auto loaded = snapshot();
            if (!loaded) {
                return error_response(id, -32002, loaded.error().detail);
            }
            const std::string content = envelope(*loaded, project_data(*loaded),
                                                 loaded->validation.diagnostics);
            return response(id, resource_contents(*uri, "application/json", content));
        }
        return error_response(id, -32602, "unknown mcutrace resource");
    }

    [[nodiscard]] std::vector<std::string> dispatch(std::string_view method,
                                                    const mcujson::JsonRef params,
                                                    std::string_view id) {
        if (method == "initialize") {
            return {initialize(params, id)};
        }
        if (method == "notifications/initialized" || method == "notifications/cancelled") {
            return {};
        }
        if (method == "ping") {
            return {response(id, "{}")};
        }
        if (method == "shutdown") {
            return {response(id, "null")};
        }
        if (!initialized) {
            return id.empty() ? std::vector<std::string>{}
                              : std::vector<std::string>{error_response(id, -32002,
                                    "server has not been initialized")};
        }
        if (method == "tools/list") {
            return {response(id, tools_catalog())};
        }
        if (method == "tools/call") {
            return {response(id, call_tool(params))};
        }
        if (method == "resources/list") {
            return {response(id, resources_catalog())};
        }
        if (method == "resources/templates/list") {
            return {response(id, R"({"resourceTemplates":[]})")};
        }
        if (method == "resources/read") {
            return {read_resource(params, id)};
        }
        return id.empty() ? std::vector<std::string>{}
                          : std::vector<std::string>{error_response(id, -32601,
                                "method not found")};
    }
};

McpServer::McpServer(std::string config_path)
    : state_(std::make_unique<State>(std::move(config_path))) {}

McpServer::~McpServer() {}

// @req REQ-0139 REQ-0140 REQ-0141 REQ-0144 REQ-0145 REQ-0146 REQ-0147 REQ-0148
// @req REQ-0149 REQ-0150 REQ-0151 REQ-0152 REQ-0153 REQ-0154 REQ-0155 REQ-0156
// @req REQ-0157 REQ-0158 REQ-0159
std::vector<std::string> McpServer::handle(const std::string_view message) {
    const auto parsed = McpJson::parse(message);
    if (!parsed || !parsed->is_object()) {
        return {error_response({}, -32700, "invalid JSON-RPC message")};
    }
    const auto method = string_value((*parsed)["method"]);
    const auto id_value = (*parsed)["id"];
    const std::string id = id_value.valid() && !id_value.is_null() ? id_value.dump() : std::string{};
    if (!method) {
        return id.empty() ? std::vector<std::string>{}
                          : std::vector<std::string>{error_response(id, -32600,
                                "method is required")};
    }
    return state_->dispatch(*method, (*parsed)["params"], id);
}

// @req REQ-0138 REQ-0140 REQ-0143 REQ-0156 REQ-0159
int run_mcp_server(std::istream& input,
                   std::ostream& output,
                   std::ostream& errors,
                   std::string config_path) {
    static_cast<void>(errors);
    McpServer server(std::move(config_path));
    std::string message;
    while (std::getline(input, message)) {
        if (!message.empty() && message.back() == '\r') {
            message.pop_back();
        }
        if (message.empty()) {
            continue;
        }
        for (const auto& response_message : server.handle(message)) {
            output << response_message << '\n';
            output.flush();
            if (!output) {
                return 2;
            }
        }
    }
    return input.bad() ? 2 : 0;
}

}  // namespace mcutrace
