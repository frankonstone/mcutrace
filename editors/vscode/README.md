# mcutrace Requirements for VS Code

This extension is a client for `mcutrace-lsp`. The language server owns parsing,
validation, diagnostics, navigation, and traceability insight, so VS Code and the
command-line tool use the same semantic implementation.

## Features

- Hover, Go to Definition, Find All References, implementation navigation, symbols,
  completion, rename, and CodeLens come from the language server.
- Duplicate requirement identifiers appear as server-produced warnings in **Problems**
  and are squiggled at every conflicting definition.
- Trace evidence appears in the dedicated **Trace** activity-bar view. Trace import or
  validation issues do not appear as entries in **Problems** or as separate tree nodes.
  Instead, affected items show colored problem icons and neutral `E`, `W`, or `I`
  badges with explanatory tooltips. Ancestors show neutral badges summarizing problems
  inside them; titles keep their normal text color.
- Documents are grouped in collapsible folders, with filenames shown inside each folder.
  Requirements show their title first and their ID as secondary text. Markdown sections
  and requirement order are preserved, and expanded branches survive analysis refreshes.
  Trace entries with source locations are navigable.
- Requirement nodes include navigable **Depends on**, **Required by**, and
  **Implementation plans** groups. Configured plan documents also appear in the
  tree with their current status, and refresh after saved or in-memory edits.
- Use **Find Requirement** in the Trace title bar to search all requirements by title or
  ID, including collapsed branches. **Show Trace Problems Only** filters the tree to
  affected items and their ancestors. **Reveal Active Requirement** in the view menu or
  Command Palette selects the requirement at the cursor in a requirements document.
  Finding or revealing a requirement clears the problems filter when needed to show it.
- Open documents, configured artifacts, and `mcutrace.toml` updates are synchronized
  with the server automatically.

Each platform-specific extension package includes `mcutrace-lsp`, `mcutrace-mcp`,
and the canonical mcutrace Agent Skill. The extension starts the bundled language
server automatically. To use a development build or another server version,
configure its workspace-relative path:

```json
{
  "mcutrace.languageServer.path": "${workspaceFolder}/build/trace/mcutrace-lsp"
}
```

The server discovers requirements, implementation plans, sources, artifacts, and validation settings solely
from `mcutrace.toml`. Run **mcutrace: Refresh Requirements** from the Command
Palette or the Trace view title menu to request a fresh analysis.
Refresh reloads the saved configuration and re-expands input patterns, so newly
configured documents and newly matching files appear without restarting the
language server. Watched-file changes use the same refresh behavior.

## Agent integration

In a trusted workspace folder containing `mcutrace.toml`, the extension contributes
the mcutrace skill to VS Code agents and registers an isolated stdio MCP definition.
VS Code owns process startup; the extension only provides a direct executable,
arguments, working directory, and versioned label. Multi-root workspaces receive one
definition per eligible folder, with resource-scoped settings and no shared project
state.

The defaults use the bundled `mcutrace-mcp` and
`${workspaceFolder}/mcutrace.toml`. These settings customize the integration:

```json
{
  "mcutrace.agent.enabled": true,
  "mcutrace.agent.skill.enabled": true,
  "mcutrace.agent.mcp.enabled": true,
  "mcutrace.agent.mcp.path": "${workspaceFolder}/build/trace/mcutrace-mcp",
  "mcutrace.agent.mcp.arguments": [],
  "mcutrace.agent.mcp.configPath": "${workspaceFolder}/mcutrace.toml"
}
```

Relative paths and `${workspaceFolder}` are resolved independently for each folder.
Settings, workspace-folder, trust, and selected configuration-file changes apply
without a window reload. Agent integration is disabled in untrusted workspaces and
does not automatically create or modify workspace files. Run **mcutrace: Show
Agent Integration Status** for resolved paths, enablement, trust, API availability,
and actionable configuration diagnostics.

To make the skill and its MCP dependency available to Codex desktop or Codex CLI,
run **mcutrace: Set Up Codex in Workspace** from the Command Palette. In a
multi-root workspace, select the target folder. The explicit command copies the
canonical skill to `.agents/skills/mcutrace`, stages the packaged `mcutrace-mcp`
executable in stable extension storage, and adds a project-scoped server definition
to `.codex/config.toml`. If the workspace has no default `mcutrace.toml`, it also
creates a minimal configuration and a neutral `requirements.md`; an existing
requirements document is preserved. It runs only in a trusted workspace, never
overwrites an existing skill or user-managed `mcutrace` server, and updates only
its own marked configuration block. A missing explicitly configured custom
`mcutrace.agent.mcp.configPath` remains an error. Start a new Codex session in that
folder after setup.

For a multi-root workspace, put folder-specific overrides in each folder's
`.vscode/settings.json`; equal folder names are disambiguated by their resolved
paths. Set `mcutrace.agent.enabled` to `false` to disable both integrations, or set
only `mcutrace.agent.skill.enabled` or `mcutrace.agent.mcp.enabled` to `false` to
keep the other component available. A missing configuration or executable is
reported only in the status output, so ordinary non-mcutrace folders remain quiet.

## Development

Open this directory in VS Code and press `F5` to launch an Extension Development Host, or run:

```sh
cmake --build --preset host --target mcutrace-lsp mcutrace-mcp
npm run stage-runtime
npm test
```
