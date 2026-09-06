# mcutrace Requirement Dependency Requirements

These requirements add the optional `@depends(...)` annotation to the
existing requirement and evidence annotations. A dependency declares that one
requirement relies on the behavior defined by another requirement. It does not
prevent their simultaneous implementation or instruct mcutrace to execute work.

## Annotation and model

### REQ-0129 Requirement dependency annotation @evidence(implementation,test)
mcutrace shall recognize `@depends(REQ-NNNN)` anywhere in a requirement block,
including its heading and any position in its body as defined by REQ-0011,
with the same semantics regardless of placement, and
support multiple prerequisites as a comma-separated list, with optional whitespace
around identifiers and separators. The annotation shall coexist with `@evidence(...)`
in either order, shall not become part of the requirement title, and shall not be
required for requirements without declared dependencies. Repeated references to
the same prerequisite shall produce one logical dependency.

### REQ-0130 Explicit dependency relationships @evidence(implementation,test)
mcutrace shall represent each declared dependency as a typed `depends_on`
relationship directed from the dependent requirement to its prerequisite, retaining
the annotation's source location and provenance. Prerequisites may be defined in
any configured requirement document. Ordinary Markdown links and incidental
requirement-ID mentions shall not create dependency relationships.

## Validation and evidence semantics

### REQ-0131 Dependency validation @evidence(implementation,test)
mcutrace shall diagnose malformed dependency annotations, including empty lists
and invalid requirement identifiers, unresolved prerequisite identifiers,
self-dependencies, and dependency cycles. Diagnostics shall have stable codes
and relevant source locations; cycle diagnostics shall identify the involved
requirements. Valid references shall be retained even when a prerequisite is
unresolved or another reference is malformed, and invalid dependency input shall
not prevent analysis of unaffected requirements.

### REQ-0132 Dependency evidence independence @evidence(implementation,test)
mcutrace shall evaluate each requirement's expected evidence independently of its
dependencies. A dependency shall neither satisfy an evidence expectation nor copy
a prerequisite's evidence or failure state onto the dependent requirement. Missing
or failed expected evidence on a prerequisite shall be exposed as prerequisite
information, identifying the prerequisite and the relevant evidence class or
failure. Dependency relationships shall indicate potential change impact without
asserting that a dependent requirement is broken or that implementation must stop.

## Reports and agent context

### REQ-0133 Dependency trace reports @evidence(implementation,test)
mcutrace shall include direct prerequisites and direct dependents in the
human-readable and JSON output of `show REQ-NNNN`, identifying each available
requirement by ID, title, and definition location. Reports shall distinguish
`Depends on` from `Required by`, expose prerequisite evidence information as
specified by REQ-0132, and retain unresolved references with their diagnostics.
The complete JSON trace output shall preserve typed dependency relationships and
their provenance. Dependency lists shall use deterministic requirement-ID order.

### REQ-0134 Requirement context output @evidence(implementation,test)
mcutrace shall provide a CLI requirement-context query with human-readable and
JSON output containing the requested requirement and its direct and transitive
prerequisites. For each resolved requirement, the output shall include its full
title and body, definition location, expected evidence classes, available
implementation and evidence links, and applicable diagnostics. The output shall
preserve which requirements depend on which prerequisites, include each resolved
requirement once in deterministic order, report unresolved references, and
terminate even when the dependency graph contains cycles.

## Editor integration

### REQ-0135 Dependency editor support @evidence(implementation,test)
The language server shall support dependency identifiers in hover, Go to
Definition, Find All References, completion, and safe requirement rename, and
shall offer `@depends(...)` as a supported annotation throughout the requirement
block, including its heading and body. These operations
shall use the shared semantic core and the latest in-memory requirement text.
Malformed dependency annotations shall be exposed through requirement-parser
diagnostics; unresolved prerequisites, self-dependencies, and cycles shall be
exposed through the trace-validation data channel.

### REQ-0136 Dependency Trace view @evidence(implementation,test)
The VS Code Trace view shall expose navigable `Depends on` and `Required by`
groups for requirements with corresponding relationships, showing direct
prerequisites and direct dependents supplied by the language server. Prerequisite
items shall explain missing or failed expected evidence and dependency validation
issues using the existing trace problem icons, badges, and tooltips. Navigating to
a related requirement shall allow the developer to inspect its own dependencies
without recursively expanding the graph or duplicating prerequisite evidence as
the dependent requirement's own evidence. The view shall refresh dependency
information after requirement edits and evidence updates.
