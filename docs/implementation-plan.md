# mcutrace Implementation Plan

This plan keeps implementation slices small and traceable back to `docs/requirements.md`.

## P0 — Repository foundation

**Goal:** establish a buildable C++23 host project before semantic implementation.

Deliverables:

- CMake project and target layout
- C++23, no exceptions, no RTTI
- warnings as errors
- mcutest smoke test
- explicit sibling source paths for mcucli, mcujson, mcutoml, and mcutest
- macOS-only GitHub Actions CI
- requirements and architecture documents

Exit criteria:

- clean configure/build/test on macOS CI
- no external dependency fetch required by CI

Requirements: REQ-0069 through REQ-0078.

## P1 — Core traceability model

**Goal:** define stable internal types independent of input formats.

Deliverables:

- node identity and node kinds
- source location type
- typed directed edge type
- provenance type
- diagnostic/error model using `std::expected`
- deterministic graph container behavior

Requirements: REQ-0019 through REQ-0032, REQ-0052, REQ-0058.

## P2 — Markdown requirement parser

**Goal:** turn requirement documents into validated requirement nodes.

Deliverables:

- Markdown heading scanner
- `REQ-NNNN` parser
- optional `@req` marker
- title/body/source extraction
- duplicate and malformed-ID diagnostics
- deterministic next-ID helper

Requirements: REQ-0006 through REQ-0018.

## P3 — Generic JSON importer contract

**Goal:** establish a producer-neutral ingestion boundary.

Deliverables:

- importer interface returning model fragments plus diagnostics
- schema/version identification
- normalized path handling
- generic artifact preservation
- explicit unsupported-version errors

Requirements: REQ-0033 through REQ-0036, REQ-0040 through REQ-0044.

## P4 — Trace graph assembly

**Goal:** merge parsed requirements and imported artifacts into one graph.

Deliverables:

- deterministic node merge
- edge insertion
- provenance merge
- duplicate/conflict detection
- dangling-reference representation for later validation

Requirements: REQ-0019 through REQ-0032, REQ-0045, REQ-0046.

## P5 — Validation engine

**Goal:** make graph completeness and evidence failures actionable.

Deliverables:

- dangling references
- missing test evidence
- missing implementation evidence
- missing coverage evidence
- failed test evidence
- static-analysis finding reporting
- configurable severities and exit policy

Requirements: REQ-0045 through REQ-0054.

## P6 — TOML configuration

**Goal:** express project inputs and validation policy explicitly.

Deliverables:

- typed configuration model
- mcutoml parser integration
- requirement file configuration
- artifact/importer configuration
- project roots and path bases
- validation policy configuration

Requirements: REQ-0004, REQ-0054, REQ-0062, REQ-0065, REQ-0066.

## P7 — CLI

**Goal:** provide a stable command interface without embedding orchestration.

Deliverables:

- mcucli integration
- `validate` mode/command
- explicit config/input options
- `--version`
- useful process exit statuses

Requirements: REQ-0053, REQ-0064 through REQ-0068.

## P8 — JSON and human-readable output

**Goal:** make results consumable by both automation and developers.

Deliverables:

- versioned mcutrace JSON schema
- mcujson serialization
- deterministic ordering
- summary counts
- untraced requirement report
- diagnostic source context

Requirements: REQ-0055 through REQ-0061, REQ-0063.

## P9 — First producer integrations

**Goal:** connect the existing MCU tool stack through files, not library coupling.

Order:

1. mcutest JSON
2. mcucov JSON
3. mcucheck JSON

Each importer gets fixture-based tests and schema/version coverage.

Requirements: REQ-0037 through REQ-0039.

## P10 — Usability and extension hardening

**Goal:** prove mcutrace remains useful beyond the initial producers.

Deliverables:

- third-party importer example/fixture
- larger graph performance tests
- stable diagnostic codes
- documentation of importer contract
- revisit Linux/Windows CI once core behavior stabilizes

Requirements: REQ-0003, REQ-0040, REQ-0078.

## Implementation discipline

Every implementation PR should identify the requirement IDs it satisfies or advances. New externally observable behavior should have a requirement before implementation. Tests should use requirement IDs in names or comments where doing so improves traceability without coupling the test framework to mcutrace itself.
