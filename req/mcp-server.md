# mcutrace MCP Server Requirements

The mcutrace MCP server exposes the configured traceability graph to local AI
agents. It gives agents structured, source-backed project context while preserving
the same semantics and validation results as the command-line and language-server
interfaces.

The first version is a read-only local interface. It does not edit project files,
run evidence producers, provide a remote HTTP service, depend on an editor, or
prescribe the prose structure of requirements and implementation plans. Agent
skills and client-specific installation metadata are separate integrations that
may use this server but are not part of the server itself.

## Server boundary and protocol

### REQ-0138 Standalone MCP server executable @evidence(implementation,test,build)
mcutrace shall provide an `mcutrace-mcp` executable that can be launched without
the mcutrace VS Code extension or any other editor process.

### REQ-0139 Shared MCP semantics @evidence(implementation,test)
The MCP server shall use the same configuration loader, parsers, traceability
model, query logic, and validation logic as the mcutrace CLI and language server;
it shall not reimplement mcutrace semantics in an MCP-specific model.

### REQ-0140 MCP stdio transport @evidence(implementation,test)
The MCP server shall support the Model Context Protocol over standard input and
standard output using the message framing and JSON-RPC behavior required by its
advertised MCP protocol revision. Standard output shall contain only protocol
messages, and human-readable logs or diagnostics shall be written to standard
error.

### REQ-0141 MCP protocol revision and capabilities @evidence(implementation,test)
The MCP server shall advertise a documented MCP protocol revision and only the
capabilities it implements. It shall implement the base protocol behavior, tool
capability, and resource capability required by that revision and return protocol
errors for unsupported methods, incompatible revisions, and malformed protocol
requests.

### REQ-0142 Project configuration selection @evidence(implementation,test)
The MCP server shall accept an explicit `mcutrace.toml` path at startup and shall
otherwise apply the same configuration discovery rules as the CLI from its launch
working directory. Project selection shall not depend on the MCP roots capability,
and project paths shall be resolved using the configured project root and existing
mcutrace normalization rules.

### REQ-0143 Read-only MCP operations @evidence(implementation,test)
MCP tools and resources shall not modify requirement, plan, source, build,
configuration, or artifact files; invoke external evidence producers; or change
traceability semantics. Reading changed configured inputs to refresh an analysis
snapshot is permitted.

## Agent-facing tools

### REQ-0144 Deterministic tool catalog @evidence(implementation,test)
The MCP server shall expose a deterministic tool catalog containing
`project_overview`, `search`, `requirement`, `requirement_context`,
`change_impact`, `plan`, and `validate`. Every tool shall have a concise usage
description and JSON input and output schemas that allow a client to validate a
call and its structured result.

### REQ-0145 Project overview tool @evidence(implementation,test)
`project_overview` shall return the selected configuration identity, normalized
project root, configured input categories, trace summary counts, validation
summary, untraced requirements, outstanding plan items, analysis revision, and
any project-loading diagnostics without requiring the caller to know an entity
identifier.

### REQ-0146 Trace search tool @evidence(implementation,test)
`search` shall find requirements and plan items by canonical identifier and by
case-insensitive text in their titles and bodies. It shall support optional filters
for entity kind, plan status, evidence state, diagnostic severity, and diagnostic
code, and shall return deterministic, bounded summaries with stable identifiers
and source locations rather than complete entity bodies.

### REQ-0147 Requirement detail tool @evidence(implementation,test) @depends(REQ-0133)
`requirement` shall return one requirement's identifier, complete title and body,
definition location, expected evidence classes, implementations, sources, tests,
coverage, builds, findings, direct prerequisites, direct dependents, linked plan
items, unresolved relationships, and applicable diagnostics. An unknown or
malformed requirement identifier shall produce an actionable tool execution error.

### REQ-0148 Requirement prerequisite context tool @evidence(implementation,test) @depends(REQ-0134)
`requirement_context` shall return the requested requirement and its direct and
transitive prerequisites, including full requirement content, dependency edges,
evidence relationships, unresolved dependencies, source locations, and applicable
diagnostics. Each resolved requirement shall occur once, and the query shall
terminate for cyclic dependency input.

### REQ-0149 Requirement change-impact tool @evidence(implementation,test) @depends(REQ-0130, REQ-0131)
`change_impact` shall return the direct and transitive dependents of a requested
requirement, the dependency edges that make each dependent reachable, unresolved
relationships relevant to the traversal, source locations, and applicable
diagnostics. Each resolved requirement shall occur once, and the query shall
terminate for cyclic dependency input without claiming that a dependent is
broken.

### REQ-0150 Plan detail tool @evidence(implementation,test) @depends(REQ-0137)
`plan` shall return one plan item's identifier, complete title and body, status,
definition location, explicitly linked requirements, unresolved links, and
applicable diagnostics. The result shall not represent a plan link or plan status
as implementation or verification evidence.

### REQ-0151 Filtered validation tool @evidence(implementation,test)
`validate` shall return project-wide diagnostics by default and shall support
optional filters for requirement or plan identifiers, configured source paths,
severity, and diagnostic code. Filtering shall not change validation semantics or
hide whether a returned result represents the complete project or a filtered
subset.

## Resources and structured results

### REQ-0152 MCP reference resources @evidence(implementation,test)
The MCP server shall expose deterministic `mcutrace://rules`,
`mcutrace://schema`, and `mcutrace://project` resources. The rules resource shall
describe the annotations and traceability semantics supported by the running
version, including evidence independence, explicit relationship creation, plan
status semantics, and mcutrace's non-orchestration boundary. The schema resource
shall describe the MCP result schema version and entity vocabulary. The project
resource shall identify the selected project, configured inputs, current analysis
revision, and loading state.

### REQ-0153 Versioned structured tool results @evidence(implementation,test)
Every successful tool call shall return structured content conforming to its
advertised output schema and containing an mcutrace MCP schema version, the
analysis revision, the requested data, and relevant diagnostics. When required
for compatibility with the advertised MCP revision, the server shall also return
an equivalent serialized JSON text content block without changing the meaning of
the structured result.

### REQ-0154 Provenance and source preservation @evidence(implementation,test)
MCP results shall preserve canonical entity identifiers, relationship types and
directions, evidence expectations and states, diagnostic codes and severities,
provenance, and normalized source locations available in the shared traceability
model. Results shall distinguish absent evidence, failed evidence, unresolved
relationships, and analysis failures.

### REQ-0155 Deterministic bounded responses @evidence(implementation,test)
For an unchanged analysis revision and identical arguments, MCP tools and
resources shall return deterministically ordered semantic content. Operations
that can return unbounded collections shall accept a caller-controlled limit up
to a documented maximum and a continuation cursor or shall explicitly report
that the result was truncated; truncation shall not be presented as a complete
result.

### REQ-0156 Actionable MCP failures @evidence(implementation,test)
Malformed tool arguments and unsupported protocol operations shall use the error
form required by the advertised MCP revision. Valid tool calls that cannot
complete because of project configuration, input, lookup, or analysis problems
shall return actionable tool execution errors with stable mcutrace diagnostic or
error codes where available. One failed request shall not terminate the server or
corrupt later requests.

## Analysis lifetime, safety, and delivery

### REQ-0157 Analysis freshness @evidence(implementation,test)
Before serving project data, the MCP server shall detect changes to configured
inputs and refresh the shared analysis snapshot when necessary. Every project
resource and successful tool result shall identify the snapshot through a stable
analysis revision so a caller can detect that related calls observed different
project states.

### REQ-0158 Project access confinement @evidence(implementation,test)
The MCP server shall obtain project content only from its selected configuration
and the inputs resolved by that configuration. Tool arguments shall not provide a
general-purpose facility for reading arbitrary filesystem paths, and invalid or
out-of-scope path filters shall be rejected without disclosing unrelated file
content.

### REQ-0159 MCP request isolation @evidence(implementation,test)
The result of a request shall depend only on its arguments, the selected project
configuration, the advertised protocol contract, and the identified analysis
snapshot. Cancellation, malformed input, or failure of one request shall not
partially alter shared semantic state observed by later requests.

### REQ-0160 MCP verification coverage @evidence(test,build)
Automated tests shall cover protocol framing, protocol revision handling,
capability and catalog discovery, every tool and resource, structured-schema
conformance, deterministic ordering, bounds and cursors, refresh behavior,
configuration and analysis failures, path confinement, cancellation or interrupted
input, and continued operation after recoverable errors. At least one integration
test shall launch `mcutrace-mcp` as a subprocess and exercise it through its stdio
protocol boundary against a fixture project.

### REQ-0161 MCP packaging and portability @evidence(build)
The normal mcutrace build and supported release packaging shall produce
`mcutrace-mcp` alongside the CLI and language-server executables on every host
platform supported by mcutrace. The executable shall report the mcutrace version,
supported MCP protocol revision, and MCP result schema version through its
protocol-visible server metadata or reference resources.
