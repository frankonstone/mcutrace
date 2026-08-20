# mcutrace Requirements for VS Code

This extension indexes mcutrace Markdown requirements and adds editor support for canonical `REQ-NNNN` references.

## Features

- Hover over a requirement tag to see its title, body, and definition location.
- Cmd-click on macOS or Ctrl-click on Windows/Linux to open its Markdown definition.
- Use **Go to Definition** or `F12` as an alternative.
- Use **Find All References** or Shift+F12 to list every matching `REQ-NNNN` reference in workspace files.
- Duplicate requirement definitions are shown together with a warning.
- Requirement documents and unsaved Markdown edits are re-indexed automatically.
- Show linked implementations, tests, coverage, and static-analysis findings in a hover when a trace report is available.

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

## Trace evidence in hovers

Generate test, coverage, static-analysis, and trace reports with:

```sh
./make.sh
```

The extension watches `build/mcutrace-report.json` by default. Set `mcutrace.traceReport` if your report lives elsewhere, or run **mcutrace: Refresh Trace Data** after updating it. The hover groups direct test evidence with coverage and static-analysis results associated with a requirement's implemented source file. Coverage rows show the covered/total probe count and percentage for each reported source-file variant.

## Development

Open this directory in VS Code and press `F5` to launch an Extension Development Host, or run:

```sh
npm test
```
