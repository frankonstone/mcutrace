#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace mcutrace {

enum class NodeKind : std::uint8_t {
    requirement,
    source,
    test,
    coverage,
    finding,
    artifact,
};

struct Node final {
    std::string id;
    NodeKind kind = NodeKind::artifact;
};

[[nodiscard]] std::string_view node_kind_name(NodeKind kind) noexcept;

}  // namespace mcutrace
