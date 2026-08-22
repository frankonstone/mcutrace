#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace mcutrace {

struct PathPatternError final {
    std::string detail;
};

[[nodiscard]] std::expected<std::vector<std::string>, PathPatternError>
expand_path_pattern(std::string_view pattern, std::string_view base_directory);

}  // namespace mcutrace
