# mcutrace Language Server Requirements

These requirements define the language-server interface for mcutrace. The server
shall expose the same requirement and traceability semantics used by the command-line
tool while serving editor clients through the Language Server Protocol (LSP).

## Core ownership

### REQ-0103 Language Server Protocol transport
mcutrace shall provide a language server that communicates with editor clients over the standard input/output JSON-RPC transport defined by LSP.

### REQ-0104 Shared semantic core
The language server and command-line tool shall use the same mcutrace requirement parser, traceability model, and validation logic; the language server shall not reimplement their semantic rules in a client-specific language.

### REQ-0105 Workspace configuration
The language server shall discover requirement documents, source inputs, artifact inputs, and validation policy from the workspace's mcutrace configuration.

### REQ-0106 Unsaved document analysis
The language server shall analyze the latest in-memory text supplied by an editor for open requirement documents without requiring that document to be saved.

### REQ-0107 Incremental analysis
The language server shall retain the latest document version supplied by the editor and publish diagnostics only for that version.

## Diagnostics

### REQ-0108 Requirement parse diagnostics
The language server shall publish mcutrace requirement-parser diagnostics to the editor Problems view with their severity, stable diagnostic code, message, and source range.

### REQ-0109 Duplicate requirement diagnostics
For a duplicated requirement identifier, the language server shall publish a warning at every conflicting definition and include the other definition locations as related diagnostic information.

### REQ-0110 Traceability validation diagnostics
When all required workspace inputs are available, the language server shall publish applicable traceability-validation diagnostics at their relevant source locations.

### REQ-0111 Artifact refresh
The language server shall refresh traceability analysis when an editor client reports that a configured artifact input changed, without requiring an editor restart.

## Editor navigation and insight

### REQ-0112 Requirement hover
The language server shall provide hover information for a canonical requirement identifier, including the requirement title, body, definition location, and available trace evidence.

### REQ-0113 Requirement definition navigation
The language server shall provide Go to Definition navigation from a canonical requirement identifier to every matching requirement definition.

### REQ-0114 Requirement reference navigation
The language server shall provide Find All References navigation for canonical requirement identifiers across configured requirement documents, source annotations, and supported evidence inputs.

### REQ-0115 Requirement implementation navigation
The language server shall provide Go to Implementation navigation from a requirement to the annotated source declarations and files that implement it.

### REQ-0116 Requirement symbols
The language server shall expose requirement identifiers and titles as document and workspace symbols.

### REQ-0117 Requirement completion
The language server shall complete known canonical requirement identifiers where requirement references are valid and shall complete supported requirement-heading annotations.

### REQ-0118 Traceability CodeLens
The language server shall provide an editor CodeLens for a requirement definition that summarizes its available implementations, tests, coverage, and findings when that evidence is available.

## Safe editing support

### REQ-0119 Requirement rename
The language server shall provide a Rename operation for a canonical requirement identifier that updates every unambiguous reference in the configured workspace.

### REQ-0120 Rename conflict prevention
The language server shall reject a requirement rename that would produce an invalid identifier or duplicate an existing requirement identifier.

### REQ-0121 Requirement code actions
The language server shall offer code actions for diagnostics only when it can construct a deterministic, non-destructive workspace edit or command to address the diagnosed condition.

## Consistency and resilience

### REQ-0122 Diagnostic consistency
For the same workspace inputs, the language server and the command-line tool shall apply equivalent mcutrace diagnostic rules and publish equivalent diagnostic codes and severities.

### REQ-0123 Analysis failure isolation
The language server shall report configuration, input, and analysis failures to the editor without terminating the language-server process or preventing analysis of unaffected workspace documents.
