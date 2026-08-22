#include <mcutrace/path_patterns.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace mcutrace {
namespace {

std::string canonical_path_string(std::filesystem::path path) {
    path = path.lexically_normal();
    while (path.has_filename() == false && path.has_parent_path() && path != path.root_path()) {
        path = path.parent_path();
    }
    return path.generic_string();
}

std::string normalize_path(std::string_view value, std::string_view base_directory) {
    std::filesystem::path path(value);
    if (path.is_relative()) {
        path = std::filesystem::path(base_directory) / path;
    }
    return canonical_path_string(std::move(path));
}

bool contains_wildcard(std::string_view value) noexcept {
    return value.find_first_of("*?") != std::string_view::npos;
}

bool match_component(std::string_view pattern, std::string_view value) {
    std::vector<bool> previous(value.size() + 1U, false);
    std::vector<bool> current(value.size() + 1U, false);
    previous[0] = true;

    for (const char pattern_character : pattern) {
        std::fill(current.begin(), current.end(), false);
        if (pattern_character == '*') {
            current[0] = previous[0];
            for (std::size_t value_index = 1U; value_index <= value.size(); ++value_index) {
                current[value_index] = current[value_index - 1U] || previous[value_index];
            }
        } else {
            for (std::size_t value_index = 1U; value_index <= value.size(); ++value_index) {
                current[value_index] = previous[value_index - 1U] &&
                    (pattern_character == '?' || pattern_character == value[value_index - 1U]);
            }
        }
        previous.swap(current);
    }
    return previous[value.size()];
}

std::vector<std::string> path_components(const std::filesystem::path& path) {
    std::vector<std::string> result;
    for (const auto& component : path) {
        result.push_back(component.generic_string());
    }
    return result;
}

bool match_components(const std::vector<std::string>& pattern,
                      const std::vector<std::string>& value,
                      std::size_t pattern_index,
                      std::size_t value_index,
                      std::vector<std::vector<std::int8_t>>& memo) {
    auto& cached = memo[pattern_index][value_index];
    if (cached != -1) {
        return cached == 1;
    }

    bool matched = false;
    if (pattern_index == pattern.size()) {
        matched = value_index == value.size();
    } else if (pattern[pattern_index] == "**") {
        matched = match_components(pattern, value, pattern_index + 1U, value_index, memo) ||
            (value_index < value.size() &&
             match_components(pattern, value, pattern_index, value_index + 1U, memo));
    } else {
        matched = value_index < value.size() &&
            match_component(pattern[pattern_index], value[value_index]) &&
            match_components(pattern, value, pattern_index + 1U, value_index + 1U, memo);
    }

    cached = matched ? 1 : 0;
    return matched;
}

bool matches(const std::filesystem::path& pattern, const std::filesystem::path& value) {
    const auto pattern_components = path_components(pattern);
    const auto value_components = path_components(value);
    std::vector<std::vector<std::int8_t>> memo(
        pattern_components.size() + 1U,
        std::vector<std::int8_t>(value_components.size() + 1U, -1));
    return match_components(pattern_components, value_components, 0U, 0U, memo);
}

std::filesystem::path static_search_root(const std::filesystem::path& pattern) {
    std::filesystem::path result;
    for (const auto& component : pattern) {
        const auto text = component.generic_string();
        if (contains_wildcard(text)) {
            break;
        }
        result /= component;
    }
    if (result.empty()) {
        return std::filesystem::path(".");
    }
    return result;
}

PathPatternError no_matches(std::string_view pattern) {
    return PathPatternError{
        .detail = "path pattern matched no regular files: " + std::string(pattern),
    };
}

}  // namespace

// @req REQ-0125
std::expected<std::vector<std::string>, PathPatternError>
expand_path_pattern(std::string_view pattern, std::string_view base_directory) {
    const std::string normalized = normalize_path(pattern, base_directory);
    const std::filesystem::path normalized_path(normalized);
    if (!contains_wildcard(normalized)) {
        return std::vector<std::string>{normalized};
    }

    const auto search_root = static_search_root(normalized_path);
    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator(
        search_root, std::filesystem::directory_options::skip_permission_denied, error);
    if (error) {
        if (error == std::errc::no_such_file_or_directory) {
            return std::unexpected(no_matches(pattern));
        }
        return std::unexpected(PathPatternError{
            .detail = "cannot inspect path pattern root '" + search_root.generic_string() +
                "': " + error.message(),
        });
    }

    std::vector<std::string> matches_found;
    const std::filesystem::recursive_directory_iterator end;
    while (iterator != end) {
        const auto entry_path = iterator->path().lexically_normal();
        std::error_code status_error;
        const bool regular_file = iterator->is_regular_file(status_error);
        if (status_error) {
            return std::unexpected(PathPatternError{
                .detail = "cannot inspect path pattern entry '" + entry_path.generic_string() +
                    "': " + status_error.message(),
            });
        }
        if (regular_file && matches(normalized_path, entry_path)) {
            matches_found.push_back(entry_path.generic_string());
        }

        iterator.increment(error);
        if (error) {
            return std::unexpected(PathPatternError{
                .detail = "cannot traverse path pattern root '" + search_root.generic_string() +
                    "': " + error.message(),
            });
        }
    }

    if (matches_found.empty()) {
        return std::unexpected(no_matches(pattern));
    }
    std::sort(matches_found.begin(), matches_found.end());
    return matches_found;
}

}  // namespace mcutrace
