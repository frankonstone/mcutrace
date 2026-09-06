#include <mcutrace/project.hpp>

#include <mcutrace/build_annotations.hpp>
#include <mcutrace/plans.hpp>
#include <mcutrace/requirements.hpp>
#include <mcutrace/source_annotations.hpp>
#include <mcutrace/trace_import.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace mcutrace {
namespace {

struct RevisionBuilder final {
    std::uint64_t value = 14695981039346656037ULL;

    void add(std::string_view text) noexcept {
        for (const unsigned char byte : text) {
            value ^= byte;
            value *= 1099511628211ULL;
        }
        value ^= 0xffU;
        value *= 1099511628211ULL;
    }

    [[nodiscard]] std::string finish() const {
        std::ostringstream stream;
        stream << std::hex << std::setfill('0') << std::setw(16) << value;
        return stream.str();
    }
};

std::expected<std::string, ProjectLoadError> read_text_file(std::string_view path) {
    std::ifstream stream(std::string(path), std::ios::binary);
    if (!stream) {
        return std::unexpected(ProjectLoadError{
            .code = "project.cannot_read",
            .detail = "cannot open configured input: " + std::string(path),
        });
    }
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::string normalize_path(std::string_view value, std::string_view base = {}) {
    std::filesystem::path path(value);
    if (path.is_relative()) {
        const std::filesystem::path base_path = base.empty()
            ? std::filesystem::current_path()
            : std::filesystem::path(base);
        path = base_path / path;
    }
    return path.lexically_normal().generic_string();
}

void remember_input(ProjectSnapshot& snapshot,
                    RevisionBuilder& revision,
                    std::string_view kind,
                    std::string_view path,
                    std::string_view content) {
    snapshot.inputs.push_back(ProjectInput{.kind = std::string(kind), .path = std::string(path)});
    revision.add(kind);
    revision.add(path);
    revision.add(content);
}

std::expected<RequirementParseResult, ProjectLoadError>
load_requirements(const ProjectConfig& config,
                  ProjectSnapshot& snapshot,
                  RevisionBuilder& revision) {
    std::vector<std::string> contents;
    contents.reserve(config.requirement_files.size());
    for (const auto& path : config.requirement_files) {
        auto content = read_text_file(path);
        if (!content) {
            return std::unexpected(content.error());
        }
        remember_input(snapshot, revision, "requirement", path, *content);
        contents.push_back(std::move(*content));
    }
    std::vector<RequirementDocument> documents;
    documents.reserve(config.requirement_files.size());
    for (std::size_t index = 0; index < config.requirement_files.size(); ++index) {
        documents.push_back(RequirementDocument{
            .path = config.requirement_files[index],
            .content = contents[index],
        });
    }
    return parse_requirements(documents);
}

std::expected<PlanParseResult, ProjectLoadError>
load_plans(const ProjectConfig& config,
           ProjectSnapshot& snapshot,
           RevisionBuilder& revision) {
    std::vector<std::string> contents;
    contents.reserve(config.plan_files.size());
    for (const auto& path : config.plan_files) {
        auto content = read_text_file(path);
        if (!content) {
            return std::unexpected(content.error());
        }
        remember_input(snapshot, revision, "plan", path, *content);
        contents.push_back(std::move(*content));
    }
    std::vector<PlanDocument> documents;
    documents.reserve(config.plan_files.size());
    for (std::size_t index = 0; index < config.plan_files.size(); ++index) {
        documents.push_back(PlanDocument{
            .path = config.plan_files[index],
            .content = contents[index],
        });
    }
    return parse_plans(documents);
}

std::expected<ImportFragment, ProjectLoadError>
load_source(std::string_view path,
            std::string_view base,
            ProjectSnapshot& snapshot,
            RevisionBuilder& revision) {
    auto content = read_text_file(path);
    if (!content) {
        return std::unexpected(content.error());
    }
    remember_input(snapshot, revision, "source", path, *content);
    auto fragment = import_source_annotations(ArtifactInput{
        .path = std::string(path),
        .base_directory = std::string(base),
        .content = std::move(*content),
    });
    if (!fragment) {
        return std::unexpected(ProjectLoadError{
            .code = "project.source_import",
            .detail = fragment.error().detail,
        });
    }
    return std::move(*fragment);
}

std::expected<ImportFragment, ProjectLoadError>
load_build(std::string_view path,
           std::string_view base,
           ProjectSnapshot& snapshot,
           RevisionBuilder& revision) {
    auto content = read_text_file(path);
    if (!content) {
        return std::unexpected(content.error());
    }
    remember_input(snapshot, revision, "build", path, *content);
    auto fragment = import_build_annotations(ArtifactInput{
        .path = std::string(path),
        .base_directory = std::string(base),
        .content = std::move(*content),
    });
    if (!fragment) {
        return std::unexpected(ProjectLoadError{
            .code = "project.build_import",
            .detail = fragment.error().detail,
        });
    }
    return std::move(*fragment);
}

std::expected<ImportFragment, ProjectLoadError>
load_artifact(const ArtifactConfig& artifact,
              ProjectSnapshot& snapshot,
              RevisionBuilder& revision) {
    auto content = read_text_file(artifact.path);
    if (!content) {
        return std::unexpected(content.error());
    }
    remember_input(snapshot, revision, "artifact", artifact.path, *content);
    auto fragment = import_trace_artifact(ArtifactInput{
        .path = artifact.path,
        .base_directory = artifact.base_directory,
        .content = std::move(*content),
    }, artifact.importer);
    if (!fragment) {
        return std::unexpected(ProjectLoadError{
            .code = "project.artifact_import",
            .detail = fragment.error().detail,
        });
    }
    return std::move(*fragment);
}

}  // namespace

// @req REQ-0142 REQ-0158
std::string discover_project_config(const std::string_view explicit_path,
                                    const std::string_view working_directory) {
    const std::string base = working_directory.empty()
        ? std::filesystem::current_path().lexically_normal().generic_string()
        : normalize_path(working_directory);
    return explicit_path.empty()
        ? normalize_path("mcutrace.toml", base)
        : normalize_path(explicit_path, base);
}

// @req REQ-0139 REQ-0142 REQ-0143 REQ-0157 REQ-0158 REQ-0159
std::expected<ProjectSnapshot, ProjectLoadError>
load_project_snapshot(const std::string_view config_path) {
    ProjectSnapshot snapshot;
    snapshot.config_path = normalize_path(config_path);
    RevisionBuilder revision;

    auto config_content = read_text_file(snapshot.config_path);
    if (!config_content) {
        return std::unexpected(config_content.error());
    }
    revision.add("config");
    revision.add(snapshot.config_path);
    revision.add(*config_content);

    auto config = parse_project_config(*config_content, snapshot.config_path);
    if (!config) {
        return std::unexpected(ProjectLoadError{
            .code = "project.configuration",
            .detail = config.error().detail,
        });
    }
    snapshot.config = std::move(*config);

    auto requirements = load_requirements(snapshot.config, snapshot, revision);
    if (!requirements) {
        return std::unexpected(requirements.error());
    }
    auto plans = load_plans(snapshot.config, snapshot, revision);
    if (!plans) {
        return std::unexpected(plans.error());
    }

    std::vector<ImportFragment> fragments;
    fragments.reserve(snapshot.config.source_files.size() +
                      snapshot.config.build_files.size() + snapshot.config.artifacts.size());
    for (const auto& path : snapshot.config.source_files) {
        auto fragment = load_source(path, snapshot.config.root, snapshot, revision);
        if (!fragment) {
            return std::unexpected(fragment.error());
        }
        fragments.push_back(std::move(*fragment));
    }
    for (const auto& path : snapshot.config.build_files) {
        auto fragment = load_build(path, snapshot.config.root, snapshot, revision);
        if (!fragment) {
            return std::unexpected(fragment.error());
        }
        fragments.push_back(std::move(*fragment));
    }
    for (const auto& artifact : snapshot.config.artifacts) {
        auto fragment = load_artifact(artifact, snapshot, revision);
        if (!fragment) {
            return std::unexpected(fragment.error());
        }
        fragments.push_back(std::move(*fragment));
    }

    std::vector<Diagnostic> diagnostics = requirements->diagnostics;
    diagnostics.insert(diagnostics.end(), plans->diagnostics.begin(), plans->diagnostics.end());
    snapshot.trace = assemble_trace(requirements->requirements, fragments, plans->plans,
                                    diagnostics);
    snapshot.validation = validate_trace(snapshot.trace, snapshot.config.validation);
    snapshot.revision = revision.finish();
    return snapshot;
}

}  // namespace mcutrace
