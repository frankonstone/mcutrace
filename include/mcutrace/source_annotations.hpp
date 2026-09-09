#pragma once

#include <cstdint>
#include <expected>
#include <string_view>

#include <mcutrace/importer.hpp>

namespace mcutrace {

enum class SourceLanguage : std::uint8_t {
    unsupported,
    cpp,
    javascript,
};

[[nodiscard]] SourceLanguage source_language_for_path(std::string_view path) noexcept;

[[nodiscard]] std::expected<ImportFragment, ImportError>
import_source_annotations(const ArtifactInput& input);

}  // namespace mcutrace
