// @req-file REQ-0089 REQ-0090 REQ-0091 REQ-0092
#pragma once

#include <expected>

#include <mcutrace/importer.hpp>

namespace mcutrace {

[[nodiscard]] std::expected<ImportFragment, ImportError>
import_source_annotations(const ArtifactInput& input);

}  // namespace mcutrace
