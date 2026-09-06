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

Each platform-specific extension package includes `mcutrace-lsp` and starts that
bundled server automatically. To use a development build or another server version,
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

## Development

Open this directory in VS Code and press `F5` to launch an Extension Development Host, or run:

```sh
cmake --build --preset host --target mcutrace-lsp
npm run stage-server
npm test
```
