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

## PLN-0015 Add the mcutrace agent skill
@link(REQ-0162, REQ-0163, REQ-0164, REQ-0165, REQ-0166, REQ-0167, REQ-0168, REQ-0169, REQ-0170, REQ-0171, REQ-0172, REQ-0173, REQ-0174, REQ-0175, REQ-0176, REQ-0177)
@status(done)

### Reason

Agents working in mcutrace-enabled projects need a concise, reusable guide for
obtaining the right traceability context through the MCP server and interpreting
it correctly. The guide must preserve mcutrace's evidence and relationship
semantics without dictating how each project writes its requirements or plans.

### Design

- Maintain the canonical portable skill at
  `integrations/skills/mcutrace/SKILL.md`, independently of the VS Code
  extension and the MCP server implementation.
- Let the skill own activation, task-to-tool routing, context selection,
  authorization boundaries, and handoff behavior; let the MCP server own project
  data, exact schemas, supported syntax, and runtime rules.
- Target projects with an existing working mcutrace configuration and MCP
  connection in the first version. Treat initial installation, configuration,
  and traceability-policy design as a separate future workflow.
- Keep the entry point concise and retrieve version-specific details from
  `mcutrace://rules`, `mcutrace://schema`, and `mcutrace://project`. Add no
  scripts or supporting references unless implementation or testing demonstrates
  a concrete need.
- Keep automatic discovery enabled with a narrow description that selects the
  skill for mcutrace-related work without capturing unrelated coding tasks.
- Store client-specific metadata alongside the portable skill only where it does
  not make the canonical `SKILL.md` dependent on one agent product.

### Work

- [x] Define representative positive and negative activation scenarios and the
  boundary between the skill, MCP server, project-local instructions, and normal
  project tools.
- [x] Create the canonical `mcutrace` skill with valid frontmatter, a precise
  discovery description, and portable instructions.
- [x] Describe the initial MCP orientation flow and how to use the rules, schema,
  and project resources without loading unnecessary reference content.
- [x] Add task-oriented routing for overview, search, requirement detail,
  prerequisite context, change impact, plan detail, and filtered validation.
- [x] Encode evidence, dependency, plan-status, diagnostic, completeness, and
  analysis-revision invariants that materially change agent decisions.
- [x] Encode project-convention preservation, read-only MCP behavior, user
  authorization boundaries, evidence freshness, and actionable degraded behavior.
- [x] Add supported agent metadata and an MCP dependency declaration where the
  target packaging format can express it without reducing portability.
- [x] Document installation into another project or agent environment and the
  connection to an explicitly selected `mcutrace.toml` through `mcutrace-mcp`.
- [x] Add only the supporting references or scripts shown to be necessary by the
  behavioral scenarios, and route to each conditional resource from `SKILL.md`.

### Verification

- [x] Run the skill structural validator and check naming, frontmatter,
  discovery metadata, dependency metadata, and the absence of scaffold
  placeholders.
- [x] Verify positive activation for traced implementation, requirement, plan,
  evidence, dependency, impact, and diagnostic tasks, plus negative activation
  for unrelated work in a repository that happens to contain `mcutrace.toml`.
- [x] Exercise tasks with a known requirement ID, a vague request requiring
  search, prerequisite-sensitive work, change-impact review, and an explicitly
  linked plan item.
- [x] Verify that bounded queries, continuation state, filtered results, and an
  analysis revision change are handled without mixing snapshots or claiming
  incomplete data is complete.
- [x] Verify that missing evidence, failed evidence, unresolved relationships,
  plan status, and diagnostics remain distinct in the agent's conclusions.
- [x] Exercise requirement and plan edits under different fixture conventions
  and verify that the skill follows those conventions without imposing its own
  document structure or prose methodology.
- [x] Verify that MCP reads do not authorize file edits or evidence generation,
  and that validation handoff reports whether tests, coverage, builds, and
  analysis artifacts were regenerated.
- [x] Exercise an unavailable server, unsupported capability, invalid project
  configuration, and partial project analysis; verify actionable reporting and
  no fabricated fallback analysis.
- [x] Install the packaged skill into an isolated consumer fixture, connect it to
  that fixture's `mcutrace-mcp`, and dogfood it against the mcutrace repository.
- [x] Run mcutrace trace validation and confirm that every skill requirement is
  linked to this plan without treating the plan as evidence.

### Result

Added a portable `mcutrace` skill with precise automatic discovery, runtime MCP
orientation, bounded task-to-tool routing, revision-consistency checks, faithful
trace semantics, project-convention preservation, explicit authorization
boundaries, validation handoff guidance, and actionable degraded behavior. Added
optional OpenAI-facing metadata with a read-only MCP dependency while keeping the
canonical `SKILL.md` client-neutral.

Added twelve repeatable behavioral scenarios and five native structural and
semantic tests covering the complete skill contract. Independent forward tests
of known-ID, truncated-search, and unavailable-server behavior passed every
skill-guided assertion; the unavailable-server case caught the intended
Markdown-fallback failure in the no-skill baseline. The canonical skill passed
the skill-creator validator, packaged successfully without repository-only evals,
installed through CMake, and connected to the MCP fixture from an isolated
consumer layout. The complete local evidence workflow passed with 124 tests and
no PLN-0015 requirement diagnostics; hosted cross-platform CI was not run
locally.

## PLN-0016 Integrate the mcutrace skill and MCP server with VS Code
@link(REQ-0178, REQ-0179, REQ-0180, REQ-0181, REQ-0182, REQ-0183, REQ-0184, REQ-0185, REQ-0186, REQ-0187, REQ-0188, REQ-0189, REQ-0190, REQ-0191)
@status(done)

### Reason

Users of the mcutrace VS Code extension should be able to give agents structured
mcutrace context without separately installing a skill, writing MCP registration
files, or locating packaged executables. The integration must remain transparent
and configurable through ordinary workspace settings while preserving VS Code's
trust and access controls.

### Design

- Package the canonical skill produced by PLN-0015 and contribute it with VS
  Code's Agent Skills contribution point. Treat the packaged copy as generated
  content so the extension cannot diverge semantically from the portable source.
- Register `mcutrace-mcp` through a VS Code MCP server definition provider rather
  than writing `.vscode/mcp.json` or another client-owned configuration file.
- Add `mcutrace.agent.*` settings to `.vscode/settings.json` as the supported
  control surface. Enable the integration by default only for folders with a
  resolvable mcutrace configuration, while allowing independent skill and MCP
  disablement and explicit launch overrides.
- Resolve configuration per workspace folder and return one isolated MCP server
  definition per eligible folder. Use direct process execution, an explicit
  configuration path, the folder as `cwd`, and distinct labels in multi-root
  workspaces.
- Let VS Code own server startup, shutdown, trust, tool approval, and agent-skill
  controls. The extension only contributes definitions and reports their status.
- Extend the existing platform packaging pipeline so the LSP, MCP server, and
  canonical skill are staged and verified together for the same extension target.

### Work

- [x] Determine and declare the minimum VS Code engine version providing the
  stable Agent Skills contribution and MCP server definition APIs used by the
  extension.
- [x] Extend the manifest with the canonical skill contribution, MCP definition
  provider contribution, agent integration settings, and a status command.
- [x] Stage the canonical `integrations/skills/mcutrace` content into the VSIX
  without introducing a separately edited skill source.
- [x] Stage the platform-specific `mcutrace-mcp` binary alongside
  `mcutrace-lsp`, and update packaging inputs and exclusions accordingly.
- [x] Add pure, testable setting and path resolution for the master, skill, and
  MCP enablement settings; executable path; extra arguments; and configuration
  path, including `${workspaceFolder}` and relative paths.
- [x] Implement the MCP server definition provider with direct stdio launch,
  explicit `--config`, workspace `cwd`, stable version metadata, and unique
  multi-root labels.
- [x] Compute project eligibility for every workspace folder and connect it to
  the contributed skill's enablement condition without emitting startup noise for
  ordinary non-mcutrace folders.
- [x] React to resource-scoped configuration, workspace-folder, and project
  configuration changes by refreshing context keys and MCP definitions.
- [x] Respect untrusted workspaces and existing VS Code MCP, skill, and tool
  controls without editing or attempting to override their settings.
- [x] Implement an Agent Integration status command and output-channel reporting
  for resolved definitions, disabled components, missing files, unsupported APIs,
  and provider failures.
- [x] Document the zero-configuration behavior and every `mcutrace.agent.*`
  workspace setting, including single-folder, multi-root, custom-binary, disabled,
  and troubleshooting examples.

### Verification

- [x] Unit-test default, disabled, overridden, malformed, and resource-scoped
  settings without depending on a running VS Code process.
- [x] Test placeholder expansion, relative and absolute paths, POSIX and Windows
  executable names, argument preservation, explicit configuration selection, and
  direct launch without shell interpretation.
- [x] Test zero, one, and multiple workspace folders; missing and custom
  configurations; duplicate folder names; addition and removal; and strict
  per-folder definition isolation.
- [x] Test manifest contribution paths, enablement conditions, stable provider
  identifiers, definition labels, version metadata, and configuration-change
  events with the VS Code API test doubles.
- [x] Verify that the master and component switches behave independently and that
  agent settings never alter language-server settings or generated workspace
  files.
- [x] Verify untrusted workspaces and unavailable VS Code APIs do not execute
  workspace-controlled programs and produce actionable, non-repeating status
  information.
- [x] Build every supported platform VSIX and inspect its contents for the
  matching LSP binary, MCP binary, canonical skill, and required manifest entries.
- [x] Inspect a produced VSIX and its contributed manifest, confirm packaged skill
  and MCP provider discovery inputs, and exercise the provider against a fixture
  project without `.vscode/mcp.json` or a copied project skill.
- [x] Change resource-scoped settings during the integration test and verify that
  enablement and MCP definitions update without a reload where the selected VS
  Code API supports it.
- [x] Run the extension regression suite and mcutrace trace validation, confirming
  that all new requirements are linked to PLN-0016 and neither the plan nor the
  contributed skill is treated as trace evidence before implementation exists.

### Result

Added stable VS Code Agent Skill and MCP provider contributions with a minimum
VS Code 1.109 engine, six resource-scoped settings, per-folder eligibility,
platform-correct bundled executable selection, explicit configuration arguments,
stable multi-root labels, live definition refresh, workspace-trust gating, and an
inspectable Agent Integration status channel. The extension never starts MCP
itself or writes agent configuration into the workspace.

The packaging path now stages both native runtimes and the generated canonical
skill, packages them for each CI platform, and inspects the resulting VSIX archive.
The local Darwin ARM64 VSIX contained both binaries and the complete canonical
skill. Twenty-nine Node tests and three native trace-evidence tests cover manifest,
settings, paths, multi-root isolation, feature flags, trust, absent APIs, events,
diagnostics, fixtures, and package contents. The complete local trace workflow
passed with no PLN-0016 requirement errors; hosted cross-platform CI was not run
locally.

## PLN-0017 Attach trace diagnostics to canonical requirement nodes
@link(REQ-0127)
@status(done)

### Reason

Requirement identifiers occur both on their canonical definition nodes and on
relationship copies under dependency and plan groups. The Trace view currently
selects the deepest identifier match, causing a relationship copy to receive the
direct diagnostic while the affected requirement retains its ordinary icon.

### Work

- [x] Prefer canonical requirement definitions when an identifier-based
  diagnostic can match both a definition and a relationship copy.
- [x] Preserve location-based ownership for evidence diagnostics, propagate
  requirement problems to relationship copies, and propagate descendant severity
  to ancestor badges and tooltips.

### Verification

- [x] Reproduce the issue with canonical, dependency, and plan-link copies of one
  requirement and verify only the canonical requirement receives the direct
  warning severity.
- [x] Run the complete VS Code extension regression suite and trace validation.

### Result

Diagnostic ownership now prefers source-location matches and canonical
requirement definitions over deeper relationship copies. A directly affected
requirement therefore receives the severity icon, while dependency and plan-link
copies retain linked-problem badges so their branches remain filterable. Folders,
documents, sections, and other ancestors retain propagated neutral badges. The VS
Code extension suite passes with 17 tests.

## PLN-0018 Show severity icons on linked requirement copies
@link(REQ-0127, REQ-0136)
@status(done)

### Reason

Dependency and plan-link copies propagated requirement problems for filtering,
badges, and tooltips, but the tree renderer only used direct diagnostic severity
for icons. A linked requirement therefore kept its normal checklist icon.

### Work

- [x] Track icon severity for diagnostics owned directly by a node or linked from
  its canonical requirement without applying severity icons to ancestors.
- [x] Render linked requirement copies with the severity-colored problem icon.
- [x] Add tree-model and provider regressions for linked warning icons.

### Verification

- [x] Run the complete VS Code extension regression suite.

### Result

Linked requirement copies now use the same severity icon and color as their
canonical requirement. Structural ancestors continue to use their ordinary icons
while retaining propagated problem badges, tooltips, and filter visibility.

## PLN-0019 Add extensible source-language scanners and JavaScript annotations
@link(REQ-0073, REQ-0089, REQ-0090, REQ-0091, REQ-0092, REQ-0093, REQ-0095, REQ-0096, REQ-0097, REQ-0192, REQ-0193, REQ-0194, REQ-0195, REQ-0196, REQ-0197, REQ-0198, REQ-0199)
@status(done)

### Reason

Source annotation discovery currently applies one C++-oriented declaration
scanner to every configured source file. This prevents mcutrace from creating
reliable declaration-scoped implementation evidence for the JavaScript code in
its own VS Code extension and makes each additional language likely to complicate
the same scanner. The next increment should establish a small language boundary
and prove it with JavaScript while retaining the current graph semantics.

### Design

- Split source annotation import into a language-neutral pipeline for normalized
  paths, implementation nodes, relationships, provenance, and diagnostics, plus
  language-specific lexical and declaration scanners.
- Select declaration scanners from normalized filename extensions. Preserve the
  existing file-scoped fallback for configured text sources and report an
  unsupported-language diagnostic for declaration-scoped annotations that cannot
  be bound safely.
- Move the current C++ recognition behind the scanner boundary without changing
  its supported extensions, declaration targets, identities, or diagnostics.
- Add a dependency-free JavaScript scanner for `.js`, `.mjs`, and `.cjs`. Use a
  deterministic lexical state machine for comments, quoted strings, template
  literals, regular-expression literals, delimiters, and declaration scope; do
  not execute JavaScript or require Node.js at analysis time.
- Bind annotations to named function and class declarations, class methods, and
  variable-bound function or arrow expressions. Account for common export and
  declaration modifiers, and diagnose anonymous or unsupported targets.
- Derive stable JavaScript identities from the normalized path, declaration kind,
  containing lexical scope, symbol, and declaration shape so repeated method
  names in different classes do not collide.
- Keep TypeScript, JSX, dynamic call-graph analysis, symbol resolution across
  modules, and JavaScript coverage production outside this first adapter. The
  scanner registry should allow those capabilities to be added independently.
- Dogfood the adapter on the VS Code extension sources. Keep implementation
  evidence distinct from test and coverage evidence, and retain explicit coverage
  exclusions until a JavaScript coverage producer is configured.

### Work

- [x] Define the source-language identifier, extension mapping, scanner result,
  and registration boundary without exposing language-specific state to graph
  assembly or validation.
- [x] Extract common annotation parsing and implementation-node construction from
  the current scanner, including canonical requirement parsing and stable
  diagnostics.
- [x] Move C++ comment and declaration recognition into its own scanner and lock
  current behavior with characterization tests before further changes.
- [x] Implement JavaScript lexical scanning for line and block comments while
  skipping quoted strings, template literal text and expressions, and regular
  expression literals with escapes and character classes.
- [x] Implement JavaScript declaration recognition for named functions, classes,
  class methods, and variable-bound function and arrow expressions, including the
  supported export and declaration modifiers.
- [x] Track containing classes and lexical scopes sufficiently to generate stable,
  collision-resistant implementation identities and complete provenance.
- [x] Add stable diagnostics for unsupported source languages, malformed
  annotations, unsupported JavaScript targets, and annotations without a following
  declaration.
- [x] Route CLI, project, language-server, and MCP analysis through the shared
  language-aware importer without adding client-specific parsing behavior.
- [x] Configure and annotate representative VS Code JavaScript sources for
  dogfooding, update source patterns and documentation, and retain explicit
  JavaScript coverage exclusions until coverage evidence exists.

### Verification

- [x] Run the existing C++ source-annotation regression suite unchanged and
  compare representative implementation IDs, edges, provenance, and diagnostics
  before and after the scanner split.
- [x] Test `.js`, `.mjs`, and `.cjs` selection plus unsupported and ambiguous
  extensions, mixed path separators, and case behavior on every supported host.
- [x] Test JavaScript functions, async and generator functions, classes,
  constructors, ordinary and computed methods, static methods, accessors, exported
  declarations, and variable-bound function and arrow expressions.
- [x] Test same-named methods in different classes, nested named declarations,
  repeated analysis, reordered source inputs, and edits that do and do not change
  stable implementation identity.
- [x] Test annotation-like text in strings, multiline strings, template literals
  and substitutions, regular expressions and character classes, comments, and
  malformed or minified JavaScript.
- [x] Verify unsupported and anonymous targets produce deterministic diagnostics
  and never fall back to a misleading file-scoped implementation.
- [x] Verify mixed C++ and JavaScript projects through CLI validation, JSON output,
  LSP navigation and refresh, and MCP requirement queries without cross-file
  scanner-state leakage.
- [x] Run the native tests, extension tests, coverage generation, static analysis,
  and mcutrace validation; confirm the JavaScript-backed requirements have explicit
  implementation and test evidence while no plan status is counted as evidence.

### Result

Source annotation import now selects registered C/C++ and JavaScript scanners by
normalized, case-sensitive extension while retaining language-neutral file-scoped
annotations. The JavaScript scanner lexes comments without interpreting marker
text in strings, templates, or regular expressions; recognizes the planned named
declaration forms; and produces stable scope-aware identities and diagnostics.
The existing CLI, project, LSP, and MCP paths consume the shared importer, and the
configured VS Code JavaScript sources now provide declaration-scoped dogfood.

Native and extension tests pass, coverage and mcutest evidence were regenerated,
static analysis reports no warnings, and focused trace validation reports complete
implementation and test evidence for `REQ-0192` through `REQ-0199`.

## PLN-0020 Add portable agent-neutral plan implementation through ACP
@link(REQ-0003, REQ-0064, REQ-0137, REQ-0138, REQ-0150, REQ-0151, REQ-0157, REQ-0158, REQ-0200, REQ-0201, REQ-0202, REQ-0203, REQ-0204, REQ-0205, REQ-0206, REQ-0207, REQ-0208, REQ-0209, REQ-0210, REQ-0211, REQ-0212, REQ-0213, REQ-0214, REQ-0215)
@status(planned)

### Reason

mcutrace can expose plan context through MCP, but it cannot yet turn a selected
plan item into an explicitly authorized implementation session. Implementing that
workflow inside one editor would make task semantics, lifecycle handling, and
completion policy unavailable to terminals and other development environments.
The portable `mcutrace implement` command should own the common workflow, use ACP
as its agent boundary, and retain `mcutrace-mcp` as the project-specific source of
traceability context.

### Design

- Add `mcutrace implement PLN-NNNN` as the portable entry point and keep its task
  construction and ACP lifecycle independent of agent vendors and host UIs. Use
  the stable ACP protocol version supported by the selected agent and isolate any
  necessary vendor adaptation in separately installed ACP executables.
- Model agent definitions as configuration data containing an ID, label, command,
  and arguments. Support a default agent and explicit selection without a closed
  vendor enumeration. Defer registry-based discovery and installation until it
  can be added without making network access or executable download implicit.
- Support `--agent <id>` as a one-run override, `--dry-run` as a no-launch preview,
  and `--dry-run --format json` as the versioned task-envelope interface for
  environment adapters and automation. Keep formatting orthogonal to execution
  by requiring dry-run for task-envelope emission.
- Resolve fresh plan detail and analysis revision before launch. Send a concise,
  provider-neutral request that names the canonical plan ID and directs the agent
  to use mcutrace context, preserve unrelated work, maintain the plan, generate
  evidence, and validate before claiming completion.
- Create ACP sessions with the owning workspace folder as `cwd` and the resolved
  project `mcutrace-mcp` stdio server in `mcpServers`. Do not require the agent to
  discover a VS Code-contributed skill; optionally advertise the canonical skill
  only when the selected agent supports a compatible mechanism.
- Implement ACP version and capability negotiation, streamed updates, permission
  mediation, cancellation, optional resumption, process supervision, and clean
  shutdown behind one portable client boundary. Expose stable lifecycle events
  that terminal and editor frontends can render appropriately.
- Provide a usable terminal frontend for output, tool activity, transient agent
  plans, permissions, stop reasons, and failures. Keep transient agent plans
  distinct from the persistent mcutrace plan document.
- Preserve project-root isolation and host trust controls. Never auto-approve agent
  actions, add workspace roots, install agents, write agent configuration files,
  or infer `done` from an ACP turn or process exit.
- Refresh the authoritative mcutrace analysis after each terminal session outcome
  and present the current plan status and focused validation findings. The agent
  may edit the plan as part of authorized implementation work, but the extension
  shall not manufacture plan completion from session state.

### Work

- [ ] Add a standards-conforming ACP client to the portable mcutrace runtime, pin
  its compatible protocol surface, and document dependency, license, and
  packaging impact.
- [ ] Define generic ACP agent settings and resolution for IDs, labels, executable
  commands, arguments, and the default agent, including actionable validation and
  redacted status output.
- [ ] Add `mcutrace implement PLN-NNNN`, `--agent <id>`, `--dry-run`, and the
  combined `--dry-run --format json` task-envelope output with clear help,
  validation, and exit-status behavior.
- [ ] Add a task-envelope builder that retrieves fresh plan and requirement
  context, records the analysis revision, and produces the common implementation
  request without vendor-specific prompt branches.
- [ ] Reuse the existing project eligibility and MCP executable resolution to
  attach the correct `mcutrace-mcp` definition to every new, loaded, or resumed
  ACP session.
- [ ] Implement ACP subprocess transport, initialization, authentication handoff,
  capability negotiation, session creation, prompt turns, streaming updates,
  permission requests, cancellation, and cleanup.
- [ ] Persist or retain minimal plan-to-session metadata outside project files,
  prevent duplicate active sessions within a client, and support resumption only
  when negotiated.
- [ ] Add a terminal progress and permission interface for active,
  waiting-for-permission, completed-turn, cancelled, and failed session states,
  plus a stable structured event representation for future adapters.
- [ ] Refresh mcutrace analysis after terminal outcomes, report stale or changed
  plan context, and show focused validation without automatically changing plan
  status.
- [ ] Document CLI usage and configuration using arbitrary ACP executables, with
  examples for multiple agents clearly identified as examples rather than
  built-in vendor dependencies.

### Verification

- [ ] Use deterministic fake ACP subprocesses to test initialization, protocol
  mismatch, capability negotiation, authentication requests, session creation,
  prompt construction, streamed content, tool updates, permission decisions,
  cancellation, resumption, malformed messages, crashes, and cleanup.
- [ ] Run the same end-to-end plan launch contract against at least two fake
  agents with different optional capabilities and verify that no production code
  branches on a vendor name.
- [ ] Verify CLI behavior for planned, in-progress, blocked, and done items; zero,
  one, and several configured agents; missing defaults; agent overrides; dry-run;
  JSON task envelopes; and duplicate launch attempts.
- [ ] Verify the session receives the project's absolute working directory and
  exact resolved mcutrace MCP definition, including explicit configurations,
  overridden executables, and path-normalization cases.
- [ ] Verify stale analysis revisions, missing MCP runtime, rejected permissions,
  cancellation, and agent failure leave useful diagnostics without claiming
  completion or losing visibility of partial changes.
- [ ] Verify tokens and unrelated environment data are absent from logs, status
  output, errors, snapshots, and test fixtures.
- [ ] Run the native CLI and agent-client regression suites, exercise at least one
  locally installed ACP adapter manually, inspect packaged runtime contents, and
  run mcutrace validation to confirm every portable requirement is linked to this
  plan item.

## PLN-0021 Add the VS Code plan implementation adapter
@link(REQ-0127, REQ-0137, REQ-0184, REQ-0187, REQ-0188, REQ-0190, REQ-0191, REQ-0200, REQ-0202, REQ-0203, REQ-0208, REQ-0209, REQ-0211, REQ-0212, REQ-0213, REQ-0216, REQ-0217, REQ-0218, REQ-0219)
@status(planned)

### Reason

The portable implementation workflow makes agent-neutral execution available
outside an editor, but the VS Code Trace view still needs a thin adapter to turn
plan-item actions into sessions and render their lifecycle using native trust,
permission, storage, and progress interfaces.

### Design

- Keep the adapter thin: select a plan and agent, invoke the portable task and ACP
  boundaries, translate lifecycle events into VS Code UI, and delegate completion
  evaluation back to refreshed mcutrace analysis.
- Add `Start Implementation` for the default agent and `Start Implementation
  With...` for explicit selection. Offer resume and cancellation only when the
  active session and negotiated capabilities allow them.
- Store minimal plan-to-session metadata in extension workspace state and derive
  command enablement and plan decorations from that state without writing project
  configuration or treating session state as persistent plan status.
- Render streamed messages, tool calls, transient agent plans, permission prompts,
  stop reasons, validation results, and failures in an inspectable VS Code surface
  associated with the selected plan.
- Reuse existing project eligibility, executable resolution, workspace trust, and
  multi-root isolation. Do not add vendor-specific code or require VS Code Agent
  Skill discovery for implementation to work.

### Work

- [ ] Add plan-item context menu and command-palette contributions for start,
  start-with, resume, and cancel, with status- and session-aware enablement.
- [ ] Add an adapter from selected Trace view plan nodes to the portable task
  builder and ACP session lifecycle without duplicating task construction.
- [ ] Resolve default and explicitly selected agent definitions through the common
  configuration layer and present actionable selection and configuration errors.
- [ ] Store and restore minimal session metadata through VS Code workspace state,
  guarding against concurrent launches of the same plan.
- [ ] Add native progress, output, permission, cancellation, failure, and focused
  post-turn validation presentation while retaining session identity across Trace
  refreshes.
- [ ] Update extension settings, status diagnostics, README content, activation
  events, packaging, and workspace-trust declarations for the new adapter.

### Verification

- [ ] Test manifest commands, plan-item context menus, command enablement, default
  and explicit agent selection, missing configuration, and duplicate launches.
- [ ] Test event rendering, permission acceptance and rejection, cancellation,
  optional resumption, crashes, deactivation, and post-turn validation using fake
  portable workflow and ACP boundaries.
- [ ] Test planned, in-progress, blocked, and done plans without conflating session
  state, agent-generated plans, persistent plan status, or trace evidence.
- [ ] Test single- and multi-root workspaces, remote extension hosts, untrusted
  workspaces, configuration changes, folder removal, and redacted diagnostics.
- [ ] Run the complete extension regression suite, inspect the produced VSIX, and
  run mcutrace validation to confirm the adapter requirements are linked to this
  plan item and the portable CLI remains independently usable.

## PLN-0022 Prevent redundant Trace view repaints
@link(REQ-0100, REQ-0127, REQ-0220)
@status(done)

### Reason

The language server can publish the complete trace several times around one edit
through document synchronization, save, and watched-file notifications. The
extension currently rebuilds and refreshes the entire tree for every publication
and also requests a global decoration repaint, which can make the Trace view blink
even when successive payloads are identical.

### Work

- [x] Coalesce rapid trace publications and apply only the newest complete
  payload after a short bounded delay.
- [x] Skip tree construction and refresh when the newest payload is identical to
  the last rendered payload.
- [x] Refresh file decorations only when their visible badge, tooltip, or
  propagation values change.
- [x] Preserve existing stable item IDs, expansion behavior, filtering,
  navigation, problem summaries, and the view badge.
- [x] Add provider-level regression coverage for coalescing, newest-payload
  selection, duplicate suppression, and decoration refresh suppression.

### Verification

- [x] Run the complete VS Code extension test suite.
- [x] Run mcutrace validation and confirm REQ-0220 is linked to this plan item
  without requirement or plan parsing errors.
- [x] Exercise the scheduled provider update path and verify that a burst renders
  its newest payload once, an identical payload performs no repaint, decoration
  events occur only for visible decoration changes, and the bounded timer applies
  a pending update.

### Result

Trace publications are now accumulated for a bounded 100 ms window so rapid
document, save, and watcher analyses render only the newest payload. A payload
identical to the last rendered snapshot no longer rebuilds or refreshes the tree,
and unchanged file decorations no longer request a second global repaint. Stable
tree IDs and all existing Trace view behavior remain unchanged. The complete
31-test extension suite and mcutrace validation passed locally; an installed VSIX
was not manually exercised during this change.

## PLN-0023 Install the mcutrace Agent Skill from VS Code
@link(REQ-0221)
@status(done)

### Reason

The VS Code extension contributes the bundled skill only to VS Code agent hosts.
Codex desktop, CLI, and other standard Agent Skills consumers cannot discover that
extension-private path, leaving users to copy the skill into their workspace by
hand.

### Work

- [x] Add a Command Palette action that installs the bundled canonical skill at
  `.agents/skills/mcutrace` in a selected workspace folder.
- [x] Preserve workspace trust and multi-root isolation, and do not overwrite an
  existing skill installation.
- [x] Improve implicit matching for canonical `REQ-NNNN` and `PLN-NNNN` prompts.
- [x] Document the command and its separate MCP-registration boundary.

### Verification

- [x] Test manifest registration, folder selection, successful copying, existing
  installations, workspace trust, and actionable failures.
- [x] Run the complete VS Code extension test suite and mcutrace validation.

### Result

Added **mcutrace: Install Codex Skill in Workspace** to copy the packaged
canonical skill into the standard project skill directory after explicit user
invocation. The command selects the target in multi-root workspaces, refuses to
write in untrusted workspaces, preserves existing installations, and explains
that standalone Codex clients still need an MCP registration. The skill
description now front-loads `REQ-NNNN` and `PLN-NNNN` identifiers for more reliable
implicit matching.

All 33 VS Code extension tests and 129 instrumented native tests passed. The
refreshed trace graph links REQ-0221 to its implementation and passing native test
and reports no diagnostic for the requirement; project validation completed with
zero errors.

## PLN-0024 Combine Codex skill and MCP setup
@link(REQ-0222)
@status(done)

### Reason

Installing the project skill makes it discoverable to Codex but leaves the skill's
required `mcutrace` MCP dependency unavailable outside VS Code. Codex supports a
trusted project-local MCP definition, so the explicit setup command can provide
both pieces without asking the user to perform a second manual configuration step.

### Work

- [x] Rename the command to describe complete Codex workspace setup.
- [x] Stage the packaged MCP executable in stable extension storage and write a
  project-scoped `.codex/config.toml` definition for the selected workspace.
- [x] Preserve unrelated Codex configuration and user-managed `mcutrace` entries,
  while allowing the command to refresh its own marked configuration block.
- [x] Update setup messages and documentation to describe the combined result.

### Verification

- [x] Test generated TOML, managed updates, conflict preservation, runtime staging,
  multi-root paths, partial existing state, and failure diagnostics.
- [x] Run the complete extension and instrumented native test suites, then refresh
  mcutrace validation.

### Result

The Command Palette now exposes **mcutrace: Set Up Codex in Workspace** as a
single explicit action. It installs the canonical Agent Skill, stages the packaged
MCP executable in stable extension storage, and adds a project-scoped STDIO server
definition using the selected workspace's configuration and working directory.
The setup preserves existing skills, unrelated Codex settings, and user-managed
`mcutrace` server definitions; subsequent runs update only the marked block created
by mcutrace.

All 34 VS Code extension tests and 129 instrumented native tests passed. The
refreshed trace graph links REQ-0222 to its three implementation functions and its
passing native test, and project validation completed with zero errors.

## PLN-0025 Add project creation and integration to the mcutrace skill
@link(REQ-0163, REQ-0164, REQ-0173, REQ-0175, REQ-0177, REQ-0223, REQ-0224)
@status(done)

### Reason

The portable skill currently assumes that `mcutrace.toml` and the MCP connection
already exist. It must also support the two setup paths that precede that state:
creating a new mcutrace project and adding mcutrace to an existing project.

### Work

- [x] Extend discovery and compatibility metadata to include project setup.
- [x] Route setup requests to focused project-creation and existing-project
  integration guidance without requiring MCP before it can be configured.
- [x] Preserve project choices, user-managed configuration, authorization
  boundaries, and the distinction between setup failures and trace findings.
- [x] Add behavioral scenarios and native structural tests for both setup paths.
- [x] Update user documentation to describe the expanded skill scope.

### Verification

- [x] Run the skill structural validator.
- [x] Build and run the skill test target.
- [x] Validate the affected requirements in the refreshed mcutrace graph.

### Result

The skill now routes new-project and existing-project setup to a focused
reference before MCP orientation. The workflow creates a minimal configuration,
uses only real project inputs, preserves user-managed integration, separates
policy choices from setup defaults, validates locally, and then confirms the
connected MCP project. Discovery metadata, Codex-facing metadata, documentation,
requirements, behavioral scenarios, and the workspace-installed skill copy were
updated consistently.

The skill structural validator passed. The instrumented build and all mcutest
binaries passed, including six skill tests, and refreshed mcutest evidence links
both new requirements to their passing setup test. Focused MCP validation of all
PLN-0025 requirements returned zero diagnostics at one analysis revision.
Coverage and static-analysis artifacts were not regenerated for this
documentation-and-test-only change.

## PLN-0026 Initialize new projects from the Codex setup command
@link(REQ-0222)
@status(done)

### Reason

The explicit Codex workspace setup command currently rejects a new project before
it can install the Agent Skill or MCP server because the default `mcutrace.toml`
does not exist yet. This makes the setup command unable to establish the project
environment it is meant to configure.

### Work

- [x] Initialize a minimal default `mcutrace.toml` and neutral requirements
  document when the selected workspace has no default configuration.
- [x] Preserve existing configuration and requirements files, and keep a missing
  explicit custom configuration path actionable.
- [x] Report whether the project was initialized or already present alongside
  the skill and MCP setup states.
- [x] Update user documentation for the complete setup behavior.

### Verification

- [x] Cover project initialization, repeat setup, file preservation, and missing
  custom configuration paths in the VS Code extension tests.
- [x] Run the complete VS Code extension and native test suites.
- [x] Refresh mcutrace artifacts and validate REQ-0222.

### Result

The explicit Codex setup command now initializes a new workspace with a minimal
`mcutrace.toml` and neutral `requirements.md` before installing the skill and MCP
configuration. Existing project files remain unchanged, while a missing explicit
custom configuration still produces the actionable diagnostic that previously
applied to every new project. Completion messages report the project state as
well as the skill and MCP states.

All 35 VS Code extension tests and 138 instrumented native tests passed. The
full traced build regenerated mcutest, mcucov, and mcucheck artifacts; mcucheck
reported no violations, and project validation completed with zero errors.

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
