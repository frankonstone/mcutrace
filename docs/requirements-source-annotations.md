# mcutrace Source Annotation Requirements

These requirements define source-local implementation traceability.

### REQ-0089 Source requirement annotations
mcutrace shall discover canonical `REQ-NNNN` references from explicitly marked source-code comments and create `implements` relationships from the annotated source artifact to those requirements.

### REQ-0090 File-scoped source annotations
mcutrace shall support `@req-file REQ-NNNN ...` in source-code comments to associate one or more requirements with the complete source or header file.

### REQ-0091 Declaration-scoped source annotations
mcutrace shall support `@req REQ-NNNN ...` immediately preceding a class, struct, enum, function, or method declaration or definition and shall associate the requirements with that declaration scope. Unsupported targets shall be diagnosed rather than silently treated as file-scoped links.

### REQ-0092 Source annotation provenance
Relationships created from source annotations shall retain the source file and annotation line and, when identifiable, the declaration scope kind and symbol name.

### REQ-0093 Source input configuration
mcutrace shall accept annotated source and header files through project configuration and through repeatable explicit CLI source inputs without treating source files as JSON producer artifacts.
