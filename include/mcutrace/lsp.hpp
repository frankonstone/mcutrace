#pragma once

#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace mcutrace {

// Stateful LSP request handler. Messages supplied to and returned from handle
// are unframed JSON-RPC payloads; run_language_server supplies stdio framing.
class LanguageServer final {
  public:
    LanguageServer();
    ~LanguageServer();

    LanguageServer(const LanguageServer&) = delete;
    LanguageServer& operator=(const LanguageServer&) = delete;

    [[nodiscard]] std::vector<std::string> handle(std::string_view message);
    [[nodiscard]] bool exit_requested() const noexcept;

  private:
    struct State;
    std::unique_ptr<State> state_;
};

// Runs a language server over an LSP stdio stream. Returns zero after the
// client sends exit, and non-zero for a malformed transport stream.
[[nodiscard]] int run_language_server(std::istream& input, std::ostream& output);

}  // namespace mcutrace
