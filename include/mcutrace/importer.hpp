#pragma once

#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <mcutrace/error.hpp>
#include <mcutrace/model.hpp>

namespace mcutrace {

struct ArtifactInput final {
    std::string path;
    std::string base_directory;
    std::string content;
};

struct InputFormat final {
    std::string producer;
    std::string schema;
    std::string version;

    friend bool operator==(const InputFormat&, const InputFormat&) = default;
};

struct ImporterInfo final {
    std::string name;
    std::string producer;
    std::vector<std::string> supported_versions;
};

enum class ImportErrorCode : std::uint8_t {
    invalid_artifact,
    unrecognized_format,
    unsupported_version,
};

[[nodiscard]] std::string_view import_error_code_name(ImportErrorCode code) noexcept;

struct ImportError final {
    ImportErrorCode code = ImportErrorCode::invalid_artifact;
    std::string detail;
    std::optional<SourceLocation> source;
};

struct GenericArtifact final {
    std::string id;
    std::string type;
    std::string media_type = "application/json";
    std::string payload;
    std::optional<SourceLocation> source;

    friend bool operator==(const GenericArtifact&, const GenericArtifact&) = default;
};

struct ImportFragment final {
    InputFormat format;
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<GenericArtifact> artifacts;
    std::vector<Diagnostic> diagnostics;
};

class Importer {
  public:
    virtual ~Importer() = default;

    [[nodiscard]] virtual ImporterInfo info() const = 0;
    [[nodiscard]] virtual std::expected<InputFormat, ImportError>
    identify(const ArtifactInput& input) const = 0;
    [[nodiscard]] virtual std::expected<ImportFragment, ImportError>
    import(const ArtifactInput& input) const = 0;
};

[[nodiscard]] bool supports_version(const ImporterInfo& info, std::string_view version) noexcept;

[[nodiscard]] std::expected<void, ImportError>
require_supported_version(const ImporterInfo& info,
                          const InputFormat& format,
                          std::optional<SourceLocation> source = std::nullopt);

[[nodiscard]] std::expected<std::string, ImportError>
normalize_artifact_path(std::string_view path, std::string_view base_directory = {});

[[nodiscard]] GenericArtifact preserve_json_artifact(
    std::string id,
    std::string type,
    std::string payload,
    std::optional<SourceLocation> source = std::nullopt);

}  // namespace mcutrace
