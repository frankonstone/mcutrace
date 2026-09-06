#pragma once

#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace mcutrace {

inline constexpr std::string_view kMcpProtocolVersion = "2025-11-25";
inline constexpr unsigned kMcpSchemaVersion = 1;

class McpServer final {
  public:
    explicit McpServer(std::string config_path);
    ~McpServer();

    McpServer(const McpServer&) = delete;
    McpServer& operator=(const McpServer&) = delete;

    [[nodiscard]] std::vector<std::string> handle(std::string_view message);

  private:
    struct State;
    std::unique_ptr<State> state_;
};

[[nodiscard]] int run_mcp_server(std::istream& input,
                                 std::ostream& output,
                                 std::ostream& errors,
                                 std::string config_path);

}  // namespace mcutrace
