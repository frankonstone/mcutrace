# mcutrace Requirements

## Scope

### REQ-0001 Independent aggregation tool
mcutrace shall operate as an independent traceability aggregation and validation tool and shall not orchestrate external tools.

### REQ-0002 External artifact ingestion
mcutrace shall ingest requirements and machine-readable artifacts produced by external tools.

### REQ-0003 Extensible integrations
mcutrace shall support adding new artifact importers without changing the core traceability model.

### REQ-0004 TOML configuration
mcutrace shall use TOML for project configuration.

### REQ-0005 Deterministic behavior
Given identical inputs and configuration, mcutrace shall produce deterministic results.

## Requirement discovery

### REQ-0006 Markdown requirements
mcutrace shall discover requirements in Markdown files.

### REQ-0007 Heading-based requirements
A Markdown heading containing a valid requirement ID shall define a requirement.

### REQ-0008 Requirement ID format
The canonical requirement ID format shall be `REQ-NNNN`, where `NNNN` is a four-digit decimal number.

### REQ-0009 Requirement marker
mcutrace shall support `@req` as an explicit requirement marker.

### REQ-0010 Requirement title
The requirement title shall be derived from the requirement heading text after the ID and marker are removed.

### REQ-0011 Requirement body
The Markdown content below a requirement heading and before the next heading of equal or higher level shall form the requirement body.

### REQ-0012 Requirement source location
Each requirement shall retain its source file and source line.

### REQ-0013 Duplicate requirement detection
mcutrace shall diagnose duplicate requirement IDs across all configured requirement files.

### REQ-0014 Malformed requirement IDs
mcutrace shall diagnose malformed strings that are intended to be requirement IDs.

### REQ-0015 Requirement ordering
Requirement discovery shall preserve deterministic document order.

### REQ-0016 Editable requirements @evidence(none)
mcutrace shall not impose ownership restrictions on who or what may modify requirement documents.

### REQ-0017 Requirement ID allocation support
mcutrace shall provide a mechanism for determining the next available requirement ID without introducing duplicates.

### REQ-0018 Requirement ID gaps
mcutrace shall permit gaps in the numeric requirement ID sequence.

## Traceability model

### REQ-0019 Unified model
mcutrace shall map all supported input formats into one internal traceability model.

### REQ-0020 Stable node identity
Every node in the traceability model shall have a stable identifier within one analysis result.

### REQ-0021 Requirement nodes
The model shall represent requirements as first-class nodes.

### REQ-0022 Source nodes
The model shall represent source artifacts as first-class nodes.

### REQ-0023 Test nodes
The model shall represent tests as first-class nodes.

### REQ-0024 Coverage nodes
The model shall represent coverage evidence as first-class nodes or evidence attached to trace relationships.

### REQ-0025 Finding nodes
The model shall represent static-analysis or policy findings as first-class nodes.

### REQ-0026 Generic artifact nodes
The model shall support generic artifact nodes for importer data not covered by a specialized node kind.

### REQ-0027 Directed relationships
The model shall represent traceability relationships as directed edges.

### REQ-0028 Relationship type
Every relationship shall have an explicit relationship type.

### REQ-0029 Relationship provenance
Every imported relationship shall retain provenance identifying the source artifact or importer that produced it.

### REQ-0030 Relationship source location
When available, relationships shall retain source locations from the producing artifact.

### REQ-0031 Multiple evidence sources
The model shall allow multiple evidence sources to support the same requirement.

### REQ-0032 Unknown relationship preservation
Importers shall be able to preserve unknown relationship types without data loss.

## Input and importer behavior

### REQ-0033 JSON tool input
mcutrace shall support JSON as the primary machine-readable interchange format for tool outputs.

### REQ-0034 Importer isolation
Each external tool format shall be handled by an isolated importer.

### REQ-0035 Importer failure reporting
Importer failures shall return structured errors rather than terminate by exception.

### REQ-0036 Partial import diagnostics
Malformed entries in an otherwise readable artifact shall produce diagnostics that identify the affected input.

### REQ-0037 mcutest integration
mcutrace shall support an importer for mcutest JSON output.

### REQ-0038 mcucov integration
mcutrace shall support an importer for mcucov JSON output.

### REQ-0039 mcucheck integration
mcutrace shall support an importer for mcucheck JSON output.

### REQ-0040 Third-party importer support
The importer architecture shall not require external producers to use an `mcu*` library.

### REQ-0041 Input version identification
Importers shall identify the input schema or format version when the producer exposes one.

### REQ-0042 Unsupported input version
Unsupported input versions shall produce explicit diagnostics.

### REQ-0043 File path normalization
mcutrace shall normalize input paths consistently before using them as traceability identities.

### REQ-0044 Relative paths
Relative artifact paths shall be resolved against a deterministic configured or artifact-specific base directory.

## Validation

### REQ-0045 Dangling reference detection
mcutrace shall diagnose relationships that reference unknown nodes or requirements.

### REQ-0046 Duplicate node detection
mcutrace shall diagnose conflicting duplicate node identities.

### REQ-0047 Missing test evidence
mcutrace shall be able to report requirements with no linked test evidence.

### REQ-0048 Missing implementation evidence
mcutrace shall report a requirement that expects implementation evidence when it has no typed `implements` relationship from a first-class implementation node.

### REQ-0049 Missing coverage evidence
mcutrace shall be able to report traceable implementation or test nodes that lack configured coverage evidence.

### REQ-0050 Failed test evidence
mcutrace shall preserve and report failing test evidence rather than treating it as missing evidence.

### REQ-0051 Static-analysis findings
mcutrace shall preserve and report linked static-analysis findings.

### REQ-0052 Validation severity
Validation rules shall report an explicit severity.

### REQ-0053 Validation exit status
The CLI shall return a non-zero status when configured validation criteria fail.

### REQ-0054 Validation configuration
Projects shall be able to enable, disable, or configure validation policies through TOML.

## Output

### REQ-0055 JSON output
mcutrace shall emit a machine-readable JSON representation of the traceability result.

### REQ-0056 Human-readable report
mcutrace shall provide a human-readable traceability summary.

### REQ-0057 Deterministic JSON ordering
JSON output ordering shall be deterministic where ordering is represented.

### REQ-0058 Diagnostic source context
Diagnostics shall include source file and line information when available.

### REQ-0059 Summary counts
The report shall include counts for requirements, trace nodes, relationships, validation errors, and validation warnings.

### REQ-0060 Untraced requirements report
The report shall identify requirements lacking configured evidence classes.

### REQ-0061 Output schema version
Machine-readable output shall carry an mcutrace schema version.

## Configuration and CLI

### REQ-0062 mcutoml usage
The host implementation shall use mcutoml for TOML parsing.

### REQ-0063 mcujson usage @evidence(build)
The host implementation shall use mcujson for JSON parsing and serialization.

### REQ-0064 mcucli usage
The host implementation shall use mcucli for command-line parsing.

### REQ-0065 Configuration discovery
The CLI shall accept an explicit configuration file path.

### REQ-0066 Explicit inputs
The CLI shall permit explicitly supplied requirement and artifact inputs where appropriate.

### REQ-0067 Validation command
The CLI shall provide a command or mode that validates the complete configured traceability graph.

### REQ-0068 Version command
The CLI shall expose the mcutrace version.

## Host implementation

### REQ-0069 C++23 @evidence(build)
The host implementation shall use C++23.

### REQ-0070 No exceptions @evidence(build)
The default host build shall compile with C++ exceptions disabled.

### REQ-0071 std::expected errors
Recoverable host errors shall be represented with `std::expected` or equivalent explicit result types rather than exceptions.

### REQ-0072 No RTTI @evidence(build)
The default host build shall compile with RTTI disabled unless a required platform integration proves otherwise.

### REQ-0073 No third-party host libraries @evidence(test,build)
The host implementation shall not introduce third-party libraries when equivalent functionality is provided by the maintained `mcu*` libraries or the C++ standard library.

### REQ-0074 Warnings as errors @evidence(build)
Project code shall build with a high warning level and warnings treated as errors in CI.

### REQ-0075 Automated tests @evidence(build)
Core model, parsers, importers, validation, and output behavior shall be covered by automated tests.

### REQ-0076 mcutest usage @evidence(build)
Host unit tests shall use mcutest.

### REQ-0077 macOS initial CI @evidence(build)
During initial development, the required CI host shall be macOS only to conserve runner time.

### REQ-0078 Future host portability
The design shall avoid unnecessary platform dependencies so Linux and Windows CI can be added later without redesigning the traceability model.
