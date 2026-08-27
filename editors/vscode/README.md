# mcutrace Requirements for VS Code

This extension is a client for `mcutrace-lsp`. The language server owns parsing,
validation, diagnostics, navigation, and traceability insight, so VS Code and the
command-line tool use the same semantic implementation.

## Features

- Hover, Go to Definition, Find All References, implementation navigation, symbols,
  completion, rename, and CodeLens come from the language server.
- Duplicate requirement identifiers appear as server-produced warnings in **Problems**
  and are squiggled at every conflicting definition.
- Trace evidence and trace import or validation issues appear in the dedicated **Trace**
  activity-bar view. They do not appear in **Problems**. Requirement nodes retain the
  Markdown heading hierarchy and trace entries with source locations are navigable.
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

The server discovers requirements, sources, artifacts, and validation settings solely
from `mcutrace.toml`. Run **mcutrace: Refresh Requirements** from the Command
Palette or the Trace view title menu to request a fresh analysis.

## Development

Open this directory in VS Code and press `F5` to launch an Extension Development Host, or run:

```sh
cmake --build --preset host --target mcutrace-lsp
npm run stage-server
npm test
```
