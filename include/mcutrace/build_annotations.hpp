#pragma once

#include <expected>

#include <mcutrace/importer.hpp>

namespace mcutrace {

// Imports file-scoped CMake build-evidence annotations of the form:
//   # @req REQ-0001 REQ-0002
[[nodiscard]] std::expected<ImportFragment, ImportError>
import_build_annotations(const ArtifactInput& input);

}  // namespace mcutrace
