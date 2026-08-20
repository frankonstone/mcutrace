#include <mcutrace/lsp.hpp>

#include <iostream>

int main() {
    return mcutrace::run_language_server(std::cin, std::cout);
}
