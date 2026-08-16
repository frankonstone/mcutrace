#pragma once

#include <span>
#include <vector>

#include <mcutrace/importer.hpp>
#include <mcutrace/model.hpp>
#include <mcutrace/requirements.hpp>

namespace mcutrace {

struct TraceResult final {
    Graph graph;
    std::vector<GenericArtifact> artifacts;
    std::vector<Diagnostic> diagnostics;
};

[[nodiscard]] TraceResult assemble_trace(
    std::span<const Requirement> requirements,
    std::span<const ImportFragment> fragments);

}  // namespace mcutrace
