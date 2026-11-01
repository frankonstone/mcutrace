#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <mcutrace/importer.hpp>

namespace mcutrace::detail {

enum class SourceMarkerKind : std::uint8_t {
    file,
    declaration,
};

struct AnnotationTarget final {
    std::uint32_t line = 0;
    std::string scope;
    std::string symbol;
    std::string identity;
};

struct ScannedMarker final {
    SourceMarkerKind kind = SourceMarkerKind::declaration;
    std::string text;
    std::uint32_t line = 0;
    std::size_t begin = 0;
    std::size_t end = 0;
    std::optional<AnnotationTarget> target;
};

struct SourceScanResult final {
    std::vector<ScannedMarker> markers;
    std::vector<Diagnostic> diagnostics;
};

[[nodiscard]] std::optional<ScannedMarker>
scan_line_marker(std::string_view line,
                 std::uint32_t line_number,
                 std::size_t line_offset,
                 std::string_view marker);

void append_comment_markers(SourceScanResult& result,
                            std::string_view comment,
                            std::uint32_t first_line,
                            std::size_t comment_offset,
                            std::size_t comment_end);

[[nodiscard]] SourceScanResult scan_cpp_source(const ArtifactInput& input);
[[nodiscard]] SourceScanResult scan_javascript_source(const ArtifactInput& input);
[[nodiscard]] SourceScanResult scan_tcl_source(const ArtifactInput& input);
[[nodiscard]] SourceScanResult scan_generic_source(const ArtifactInput& input);

}  // namespace mcutrace::detail
