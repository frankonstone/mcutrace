# mcutrace Source Annotation Requirements

These requirements define source-local implementation traceability.

### REQ-0089 Source requirement annotations @evidence(implementation,test)
mcutrace shall discover canonical `REQ-NNNN` references from explicitly marked source-code comments and create first-class implementation nodes with typed `implements` relationships to those requirements.

### REQ-0090 File-scoped source annotations @evidence(implementation,test)
mcutrace shall support `@req-file REQ-NNNN ...` in source-code comments as an explicit fallback that creates a file-scoped implementation node for the complete source or header file.

### REQ-0091 Declaration-scoped source annotations @evidence(implementation,test)
mcutrace shall support `@req REQ-NNNN ...` immediately preceding a class, struct, enum, function, or method declaration or definition and shall create a distinct implementation node for that declaration. Unsupported targets shall be diagnosed rather than silently treated as file-scoped links.

### REQ-0092 Source annotation provenance @evidence(implementation,test)
Relationships created from source annotations shall retain the source file and annotation line and, when identifiable, the declaration scope kind and symbol name.

### REQ-0093 Source input configuration @evidence(implementation,test)
mcutrace shall accept annotated source and header files through project configuration and through repeatable explicit CLI source inputs without treating source files as JSON producer artifacts.

For example, configure source inputs in TOML under `[project]`:

```toml
[project]
sources = ["src/controller.cpp", "include/controller.hpp"]
```

Or provide them directly when validating, repeating `--source` for each file:

```sh
mcutrace validate --source src/controller.cpp --source include/controller.hpp
```

### REQ-0095 Stable implementation identity @evidence(implementation,test)
Every implementation node created from a source annotation shall have a deterministic identity derived from its normalized source path and its file or declaration scope. Overloaded declarations in one source file shall receive distinct identities.

### REQ-0096 Implementation evidence semantics @evidence(implementation,test)
Implementation evidence shall satisfy a requirement only through an `implements` relationship from a first-class implementation node; an arbitrary relationship to a source artifact shall not satisfy the implementation expectation.

### REQ-0097 Implementation trace output @evidence(implementation,test)
Human-readable and machine-readable reports shall expose implementation links. Machine-readable relationship output shall include available provenance, including source path, annotation line, scope kind, and symbol name.

### REQ-0192 Language-aware source annotation architecture @evidence(implementation,test) @depends(REQ-0089, REQ-0090, REQ-0091)
mcutrace shall separate language-neutral annotation-to-model behavior from
language-specific comment and declaration recognition so that a new source
language can be added without changing implementation-node, relationship,
provenance, or diagnostic semantics shared by existing languages.

### REQ-0193 Deterministic source-language selection @evidence(implementation,test) @depends(REQ-0093, REQ-0192)
mcutrace shall select a declaration scanner deterministically from the normalized
source path. Existing language-neutral file-scoped annotation behavior shall remain
available for configured source inputs, while a declaration-scoped annotation in a
file without a supported scanner shall produce a stable diagnostic rather than be
interpreted using another language's grammar.

### REQ-0194 Backward-compatible C++ source scanning @evidence(implementation,test) @depends(REQ-0091, REQ-0092, REQ-0095, REQ-0192)
Refactoring source annotation handling into language-specific scanners shall
preserve the supported C++ source and header extensions, declaration targets,
implementation identities, provenance, and diagnostics for existing project
inputs.

### REQ-0195 JavaScript source annotation support @evidence(implementation,test) @depends(REQ-0073, REQ-0089, REQ-0090, REQ-0192, REQ-0193)
mcutrace shall recognize source annotations in JavaScript `.js`, `.mjs`, and
`.cjs` files without executing the source, launching a JavaScript runtime, or
introducing a third-party host parsing dependency.

### REQ-0196 JavaScript declaration targets @evidence(implementation,test) @depends(REQ-0091, REQ-0195)
A JavaScript `@req` annotation shall bind to the immediately following named
function declaration, class declaration, class method, or variable declaration
initialized with a function or arrow expression, including supported `export`,
`default`, `async`, generator, `static`, getter, and setter modifiers. Targets
without a deterministic declaration name shall be diagnosed rather than silently
treated as file-scoped links.

### REQ-0197 JavaScript lexical correctness @evidence(implementation,test) @depends(REQ-0089, REQ-0195)
The JavaScript scanner shall recognize annotations only in line or block comments
and shall not interpret marker-like text in quoted strings, template literals, or
regular-expression literals as source annotations. Escapes, multiline constructs,
and comments between an annotation and its declaration shall not change the
selected target.

### REQ-0198 JavaScript implementation identity and provenance @evidence(implementation,test) @depends(REQ-0092, REQ-0095, REQ-0196)
JavaScript implementation nodes shall have deterministic identities that include
the normalized path and sufficient lexical declaration scope to distinguish
same-named methods or bindings in different containers. Their implementation
relationships shall retain the annotation location, declaration kind, and symbol
name.

### REQ-0199 Mixed-language source analysis @evidence(implementation,test) @depends(REQ-0005, REQ-0193, REQ-0194, REQ-0195, REQ-0198)
A project containing supported C++ and JavaScript source inputs shall produce one
deterministically ordered trace graph. Syntax, scanner state, and diagnostics from
one input language shall not alter the targets, identities, or diagnostics emitted
for another input language.

### REQ-0225 Tcl file annotation scanning @evidence(implementation,test) @depends(REQ-0192, REQ-0193)
mcutrace shall recognize `.tcl` source inputs and file-scoped `# @req-file`
annotations in outer-script comments. Quoted/braced data, escaped separators,
variable indexes, command substitutions and deferred procedure bodies shall not
create outer-script claims. Unsupported declaration targets, unbalanced input
and excessive nesting shall be diagnosed without executing Tcl.

### REQ-0226 Tcl test source declarations @evidence(implementation,test) @depends(REQ-0090, REQ-0093, REQ-0225)
mcutrace shall accept Tcl `test_sources` and repeatable `--test-source` inputs.
Their annotations shall declare first-class tests and `verifies` relationships,
with normalized script identity and annotation provenance, independently of
production Tcl implementation claims. A declaration alone shall not claim a
passing execution or production-source coverage.

### REQ-0227 Tcl execution evidence and source freshness @evidence(implementation,test) @depends(REQ-0226)
mcutrace shall join Tcl-tagged mcutest results to configured test declarations by
normalized file identity. Only completed results with positive check counts and
matching content fingerprints may establish passing verification. Missing,
skipped, incomplete, failed, stale, undeclared and conflicting results shall
remain distinguishable; stale or conflicting results shall not pass validation.
CLI, LSP and MCP shall use the same reconciliation and validation behavior.
