# mcutrace VS Code Language Client Requirements

### REQ-0098 Requirement hover @evidence(implementation)
The mcutrace VS Code extension shall obtain requirement hover information from the mcutrace language server rather than maintain a client-side requirement index.

### REQ-0099 Requirement navigation @evidence(implementation)
The mcutrace VS Code extension shall obtain native requirement definition navigation from the mcutrace language server.

### REQ-0100 Language-server workspace synchronization @evidence(implementation)
The mcutrace VS Code extension shall start the mcutrace language server, @depends(REQ-0099)synchronize open documents and relevant workspace file changes, and allow the server to discover inputs from `mcutrace.toml`.

### REQ-0101 Duplicate requirement definitions @evidence(implementation)
When a requirement identifier has multiple definitions, the mcutrace VS Code extension shall surface the language server's warning diagnostics in Problems and its squiggles at every conflicting definition.

### REQ-0102 Requirement references @evidence(implementation)
The mcutrace VS Code extension shall obtain native Find All References navigation from the mcutrace language server.

### REQ-0124 Language-server launch configuration @evidence(implementation)
The mcutrace VS Code extension shall allow the language-server executable path and arguments to be configured, including `${workspaceFolder}` substitution.

### REQ-0127 Dedicated trace view @evidence(implementation)
The mcutrace VS Code extension shall display trace evidence in a dedicated Trace view, group documents in collapsible folders, show requirement titles before their identifiers, preserve the Markdown requirement heading hierarchy within each configured document, retain expansion state across analysis refreshes, and navigate from trace items to available source locations. Trace import and validation problems shall not appear as separate entries in Problems or the Trace tree; directly affected nodes shall instead expose severity-colored icons, neutral severity badges, and explanatory tooltips. Ancestors shall summarize descendant problems with neutral badges and tooltips while labels retain their normal text color. The view shall support finding requirements by title or identifier, filtering to items with trace problems and their ancestors, and revealing the requirement at the active editor cursor.

### REQ-0128 Test evidence navigation @evidence(implementation)
Clicking test evidence in the Trace view shall navigate to the producer-supplied source location. If an mcutest artifact does not contain source metadata, the extension shall resolve slash-separated and legacy dotted test names to the corresponding `TEST` declaration in the workspace.

### REQ-0220 Stable Trace view refresh @evidence(implementation,test) @depends(REQ-0100, REQ-0127)
The VS Code extension shall coalesce a burst of trace publications into one Trace
view update that applies the newest complete payload, shall suppress a publication
identical to the last rendered payload, and shall refresh file decorations only
when their visible values change. Refresh optimization shall not lose the final
analysis state, delay an update indefinitely, or change stable tree identities,
problem filtering, navigation, badges, or tooltips.

### REQ-0221 Workspace Agent Skill installation @evidence(implementation,test) @depends(REQ-0179, REQ-0187, REQ-0190)
The VS Code extension shall provide an explicit command that installs its bundled
canonical mcutrace Agent Skill into `.agents/skills/mcutrace` in a selected trusted
workspace folder so Codex sessions outside VS Code can discover it. The command
shall support multi-root folder selection, shall not overwrite an existing skill,
and shall report successful installation, existing content, missing workspace or
trust, and copy failures with an actionable message.

### REQ-0222 Combined Codex workspace setup @evidence(implementation,test) @depends(REQ-0142, REQ-0182, REQ-0187, REQ-0190, REQ-0221)
The VS Code workspace setup command shall also make the packaged `mcutrace-mcp`
runtime available from a stable extension storage location and add a project-scoped
`mcutrace` STDIO server to `.codex/config.toml` using the selected folder's resolved
configuration and working directory. It shall update only a clearly marked block
that it previously generated, preserve unrelated configuration and an existing
user-managed `mcutrace` server, and report the resulting skill and MCP states. A
missing project configuration, packaged runtime, trust decision, or writable Codex
configuration shall fail with an actionable diagnostic rather than leave a claimed
complete setup.
