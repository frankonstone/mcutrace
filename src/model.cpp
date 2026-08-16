#include <mcutrace/model.hpp>

namespace mcutrace {

std::string_view node_kind_name(NodeKind kind) noexcept {
    switch (kind) {
    case NodeKind::requirement:
        return "requirement";
    case NodeKind::source:
        return "source";
    case NodeKind::test:
        return "test";
    case NodeKind::coverage:
        return "coverage";
    case NodeKind::finding:
        return "finding";
    case NodeKind::artifact:
        return "artifact";
    }
    return "artifact";
}

}  // namespace mcutrace
