#include <mcutrace/cli.hpp>

#include <mcutrace/assembly.hpp>
#include <mcutrace/build_annotations.hpp>
#include <mcutrace/config.hpp>
#include <mcutrace/output.hpp>
#include <mcutrace/path_patterns.hpp>
#include <mcutrace/requirements.hpp>
#include <mcutrace/source_annotations.hpp>
#include <mcutrace/trace_import.hpp>
#include <mcutrace/validation.hpp>

#include <mcucli/mcucli.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mcutrace {
namespace {

constexpr std::string_view kVersion = "0.1.0";

std::string cli_error_text(const mcucli::Error& error) {
    std::string result = "command line error";
    if (!error.subject.empty()) {
        result += " for " + std::string(error.subject);
    }
    if (!error.detail.empty()) {
        result += ": " + std::string(error.detail);
    }
    if (!error.token.empty()) {
        result += " (" + std::string(error.token) + ")";
    }
    return result;
}

std::expected<std::string, CliError> read_text_file(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return std::unexpected(CliError{.message = "cannot open file: " + path});
    }
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::string normalize_explicit_path(std::string_view value) {
    std::filesystem::path path(value);
    if (path.is_relative()) {
        path = std::filesystem::current_path() / path;
    }
    return path.lexically_normal().generic_string();
}

void print_diagnostic(const Diagnostic& diagnostic) {
    if (diagnostic.source) {
        std::cerr << diagnostic.source->path;
        if (diagnostic.source->line != 0) {
            std::cerr << ':' << diagnostic.source->line;
        }
        std::cerr << ": ";
    }
    std::cerr << severity_name(diagnostic.severity) << ' ' << diagnostic.code << ": "
              << diagnostic.message << '\n';
}

std::expected<void, CliError> parse_output_format(std::string_view value,
                                                  OutputFormat& output_format) {
    if (value == "text") {
        output_format = OutputFormat::text;
        return {};
    }
    if (value == "json") {
        output_format = OutputFormat::json;
        return {};
    }
    return std::unexpected(CliError{.message = "output format must be 'text' or 'json'"});
}

std::expected<ImportFragment, CliError>
load_artifact(const std::string& path,
              const std::string& base_directory,
              std::string_view importer) {
    auto content = read_text_file(path);
    if (!content) {
        return std::unexpected(content.error());
    }
    auto fragment = import_trace_artifact(ArtifactInput{
        .path = path,
        .base_directory = base_directory,
        .content = std::move(*content),
    }, importer);
    if (!fragment) {
        return std::unexpected(CliError{
            .message = "artifact import error: " + fragment.error().detail,
        });
    }
    return std::move(*fragment);
}

std::expected<ImportFragment, CliError>
load_source(const std::string& path, const std::string& base_directory) {
    auto content = read_text_file(path);
    if (!content) {
        return std::unexpected(content.error());
    }
    auto fragment = import_source_annotations(ArtifactInput{
        .path = path,
        .base_directory = base_directory,
        .content = std::move(*content),
    });
    if (!fragment) {
        return std::unexpected(CliError{
            .message = "source annotation import error: " + fragment.error().detail,
        });
    }
    return std::move(*fragment);
}

std::expected<ImportFragment, CliError>
load_build(const std::string& path, const std::string& base_directory) {
    auto content = read_text_file(path);
    if (!content) {
        return std::unexpected(content.error());
    }
    auto fragment = import_build_annotations(ArtifactInput{
        .path = path,
        .base_directory = base_directory,
        .content = std::move(*content),
    });
    if (!fragment) {
        return std::unexpected(CliError{
            .message = "build annotation import error: " + fragment.error().detail,
        });
    }
    return std::move(*fragment);
}

std::expected<ProjectConfig, CliError> load_project_config(const CliOptions& options) {
    if (options.config_path.empty()) {
        ProjectConfig config;
        config.root = std::filesystem::current_path().lexically_normal().generic_string();
        return config;
    }

    const std::string config_path = normalize_explicit_path(options.config_path);
    auto content = read_text_file(config_path);
    if (!content) {
        return std::unexpected(content.error());
    }
    auto parsed = parse_project_config(*content, config_path);
    if (!parsed) {
        return std::unexpected(CliError{
            .message = "configuration error: " + parsed.error().detail,
        });
    }
    return std::move(*parsed);
}

std::expected<RequirementParseResult, CliError>
load_requirements(const ProjectConfig& config, const CliOptions& options) {
    std::vector<std::string> paths = config.requirement_files;
    paths.reserve(paths.size() + options.requirement_files.size());
    const std::string explicit_base =
        std::filesystem::current_path().lexically_normal().generic_string();
    for (const auto& path : options.requirement_files) {
        const auto expanded = expand_path_pattern(path, explicit_base);
        if (!expanded) {
            return std::unexpected(CliError{.message = expanded.error().detail});
        }
        paths.insert(paths.end(), expanded->begin(), expanded->end());
    }

    std::vector<std::string> contents;
    contents.reserve(paths.size());
    for (const auto& path : paths) {
        auto content = read_text_file(path);
        if (!content) {
            return std::unexpected(content.error());
        }
        contents.push_back(std::move(*content));
    }

    std::vector<RequirementDocument> documents;
    documents.reserve(paths.size());
    for (std::size_t index = 0; index < paths.size(); ++index) {
        documents.push_back(RequirementDocument{.path = paths[index], .content = contents[index]});
    }
    return parse_requirements(documents);
}

using FragmentLoader = std::expected<ImportFragment, CliError> (*)(const std::string&,
                                                                     const std::string&);

std::expected<void, CliError> append_path_fragments(std::vector<ImportFragment>& fragments,
                                                     const std::vector<std::string>& paths,
                                                     const std::string& base_directory,
                                                     FragmentLoader loader) {
    for (const auto& path : paths) {
        auto fragment = loader(path, base_directory);
        if (!fragment) {
            return std::unexpected(fragment.error());
        }
        fragments.push_back(std::move(*fragment));
    }
    return {};
}

std::vector<std::string> normalized_paths(const std::vector<std::string>& paths) {
    std::vector<std::string> result;
    result.reserve(paths.size());
    for (const auto& path : paths) {
        result.push_back(normalize_explicit_path(path));
    }
    return result;
}

std::expected<std::vector<std::string>, CliError>
expand_explicit_paths(const std::vector<std::string>& paths,
                      const std::string& base_directory) {
    std::vector<std::string> result;
    for (const auto& path : paths) {
        const auto expanded = expand_path_pattern(path, base_directory);
        if (!expanded) {
            return std::unexpected(CliError{.message = expanded.error().detail});
        }
        result.insert(result.end(), expanded->begin(), expanded->end());
    }
    return result;
}

std::expected<void, CliError> append_config_artifacts(std::vector<ImportFragment>& fragments,
                                                       const std::vector<ArtifactConfig>& artifacts) {
    for (const auto& artifact : artifacts) {
        auto fragment = load_artifact(artifact.path, artifact.base_directory, artifact.importer);
        if (!fragment) {
            return std::unexpected(fragment.error());
        }
        fragments.push_back(std::move(*fragment));
    }
    return {};
}

std::expected<void, CliError> append_explicit_artifacts(
    std::vector<ImportFragment>& fragments,
    const std::vector<std::string>& paths,
    const std::string& base_directory) {
    for (const auto& path : paths) {
        auto fragment = load_artifact(normalize_explicit_path(path), base_directory, {});
        if (!fragment) {
            return std::unexpected(fragment.error());
        }
        fragments.push_back(std::move(*fragment));
    }
    return {};
}

std::expected<std::vector<ImportFragment>, CliError>
load_fragments(const ProjectConfig& config, const CliOptions& options) {
    std::vector<ImportFragment> fragments;
    fragments.reserve(config.source_files.size() + options.source_files.size() +
                      config.build_files.size() + options.build_files.size() +
                      config.artifacts.size() + options.artifact_files.size());

    const std::string explicit_base =
        std::filesystem::current_path().lexically_normal().generic_string();
    if (auto status = append_path_fragments(fragments, config.source_files, config.root, load_source);
        !status) {
        return std::unexpected(status.error());
    }
    if (auto status = append_path_fragments(fragments, config.build_files, config.root, load_build);
        !status) {
        return std::unexpected(status.error());
    }
    const auto explicit_sources = expand_explicit_paths(options.source_files, explicit_base);
    if (!explicit_sources) {
        return std::unexpected(explicit_sources.error());
    }
    if (auto status = append_path_fragments(fragments, *explicit_sources, explicit_base,
                                            load_source); !status) {
        return std::unexpected(status.error());
    }
    if (auto status = append_path_fragments(fragments, normalized_paths(options.build_files),
                                            explicit_base, load_build); !status) {
        return std::unexpected(status.error());
    }
    if (auto status = append_config_artifacts(fragments, config.artifacts); !status) {
        return std::unexpected(status.error());
    }
    if (auto status = append_explicit_artifacts(fragments, options.artifact_files, explicit_base);
        !status) {
        return std::unexpected(status.error());
    }
    return fragments;
}

int render_validation_result(const CliOptions& options,
                             const TraceResult& trace,
                             const ValidationResult& validation) {
    if (options.output_format == OutputFormat::json) {
        const auto report = render_json_report(trace, validation);
        if (!report) {
            std::cerr << report.error().detail << '\n';
            return 2;
        }
        std::cout << *report << '\n';
    } else {
        for (const auto& diagnostic : validation.diagnostics) {
            print_diagnostic(diagnostic);
        }
        std::cout << render_text_report(trace, validation);
    }
    return validation.failed ? 1 : 0;
}

std::expected<void, CliError> add_file_input_option(mcucli::Command* command,
                                                    std::string_view names,
                                                    std::string_view description,
                                                    std::vector<std::string>& values) {
    auto option = command->add_option(names, description, values);
    if (!option) {
        return std::unexpected(CliError{.message = cli_error_text(option.error())});
    }
    (*option)->metavar("FILE").repeatable();
    return {};
}

std::expected<void, CliError> configure_inputs(mcucli::Command* command,
                                                CliOptions& options,
                                                std::string& format) {
    if (auto status = add_file_input_option(command, "-r, --requirement",
                                            "Additional requirement Markdown file",
                                            options.requirement_files); !status) {
        return std::unexpected(status.error());
    }
    if (auto status = add_file_input_option(command, "-s, --source",
                                            "Additional annotated source or header file",
                                            options.source_files); !status) {
        return std::unexpected(status.error());
    }
    if (auto status = add_file_input_option(command, "-b, --build",
                                            "Additional annotated CMake build-definition file",
                                            options.build_files); !status) {
        return std::unexpected(status.error());
    }
    if (auto status = add_file_input_option(command, "-a, --artifact",
                                            "Additional producer artifact file (auto-detected)",
                                            options.artifact_files); !status) {
        return std::unexpected(status.error());
    }
    auto output = command->add_option("-f, --format", "Report format: text or json", format);
    if (!output) {
        return std::unexpected(CliError{.message = cli_error_text(output.error())});
    }
    (*output)->metavar("FORMAT");
    return {};
}

struct CliCommands final {
    mcucli::Command* validate = nullptr;
    mcucli::Command* show = nullptr;
};

std::expected<CliCommands, CliError> configure_commands(mcucli::Application& app,
                                                         CliOptions& options,
                                                         std::string& format) {
    auto validate = app.add_command("validate", "Validate the configured traceability graph");
    if (!validate) {
        return std::unexpected(CliError{.message = cli_error_text(validate.error())});
    }
    auto show = app.add_command("show", "Show trace evidence for one requirement");
    if (!show) {
        return std::unexpected(CliError{.message = cli_error_text(show.error())});
    }
    auto config = app.add_option("-c, --config", "TOML configuration file", options.config_path);
    if (!config) {
        return std::unexpected(CliError{.message = cli_error_text(config.error())});
    }
    (*config)->metavar("FILE");
    if (auto status = configure_inputs(*validate, options, format); !status) {
        return std::unexpected(status.error());
    }
    if (auto status = configure_inputs(*show, options, format); !status) {
        return std::unexpected(status.error());
    }
    if (auto requirement = (*show)->add_positional("REQ-NNNN", "Requirement identifier",
                                                    options.requirement_id); !requirement) {
        return std::unexpected(CliError{.message = cli_error_text(requirement.error())});
    }
    return CliCommands{.validate = *validate, .show = *show};
}

int render_requirement_result(const CliOptions& options, const TraceResult& trace) {
    const auto report = build_requirement_trace_report(trace, options.requirement_id);
    if (!report) {
        std::cerr << "unknown requirement: " << options.requirement_id << '\n';
        return 2;
    }
    if (options.output_format == OutputFormat::json) {
        const auto json = render_requirement_json_report(*report);
        if (!json) {
            std::cerr << json.error().detail << '\n';
            return 2;
        }
        std::cout << *json << '\n';
        return 0;
    }
    std::cout << render_requirement_text_report(*report);
    return 0;
}

}  // namespace

// @req REQ-0053 REQ-0064 REQ-0065 REQ-0066 REQ-0067 REQ-0068 REQ-0093
std::expected<CliOptions, CliError> parse_cli(int argc, const char* const* argv) {
    CliOptions options;
    std::string format = "text";
    mcucli::Application app("mcutrace", "Traceability aggregation and validation");
    app.set_version(kVersion);

    auto commands = configure_commands(app, options, format);
    if (!commands) {
        return std::unexpected(commands.error());
    }
    auto parsed = app.parse(argc, argv);
    if (!parsed) {
        return std::unexpected(CliError{.message = cli_error_text(parsed.error())});
    }

    if (parsed->kind() == mcucli::ParseEventKind::version) {
        options.action = CliAction::version;
        return options;
    }
    if (parsed->kind() == mcucli::ParseEventKind::help) {
        options.action = CliAction::help;
        auto help = app.help(parsed->command());
        if (!help) {
            return std::unexpected(CliError{.message = cli_error_text(help.error())});
        }
        options.help_text = std::move(*help);
        return options;
    }
    if (auto status = parse_output_format(format, options.output_format); !status) {
        return std::unexpected(status.error());
    }
    if (parsed->command() == commands->validate->id()) {
        options.action = CliAction::validate;
        return options;
    }
    if (parsed->command() == commands->show->id()) {
        options.action = CliAction::show;
        return options;
    }
    return std::unexpected(CliError{.message = "a command is required; use 'mcutrace validate' or 'mcutrace show REQ-NNNN'"});
}

// @req REQ-0001 REQ-0002 REQ-0067 REQ-0093 REQ-0125
int run_cli(const CliOptions& options) {
    if (options.action == CliAction::version) {
        std::cout << "mcutrace " << kVersion << '\n';
        return 0;
    }
    if (options.action == CliAction::help) {
        std::cout << options.help_text;
        return 0;
    }

    auto config = load_project_config(options);
    if (!config) {
        std::cerr << config.error().message << '\n';
        return 2;
    }
    auto requirements = load_requirements(*config, options);
    if (!requirements) {
        std::cerr << requirements.error().message << '\n';
        return 2;
    }
    for (const auto& diagnostic : requirements->diagnostics) {
        print_diagnostic(diagnostic);
    }

    auto fragments = load_fragments(*config, options);
    if (!fragments) {
        std::cerr << fragments.error().message << '\n';
        return 2;
    }

    const TraceResult trace = assemble_trace(requirements->requirements, *fragments);
    const ValidationResult validation = validate_trace(trace, config->validation);
    if (options.action == CliAction::show) {
        return render_requirement_result(options, trace);
    }
    return render_validation_result(options, trace, validation);
}

}  // namespace mcutrace
