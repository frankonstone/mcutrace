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
