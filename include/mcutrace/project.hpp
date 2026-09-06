#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include <mcutrace/assembly.hpp>
#include <mcutrace/config.hpp>
#include <mcutrace/validation.hpp>

namespace mcutrace {

struct ProjectInput final {
    std::string kind;
    std::string path;

    friend bool operator==(const ProjectInput&, const ProjectInput&) = default;
};

struct ProjectSnapshot final {
    std::string config_path;
    ProjectConfig config;
    TraceResult trace;
    ValidationResult validation;
    std::vector<ProjectInput> inputs;
    std::string revision;
};

struct ProjectLoadError final {
    std::string code;
    std::string detail;
};

[[nodiscard]] std::string discover_project_config(
    std::string_view explicit_path = {},
    std::string_view working_directory = {});

[[nodiscard]] std::expected<ProjectSnapshot, ProjectLoadError>
load_project_snapshot(std::string_view config_path);

}  // namespace mcutrace
