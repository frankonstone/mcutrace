# mcutrace Evidence Expectation Requirements

### REQ-0084 Requirement evidence expectations
mcutrace shall allow a requirement heading to declare expected evidence classes using `@evidence(...)`, with `test`, `implementation`, `coverage`, and `build` as supported classes.

### REQ-0085 Default evidence expectations
A requirement without an explicit evidence annotation shall expect test evidence. Implementation, coverage, and build evidence shall be required only when explicitly named by the requirement.

### REQ-0086 No-evidence requirements
mcutrace shall support `@evidence(none)` for process or policy requirements for which trace evidence is intentionally not required.

### REQ-0087 Producer finding state preservation
mcutrace shall preserve a producer-reported static-analysis finding state and expose it in machine-readable trace output.

### REQ-0088 State-aware static-analysis validation
mcutrace shall treat active or incomplete static-analysis states such as `violation`, `unavailable`, `failed`, and `limited` as actionable findings, while retaining but not reporting `informational`, `suppressed`, `deviated`, and `baselined` findings as active validation warnings.

### REQ-0094 Clean static analysis @evidence(build)
The mcutrace project shall produce no active mcucheck violations under the configured project ruleset in its dogfood CI workflow.
