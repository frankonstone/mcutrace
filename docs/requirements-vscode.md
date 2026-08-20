# mcutrace VS Code Extension Requirements

### REQ-0098 Requirement hover @evidence(implementation)
The mcutrace VS Code extension shall display the title, body, and definition location of a canonical `REQ-NNNN` requirement when a user hovers over its reference.

### REQ-0099 Requirement navigation @evidence(implementation)
The mcutrace VS Code extension shall provide native Go to Definition navigation from a canonical `REQ-NNNN` reference to its Markdown heading.

### REQ-0100 Requirement document discovery @evidence(implementation)
The mcutrace VS Code extension shall discover requirement Markdown files through configurable workspace glob patterns and shall refresh its index when matching documents or settings change.

### REQ-0101 Duplicate requirement definitions @evidence(implementation)
When a requirement identifier has multiple definitions, the mcutrace VS Code extension shall warn in hover content and return every definition as a navigation target.

### REQ-0102 Requirement references @evidence(implementation)
The mcutrace VS Code extension shall provide native Find All References navigation from a canonical `REQ-NNNN` reference and return every matching canonical reference in workspace files.
