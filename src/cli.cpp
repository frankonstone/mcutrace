#include <mcutrace/cli.hpp>

#include <mcutrace/assembly.hpp>
#include <mcutrace/config.hpp>
#include <mcutrace/output.hpp>
#include <mcutrace/producer_importers.hpp>
#include <mcutrace/requirements.hpp>
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
    if (!error.subject.empty()) result += " for " + std::string(error.subject);
    if (!error.detail.empty()) result += ": " + std::string(error.detail);
    if (!error.token.empty()) result += " (" + std::string(error.token) + ")";
    return result;
}

std::expected<std::string, CliError> read_text_file(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return std::unexpected(CliError{.message = "cannot open file: " + path});
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::string normalize_explicit_path(std::string_view value) {
    std::filesystem::path path(value);
    if (path.is_relative()) path = std::filesystem::current_path() / path;
    return path.lexically_normal().generic_string();
}

void print_diagnostic(const Diagnostic& diagnostic) {
    if (diagnostic.source) {
        std::cerr << diagnostic.source->path;
        if (diagnostic.source->line != 0) std::cerr << ':' << diagnostic.source->line;
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
    if (!content) return std::unexpected(content.error());
    auto fragment = import_producer_artifact(ArtifactInput{
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

}  // namespace

std::expected<CliOptions, CliError> parse_cli(int argc, const char* const* argv) {
    CliOptions options;
    std::string format = "text";
    mcucli::Application app("mcutrace", "Traceability aggregation and validation");
    app.set_version(kVersion);

    auto config = app.add_option("-c, --config", "TOML configuration file", options.config_path);
    if (!config) return std::unexpected(CliError{.message = cli_error_text(config.error())});
    (*config)->metavar("FILE");

    auto validate = app.add_command("validate", "Validate the configured traceability graph");
    if (!validate) return std::unexpected(CliError{.message = cli_error_text(validate.error())});

    auto requirements = (*validate)->add_option(
        "-r, --requirement", "Additional requirement Markdown file", options.requirement_files);
    if (!requirements) return std::unexpected(CliError{.message = cli_error_text(requirements.error())});
    (*requirements)->metavar("FILE").repeatable();

    auto artifacts = (*validate)->add_option(
        "-a, --artifact", "Additional producer artifact file (auto-detected)", options.artifact_files);
    if (!artifacts) return std::unexpected(CliError{.message = cli_error_text(artifacts.error())});
    (*artifacts)->metavar("FILE").repeatable();

    auto output = (*validate)->add_option(
        "-f, --format", "Report format: text or json", format);
    if (!output) return std::unexpected(CliError{.message = cli_error_text(output.error())});
    (*output)->metavar("FORMAT");

    auto parsed = app.parse(argc, argv);
    if (!parsed) return std::unexpected(CliError{.message = cli_error_text(parsed.error())});

    if (parsed->kind() == mcucli::ParseEventKind::version) {
        options.action = CliAction::version;
        return options;
    }
    if (parsed->kind() == mcucli::ParseEventKind::help) {
        options.action = CliAction::help;
        auto help = app.help(parsed->command());
        if (!help) return std::unexpected(CliError{.message = cli_error_text(help.error())});
        options.help_text = std::move(*help);
        return options;
    }

    if (parsed->command() != (*validate)->id()) {
        return std::unexpected(CliError{.message = "a command is required; use 'mcutrace validate'"});
    }
    if (auto status = parse_output_format(format, options.output_format); !status) {
        return std::unexpected(status.error());
    }
    options.action = CliAction::validate;
    return options;
}

int run_cli(const CliOptions& options) {
    if (options.action == CliAction::version) {
        std::cout << "mcutrace " << kVersion << '\n';
        return 0;
    }
    if (options.action == CliAction::help) {
        std::cout << options.help_text;
        return 0;
    }

    ProjectConfig config;
    if (!options.config_path.empty()) {
        auto content = read_text_file(options.config_path);
        if (!content) {
            std::cerr << content.error().message << '\n';
            return 2;
        }
        auto parsed = parse_project_config(*content, options.config_path);
        if (!parsed) {
            std::cerr << "configuration error: " << parsed.error().detail << '\n';
            return 2;
        }
        config = std::move(*parsed);
    } else {
        config.root = std::filesystem::current_path().lexically_normal().generic_string();
    }

    std::vector<std::string> paths = config.requirement_files;
    paths.reserve(paths.size() + options.requirement_files.size());
    for (const auto& path : options.requirement_files) paths.push_back(normalize_explicit_path(path));

    std::vector<std::string> contents;
    contents.reserve(paths.size());
    for (const auto& path : paths) {
        auto content = read_text_file(path);
        if (!content) {
            std::cerr << content.error().message << '\n';
            return 2;
        }
        contents.push_back(std::move(*content));
    }

    std::vector<RequirementDocument> documents;
    documents.reserve(paths.size());
    for (std::size_t index = 0; index < paths.size(); ++index) {
        documents.push_back(RequirementDocument{.path = paths[index], .content = contents[index]});
    }

    const auto parsed_requirements = parse_requirements(documents);
    for (const auto& diagnostic : parsed_requirements.diagnostics) print_diagnostic(diagnostic);

    std::vector<ImportFragment> fragments;
    fragments.reserve(config.artifacts.size() + options.artifact_files.size());
    for (const auto& artifact : config.artifacts) {
        auto fragment = load_artifact(artifact.path, artifact.base_directory, artifact.importer);
        if (!fragment) {
            std::cerr << fragment.error().message << '\n';
            return 2;
        }
        fragments.push_back(std::move(*fragment));
    }
    const std::string explicit_base = std::filesystem::current_path().lexically_normal().generic_string();
    for (const auto& artifact : options.artifact_files) {
        const std::string normalized = normalize_explicit_path(artifact);
        auto fragment = load_artifact(normalized, explicit_base, {});
        if (!fragment) {
            std::cerr << fragment.error().message << '\n';
            return 2;
        }
        fragments.push_back(std::move(*fragment));
    }

    const TraceResult trace = assemble_trace(parsed_requirements.requirements, fragments);
    const ValidationResult validation = validate_trace(trace, config.validation);

    if (options.output_format == OutputFormat::json) {
        const auto report = render_json_report(trace, validation);
        if (!report) {
            std::cerr << report.error().detail << '\n';
            return 2;
        }
        std::cout << *report << '\n';
    } else {
        for (const auto& diagnostic : validation.diagnostics) print_diagnostic(diagnostic);
        std::cout << render_text_report(trace, validation);
    }

    return validation.failed ? 1 : 0;
}

}  // namespace mcutrace
