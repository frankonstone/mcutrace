#pragma once

#include <expected>
#include <string_view>

#include <mcutrace/importer.hpp>

namespace mcutrace {

[[nodiscard]] std::expected<ImportFragment, ImportError>
import_trace_artifact(const ArtifactInput& input,
                      std::string_view requested_importer = {});

}  // namespace mcutrace
