# mcutrace Implementation Plan

This living document records implementation work linked to the requirement
documents in `docs/`. Each plan item has a stable heading ID, explicit requirement
links, status, work checklist, verification, and result. Annotations may appear
anywhere within the item block.

The original P0-P10 phases are preserved as PLN-0003 through PLN-0013; existing
PLN-0001 and PLN-0002 retain their IDs. Items remain in their original phase order,
followed by subsequent work. IDs identify items rather than execution order.

Historical completion statuses below reflect a review of the current source,
tests, and build configuration. Checked verification entries record that review;
they do not claim a fresh test or CI run during this documentation conversion.
Incomplete hardening work remains open. Later requirement changes are tracked in
follow-up items without rewriting completed work as unfinished.

## PLN-0003 Repository foundation
@link(REQ-0069, REQ-0070, REQ-0071, REQ-0072, REQ-0073, REQ-0074, REQ-0075, REQ-0076, REQ-0077, REQ-0078)
@status(done)

### Reason

Original phase P0: establish a buildable C++23 host project before semantic implementation.

### Work

- [x] CMake project and target layout
- [x] C++23, no exceptions, no RTTI
- [x] Warnings as errors
- [x] mcutest smoke test
- [x] Explicit sibling source paths for mcucli, mcujson, mcutoml, and mcutest
- [x] macOS-only GitHub Actions CI
- [x] Requirements and architecture documents

### Verification

- [x] Reviewed CMake target options and mcutest registration.
- [x] Reviewed CI configure/build/test steps and explicit dependency paths; configuration and compilation use the checked-out libraries without fetching dependencies themselves.

### Result

Implemented in CMakeLists.txt, tests/CMakeLists.txt, and .github/workflows/ci.yml, with requirements and architecture documentation in docs/. The build uses explicit mcu library checkouts. CI execution was not repeated for this plan conversion.

## PLN-0004 Core traceability model
@link(REQ-0019, REQ-0020, REQ-0021, REQ-0022, REQ-0023, REQ-0024, REQ-0025, REQ-0026, REQ-0027, REQ-0028, REQ-0029, REQ-0030, REQ-0031, REQ-0032, REQ-0052, REQ-0058)
@status(done)

### Reason

Original phase P1: define stable internal types independent of input formats.

### Work

- [x] Node identity and node kinds
- [x] Source location type
- [x] Typed directed edge type
- [x] Provenance type
- [x] Diagnostic/error model using `std::expected`
- [x] Deterministic graph container behavior

### Verification

- [x] Reviewed tests/test_model.cpp for deterministic ordering, duplicate identities, relationship types, provenance, dangling edges, and explicit errors.

### Result

Implemented in include/mcutrace/model.hpp, include/mcutrace/error.hpp, and src/model.cpp, with model regression tests in tests/test_model.cpp.

## PLN-0005 Markdown requirement parser
@link(REQ-0006, REQ-0007, REQ-0008, REQ-0009, REQ-0010, REQ-0011, REQ-0012, REQ-0013, REQ-0014, REQ-0015, REQ-0016, REQ-0017, REQ-0018)
@status(done)

### Reason

Original phase P2: turn requirement documents into validated requirement nodes.

### Work

- [x] Markdown heading scanner
- [x] `REQ-NNNN` parser
- [x] Optional `@req` marker
- [x] Title/body/source extraction
- [x] Duplicate and malformed-ID diagnostics
- [x] Deterministic next-ID helper

### Verification

- [x] Reviewed tests/test_requirements.cpp for heading/body extraction, fenced-code exclusion, malformed and duplicate IDs, ordering, and next-ID allocation.

### Result

Implemented in src/requirements.cpp and include/mcutrace/requirements.hpp, with regression tests in tests/test_requirements.cpp. The later requirement for annotations throughout a block remains outstanding in PLN-0002.

## PLN-0006 Generic JSON importer contract
@link(REQ-0033, REQ-0034, REQ-0035, REQ-0036, REQ-0040, REQ-0041, REQ-0042, REQ-0043, REQ-0044)
@status(done)

### Reason

Original phase P3: establish a producer-neutral ingestion boundary.

### Work

- [x] Importer interface returning model fragments plus diagnostics
- [x] Schema/version identification
- [x] Normalized path handling
- [x] Generic artifact preservation
- [x] Explicit unsupported-version errors

### Verification

- [x] Reviewed tests/test_importer.cpp for JSON ingestion, partial diagnostics, version rejection, path normalization, and payload preservation.

### Result

Implemented through include/mcutrace/importer.hpp and src/importer.cpp, with contract tests in tests/test_importer.cpp and extension guidance in docs/importer-contract.md.

## PLN-0007 Trace graph assembly
@link(REQ-0019, REQ-0020, REQ-0021, REQ-0022, REQ-0023, REQ-0024, REQ-0025, REQ-0026, REQ-0027, REQ-0028, REQ-0029, REQ-0030, REQ-0031, REQ-0032, REQ-0045, REQ-0046)
@status(done)

### Reason

Original phase P4: merge parsed requirements and imported artifacts into one graph.

### Work

- [x] Deterministic node merge
- [x] Edge insertion
- [x] Provenance merge
- [x] Duplicate/conflict detection
- [x] Dangling-reference representation for later validation

### Verification

- [x] Reviewed tests/test_assembly.cpp for deterministic merges, multiple provenance sources, dangling edges, duplicate conflicts, and imported diagnostics.

### Result

Implemented in src/assembly.cpp, with merge and diagnostic regression tests in tests/test_assembly.cpp.

## PLN-0008 Validation engine
@link(REQ-0045, REQ-0046, REQ-0047, REQ-0048, REQ-0049, REQ-0050, REQ-0051, REQ-0052, REQ-0053, REQ-0054)
@status(done)

### Reason

Original phase P5: make graph completeness and evidence failures actionable.

### Work

- [x] Dangling references
- [x] Missing test evidence
- [x] Missing implementation evidence
- [x] Missing coverage evidence
- [x] Failed test evidence
- [x] Static-analysis finding reporting
- [x] Configurable severities and exit policy

### Verification

- [x] Reviewed tests/test_validation.cpp for missing and failed evidence, typed implementation links, coverage policy, finding states, rule configuration, and deterministic output.

### Result

Implemented in src/validation.cpp, with rule and policy regression tests in tests/test_validation.cpp.

## PLN-0009 TOML configuration
@link(REQ-0004, REQ-0054, REQ-0062, REQ-0065, REQ-0066)
@status(done)

### Reason

Original phase P6: express project inputs and validation policy explicitly.

### Work

- [x] Typed configuration model
- [x] Mcutoml parser integration
- [x] Requirement file configuration
- [x] Artifact/importer configuration
- [x] Project roots and path bases
- [x] Validation policy configuration

### Verification

- [x] Reviewed tests/test_config.cpp for input paths, validation policy, invalid severities, and TOML parse failures.

### Result

Implemented in src/config.cpp and include/mcutrace/config.hpp using mcutoml, with configuration tests in tests/test_config.cpp.

## PLN-0010 CLI
@link(REQ-0053, REQ-0064, REQ-0065, REQ-0066, REQ-0067, REQ-0068)
@status(done)

### Reason

Original phase P7: provide a stable command interface without embedding orchestration.

### Work

- [x] Mcucli integration
- [x] `validate` mode/command
- [x] Explicit config/input options
- [x] `--version`
- [x] Useful process exit statuses

### Verification

- [x] Reviewed tests/test_cli.cpp for explicit inputs, configuration errors, validation invocation, help, version reporting, and exit results.

### Result

Implemented in src/cli.cpp and src/main.cpp using mcucli, with CLI regression tests in tests/test_cli.cpp.

## PLN-0011 JSON and human-readable output
@link(REQ-0055, REQ-0056, REQ-0057, REQ-0058, REQ-0059, REQ-0060, REQ-0061, REQ-0063)
@status(done)

### Reason

Original phase P8: make results consumable by both automation and developers.

### Work

- [x] Versioned mcutrace JSON schema
- [x] Mcujson serialization
- [x] Deterministic ordering
- [x] Summary counts
- [x] Untraced requirement report
- [x] Diagnostic source context

### Verification

- [x] Reviewed tests/test_output.cpp for versioned deterministic JSON, summaries, untraced requirements, evidence details, provenance, and text reports.

### Result

Implemented in src/output.cpp using mcujson, with serialization and report regression tests in tests/test_output.cpp.

## PLN-0012 First producer integrations
@link(REQ-0037, REQ-0038, REQ-0039)
@status(done)

### Reason

Original phase P9: connect the existing MCU tool stack through files, not library coupling.

### Work

- [x] Import mcutest JSON.
- [x] Import mcucov JSON.
- [x] Import mcucheck JSON.

### Verification

- [x] Reviewed tests/test_producer_importers.cpp for the three producer formats, large coverage reports, version rejection, and importer mismatch; tests/test_trace_import.cpp covers explicit requirement links.

### Result

Implemented in src/producer_importers.cpp and src/trace_import.cpp, with producer and requirement-link tests in tests/test_producer_importers.cpp and tests/test_trace_import.cpp.

## PLN-0013 Usability and extension hardening
@link(REQ-0003, REQ-0040, REQ-0078, REQ-0083)
@status(done)

### Reason

Original phase P10: prove mcutrace remains useful beyond the initial producers.

### Work

- [x] Third-party importer example/fixture
- [x] Larger graph performance tests
- [x] Stable diagnostic codes
- [x] Documentation of importer contract
- [x] Revisit Linux/Windows CI once core behavior stabilizes

### Verification

- [x] Reviewed tests/test_hardening.cpp and its third-party fixture for public importer extension and deterministic assembly/validation of 1,000 requirements with 1,000 tests.
- [x] Added and ran a repeatable 5,000-requirement assembly and validation benchmark with a generous regression threshold.
- [x] Reviewed Linux/Windows CI readiness and added GCC/Linux and MSVC/Windows jobs alongside Apple Clang/macOS.

### Result

The third-party fixture, importer documentation, stable diagnostic-code policy,
deterministic large-graph test, and measured 5,000-requirement pipeline benchmark
are present. CI now covers Apple Clang/macOS, GCC/Linux, and MSVC/Windows. The
host-debug build and complete native test suite passed locally; hosted CI execution
remains the responsibility of the workflow run.

## PLN-0001 Support annotated implementation plans
@link(REQ-0137)
@status(done)

### Reason

Existing implementation and test traces can remain present after a requirement
changes. The living implementation plan needs to expose the outstanding work
associated with that change alongside the existing evidence.

### Work

- [x] Add configuration for Markdown plan inputs.
- [x] Parse plan headings, block boundaries, requirement links, statuses, and source locations through the shared semantic core.
- [x] Diagnose malformed annotations, missing links or status, conflicting statuses, duplicate plan IDs, and unresolved requirement links.
- [x] Add plan nodes and explicit requirement relationships to the traceability model without treating them as implementation or test evidence.
- [x] Expose linked plan items and outstanding work in CLI reports and machine-readable output.
- [x] Expose navigable plan items and statuses in the language server's Trace view, refreshing them after plan edits.

### Verification

- [x] Test annotations in headings and throughout bodies, including nested sections and isolation between adjacent plan items.
- [x] Test multiple requirement links, whitespace, deduplication, and all supported statuses.
- [x] Test invalid input diagnostics and source locations.
- [x] Verify reverse navigation is derived entirely from plan links.
- [x] Verify outstanding work remains visible with passing tests and that completed plan items do not satisfy evidence expectations.
- [x] Verify plan input configuration and consistent CLI, JSON, and editor output.

### Result

Implemented plan parsing, configuration and explicit CLI inputs, graph nodes and
links, stable diagnostics, status-aware text/JSON output, and live language-server
and Trace-view integration. Native and extension regression suites passed locally.

## PLN-0002 Support requirement dependencies and body annotations
@link(REQ-0084, REQ-0129, REQ-0130, REQ-0131, REQ-0132, REQ-0133, REQ-0134, REQ-0135, REQ-0136)
@status(done)

### Reason

Requirement annotations currently assume header placement. Requirements need to
declare evidence expectations and dependencies naturally within prose paragraphs,
while dependency information remains available consistently in validation,
reports, and editor workflows.

### Work

- [x] Scan the complete requirement block for `@evidence(...)` and
  `@depends(...)`, accepting either annotation anywhere within a paragraph as
  well as in the requirement heading.
- [x] Parse, normalize, and deduplicate dependency identifiers while preserving
  annotation source locations and isolating annotations between adjacent
  requirement blocks.
- [x] Add typed `depends_on` relationships and retain unresolved references for
  validation and reporting.
- [x] Diagnose malformed annotations, unresolved prerequisites,
  self-dependencies, and dependency cycles without discarding unaffected
  requirements or valid references.
- [x] Keep dependency state independent from each requirement's evidence
  expectations and evidence results.
- [x] Expose direct prerequisites and dependents in `show`, JSON trace output,
  and the VS Code Trace view, including prerequisite evidence problems.
- [x] Add deterministic human-readable and JSON requirement-context output for
  direct and transitive prerequisites.
- [x] Add dependency-aware hover, definition, references, completion, and safe
  rename using the shared in-memory semantic model.

### Verification

- [x] Test `@evidence(...)` and `@depends(...)` at the beginning, middle, and end
  of prose paragraphs, in headings, and in either order within the same block.
- [x] Test multiple body paragraphs, nested headings, adjacent requirements, and
  annotations split across heading and body positions.
- [x] Test dependency whitespace, multiple identifiers, deduplication,
  cross-file references, and stable annotation source locations.
- [x] Test malformed and partially valid annotations, unresolved and self
  references, cycles, and deterministic diagnostic behavior.
- [x] Verify dependencies neither satisfy evidence expectations nor copy
  prerequisite evidence state to dependent requirements.
- [x] Verify CLI, JSON, language-server navigation and rename, Trace view refresh,
  and cycle-safe context output.

### Result

Implemented block-wide annotation parsing outside Markdown code spans, typed and
validated dependency relationships, evidence-independent semantics, direct and
transitive CLI/JSON reporting, and live editor navigation, rename, completion,
diagnostics, and Trace-view groups. Native and extension regression suites passed
locally.

## PLN-0014 Add the read-only MCP server
@link(REQ-0138, REQ-0139, REQ-0140, REQ-0141, REQ-0142, REQ-0143, REQ-0144, REQ-0145, REQ-0146, REQ-0147, REQ-0148, REQ-0149, REQ-0150, REQ-0151, REQ-0152, REQ-0153, REQ-0154, REQ-0155, REQ-0156, REQ-0157, REQ-0158, REQ-0159, REQ-0160, REQ-0161)
@status(done)

### Reason

AI agents need structured access to the configured mcutrace graph, its source
material, and its validation state without coupling that access to VS Code or
teaching each client to parse CLI reports. The MCP interface must preserve the
shared model's semantics and provenance while remaining a read-only local query
surface.

### Design

- Add `mcutrace-mcp` as a third executable over the existing semantic core,
  alongside `mcutrace` and `mcutrace-lsp`.
- Keep MCP framing, capability discovery, schema validation, and error mapping in
  a protocol adapter that does not own traceability semantics.
- Extend shared query/report services where the current CLI reports do not yet
  provide search, reverse transitive impact, filtered validation, or analysis
  revision information.
- Use stdio as the only v1 transport. Select and document the stable MCP protocol
  revision implemented by the server before fixing protocol fixtures.
- Use mcujson and the existing host stack unless the protocol contract reveals a
  capability that cannot be implemented within the project's dependency policy.
- Keep agent skills and client-specific launch configuration outside the server;
  they consume the server's tools and versioned reference resources.

### Work

- [x] Define the MCP protocol adapter boundary, supported protocol revision,
  capability metadata, server metadata, error taxonomy, and result schema version.
- [x] Add shared project-analysis snapshot services with configuration identity,
  input change detection, and a deterministic analysis revision.
- [x] Add reusable queries for project overview, bounded requirement/plan search,
  requirement detail, prerequisite context, reverse dependency impact, plan detail,
  and filtered validation.
- [x] Define versioned result types that preserve node and edge semantics,
  evidence distinctions, diagnostics, provenance, normalized source locations,
  completeness, and continuation state.
- [x] Implement stdio JSON-RPC input/output, request dispatch, argument validation,
  capability and catalog discovery, cancellation handling where required by the
  selected revision, and strict separation of stdout protocol messages from stderr
  logs.
- [x] Implement `project_overview`, `search`, `requirement`,
  `requirement_context`, `change_impact`, `plan`, and `validate` with deterministic
  tool definitions, input schemas, output schemas, and structured results.
- [x] Implement `mcutrace://rules`, `mcutrace://schema`, and
  `mcutrace://project`, deriving dynamic values from the running version and active
  project snapshot.
- [x] Constrain calls to the startup-selected configuration and its resolved
  inputs; reject arbitrary path access and ensure failed or cancelled requests do
  not alter later results.
- [x] Add the `mcutrace-mcp` target to CMake, supported host builds, installation,
  release packaging, and repository dogfooding configuration without introducing
  an editor dependency.
- [x] Document standalone launch, explicit and discovered configuration,
  advertised tools and resources, schema versioning, read-only behavior, client
  integration, and troubleshooting.

### Verification

- [x] Unit-test request parsing, revision and capability handling, deterministic
  tool/resource catalogs, schema validation, error mapping, and protocol-safe
  stdout behavior.
- [x] Unit-test every shared query for stable ordering, complete semantic fields,
  source and provenance preservation, evidence distinctions, unresolved links,
  cyclic dependencies, filtering, bounds, truncation, and cursor continuation.
- [x] Test configuration discovery and explicit selection, changed-input refresh,
  stable and changing analysis revisions, malformed projects, partial analysis,
  invalid identifiers, out-of-scope paths, cancellation, and recovery after failed
  requests.
- [x] Launch `mcutrace-mcp` as a subprocess in an integration test and exercise
  protocol discovery, all tools, and all resources over stdio against a fixture
  project.
- [x] Validate advertised input and output schema shapes against representative success,
  empty, partial, truncated, and error results.
- [x] Run the MCP protocol conformance tests applicable to the selected revision,
  or maintain equivalent checked-in protocol fixtures when no suitable official
  conformance runner is available for the host implementation.
- [x] Build and run the native test suite locally and include `mcutrace-mcp` in the
  existing Apple Clang/macOS, GCC/Linux, and MSVC/Windows CMake/CI build path,
  confirming locally that the CLI and LSP retain equivalent trace and validation
  behavior. Hosted CI execution remains the responsibility of the workflow run.
- [x] Dogfood the server against this repository and verify that an MCP client can
  discover related requirements, retrieve prerequisite and impact context, inspect
  outstanding plans, and distinguish missing from failed evidence without reading
  the Markdown documents directly.

### Result

Implemented `mcutrace-mcp` as a standalone read-only stdio server over the shared
semantic core. The server supports MCP revisions 2025-11-25 and 2025-06-18,
publishes seven structured tools and three versioned resources, refreshes a
deterministically identified project snapshot for project queries, confines path
filters to configured source inputs, and preserves traceability semantics,
diagnostics, provenance, and source locations.

Added native protocol/query tests, malformed and partial-project recovery tests,
cycle and pagination coverage, a real subprocess stdio fixture, CMake installation,
client documentation, and repository dogfooding. The complete local evidence
workflow passed with 119 tests, including six MCP-specific native tests and the
subprocess smoke test; hosted cross-platform CI was not run locally.

## Implementation discipline

Every implementation PR should identify the requirement IDs it satisfies or advances. New externally observable behavior should have a requirement before implementation. Tests should use requirement IDs in names or comments where doing so improves traceability without coupling the test framework to mcutrace itself.

When an agent changes requirement behavior, it shall update an existing unfinished
plan item or create a new item linked to the affected requirements, describing the
implementation gap and required verification. Completed items shall remain as
history; subsequent work shall use a new item. The agent shall maintain the work
checklist and status as implementation progresses and record the implemented
changes and verification results before setting `@status(done)`. Git diffs support
review of requirement changes; plan annotations alone do not automatically detect
changes for which no plan item was updated or created.
