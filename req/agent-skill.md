# mcutrace Agent Skill Requirements

The mcutrace agent skill teaches an AI agent how to use mcutrace in a project
that already has a working mcutrace configuration and MCP server. The skill
routes project questions to the MCP server, preserves mcutrace semantics, and
helps the agent gather the traceability context relevant to its assigned work.

The skill does not define an ideal prose structure, template, planning method,
or development process for requirements and implementation plans. Those choices
belong to the consuming project and its users. Initial mcutrace installation and
project configuration are outside the first version of the skill.

## Distribution and scope

### REQ-0162 Distributable mcutrace agent skill @evidence(implementation,test)
mcutrace shall provide a canonical `mcutrace` agent skill that can be installed
in another project's agent environment independently of the mcutrace VS Code
extension. The canonical skill source shall be maintained in the mcutrace
repository and shall identify the MCP integration it requires.

### REQ-0163 Precise skill discovery @evidence(implementation,test)
The skill name and discovery description shall select the skill for tasks that
involve mcutrace requirements, implementation plans, trace evidence,
dependencies, change impact, or validation diagnostics. The description shall
not cause the skill to be selected for unrelated project work merely because a
project contains an mcutrace configuration.

### REQ-0164 Enabled-project boundary @evidence(implementation,test)
The first version of the skill shall support projects that already have an
accessible `mcutrace-mcp` server configured for the project. It shall not claim
to install mcutrace, design a project's traceability policy, create an initial
`mcutrace.toml`, or choose evidence producers on the project's behalf.

## MCP context acquisition

### REQ-0165 MCP as the semantic authority @evidence(implementation,test) @depends(REQ-0139, REQ-0144, REQ-0152)
The skill shall use the configured mcutrace MCP server as the authoritative
source for the analyzed trace graph, validation state, supported annotations,
and mcutrace semantics. It shall not reconstruct the graph by parsing mcutrace
Markdown or generated reports. Reading project files remains permitted when it
is necessary for an authorized edit or for observing project-local writing
conventions.

### REQ-0166 Runtime orientation and rule discovery @evidence(implementation,test) @depends(REQ-0145, REQ-0152)
Before relying on mcutrace semantics in a connection, the skill shall obtain the
running server's rules and orient itself to the selected project. It shall use
`mcutrace://rules` for the supported annotation and relationship rules,
`mcutrace://project` or `project_overview` for project identity and state, and
`mcutrace://schema` when result vocabulary or schema interpretation is needed.

### REQ-0167 Task-oriented MCP query routing @evidence(implementation,test) @depends(REQ-0144)
The skill shall explain how to choose among `project_overview`, `search`,
`requirement`, `requirement_context`, `change_impact`, `plan`, and `validate`
according to the task and known identifiers. It shall begin without a known
identifier by orienting or searching, obtain prerequisite context before work
whose meaning depends on other requirements, and obtain dependent impact when a
requirement may change.

### REQ-0168 Relevant and bounded context @evidence(implementation,test) @depends(REQ-0155)
The skill shall request only the mcutrace context relevant to the current task,
use tool filters and bounds when available, follow continuation cursors only as
needed, and preserve whether a result is filtered, complete, or truncated. It
shall not load the complete trace graph by default.

### REQ-0169 Analysis revision consistency @evidence(implementation,test) @depends(REQ-0157)
When an agent combines multiple MCP results, the skill shall compare their
`analysis_revision` values. If a revision changes in a way that can affect the
task, the skill shall refresh the affected context rather than presenting data
from different snapshots as one consistent analysis.

## Agent behavior and semantics

### REQ-0170 Faithful traceability interpretation @evidence(implementation,test) @depends(REQ-0154)
The skill shall preserve the distinctions reported by mcutrace between absent
and failed evidence, direct and transitive relationships, prerequisites and
dependents, resolved and unresolved relationships, diagnostics and evidence,
and project-loading failures and validation findings. It shall not infer that a
diagnostic proves the implementation is incorrect without supporting evidence.

### REQ-0171 Non-prescriptive requirement and plan authoring @evidence(implementation,test)
When the user's task authorizes creating or editing requirements or plans, the
skill shall follow explicit user instructions and the consuming project's
existing conventions. It shall not impose a particular template, heading
hierarchy, prose style, user-story form, planning methodology, section set, or
level of detail except for syntax that the running mcutrace version requires.

### REQ-0172 Server-derived annotation guidance @evidence(implementation,test) @depends(REQ-0152)
When an authorized change requires mcutrace identifiers, annotations, or
relationships, the skill shall use the syntax and semantics exposed by the
running server. It shall preserve canonical identifiers, require explicit
relationships where mcutrace requires them, and shall not invent evidence or
trace links from textual proximity or implication.

### REQ-0173 Read-only MCP and authorization boundary @evidence(implementation,test) @depends(REQ-0143)
The skill shall treat all mcutrace MCP operations as read-only. MCP access shall
not be represented as permission to edit project files, run builds or evidence
producers, regenerate artifacts, or expand the user's task. Such actions may be
performed only through normal project tools when they are within the authority
granted by the user's request.

### REQ-0174 Validation and evidence freshness at handoff @evidence(implementation,test) @depends(REQ-0151, REQ-0154)
For work that can affect traced entities, the skill shall inspect relevant
validation results before reporting completion. It shall state when evidence
artifacts may be stale or were not regenerated and shall not claim that an MCP
validation call ran tests, coverage, builds, static analysis, or other evidence
producers.

## Failure behavior, maintainability, and delivery

### REQ-0175 Actionable degraded behavior @evidence(implementation,test) @depends(REQ-0156)
If the mcutrace MCP server is unavailable, the selected project cannot load, or a
required capability is absent, the skill shall identify the blocking condition
and provide actionable setup or troubleshooting information. It shall not
silently replace MCP-backed project context with guesses or claim a complete
mcutrace analysis. It may continue with other authorized project work only while
making the missing mcutrace context explicit.

### REQ-0176 Concise and maintainable skill content @evidence(implementation,test)
The skill entry point shall contain only the activation boundary, essential
workflow, non-obvious semantic invariants, authorization boundary, and routing
needed for reliable use. It shall obtain version-specific tool schemas and rules
from the MCP server instead of duplicating the mcutrace manual, and shall use
supporting references or scripts only when they provide a demonstrated benefit.

### REQ-0177 Skill packaging and behavioral verification @evidence(implementation,test,build)
The repository shall document how another project installs the skill and connects
it to that project's `mcutrace-mcp` process. Automated structural validation and
repeatable behavioral scenarios shall cover positive and negative skill
selection, MCP orientation and routing, bounded context, revision changes,
semantic interpretation, project-convention preservation, authorization
boundaries, validation handoff, and unavailable or invalid MCP projects.
