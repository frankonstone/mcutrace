# mcutrace Architecture

## Purpose

mcutrace is an independent traceability aggregation and validation tool. It consumes project requirements and machine-readable artifacts produced by other tools, maps them into one internal model, validates the resulting graph, and emits reports.

mcutrace does not orchestrate external tools and does not depend on them as linked libraries merely to read their output.

## Data flow

```text
Markdown requirements ─┐
mcutest JSON ──────────┤
mcucov JSON ───────────┤
mcucheck JSON ─────────┼─> parsers/importers ─> Traceability Model
other tool JSON ───────┤                              │
source annotations ────┘                    ┌─────────┼─────────┐
                                            ▼         ▼         ▼
                                        validation  reports    JSON
```

## Layers

### Input layer

Input readers are responsible only for obtaining bytes/text and stable source locations. Filesystem concerns shall remain outside semantic model code.

### Requirement parser

The Markdown requirement parser discovers heading-based requirements, validates IDs, retains document ordering, and produces requirement nodes plus diagnostics.

### Importers

Each machine-readable producer has an isolated importer. An importer translates its external schema into the shared model without leaking producer-specific structures into core graph code.

Initial importers are planned for:

- mcutest
- mcucov
- mcucheck

The interface shall also support unrelated third-party producers.

### Traceability model

The core model contains nodes, typed directed relationships, provenance, and source locations. Producer-specific JSON objects are not stored as core model types unless preserved as generic artifact metadata.

Initial node kinds are:

- requirement
- source
- test
- coverage
- finding
- artifact

### Validation

Validation operates only on the normalized model and project policy. It detects duplicate identities, dangling references, missing evidence, failed evidence, and policy violations.

### Output

Output adapters render the same validated model as machine-readable JSON and human-readable reports. Output ordering must be deterministic.

## Dependency direction

```text
CLI / report / JSON output
          │
      validation
          │
    traceability model
       ▲        ▲
       │        │
requirements  importers
       ▲        ▲
       └── input layer
```

Core model code shall not depend on CLI, filesystem traversal policy, or specific external tool implementations.

## Host dependencies

mcutrace uses the maintained host stack:

- mcucli — command-line parsing
- mcujson — JSON parsing and serialization
- mcutoml — TOML configuration
- mcutest — unit tests

mcucov and mcucheck are data producers, not linked runtime dependencies of mcutrace.

## Error model

Recoverable errors are explicit values using `std::expected`. Exceptions and RTTI are disabled by default. Diagnostics contain stable error codes plus source context when available.

## Configuration

TOML configuration defines requirement inputs, artifact inputs, project roots, importer selection/options, and validation policy. The configuration layer translates TOML into typed internal options before parsing/importing begins.

## Determinism

All externally observable ordering must be deterministic. Filesystem discovery results shall be sorted before processing. Graph output shall use stable ordering independent of hash-table iteration or host platform.

## Platform strategy

Early CI intentionally targets macOS only. Production code must nevertheless use portable C++23 and normalized path semantics so Linux and Windows can be enabled later without architectural changes.
