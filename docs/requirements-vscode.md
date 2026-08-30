# mcutrace VS Code Language Client Requirements

### REQ-0098 Requirement hover @evidence(implementation)
The mcutrace VS Code extension shall obtain requirement hover information from the mcutrace language server rather than maintain a client-side requirement index.

### REQ-0099 Requirement navigation @evidence(implementation)
The mcutrace VS Code extension shall obtain native requirement definition navigation from the mcutrace language server.

### REQ-0100 Language-server workspace synchronization @evidence(implementation)
The mcutrace VS Code extension shall start the mcutrace language server, synchronize open documents and relevant workspace file changes, and allow the server to discover inputs from `mcutrace.toml`.

### REQ-0101 Duplicate requirement definitions @evidence(implementation)
When a requirement identifier has multiple definitions, the mcutrace VS Code extension shall surface the language server's warning diagnostics in Problems and its squiggles at every conflicting definition.

### REQ-0102 Requirement references @evidence(implementation)
The mcutrace VS Code extension shall obtain native Find All References navigation from the mcutrace language server.

### REQ-0124 Language-server launch configuration @evidence(implementation)
The mcutrace VS Code extension shall allow the language-server executable path and arguments to be configured, including `${workspaceFolder}` substitution.

### REQ-0127 Dedicated trace view @evidence(implementation)
The mcutrace VS Code extension shall display trace evidence in a dedicated Trace view, preserve the Markdown requirement heading hierarchy within each configured document, and navigate from trace items to available source locations. Trace import and validation problems shall not appear as separate entries in Problems or the Trace tree; affected nodes and their ancestors shall instead expose severity-colored text decorations, severity badges, and explanatory tooltips.

### REQ-0128 Test evidence navigation @evidence(implementation)
Clicking test evidence in the Trace view shall navigate to the producer-supplied source location. If an mcutest artifact does not contain source metadata, the extension shall resolve slash-separated and legacy dotted test names to the corresponding `TEST` declaration in the workspace.
