# mcutrace Evidence Expectation Requirements

### REQ-0084 Requirement evidence expectations
mcutrace shall allow a requirement heading to declare expected evidence classes using `@evidence(...)`, with `test`, `implementation`, `coverage`, and `build` as supported classes.

### REQ-0085 Default evidence expectations
A requirement without an explicit evidence annotation shall continue to expect test and implementation evidence.

### REQ-0086 No-evidence requirements
mcutrace shall support `@evidence(none)` for process or policy requirements for which trace evidence is intentionally not required.
