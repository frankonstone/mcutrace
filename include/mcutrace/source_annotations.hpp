#pragma once

#include <expected>

#include <mcutrace/importer.hpp>

namespace mcutrace {

[[nodiscard]] std::expected<ImportFragment, ImportError>
import_source_annotations(const ArtifactInput& input);

}  // namespace mcutrace
