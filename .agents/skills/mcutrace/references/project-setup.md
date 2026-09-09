# Project setup

Use this workflow when creating a new mcutrace project or adding mcutrace to an
existing project. Setup uses ordinary filesystem and command tools because an MCP
server cannot load the project until its configuration exists.

## Inspect before changing

Resolve the exact project root, then inspect its existing files, local
instructions, version-control state, build and test conventions, requirement or
plan documents, supported annotated source files, build definitions, evidence
outputs, `mcutrace.toml`, agent-skill locations, and MCP client configuration.
Check available `mcutrace` and `mcutrace-mcp` executables with their `--version`
and `--help` output. Preserve existing content and unrelated changes.

Treat an existing `mcutrace.toml` as an integration or repair, never as a blank
file to replace. Do not add broad Markdown or source globs merely because they
match files: configure only intended trace inputs.

## Create a new project

Create the project structure the user requested, using their chosen language,
build system, and conventions. If those choices are essential but missing, ask
for them before generating unrelated application boilerplate.

Establish a minimal mcutrace foundation:

1. Create a requirement document if none was requested or supplied. A neutral
   document heading with no invented `REQ-NNNN` blocks is a valid starting point.
   Add product requirements only from user-provided intent.
2. Create `mcutrace.toml` at the project root with only files that exist. This is
   a minimal configuration:

   ```toml
   [project]
   root = "."
   requirements = ["requirements.md"]

   [validation]
   fail_at_or_above = "error"
   ```

3. Add `plans`, `sources`, or `build_files` only for actual intended inputs.
   Add `[[artifacts]]` only for an existing or explicitly requested producer and
   its real output path and importer. Do not fabricate evidence files.

## Add mcutrace to an existing project

Reuse the project's established requirement and planning conventions. If it has
no requirement document, create a neutral empty one rather than reverse-engineer
requirements from implementation. Select precise existing files or narrow
patterns for `requirements`, `plans`, `sources`, and supported build definitions.

Keep validation minimal unless the user supplied a policy or the repository
already documents one. Missing-evidence severities and exclusions are project
policy, not defaults to guess. Integrate existing producers only when their
artifact format and output location are known; adding new test, coverage, build,
or analysis producers requires authorization.

When project-scoped agent integration is requested, prefer a supported installer
such as **mcutrace: Set Up Codex in Workspace**. Otherwise use the client and
distribution's documented skill location, and register `mcutrace-mcp` with an
absolute executable path, `--config` followed by the absolute `mcutrace.toml`
path, and the project root as its working directory. Preserve an existing skill
directory or user-managed server entry unless the user explicitly asks to
replace or migrate it.

## Verify the setup

From the project root, run the available CLI's equivalent of:

```sh
mcutrace --config mcutrace.toml validate
```

Fix configuration and parse failures introduced by setup. Report validation
findings separately from setup failures; a valid initial project may still have
missing evidence until its producers are configured and run.

After the MCP client is configured or refreshed, read `mcutrace://rules` and
`mcutrace://project` or call `project_overview`. Confirm that the resolved config
and project root match the target. If the executable, client integration, or MCP
connection is unavailable, finish every safe local setup step and report the
exact remaining action instead of claiming the integration is complete.
