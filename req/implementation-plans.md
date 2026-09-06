# mcutrace Implementation Plan Requirements

### REQ-0137 Annotated implementation plans @evidence(implementation,test)
mcutrace shall discover implementation plan items in configured Markdown plan
documents. A heading containing a canonical `PLN-NNNN` identifier, where `NNNN`
is a four-digit decimal number, shall define a plan item. Its body shall extend
to the next heading of equal or higher level. Each item shall retain its ID,
title, Markdown body, and source location.

Each plan item shall declare one or more requirement links using
`@link(REQ-NNNN, ...)` and its status using `@status(...)`, accepting `planned`,
`in_progress`, `blocked`, and `done`. Both annotations shall be accepted anywhere
in the plan item block, including its heading and body. Requirement links shall
accept comma-separated identifiers with optional whitespace and shall deduplicate
repeated references. Ordinary requirement-ID mentions shall not create links.
Malformed annotations, missing links or status, conflicting statuses, duplicate
plan IDs, and unresolved requirement links shall produce diagnostics with stable
codes and relevant source locations.

mcutrace shall represent plan items and their explicit requirement links in the
shared traceability model. CLI reports, machine-readable output, and the language
server's Trace view shall expose linked plan items, their statuses, each plan
item's linked requirements, and navigation
locations alongside a requirement's existing evidence. Items in `planned`,
`in_progress`, or `blocked` status shall appear as outstanding work even when
existing tests pass. A plan link or `done` status shall not satisfy an evidence
expectation or establish that an implementation has been verified.

The Markdown plan shall be the persistent record of planned work and progress;
analysis shall not require a companion state file. Links shall be declared only
in plan documents, with reverse navigation derived by mcutrace. Requirement
documents shall not need references to individual plan items or plan metadata.
