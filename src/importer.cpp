#include <mcutrace/importer.hpp>

#include <algorithm>
#include <filesystem>
#include <utility>

namespace mcutrace {

std::string_view import_error_code_name(ImportErrorCode code) noexcept {
    switch (code) {
    case ImportErrorCode::invalid_artifact:
        return "invalid_artifact";
    case ImportErrorCode::unrecognized_format:
        return "unrecognized_format";
    case ImportErrorCode::unsupported_version:
        return "unsupported_version";
    }
    return "invalid_artifact";
}

bool supports_version(const ImporterInfo& info, std::string_view version) noexcept {
    return std::find(info.supported_versions.begin(), info.supported_versions.end(), version) !=
           info.supported_versions.end();
}

// @req REQ-0035 REQ-0041 REQ-0042 REQ-0071
std::expected<void, ImportError>
require_supported_version(const ImporterInfo& info,
                          const InputFormat& format,
                          std::optional<SourceLocation> source) {
    if (format.producer != info.producer) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::unrecognized_format,
            .detail = "importer '" + info.name + "' does not handle producer '" +
                      format.producer + "'",
            .source = std::move(source),
        });
    }
    if (!supports_version(info, format.version)) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::unsupported_version,
            .detail = "unsupported " + format.producer + " input version: " + format.version,
            .source = std::move(source),
        });
    }
    return {};
}

// @req REQ-0043 REQ-0044 REQ-0071
std::expected<std::string, ImportError>
normalize_artifact_path(std::string_view path, std::string_view base_directory) {
    if (path.empty()) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::invalid_artifact,
            .detail = "artifact path must not be empty",
            .source = std::nullopt,
        });
    }

    std::filesystem::path normalized{std::string(path)};
    if (normalized.is_relative() && !base_directory.empty()) {
        normalized = std::filesystem::path(std::string(base_directory)) / normalized;
    }
    normalized = normalized.lexically_normal();
    return normalized.generic_string();
}

// @req REQ-0026 REQ-0032 REQ-0040
GenericArtifact preserve_json_artifact(std::string id,
                                       std::string type,
                                       std::string payload,
                                       std::optional<SourceLocation> source) {
    return GenericArtifact{
        .id = std::move(id),
        .type = std::move(type),
        .media_type = "application/json",
        .payload = std::move(payload),
        .source = std::move(source),
    };
}

}  // namespace mcutrace
