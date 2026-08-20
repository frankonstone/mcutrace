#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mcutrace {

struct SourceLocation final {
    std::string path;
    std::uint32_t line = 0;
    std::uint32_t column = 0;
    std::uint32_t end_line = 0;
    std::uint32_t end_column = 0;

    [[nodiscard]] bool available() const noexcept { return !path.empty(); }

    friend bool operator==(const SourceLocation&, const SourceLocation&) = default;
};

enum class Severity : std::uint8_t {
    note,
    warning,
    error,
};

[[nodiscard]] std::string_view severity_name(Severity severity) noexcept;

enum class ErrorCode : std::uint8_t {
    invalid_node_id,
    duplicate_node,
    invalid_edge,
    duplicate_edge,
    invalid_requirement_id,
    requirement_id_space_exhausted,
};

[[nodiscard]] std::string_view error_code_name(ErrorCode code) noexcept;

struct Error final {
    ErrorCode code = ErrorCode::invalid_node_id;
    std::string detail;
    std::optional<SourceLocation> source;
};

struct Diagnostic final {
    std::string code;
    Severity severity = Severity::error;
    std::string message;
    std::optional<SourceLocation> source;

    friend bool operator==(const Diagnostic&, const Diagnostic&) = default;
};

}  // namespace mcutrace
