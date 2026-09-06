# mcutrace MCP server

`mcutrace-mcp` gives local MCP clients read-only, structured access to one
mcutrace project. It is a standalone process and does not require VS Code or the
mcutrace extension.

## Build and launch

Build the normal CMake project, then launch the server with an explicit project
configuration:

```sh
cmake --build build/host --target mcutrace-mcp
build/host/mcutrace-mcp --config /absolute/path/to/mcutrace.toml
```

Without `--config`, the server selects `mcutrace.toml` in its launch working
directory. A generic local MCP client configuration looks like this:

```json
{
  "mcpServers": {
    "mcutrace": {
      "command": "/absolute/path/to/mcutrace-mcp",
      "args": ["--config", "/absolute/path/to/project/mcutrace.toml"]
    }
  }
}
```

The server uses newline-delimited JSON-RPC over stdio. Protocol output is written
only to stdout. It advertises MCP revision `2025-11-25` and also accepts clients
requesting revision `2025-06-18` during initialization.

## Tools

The tool list is deterministic. Each tool advertises JSON input and output
schemas and returns the same JSON object in `structuredContent` and in a text
content block for client compatibility.

| Tool | Use |
| --- | --- |
| `project_overview` | Start without a known ID; inspect inputs, graph counts, validation totals, untraced requirements, and outstanding plans. |
| `search` | Find requirements or plans by ID or text and filter by kind, status, evidence state, or diagnostic. |
| `requirement` | Inspect one requirement and its direct trace evidence, dependencies, dependents, plans, provenance, and diagnostics. |
| `requirement_context` | Retrieve one requirement and its cycle-safe transitive prerequisite context. |
| `change_impact` | Retrieve cycle-safe direct and transitive dependents of a requirement. |
| `plan` | Inspect one plan item, its status, complete body, and explicit requirement links. |
| `validate` | Retrieve complete or filtered validation diagnostics. |

Search and validation collections accept `limit` values from 1 to 100 and an
opaque decimal `cursor` returned as `next_cursor`. Their results state whether
they are filtered, complete, or truncated.

## Resources

- `mcutrace://rules` describes the annotations, relationship meanings, evidence
  independence, plan semantics, and non-orchestration boundary implemented by the
  running server.
- `mcutrace://schema` describes MCP result schema version 1 and the node,
  relationship, evidence, and plan-status vocabularies.
- `mcutrace://project` reports the selected configuration, resolved inputs,
  validation summary, and current analysis revision.

## Result envelope and freshness

Every successful tool result has this envelope:

```json
{
  "schema_version": 1,
  "analysis_revision": "deterministic-snapshot-id",
  "data": {},
  "diagnostics": []
}
```

The server reloads the selected configuration and its resolved inputs before
serving project data. An unchanged input set produces the same
`analysis_revision`; changed content produces a different revision. Agents should
compare revisions when combining results from multiple calls.

The server preserves mcutrace IDs, relationship direction, evidence state,
diagnostic codes, provenance, and normalized source locations. Missing evidence,
failed evidence, unresolved relationships, findings, and analysis failures remain
distinct states. A plan link or `done` status never becomes evidence.

## Read-only and path behavior

MCP calls cannot edit files or run build, test, coverage, or analysis tools. The
server reads only the configuration selected at launch and the inputs resolved by
that configuration. The validation path filter accepts configured source files;
it is not a general filesystem-reading interface.

## Troubleshooting

- `project.cannot_read` means the selected configuration or one of its resolved
  inputs is unavailable. Prefer absolute paths in client launch configuration.
- `project.configuration` means `mcutrace.toml` could not be parsed or validated.
- `mcp.invalid_arguments` identifies a tool argument that does not satisfy the
  documented contract. Correct the argument and retry.
- Unknown requirement and plan IDs are tool execution errors and do not terminate
  the server.
- If a client reports invalid protocol output, verify that no wrapper writes logs
  to stdout; diagnostics belong on stderr.
- Use `mcutrace-mcp --version` to inspect the binary, MCP revision, and result
  schema version without starting protocol mode.
