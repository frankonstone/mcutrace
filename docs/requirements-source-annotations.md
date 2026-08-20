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
