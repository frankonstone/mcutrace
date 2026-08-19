# mcutrace Requirements for VS Code

This extension indexes mcutrace Markdown requirements and adds editor support for canonical `REQ-NNNN` references.

## Features

- Hover over a requirement tag to see its title, body, and definition location.
- Cmd-click on macOS or Ctrl-click on Windows/Linux to open its Markdown definition.
- Use **Go to Definition** or `F12` as an alternative.
- Duplicate requirement definitions are shown together with a warning.
- Requirement documents and unsaved Markdown edits are re-indexed automatically.

By default, the extension discovers `**/requirements*.md`, excluding `.git`, `build`, and `node_modules` directories. Customize discovery in workspace settings:

```json
{
  "mcutrace.requirementFiles": [
    "docs/requirements.md",
    "docs/requirements-*.md"
  ]
}
```

Run **mcutrace: Refresh Requirements** from the Command Palette to force a refresh.

## Development

Open this directory in VS Code and press `F5` to launch an Extension Development Host, or run:

```sh
npm test
```
