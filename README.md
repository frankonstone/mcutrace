# mcutrace

`mcutrace` is an independent traceability aggregation and validation tool for embedded C/C++ projects.

It reads requirements, annotated source files, and machine-readable outputs from development tools, maps them into one traceability model, validates relationships, and emits deterministic reports. It does not orchestrate or run those tools.

## Source requirement annotations

Implementation links can live next to the declaration or definition they describe:

```cpp
// @req REQ-0043 REQ-0044
std::expected<std::string, ImportError>
normalize_artifact_path(...);
```

This creates a first-class implementation node for the declaration and connects it to both requirements with `implements` relationships. File-level annotations remain available as an explicit fallback when a requirement genuinely applies to a complete source or header file:

```cpp
// @req-file REQ-0078
```

Multiple canonical `REQ-NNNN` identifiers may be listed on either annotation. Source links identify where a requirement is implemented; tests and other evidence independently establish whether it is verified.

Annotated files can be configured with `sources = ["src/foo.cpp", "include/foo.hpp"]` in the project configuration or supplied explicitly with repeatable `--source FILE` options.

## Requirement metadata and implementation plans

Requirements may declare evidence expectations and prerequisites in either their
heading or any prose paragraph in their block:

```markdown
### REQ-0042 Start controller
This behavior needs @evidence(implementation, test) and
@depends(REQ-0010, REQ-0011).
```

`@depends(...)` creates typed dependency relationships; it does not satisfy or
copy evidence. `mcutrace show REQ-0042` reports direct prerequisites and
dependents, while `mcutrace context REQ-0042` emits the requirement and its
cycle-safe transitive prerequisite context in text or JSON.

Living implementation plans use `PLN-NNNN` headings with `@link(...)` and one
of `@status(planned)`, `@status(in_progress)`, `@status(blocked)`, or
`@status(done)`. Configure them independently from requirement documents:

```toml
[project]
requirements = ["docs/requirements.md"]
plans = ["docs/implementation-plan.md"]
```

Plan links and status appear in CLI/JSON reports and the VS Code Trace view, but
never count as implementation or test evidence. Repeatable `--plan FILE` inputs
are also supported by `validate`, `show`, and `context`.

## CMake build evidence

Declare build evidence beside the relevant CMake configuration, then list the CMake file under `build_files`:

```cmake
# @req REQ-0069 REQ-0074
function(project_build_options target)
  # ...
endfunction()
```

```toml
[project]
build_files = ["CMakeLists.txt"]
```

This creates an artifact-to-requirement `verifies` link without a generated JSON sidecar. You can also supply a CMake file directly with `mcutrace validate --build CMakeLists.txt`.

## Project traceability

This repository includes [`mcutrace.toml`](mcutrace.toml), which indexes its requirement documents and annotated production sources. [`make.sh`](make.sh) builds with mcucov instrumentation, runs every test, generates mcutest/mcucov/mcucheck artifacts, validates the graph, and writes the hover report:

```sh
./make.sh
build/trace/mcutrace --config mcutrace.toml show REQ-0097
```

`show` prints the requirement's dependencies, linked plan items, implementations,
sources, tests, coverage, build evidence, and static-analysis findings. mcucov's
host tools need `nlohmann_json` and `CLI11`; set
`MCUCOV_FETCH_DEPENDENCIES=ON` when running the script if CMake should obtain
missing copies.

## VS Code integration

The extension in `editors/vscode` is a thin client for `mcutrace-lsp`: the C++
language server provides diagnostics, hover, navigation, symbols, completion,
CodeLens, and rename using the same semantic core as the CLI.

Test, package, and install the extension locally with:

```sh
./make.sh vscode
```

To install an already packaged extension manually:

```sh
code --install-extension editors/vscode/mcutrace-requirements-0.2.4-darwin-arm64.vsix --force
```

The platform-specific VSIX includes `mcutrace-lsp` and uses the bundled server by
default. Configure `mcutrace.languageServer.path` to override it with a
workspace-local executable, for example
`${workspaceFolder}/build/trace/mcutrace-lsp`. The server reads requirement and
artifact inputs exclusively from `mcutrace.toml`.

Install the separate mcucov Coverage extension to display `mcucov.lcov` files in VS Code's native Test Coverage view and editor gutter.

## Language server

`mcutrace-lsp` is a standard-input/output Language Server Protocol server. It loads the workspace `mcutrace.toml` from the LSP `rootUri` (or an `initializationOptions.configPath` override) and uses the same C++ parser, traceability model, and validation rules as the CLI.

Build it with the normal CMake build, then configure an LSP client to launch `mcutrace-lsp` over stdio. It provides diagnostics, hover, definition/reference/implementation navigation, symbols, completion, CodeLens, and safe requirement rename edits. See [`req/language-server.md`](req/language-server.md) for the supported contract.

## MCP server

`mcutrace-mcp` is a read-only local Model Context Protocol server for AI agents.
It uses the same configuration, semantic model, reports, and validation engine as
the CLI and language server, but exposes focused tools for project overview,
search, requirement detail, prerequisite context, change impact, plan detail, and
filtered validation. It also publishes versioned resources describing mcutrace's
rules, result schema, and active project.

Build it with the normal CMake build and configure an MCP client to launch it over
stdio from the project directory:

```sh
build/trace/mcutrace-mcp --config mcutrace.toml
```

The server speaks MCP revision `2025-11-25`, accepts `2025-06-18` clients, reads
only inputs selected by `mcutrace.toml`, and never edits files or runs evidence
producers. See [`integrations/mcp/README.md`](integrations/mcp/README.md) for client
setup and tool usage, and [`req/mcp-server.md`](req/mcp-server.md) for the supported
contract.

## Initial host stack

- C++23
- no exceptions / no RTTI by default
- `std::expected` for recoverable errors
- `mcucli` for CLI parsing
- `mcujson` for JSON input/output
- `mcutoml` for TOML configuration
- `mcutest` for tests

CI builds and tests with Apple Clang on macOS, GCC on Linux, and MSVC on Windows.

See `req/mcutrace.md`, `req/source-annotations.md`, `req/architecture.md`, and
`plan/implementation-plan.md`.
