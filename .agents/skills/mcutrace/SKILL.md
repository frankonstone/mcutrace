---
name: mcutrace
description: Create a new mcutrace project, add mcutrace to an existing project, or use the configured mcutrace MCP server for work involving requirement IDs (`REQ-NNNN`), plan IDs (`PLN-NNNN`), trace evidence, dependencies, change impact, or validation diagnostics. Apply it for mcutrace setup and traced work. Do not select it for unrelated project work merely because mcutrace.toml exists.
metadata:
  author: mcutrace
  version: "1"
---

<!-- // @req-file REQ-0162 REQ-0163 REQ-0164 REQ-0165 REQ-0166 REQ-0167 REQ-0168 REQ-0169 REQ-0170 REQ-0171 REQ-0172 REQ-0173 REQ-0174 REQ-0175 REQ-0176 REQ-0177 REQ-0223 REQ-0224 -->

# mcutrace

Use mcutrace to establish a project's traceability foundation and as the source
of truth for an enabled project's analyzed graph. The server is a read-only
context service; ordinary project tools remain responsible for authorized edits,
builds, tests, and evidence generation.

Setup requires ordinary local project tools and available mcutrace executables.
Analyzed project workflows require an accessible read-only mcutrace MCP server.

## Choose the operating mode

- For a request to create a new mcutrace project or add mcutrace to an existing
  project, read [references/project-setup.md](references/project-setup.md) and
  follow its setup workflow. An absent MCP connection is expected during
  bootstrap; once the server is connected, continue with the orientation below.
- For an already-enabled project, begin with MCP orientation.

Do not turn setup into permission to download software, modify global client
configuration, choose an application architecture, invent product requirements,
or select evidence policy. Make those changes only when the request authorizes
them; obtain a missing choice when it would materially change the project.

## Establish the project context

On the first mcutrace task in a connection:

1. Read `mcutrace://rules` before relying on annotation or relationship
   semantics.
2. Read `mcutrace://project` or call `project_overview` to confirm the selected
   configuration, project root, loading state, summary, and
   `analysis_revision`.
3. Read `mcutrace://schema` only when the result schema or vocabulary is unclear.

Treat the configured MCP server as authoritative for the graph, validation
state, supported syntax, and runtime rules. Do not reconstruct those semantics
by parsing requirement Markdown or generated reports. Project files may still
be read to make an authorized edit and to learn the project's writing
conventions.

If the project cannot load, the server is unavailable, or a needed capability
is absent, identify that condition and give an actionable connection,
configuration, or server-version next step. Do not replace missing MCP context
with a guessed trace analysis. Continue with other requested work only while
making the missing trace context explicit.

## Choose the smallest useful query

Use the tool schemas advertised by the running server rather than assuming
argument details.

| Need | Query |
| --- | --- |
| Start without an identifier or inspect project health | `project_overview` |
| Find requirement or plan IDs from text or filtered state | `search` |
| Inspect one requirement and its direct evidence and links | `requirement` |
| Understand prerequisite-sensitive work | `requirement_context` |
| Assess what a changed requirement can affect | `change_impact` |
| Inspect an explicitly linked implementation plan | `plan` |
| Review project-wide or focused diagnostics | `validate` |

Prefer a known canonical ID over broad search. Use filters and a practical limit
for collection queries, and follow continuation cursors only while more results
are relevant. Preserve whether a result is filtered, complete, or truncated;
never describe a partial page as the complete graph.

Before implementing or reviewing a requirement, inspect `requirement`, then use
`requirement_context` when prerequisites can affect its meaning. Before changing
requirement meaning or relationships, use `change_impact` to inspect direct and
transitive dependents. Fetch linked plans with `plan` when their scope or status
matters.

When combining calls, compare every `analysis_revision`. If it changes in a way
that can affect the task, refresh the affected queries and do not merge old and
new snapshots into one conclusion.

## Preserve mcutrace semantics

- Keep absent evidence distinct from evidence that ran and failed.
- Keep implementation, test, coverage, build, and analysis evidence classes
  distinct, including their freshness and result states.
- Keep prerequisites distinct from dependents, direct relationships from
  transitive reachability, and resolved relationships from unresolved ones.
- Treat diagnostics as validation findings, not automatic proof that product
  behavior is incorrect.
- Treat plan links and plan status as planning metadata, never as implementation
  or verification evidence.
- Create relationships only through syntax supported by `mcutrace://rules`.
  Preserve canonical IDs and do not infer links from proximity or prose.

## Make only authorized project changes

MCP calls are read-only and grant no permission to edit files, regenerate
artifacts, run evidence producers, or expand the task. Use normal project tools
for those actions only when the user's request authorizes them.

For authorized requirement or plan edits, first inspect neighboring project
content and explicit user instructions. Follow the project's existing structure,
prose, status usage, and level of detail. Add only the identifiers, annotations,
and explicit relationships required by the running rules; do not impose a
template, user-story form, heading hierarchy, or planning method.

## Validate and hand off

For work that can affect traced entities, call `validate` with the narrowest
useful filters before reporting completion. Explain relevant diagnostics without
collapsing their distinct meanings.

State which builds, tests, coverage runs, static analysis, or other evidence
producers were actually run. If evidence artifacts were not regenerated or may
be stale after the change, say so explicitly. A validation query reads current
artifacts; it does not execute or refresh their producers.
