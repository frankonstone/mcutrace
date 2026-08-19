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
