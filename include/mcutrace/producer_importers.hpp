#pragma once

#include <expected>
#include <string_view>
#include <vector>

#include <mcutrace/importer.hpp>

namespace mcutrace {

[[nodiscard]] std::vector<ImporterInfo> producer_importer_info();

[[nodiscard]] std::expected<ImportFragment, ImportError>
import_producer_artifact(const ArtifactInput& input,
                         std::string_view requested_importer = {});

}  // namespace mcutrace
