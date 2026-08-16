#include <mcutrace/cli.hpp>

#include <iostream>

int main(int argc, char* argv[]) {
    const auto options = mcutrace::parse_cli(argc, argv);
    if (!options) {
        std::cerr << options.error().message << '\n';
        return 2;
    }
    return mcutrace::run_cli(*options);
}
