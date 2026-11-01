#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include <mcutrace/importer.hpp>

namespace mcutrace {

enum class SourceLanguage : std::uint8_t {
    unsupported,
    cpp,
    javascript,
    tcl,
};

[[nodiscard]] SourceLanguage source_language_for_path(std::string_view path) noexcept;

[[nodiscard]] std::expected<ImportFragment, ImportError>
import_source_annotations(const ArtifactInput& input);

// File-scoped test declarations; execution evidence is imported independently.
[[nodiscard]] std::expected<ImportFragment, ImportError>
import_test_annotations(const ArtifactInput& input);

[[nodiscard]] std::string source_digest(std::string_view content);

}  // namespace mcutrace
