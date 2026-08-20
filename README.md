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

## Project traceability

This repository includes [`mcutrace.toml`](mcutrace.toml), which indexes its requirement documents and annotated production sources. [`make.sh`](make.sh) builds with mcucov instrumentation, runs every test, generates mcutest/mcucov/mcucheck artifacts, validates the graph, and writes the hover report:

```sh
./make.sh
build/trace/mcutrace --config mcutrace.toml show REQ-0097
```

`show` prints the requirement's implementations, sources, tests, coverage, and static-analysis findings. The JSON report at `build/mcutrace-report.json` supplies the optional evidence sections in the VS Code hover. mcucov's host tools need `nlohmann_json` and `CLI11`; set `MCUCOV_FETCH_DEPENDENCIES=ON` when running the script if CMake should obtain missing copies.

## VS Code integration

The extension in `editors/vscode` shows the title, body, and available trace evidence for a `REQ-NNNN` requirement on hover. It provides native Go to Definition navigation with Cmd-click on macOS, Ctrl-click on Windows/Linux, or `F12`.

Test, package, and install the extension locally with:

```sh
./make.sh vscode
```

To install an already packaged extension manually:

```sh
code --install-extension editors/vscode/mcutrace-requirements-0.1.2.vsix --force
```

Requirement documents are discovered with `**/requirements*.md` by default. The `mcutrace.requirementFiles` workspace setting accepts explicit glob patterns when a project uses different filenames.

Install the separate mcucov Coverage extension to display `mcucov.lcov` files in VS Code's native Test Coverage view and editor gutter.

## Initial host stack

- C++23
- no exceptions / no RTTI by default
- `std::expected` for recoverable errors
- `mcucli` for CLI parsing
- `mcujson` for JSON input/output
- `mcutoml` for TOML configuration
- `mcutest` for tests

CI intentionally runs on macOS only during early development to conserve runner time.

See `docs/requirements.md`, `docs/requirements-source-annotations.md`, `docs/architecture.md`, and `docs/implementation-plan.md`.
