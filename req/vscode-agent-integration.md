# mcutrace VS Code Agent Integration Requirements

The mcutrace VS Code extension shall make the portable mcutrace agent skill and
the read-only MCP server available to VS Code agents with minimal project setup.
The extension owns VS Code-specific contribution, configuration, lifecycle, and
packaging behavior; the canonical skill and MCP server remain independently
usable integrations.

Workspace settings are the user-facing configuration surface. The integration
shall use supported VS Code skill and MCP extension APIs rather than generating
agent configuration files in the workspace.

## Integration and contribution

### REQ-0178 Integrated VS Code agent experience @evidence(implementation,test) @depends(REQ-0138, REQ-0162)
The mcutrace VS Code extension shall make both the canonical mcutrace agent skill
and a project-scoped `mcutrace-mcp` server available to VS Code agents. Installing
the extension shall be sufficient to provide these components for an eligible
mcutrace workspace, subject to the user's settings and VS Code trust and access
controls.

### REQ-0179 Canonical skill contribution @evidence(implementation,test,build) @depends(REQ-0162, REQ-0176, REQ-0177)
The extension shall contribute the canonical mcutrace skill through the supported
VS Code Agent Skills contribution point. The packaged skill shall be produced
from the canonical skill source without a separately maintained or semantically
divergent VS Code copy, and its availability shall respect the mcutrace agent and
skill enablement settings.

### REQ-0180 MCP server definition provider @evidence(implementation,test) @depends(REQ-0138, REQ-0140, REQ-0142)
The extension shall register a supported VS Code MCP server definition provider
that supplies a stdio definition for each eligible workspace folder. The
definition shall launch `mcutrace-mcp` directly without a shell, select that
folder's configuration explicitly, set that folder as the working directory,
and expose stable labels and version metadata suitable for VS Code lifecycle and
tool refresh behavior.

## Workspace configuration and defaults

### REQ-0181 Agent integration settings @evidence(implementation,test)
The extension shall contribute documented settings usable from
`.vscode/settings.json` for `mcutrace.agent.enabled`,
`mcutrace.agent.skill.enabled`, `mcutrace.agent.mcp.enabled`,
`mcutrace.agent.mcp.path`, `mcutrace.agent.mcp.arguments`, and
`mcutrace.agent.mcp.configPath`. The enablement settings shall be booleans; the
path and configuration settings shall be strings; and additional arguments shall
be an array of strings.

### REQ-0182 Eligible zero-configuration defaults @evidence(implementation,test) @depends(REQ-0179, REQ-0180, REQ-0181)
Agent integration, the skill contribution, and MCP registration shall be enabled
by default. With their default values, a workspace folder containing
`mcutrace.toml` shall use the skill and bundled MCP executable without requiring
the user to copy a skill, create an MCP configuration file, or add workspace
settings. A folder without the resolved configuration shall not receive a
project MCP definition or repeated startup error notifications.

### REQ-0183 MCP executable and project resolution @evidence(implementation,test) @depends(REQ-0142, REQ-0161, REQ-0181)
An empty `mcutrace.agent.mcp.path` shall select the platform-specific MCP
executable bundled with the extension. A non-empty path shall override the
bundled executable. The MCP path, configuration path, and arguments shall support
documented `${workspaceFolder}` substitution; relative paths shall be resolved
deterministically against the applicable workspace folder. The default
configuration path shall resolve to that folder's `mcutrace.toml`.

### REQ-0184 Multi-root project isolation @evidence(implementation,test) @depends(REQ-0142, REQ-0158, REQ-0180)
In a multi-root workspace, the extension shall evaluate resource-scoped settings
for each workspace folder and return no more than one mcutrace MCP definition per
eligible folder. Definitions shall have distinguishable labels and shall not use
another folder's configuration, working directory, arguments, or resolved paths.

### REQ-0185 Live agent integration reconfiguration @evidence(implementation,test) @depends(REQ-0181, REQ-0184)
When an applicable mcutrace agent setting, workspace folder, or resolved project
configuration changes, the extension shall update skill eligibility and notify
VS Code that MCP definitions changed without requiring a window reload where the
supported API permits live reconfiguration. Removed or disabled folders shall no
longer contribute an MCP definition.

### REQ-0186 Independent user control @evidence(implementation,test) @depends(REQ-0181)
Setting `mcutrace.agent.enabled` to false shall disable both agent integrations.
When the master setting is enabled, users shall be able to disable the contributed
skill or MCP definition independently. Explicit executable, argument, and
configuration overrides shall take precedence over defaults without changing the
language-server settings.

## Safety, diagnostics, and delivery

### REQ-0187 VS Code trust and access preservation @evidence(implementation,test)
The extension shall not bypass, weaken, pre-approve, or overwrite VS Code's
workspace trust, MCP server trust, MCP access, agent-skill enablement, or tool
approval controls. It shall not start the MCP server itself when VS Code owns the
server lifecycle, and it shall avoid executing workspace-controlled paths in an
untrusted workspace.

### REQ-0188 No implicit workspace mutation @evidence(implementation,test)
Automatic integration shall not create or modify `.vscode/mcp.json`, `.mcp.json`,
`.vscode/settings.json`, project skill directories, `mcutrace.toml`, or other
workspace files. Commands may open the relevant settings or documentation, but
persisted changes shall occur only through an explicit user action.

### REQ-0189 Agent integration status and diagnostics @evidence(implementation,test)
The extension shall provide an inspectable status or diagnostic command that
reports whether the skill and MCP provider are enabled, which workspace folders
are eligible, and the resolved executable and configuration identities without
exposing unrelated environment data. Missing executables, invalid setting types,
unresolved paths, unsupported VS Code capabilities, and provider failures shall
produce actionable output without repeated disruptive notifications.

### REQ-0190 VSIX compatibility and platform packaging @evidence(build,test) @depends(REQ-0161, REQ-0179)
The extension shall declare a VS Code engine version that supports the stable
skill contribution and MCP server definition APIs used by the integration. Every
supported platform VSIX shall contain matching `mcutrace-lsp` and `mcutrace-mcp`
binaries plus the canonical skill, shall select executable names correctly on
Windows and POSIX hosts, and shall run the binaries in the workspace extension
host, including supported remote workspaces.

### REQ-0191 VS Code agent integration verification @evidence(test,build)
Automated tests shall cover manifest contributions, setting types and defaults,
placeholder and relative-path resolution, bundled and overridden executables,
eligible and ineligible folders, multi-root isolation, definition labels and
arguments, live configuration changes, independent enablement, untrusted
workspaces, absent APIs, diagnostics, and package contents. An integration test
shall install or inspect a produced VSIX and verify that VS Code can discover the
packaged skill and MCP server definition for a fixture mcutrace project without
generated workspace configuration files.
