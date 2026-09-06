#include <mcutrace/mcp.hpp>
#include <mcutrace/project.hpp>

#include <iostream>
#include <string>

namespace {

constexpr std::string_view kVersion = "0.1.0";

}  // namespace

// @req REQ-0138 REQ-0140 REQ-0142 REQ-0161
int main(int argc, char* argv[]) {
    std::string config_path;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--config" && index + 1 < argc) {
            config_path = argv[++index];
            continue;
        }
        if (argument == "--version") {
            std::cout << "mcutrace-mcp " << kVersion << " (MCP "
                      << mcutrace::kMcpProtocolVersion << ", schema "
                      << mcutrace::kMcpSchemaVersion << ")\n";
            return 0;
        }
        if (argument == "--help") {
            std::cout << "Usage: mcutrace-mcp [--config FILE] [--version]\n";
            return 0;
        }
        std::cerr << "unknown argument: " << argument << '\n';
        return 2;
    }
    config_path = mcutrace::discover_project_config(config_path);
    return mcutrace::run_mcp_server(std::cin, std::cout, std::cerr,
                                    std::move(config_path));
}
