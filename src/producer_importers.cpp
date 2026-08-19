#include <mcutrace/producer_importers.hpp>

#include <mcutrace/requirements.hpp>

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef MCUJSON_MAX_NODES
#define MCUJSON_MAX_NODES 8192
#endif
#ifndef MCUJSON_STR_BUF
#define MCUJSON_STR_BUF 65534
#endif
#include <mcujson/mcujson.hpp>

namespace mcutrace {
namespace {

constexpr std::string_view kMcutest = "mcutest";
constexpr std::string_view kMcucov = "mcucov";
constexpr std::string_view kMcucheck = "mcucheck";

std::optional<Severity> parse_severity(std::string_view value) noexcept {
    if (value == "note" || value == "info") {
        return Severity::note;
    }
    if (value == "warning") {
        return Severity::warning;
    }
    if (value == "error") {
        return Severity::error;
    }
    return std::nullopt;
}

std::string artifact_id(std::string_view producer, std::string_view path) {
    return "artifact:" + std::string(producer) + ":" + std::string(path);
}

bool contains_node(const std::vector<Node>& nodes, std::string_view id) {
    return std::any_of(nodes.begin(), nodes.end(), [id](const Node& node) {
        return node.id == id;
    });
}

void add_node_once(std::vector<Node>& nodes, Node node) {
    if (!contains_node(nodes, node.id)) {
        nodes.push_back(std::move(node));
    }
}

std::expected<std::string, ImportError>
source_path(std::string_view path, const ArtifactInput& input) {
    return normalize_artifact_path(path, input.base_directory);
}

void add_source_and_edge(ImportFragment& fragment,
                         std::string evidence_id,
                         RelationshipKind relationship,
                         const ArtifactInput& input,
                         SourceLocation source) {
    const std::string source_id = "source:" + source.path;
    add_node_once(fragment.nodes, Node{
        .id = source_id,
        .kind = NodeKind::source,
        .label = source.path,
        .evidence_state = EvidenceState::unknown,
        .finding_state = {},
        .source = source,
        .expected_evidence = std::nullopt,
    });
    fragment.edges.push_back(Edge{
        .source_id = std::move(evidence_id),
        .target_id = source_id,
        .type = RelationshipType::known(relationship),
        .provenance = Provenance{
            .importer = fragment.format.producer,
            .artifact = input.path,
            .source = std::nullopt,
        },
        .source = std::nullopt,
    });
}

void add_requirement_edge(ImportFragment& fragment,
                          std::string source_id,
                          std::string target_id,
                          RelationshipKind relationship,
                          const ArtifactInput& input) {
    fragment.edges.push_back(Edge{
        .source_id = std::move(source_id),
        .target_id = std::move(target_id),
        .type = RelationshipType::known(relationship),
        .provenance = Provenance{
            .importer = fragment.format.schema,
            .artifact = input.path,
            .source = SourceLocation{.path = input.path},
        },
        .source = SourceLocation{.path = input.path},
    });
}

std::expected<mcujson::Json, ImportError> parse_json(const ArtifactInput& input) {
    auto parsed = mcujson::Json::parse(input.content);
    if (!parsed) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::invalid_artifact,
            .detail = "invalid JSON artifact: " + input.path,
            .source = SourceLocation{.path = input.path},
        });
    }
    return std::move(*parsed);
}

std::expected<InputFormat, ImportError>
identify_json(const ArtifactInput& input) {
    const auto format_value = mcujson::json_get<std::string_view>(input.content, "format");
    const auto version_value = mcujson::json_get<long long>(input.content, "version");
    if (!format_value || !version_value) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::unrecognized_format,
            .detail = "artifact has no recognized format/version header: " + input.path,
            .source = SourceLocation{.path = input.path},
        });
    }

    const std::string format(*format_value);
    std::string producer;
    if (format == "mcutest-results") {
        producer = std::string(kMcutest);
    } else if (format == "mcucov-report") {
        producer = std::string(kMcucov);
    } else if (format == "mcucheck-results") {
        producer = std::string(kMcucheck);
    } else {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::unrecognized_format,
            .detail = "unrecognized producer format '" + format + "'",
            .source = SourceLocation{.path = input.path},
        });
    }
    return InputFormat{
        .producer = producer,
        .schema = format,
        .version = std::to_string(*version_value),
    };
}

ImporterInfo info_for(std::string_view producer) {
    return ImporterInfo{
        .name = std::string(producer),
        .producer = std::string(producer),
        .supported_versions = {"1"},
    };
}

ImportFragment make_fragment(InputFormat format) {
    return ImportFragment{
        .format = std::move(format),
        .nodes = {},
        .edges = {},
        .artifacts = {},
        .diagnostics = {},
    };
}

void append_mcutest_test(ImportFragment& fragment,
                         const mcujson::JsonRef& test,
                         const ArtifactInput& input) {
    if (!test.is_object() || !test["name"].is_string() || !test["status"].is_string()) {
        fragment.diagnostics.push_back(Diagnostic{
            .code = "import.mcutest.invalid_test",
            .severity = Severity::warning,
            .message = "ignored malformed mcutest test entry",
            .source = SourceLocation{.path = input.path},
        });
        return;
    }

    const std::string name = test["name"].get<std::string>();
    const std::string status = test["status"].get<std::string>();
    EvidenceState evidence = EvidenceState::unknown;
    if (status == "passed") {
        evidence = EvidenceState::passed;
    } else if (status == "failed") {
        evidence = EvidenceState::failed;
    } else {
        fragment.diagnostics.push_back(Diagnostic{
            .code = "import.mcutest.unknown_status",
            .severity = Severity::warning,
            .message = "unknown mcutest status for " + name + ": " + status,
            .source = SourceLocation{.path = input.path},
        });
    }

    Node node{
        .id = "test:mcutest:" + name,
        .kind = NodeKind::test,
        .label = name,
        .evidence_state = evidence,
        .finding_state = {},
        .source = std::nullopt,
        .expected_evidence = std::nullopt,
    };
    if (test["file"].is_string()) {
        auto path = source_path(test["file"].get<std::string_view>(), input);
        if (path) {
            const auto line = test["line"].is_number()
                ? static_cast<std::uint32_t>(test["line"].get<long long>()) : 0U;
            node.source = SourceLocation{.path = *path, .line = line};
        }
    }
    add_node_once(fragment.nodes, std::move(node));
}

std::expected<ImportFragment, ImportError>
import_mcutest(const ArtifactInput& input, const mcujson::Json& root, InputFormat format) {
    if (auto supported = require_supported_version(info_for(kMcutest), format,
            SourceLocation{.path = input.path}); !supported) {
        return std::unexpected(supported.error());
    }
    const auto tests = root["tests"];
    if (!tests.is_array()) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::invalid_artifact,
            .detail = "mcutest-results requires a tests array",
            .source = SourceLocation{.path = input.path},
        });
    }

    auto fragment = make_fragment(std::move(format));
    fragment.artifacts.push_back(preserve_json_artifact(
        artifact_id(kMcutest, input.path), "mcutest-results", input.content,
        SourceLocation{.path = input.path}));
    for (const auto test : tests) {
        append_mcutest_test(fragment, test, input);
    }
    return fragment;
}

struct ParsedSkipped final {
    std::string state;
    std::string severity;
    std::string detail;
    std::uint32_t line = 0;
    std::uint32_t column = 0;
};

struct ParsedMcucovModule final {
    std::string path;
    std::string variant;
    std::vector<std::string> requirements;
    std::vector<ParsedSkipped> skipped;
};

class McucovSaxHandler final : public mcujson::SaxHandler {
public:
    bool on_object_start() noexcept override {
        ++depth_;
        if (in_modules_ && depth_ == 3) {
            current_module_ = ParsedMcucovModule{};
            in_module_ = true;
        } else if (in_skipped_ && depth_ == 5) {
            current_skipped_ = ParsedSkipped{};
            in_skipped_object_ = true;
        }
        return !failed_;
    }

    bool on_object_end() noexcept override {
        if (in_skipped_object_ && depth_ == 5) {
            current_module_.skipped.push_back(std::move(current_skipped_));
            in_skipped_object_ = false;
        } else if (in_module_ && depth_ == 3) {
            modules.push_back(std::move(current_module_));
            in_module_ = false;
        }
        --depth_;
        return !failed_;
    }

    bool on_array_start() noexcept override {
        ++depth_;
        if (depth_ == 2 && key_ == "modules") {
            saw_modules = true;
            in_modules_ = true;
        } else if (in_module_ && depth_ == 4 && key_ == "requirements") {
            in_requirements_ = true;
        } else if (in_module_ && depth_ == 4 && key_ == "skipped") {
            in_skipped_ = true;
        }
        return !failed_;
    }

    bool on_array_end() noexcept override {
        if (depth_ == 4 && in_requirements_) {
            in_requirements_ = false;
        }
        if (depth_ == 4 && in_skipped_) {
            in_skipped_ = false;
        }
        if (depth_ == 2 && in_modules_) {
            in_modules_ = false;
        }
        --depth_;
        return !failed_;
    }

    bool on_key(std::string_view key) noexcept override {
        key_.assign(key);
        return true;
    }

    bool on_string(std::string_view value) noexcept override {
        if (decode_module_string(value)) {
            return !failed_;
        }
        if (decode_requirement(value)) {
            return !failed_;
        }
        (void)decode_skipped_string(value);
        return !failed_;
    }

    bool on_int(long long value) noexcept override {
        if (!in_skipped_object_ || depth_ != 5 || value < 0) {
            return true;
        }
        if (key_ == "line") {
            current_skipped_.line = static_cast<std::uint32_t>(value);
        } else if (key_ == "column") {
            current_skipped_.column = static_cast<std::uint32_t>(value);
        }
        return true;
    }

    bool saw_modules = false;
    std::vector<ParsedMcucovModule> modules;

private:
    bool decode(std::string_view raw, std::string& output) noexcept {
        output.resize(raw.size());
        auto decoded = mcujson::decode_string(raw,
            std::span<char>{output.data(), output.size()});
        if (!decoded) {
            failed_ = true;
            return false;
        }
        output.resize(*decoded);
        return true;
    }

    bool decode_module_string(std::string_view value) noexcept {
        if (!in_module_ || depth_ != 3) {
            return false;
        }
        if (key_ == "path") {
            (void)decode(value, current_module_.path);
            return true;
        }
        if (key_ == "variant") {
            (void)decode(value, current_module_.variant);
            return true;
        }
        return false;
    }

    bool decode_requirement(std::string_view value) noexcept {
        if (!in_module_ || !in_requirements_ || depth_ != 4) {
            return false;
        }
        std::string requirement;
        if (decode(value, requirement)) {
            current_module_.requirements.push_back(std::move(requirement));
        }
        return true;
    }

    bool decode_skipped_string(std::string_view value) noexcept {
        if (!in_skipped_object_ || depth_ != 5) {
            return false;
        }
        if (key_ == "state") {
            (void)decode(value, current_skipped_.state);
            return true;
        }
        if (key_ == "severity") {
            (void)decode(value, current_skipped_.severity);
            return true;
        }
        if (key_ == "detail") {
            (void)decode(value, current_skipped_.detail);
            return true;
        }
        return false;
    }

    int depth_ = 0;
    std::string key_;
    bool failed_ = false;
    bool in_modules_ = false;
    bool in_module_ = false;
    bool in_requirements_ = false;
    bool in_skipped_ = false;
    bool in_skipped_object_ = false;
    ParsedMcucovModule current_module_;
    ParsedSkipped current_skipped_;
};

void append_mcucov_requirement_links(ImportFragment& fragment,
                                     const ParsedMcucovModule& module,
                                     std::string_view coverage_id,
                                     const ArtifactInput& input) {
    for (const auto& requirement : module.requirements) {
        if (!is_requirement_id(requirement)) {
            fragment.diagnostics.push_back(Diagnostic{
                .code = "import.requirements.invalid_id",
                .severity = Severity::warning,
                .message = "ignored invalid requirement reference",
                .source = SourceLocation{.path = input.path},
            });
            continue;
        }
        add_requirement_edge(fragment, std::string(coverage_id), requirement,
                             RelationshipKind::covers, input);
    }
}

void append_mcucov_skipped(ImportFragment& fragment,
                           const ParsedMcucovModule& module,
                           std::string_view normalized_path) {
    for (const auto& skipped : module.skipped) {
        if (skipped.state == "justified") {
            continue;
        }
        fragment.diagnostics.push_back(Diagnostic{
            .code = "import.mcucov.not_instrumented",
            .severity = parse_severity(skipped.severity).value_or(Severity::warning),
            .message = skipped.detail.empty() ? "construct was not instrumented" : skipped.detail,
            .source = SourceLocation{
                .path = std::string(normalized_path),
                .line = skipped.line,
                .column = skipped.column,
            },
        });
    }
}

std::expected<void, ImportError> append_mcucov_module(ImportFragment& fragment,
                                                       const ParsedMcucovModule& module,
                                                       const ArtifactInput& input) {
    if (module.path.empty()) {
        fragment.diagnostics.push_back(Diagnostic{
            .code = "import.mcucov.invalid_module",
            .severity = Severity::warning,
            .message = "ignored malformed mcucov module entry",
            .source = SourceLocation{.path = input.path},
        });
        return {};
    }
    auto normalized = source_path(module.path, input);
    if (!normalized) {
        return std::unexpected(normalized.error());
    }

    const std::string coverage_id = "coverage:mcucov:" + *normalized + ":" + module.variant;
    add_node_once(fragment.nodes, Node{
        .id = coverage_id,
        .kind = NodeKind::coverage,
        .label = module.path,
        .evidence_state = EvidenceState::unknown,
        .finding_state = {},
        .source = SourceLocation{.path = *normalized},
        .expected_evidence = std::nullopt,
    });
    add_source_and_edge(fragment, coverage_id, RelationshipKind::covers, input,
                        SourceLocation{.path = *normalized});
    append_mcucov_requirement_links(fragment, module, coverage_id, input);
    append_mcucov_skipped(fragment, module, *normalized);
    return {};
}

std::expected<ImportFragment, ImportError>
import_mcucov(const ArtifactInput& input, InputFormat format) {
    if (auto supported = require_supported_version(info_for(kMcucov), format,
            SourceLocation{.path = input.path}); !supported) {
        return std::unexpected(supported.error());
    }

    McucovSaxHandler handler;
    const auto parsed = mcujson::json_sax_parse(input.content, handler);
    if (!parsed || !handler.saw_modules) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::invalid_artifact,
            .detail = "mcucov-report requires a valid modules array",
            .source = SourceLocation{.path = input.path},
        });
    }

    auto fragment = make_fragment(std::move(format));
    fragment.artifacts.push_back(preserve_json_artifact(
        artifact_id(kMcucov, input.path), "mcucov-report", input.content,
        SourceLocation{.path = input.path}));
    for (const auto& module : handler.modules) {
        if (auto status = append_mcucov_module(fragment, module, input); !status) {
            return std::unexpected(status.error());
        }
    }
    return fragment;
}

std::expected<void, ImportError> append_mcucheck_diagnostic(ImportFragment& fragment,
                                                            const mcujson::JsonRef& diagnostic,
                                                            const ArtifactInput& input,
                                                            std::size_t fallback_index) {
    const auto location = diagnostic["location"];
    if (!diagnostic.is_object() || !diagnostic["rule_id"].is_string() ||
        !diagnostic["message"].is_string() || !location.is_object() ||
        !location["path"].is_string()) {
        fragment.diagnostics.push_back(Diagnostic{
            .code = "import.mcucheck.invalid_diagnostic",
            .severity = Severity::warning,
            .message = "ignored malformed mcucheck diagnostic entry",
            .source = SourceLocation{.path = input.path},
        });
        return {};
    }

    auto normalized = source_path(location["path"].get<std::string_view>(), input);
    if (!normalized) {
        return std::unexpected(normalized.error());
    }
    const auto line = location["line"].is_number()
        ? static_cast<std::uint32_t>(location["line"].get<long long>()) : 0U;
    const auto column = location["column"].is_number()
        ? static_cast<std::uint32_t>(location["column"].get<long long>()) : 0U;
    const std::string rule = diagnostic["rule_id"].get<std::string>();
    const std::string message = diagnostic["message"].get<std::string>();
    const std::string stable = diagnostic["id"].is_string()
        ? diagnostic["id"].get<std::string>()
        : rule + ":" + *normalized + ":" + std::to_string(line) + ":" +
          std::to_string(fallback_index);
    const std::string finding_id = "finding:mcucheck:" + stable;
    add_node_once(fragment.nodes, Node{
        .id = finding_id,
        .kind = NodeKind::finding,
        .label = rule + ": " + message,
        .evidence_state = EvidenceState::unknown,
        .finding_state = {},
        .source = SourceLocation{.path = *normalized, .line = line, .column = column},
        .expected_evidence = std::nullopt,
    });
    add_source_and_edge(fragment, finding_id, RelationshipKind::reports, input,
                        SourceLocation{.path = *normalized, .line = line, .column = column});
    return {};
}

std::expected<ImportFragment, ImportError>
import_mcucheck(const ArtifactInput& input, const mcujson::Json& root, InputFormat format) {
    if (auto supported = require_supported_version(info_for(kMcucheck), format,
            SourceLocation{.path = input.path}); !supported) {
        return std::unexpected(supported.error());
    }
    const auto diagnostics = root["diagnostics"];
    if (!diagnostics.is_array()) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::invalid_artifact,
            .detail = "mcucheck-results requires a diagnostics array",
            .source = SourceLocation{.path = input.path},
        });
    }

    auto fragment = make_fragment(std::move(format));
    fragment.artifacts.push_back(preserve_json_artifact(
        artifact_id(kMcucheck, input.path), "mcucheck-results", input.content,
        SourceLocation{.path = input.path}));

    std::size_t fallback_index = 0;
    for (const auto diagnostic : diagnostics) {
        ++fallback_index;
        if (auto status = append_mcucheck_diagnostic(fragment, diagnostic, input, fallback_index);
            !status) {
            return std::unexpected(status.error());
        }
    }
    return fragment;
}

}  // namespace

std::vector<ImporterInfo> producer_importer_info() {
    return {info_for(kMcutest), info_for(kMcucov), info_for(kMcucheck)};
}

// @req REQ-0037 REQ-0038 REQ-0039 REQ-0041 REQ-0042 REQ-0080
std::expected<ImportFragment, ImportError>
import_producer_artifact(const ArtifactInput& input, std::string_view requested_importer) {
    auto format = identify_json(input);
    if (!format) {
        return std::unexpected(format.error());
    }
    if (!requested_importer.empty() && requested_importer != format->producer) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::unrecognized_format,
            .detail = "artifact identifies as '" + format->producer +
                      "' but importer '" + std::string(requested_importer) + "' was requested",
            .source = SourceLocation{.path = input.path},
        });
    }

    if (format->producer == kMcucov) {
        return import_mcucov(input, std::move(*format));
    }

    auto root = parse_json(input);
    if (!root) {
        return std::unexpected(root.error());
    }
    if (format->producer == kMcutest) {
        return import_mcutest(input, *root, std::move(*format));
    }
    if (format->producer == kMcucheck) {
        return import_mcucheck(input, *root, std::move(*format));
    }
    return std::unexpected(ImportError{
        .code = ImportErrorCode::unrecognized_format,
        .detail = "no importer registered for artifact",
        .source = SourceLocation{.path = input.path},
    });
}

}  // namespace mcutrace
