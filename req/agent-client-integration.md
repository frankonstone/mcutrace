# mcutrace Agent-Neutral Implementation Requirements

These requirements define how an explicit user action can start implementation
of an mcutrace plan item with a selected coding agent from a command line or a
supported environment adapter. The portable workflow uses the Agent Client
Protocol (ACP) as its agent-facing boundary and supplies project-specific
traceability context through `mcutrace-mcp`. Agent products, models,
authentication, vendor-specific APIs, and host-specific user interfaces remain
outside the workflow core.

## Interoperability and configuration

### REQ-0200 Agent-neutral plan implementation @evidence(implementation,test) @depends(REQ-0003, REQ-0137)
mcutrace shall allow a user to start implementation of a selected plan item
without coupling the workflow to Codex, Claude, GitHub Copilot, OpenCode, a
particular editor, or another named agent product. The same task construction,
agent lifecycle, safety, and completion rules shall apply to every compatible
agent and host environment.

### REQ-0201 ACP client boundary @evidence(implementation,test)
The portable implementation workflow shall act as an Agent Client Protocol
client and communicate with configured coding agents through a supported,
negotiated ACP protocol version. Vendor-specific session APIs, executable
behavior, message formats, and response events shall not enter plan task
construction or mcutrace context handling. An agent that requires adaptation
shall be integrated through an external ACP adapter rather than a vendor branch
in mcutrace.

### REQ-0202 Generic ACP agent configuration and discovery @evidence(implementation,test)
mcutrace shall support one or more user-configured ACP agent definitions,
each with a stable identifier, display label, executable command, and argument
list, plus selection of a default agent. Agent definitions shall be data rather
than a closed vendor enumeration. Optional discovery from an ACP registry shall
not prevent use of an unlisted local agent, and mcutrace shall not download,
install, update, or authenticate an agent without a separate explicit user
action.

### REQ-0203 Portable implement command @evidence(implementation,test) @depends(REQ-0064, REQ-0137, REQ-0202)
The CLI shall provide `mcutrace implement PLN-NNNN` to start implementation of an
eligible plan item with the configured default ACP agent. `--agent <id>` shall
select another configured agent without changing the default. `--dry-run` shall
prepare and display the task without creating an agent process or session, and
combining `--dry-run` with `--format json` shall emit the versioned portable task
envelope for adapters and automation. Unknown agents, unknown plan IDs, and
completed plans shall fail before agent launch with actionable diagnostics;
restarting completed work, if supported, shall require a separate explicit
option.

## Task and session lifecycle

### REQ-0204 Fresh plan task construction @evidence(implementation,test) @depends(REQ-0150, REQ-0157)
Immediately before creating an agent session, the workflow shall obtain the
selected plan item, its linked requirements, project identity, and current
analysis revision from the authoritative mcutrace analysis. It shall construct a
provider-neutral implementation request containing the canonical plan ID and
instructions to inspect linked requirement and prerequisite context, preserve
unrelated changes, maintain the plan record, generate required evidence, and
validate before completion. If the selected item or project changed materially
since task selection, the workflow shall stop or request renewed confirmation
rather than launch with silently stale context.

### REQ-0205 Project-scoped mcutrace MCP handoff @evidence(implementation,test) @depends(REQ-0138, REQ-0142, REQ-0158)
When creating or restoring an ACP session, the workflow shall supply the
eligible workspace folder as the session working directory and shall pass that
project's `mcutrace-mcp` stdio definition in the session's MCP server list. The
definition shall use the same executable, configuration, arguments, and project
isolation semantics in every host environment. Failure to provide the project
MCP server shall prevent the implementation launch and produce an actionable
diagnostic.

### REQ-0206 Agent-skill independence @evidence(implementation,test) @depends(REQ-0165, REQ-0179)
The implementation workflow shall remain functional when the selected ACP agent
does not support Agent Skills or cannot discover the packaged mcutrace skill.
Required task behavior shall be present in the provider-neutral request and
available MCP resources and tools. When an agent or host supports a compatible
skill mechanism, an adapter may additionally reference the canonical mcutrace
skill without maintaining an agent-specific copy.

### REQ-0207 ACP capability negotiation and degradation @evidence(implementation,test) @depends(REQ-0201)
The ACP client shall negotiate the protocol version and capabilities before
using optional operations. Session loading or resumption, modes, model selection,
filesystem access, terminal access, plan updates, and other optional features
shall be used only when advertised. Missing optional capabilities shall disable
or degrade only the affected user experience and shall not be reported as agent
support when the minimum initialization, new-session, prompt, update, permission,
and cancellation workflow is unavailable.

### REQ-0208 Plan-scoped agent session lifecycle @evidence(implementation,test) @depends(REQ-0207)
For each launched implementation, the ACP client shall track the project root,
plan ID, ACP agent ID, ACP session ID, analysis revision, and lifecycle state in
client-managed state rather than a generated project file. It shall permit
resuming an existing implementation only when the selected agent advertises the
required capability, prevent accidental concurrent sessions for the same plan
item within that client, and keep sessions for different plan items independent.

### REQ-0209 Agent progress presentation @evidence(implementation,test) @depends(REQ-0208)
The portable ACP client shall expose streamed messages, tool-call state, agent
plan updates, permission requests, stop reasons, and failures through a stable
event representation usable by terminal and editor frontends. Each frontend
shall present those events in an inspectable interface associated with the
selected mcutrace plan item. Agent-generated execution plans shall remain
visually and semantically distinct from the persistent mcutrace plan document.

### REQ-0210 Cancellation and process cleanup @evidence(implementation,test) @depends(REQ-0208)
The user shall be able to cancel an active implementation through the active
frontend. Cancellation, client shutdown, project removal, agent process exit,
and protocol failure shall release ACP transport and process resources, leave the
project analyzable, and report whether the agent may have left partial file
changes. The client shall not interpret cancellation as plan completion or
silently terminate an unrelated session.

## Safety, completion, and verification

### REQ-0211 Permission and host-trust mediation @evidence(implementation,test) @depends(REQ-0187, REQ-0188)
Starting implementation shall require an explicit user command. The active
frontend shall mediate ACP permission requests through a visible user decision
and shall honor applicable host workspace-trust controls. It shall not
pre-approve file writes, terminal commands, network access, additional project
roots, or other privileged actions. Agent launch and tool access shall remain
confined to the selected project root unless the user separately authorizes a
capability advertised by the agent.

### REQ-0212 Project and multi-root isolation @evidence(implementation,test) @depends(REQ-0158, REQ-0184, REQ-0205)
Agent selection, working directory, mcutrace MCP configuration, session identity,
and status shall be resolved for the project that owns the selected plan item.
In a multi-root host, no prompt, MCP definition, permission response, or session
event from one folder shall be routed to another folder's implementation.

### REQ-0213 Validation-based implementation handoff @evidence(implementation,test) @depends(REQ-0137, REQ-0151, REQ-0174)
An ACP prompt response, successful agent process exit, or agent-reported plan
completion shall not by itself mark an mcutrace plan item `done`. After the agent
turn ends, the workflow shall refresh mcutrace analysis and expose the selected
plan's current status and relevant validation findings to the frontend.
Persistent status changes shall occur only through authorized edits to the plan
document, and completion shall retain the existing distinction between plan
metadata and implementation, test, coverage, build, and analysis evidence.

### REQ-0214 Actionable and confidential diagnostics @evidence(implementation,test)
Agent integration status and failures shall identify the applicable project,
plan, agent definition, lifecycle phase, protocol error, and a practical next
step. Diagnostics and logs shall not expose authentication tokens, unrelated
environment variables, complete environment snapshots, or agent configuration
values designated as secret, and repeated connection failures shall not produce
repeated disruptive notifications.

### REQ-0215 Agent-neutral integration verification @evidence(test,build) @depends(REQ-0190, REQ-0191)
Automated tests shall cover the `implement` command and its agent, dry-run, and
JSON modes; generic agent configuration; default and explicit selection; ACP
initialization and capability negotiation; new and resumed sessions; mcutrace MCP
handoff; streaming updates; permission decisions; cancellation; process failure;
stale analysis; concurrent-plan protection; project isolation; validation
refresh; redacted diagnostics; and package contents. Contract tests shall run the
same implementation workflow against at least two behaviorally different fake
ACP agents and shall not require credentials or network access to a commercial
agent.

## Environment adapters

### REQ-0216 Thin environment-adapter boundary @evidence(implementation,test) @depends(REQ-0200, REQ-0209)
Environment integrations shall delegate task construction, ACP semantics, and
completion evaluation to the portable implementation workflow. An adapter may
provide host-native commands, plan selection, progress rendering, permission
prompts, and session-state storage, but shall not create a divergent task schema,
vendor integration, or completion policy. The command-line frontend shall remain
usable without an editor adapter.

### REQ-0217 VS Code plan-item implementation actions @evidence(implementation,test) @depends(REQ-0127, REQ-0202, REQ-0203, REQ-0216)
The Trace view shall provide `Start Implementation` on the context menu of an
eligible plan item and shall provide `Start Implementation With...` when more
than one ACP agent is available. The default action shall use the configured
default agent; the selection action shall identify available agents without
changing that default. Active implementations shall provide resume or cancel
actions only when their session state and negotiated capabilities permit them.

### REQ-0218 VS Code implementation-session experience @evidence(implementation,test) @depends(REQ-0184, REQ-0187, REQ-0208, REQ-0209, REQ-0211, REQ-0212)
The VS Code adapter shall associate implementation sessions with the owning plan
item, store minimal session metadata in extension-managed workspace state, render
streamed progress and permission requests using VS Code interfaces, and preserve
workspace trust and multi-root isolation. Trace refreshes shall retain active
session identity, and agent execution plans shall not be presented as persistent
mcutrace plan content.

### REQ-0219 VS Code adapter verification @evidence(test,build) @depends(REQ-0190, REQ-0191, REQ-0217, REQ-0218)
Automated VS Code tests shall cover manifest commands and plan-item menus,
default and explicit agent selection, command enablement for every plan and
session state, event rendering, permission decisions, cancellation, resumption,
duplicate-launch prevention, untrusted workspaces, multi-root routing, extension
deactivation, redacted diagnostics, and packaged runtime contents. Adapter tests
shall use fake portable workflow and ACP boundaries and shall not require a
commercial agent installation.
