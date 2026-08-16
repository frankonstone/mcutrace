# mcutrace Explicit Link Requirements

These requirements extend the core set in `docs/requirements.md` for the P9.1 trace-link slice.

### REQ-0079 Explicit evidence requirement references
mcutrace shall support explicit references from imported evidence to canonical `REQ-NNNN` requirement identifiers without inferring relationships from names or paths.

### REQ-0080 Producer requirement references
When a supported producer entry contains a `requirements` array, mcutrace shall convert valid requirement identifiers into typed relationships appropriate to that evidence kind and shall diagnose invalid requirement identifiers.

### REQ-0081 Trace link sidecar
mcutrace shall support a versioned producer-neutral `mcutrace-links` JSON artifact containing explicit source node ID, target node ID, and relationship type entries.

### REQ-0082 Dangling explicit links
mcutrace shall preserve explicit links whose endpoints are not present in the imported fragments so the validation engine can diagnose dangling references.
