# mcutrace

`mcutrace` is an independent traceability aggregation and validation tool for embedded C/C++ projects.

It reads requirements, annotated source files, and machine-readable outputs from development tools, maps them into one traceability model, validates relationships, and emits deterministic reports. It does not orchestrate or run those tools.

## Source requirement annotations

Implementation links can live next to the code they describe:

```cpp
// @req-file REQ-0043 REQ-0044
```

associates requirements with the complete source or header file, while:

```cpp
// @req REQ-0006
std::expected<Requirement, Error> parse_requirement(...);
```

associates requirements with the immediately following class, struct, enum, function, or method declaration/definition. Multiple canonical `REQ-NNNN` identifiers may be listed on one annotation.

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
