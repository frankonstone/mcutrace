'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');

const {
  MCP_PROVIDER_ID,
  MCP_SERVER_VERSION,
  SETUP_CODEX_COMMAND,
  WORKSPACE_SKILL_PATH,
  readAgentSettings,
  resolveWorkspacePath,
  bundledMcpPath,
  collectFolderStates,
  createMcpDefinition,
  formatStatus,
  installWorkspaceSkill,
  mergeMcpConfig,
  runSetupCodexCommand,
  activateAgentIntegration,
} = require('../agent-integration');

const manifest = JSON.parse(fs.readFileSync(path.resolve(__dirname, '../package.json'), 'utf8'));
const MANAGED_MCP_BEGIN_FOR_TEST = '# BEGIN mcutrace VS Code managed MCP';

function configuration(values = {}) {
  return { get: (key, fallback) => Object.hasOwn(values, key) ? values[key] : fallback };
}

function folder(name, fsPath) {
  return { name, uri: { fsPath, toString: () => `file://${fsPath}` } };
}

function mockVscode(options = {}) {
  const settings = options.settings || new Map();
  const existing = options.existing || new Set();
  const files = options.files || new Map();
  const definitions = {};
  const fileOperations = { created: [], copied: [] };
  class McpStdioServerDefinition {
    constructor(label, command, args, env, version) {
      Object.assign(this, { label, command, args, env, version });
    }
  }
  return {
    McpStdioServerDefinition,
    lm: {
      registerMcpServerDefinitionProvider(id, provider) {
        definitions.id = id;
        definitions.provider = provider;
        return { dispose() {} };
      },
    },
    Uri: {
      file: (fsPath) => ({ fsPath, toString: () => `file://${fsPath}` }),
      joinPath: (uri, ...segments) => {
        const fsPath = path.posix.join(uri.fsPath, ...segments);
        return { fsPath, toString: () => `file://${fsPath}` };
      },
    },
    workspace: {
      isTrusted: options.trusted !== false,
      workspaceFolders: options.folders || [],
      fs: {
        stat: async (uri) => {
          if (!existing.has(uri.fsPath) && !files.has(uri.fsPath)) throw new Error('ENOENT');
          return { type: 1 };
        },
        createDirectory: async (uri) => {
          fileOperations.created.push(uri.fsPath);
          existing.add(uri.fsPath);
        },
        copy: async (source, target, options) => {
          if (!existing.has(source.fsPath)) throw new Error('ENOENT');
          if (existing.has(target.fsPath) && !options?.overwrite) throw new Error('EEXIST');
          fileOperations.copied.push({ source: source.fsPath, target: target.fsPath, options });
          existing.add(target.fsPath);
          if (files.has(source.fsPath)) files.set(target.fsPath, files.get(source.fsPath));
        },
        readFile: async (uri) => {
          if (!files.has(uri.fsPath)) throw new Error('ENOENT');
          return Buffer.from(files.get(uri.fsPath), 'utf8');
        },
        writeFile: async (uri, content) => {
          files.set(uri.fsPath, Buffer.from(content).toString('utf8'));
        },
      },
      getConfiguration: (_section, uri) => settings.get(uri.fsPath) || configuration(),
    },
    definitions,
    fileOperations,
    files,
  };
}

test('declares the skill, provider, commands, and resource-scoped settings [REQ-0178] [REQ-0179] [REQ-0180] [REQ-0181] [REQ-0190] [REQ-0221]', () => {
  assert.equal(manifest.engines.vscode, '^1.109.0');
  assert.deepEqual(manifest.contributes.chatSkills, [{
    path: './skills/mcutrace/SKILL.md', when: 'mcutrace.agent.skillAvailable',
  }]);
  assert.deepEqual(manifest.contributes.mcpServerDefinitionProviders, [{
    id: MCP_PROVIDER_ID, label: 'mcutrace', when: 'mcutrace.agent.mcpAvailable',
  }]);
  assert.ok(manifest.contributes.commands.some((entry) => (
    entry.command === 'mcutrace.showAgentIntegrationStatus'
  )));
  assert.ok(manifest.contributes.commands.some((entry) => (
    entry.command === SETUP_CODEX_COMMAND
  )));
  assert.equal(manifest.capabilities.untrustedWorkspaces.supported, 'limited');

  const properties = manifest.contributes.configuration.properties;
  const expected = {
    'mcutrace.agent.enabled': ['boolean', true],
    'mcutrace.agent.skill.enabled': ['boolean', true],
    'mcutrace.agent.mcp.enabled': ['boolean', true],
    'mcutrace.agent.mcp.path': ['string', ''],
    'mcutrace.agent.mcp.arguments': ['array', []],
    'mcutrace.agent.mcp.configPath': ['string', ''],
  };
  for (const [key, [type, defaultValue]] of Object.entries(expected)) {
    assert.equal(properties[key].type, type, key);
    assert.deepEqual(properties[key].default, defaultValue, key);
    assert.equal(properties[key].scope, 'resource', key);
  }
});

test('installs the bundled skill without overwriting an existing workspace skill [REQ-0221]', async () => {
  const project = folder('app', '/work/app');
  const source = '/extension/skills/mcutrace';
  const target = `/work/app/${WORKSPACE_SKILL_PATH}`;
  const existing = new Set([source]);
  const vscode = mockVscode({ folders: [project], existing });
  const context = { extensionPath: '/extension' };

  const installed = await installWorkspaceSkill(vscode, context, project);
  assert.equal(installed.status, 'installed');
  assert.equal(installed.target.fsPath, target);
  assert.deepEqual(vscode.fileOperations.created, ['/work/app/.agents/skills']);
  assert.deepEqual(vscode.fileOperations.copied, [{
    source, target, options: { overwrite: false },
  }]);

  const present = await installWorkspaceSkill(vscode, context, project);
  assert.equal(present.status, 'exists');
  assert.equal(vscode.fileOperations.copied.length, 1);
});

test('merges managed MCP configuration without replacing user-owned entries [REQ-0222]', () => {
  const block = [
    '# BEGIN mcutrace VS Code managed MCP',
    '[mcp_servers.mcutrace]',
    'command = "/new/mcutrace-mcp"',
    '# END mcutrace VS Code managed MCP',
  ].join('\n');
  const installed = mergeMcpConfig('model = "gpt"\n', block);
  assert.equal(installed.status, 'installed');
  assert.match(installed.text, /model = "gpt"\n\n# BEGIN/);

  const updated = mergeMcpConfig([
    'model = "gpt"',
    '# BEGIN mcutrace VS Code managed MCP',
    '[mcp_servers.mcutrace]',
    'command = "/old/mcutrace-mcp"',
    '# END mcutrace VS Code managed MCP',
    '',
  ].join('\n'), block);
  assert.equal(updated.status, 'updated');
  assert.match(updated.text, /command = "\/new\/mcutrace-mcp"/);
  assert.doesNotMatch(updated.text, /\/old\/mcutrace-mcp/);

  for (const content of [
    '[mcp_servers.mcutrace]\ncommand = "/custom"\n',
    '[mcp_servers."mcutrace"]\ncommand = "/custom"\n',
    '[mcp_servers]\nmcutrace = { command = "/custom" }\n',
  ]) {
    const existing = mergeMcpConfig(content, block);
    assert.equal(existing.status, 'existing');
    assert.equal(existing.text, content);
  }
  assert.throws(
    () => mergeMcpConfig(`${MANAGED_MCP_BEGIN_FOR_TEST}\ncommand = "broken"\n`, block),
    /missing its end marker/
  );
});

test('setup command installs both skill and MCP configuration and reports edge cases [REQ-0221] [REQ-0222]', async () => {
  const first = folder('one', '/work/one');
  const second = folder('two', '/work/two');
  const messages = { information: [], warnings: [], errors: [] };
  const existing = new Set([
    '/extension/skills/mcutrace',
    '/extension/bin/mcutrace-mcp',
    '/work/two/mcutrace.toml',
  ]);
  const files = new Map();
  const vscode = mockVscode({ folders: [first, second], existing, files });
  vscode.window = {
    showWorkspaceFolderPick: async () => second,
    showInformationMessage: async (message) => messages.information.push(message),
    showWarningMessage: async (message) => messages.warnings.push(message),
    showErrorMessage: async (message) => messages.errors.push(message),
  };
  const context = {
    extensionPath: '/extension',
    globalStorageUri: vscode.Uri.file('/storage'),
    chmodExecutable: async () => {},
  };

  await runSetupCodexCommand(vscode, context);
  assert.equal(vscode.fileOperations.copied[0].target, '/work/two/.agents/skills/mcutrace');
  assert.equal(vscode.fileOperations.copied[1].target, '/storage/bin/mcutrace-mcp');
  assert.match(files.get('/work/two/.codex/config.toml'), /\[mcp_servers\.mcutrace\]/);
  assert.match(files.get('/work/two/.codex/config.toml'), /command = "\/storage\/bin\/mcutrace-mcp"/);
  assert.match(files.get('/work/two/.codex/config.toml'), /"--config", "\/work\/two\/mcutrace.toml"/);
  assert.match(messages.information[0], /Codex setup completed.*skill installed; MCP installed/);

  await runSetupCodexCommand(vscode, context);
  assert.match(messages.information[1], /skill already present; MCP already current/);

  const untrusted = mockVscode({ folders: [first], trusted: false, existing });
  untrusted.window = vscode.window;
  await runSetupCodexCommand(untrusted, context);
  assert.match(messages.warnings[0], /Trust this workspace/);

  const empty = mockVscode();
  empty.window = vscode.window;
  await runSetupCodexCommand(empty, context);
  assert.match(messages.information[2], /Open a workspace folder/);

  const unconfigured = mockVscode({
    folders: [first],
    existing: new Set(['/extension/skills/mcutrace', '/extension/bin/mcutrace-mcp']),
  });
  unconfigured.window = vscode.window;
  await runSetupCodexCommand(unconfigured, context);
  assert.match(messages.errors[0], /selected mcutrace configuration does not exist/);

  const missing = mockVscode({
    folders: [first],
    existing: new Set(['/extension/skills/mcutrace', '/work/one/mcutrace.toml']),
  });
  missing.window = vscode.window;
  await runSetupCodexCommand(missing, context);
  assert.match(messages.errors[1], /bundled mcutrace MCP executable is missing/);
  assert.match(messages.errors[1], /run the setup command again/);
});

test('uses safe defaults for absent and malformed settings [REQ-0181] [REQ-0182] [REQ-0189]', () => {
  assert.deepEqual(readAgentSettings(configuration()), {
    enabled: true,
    skillEnabled: true,
    mcpEnabled: true,
    mcpPath: '',
    arguments: [],
    configPath: '',
    diagnostics: [],
  });
  const malformed = readAgentSettings(configuration({
    'agent.enabled': 'yes',
    'agent.skill.enabled': 1,
    'agent.mcp.enabled': null,
    'agent.mcp.path': false,
    'agent.mcp.arguments': ['valid', 7],
    'agent.mcp.configPath': {},
  }));
  assert.equal(malformed.enabled, true);
  assert.equal(malformed.skillEnabled, true);
  assert.equal(malformed.mcpEnabled, true);
  assert.equal(malformed.mcpPath, '');
  assert.deepEqual(malformed.arguments, []);
  assert.equal(malformed.configPath, '');
  assert.equal(malformed.diagnostics.length, 6);
});

test('resolves POSIX and Windows workspace paths deterministically [REQ-0181] [REQ-0182] [REQ-0191]', () => {
  assert.equal(
    resolveWorkspacePath('${workspaceFolder}/build/mcutrace-mcp', '/work/one', 'linux'),
    '/work/one/build/mcutrace-mcp'
  );
  assert.equal(resolveWorkspacePath('cfg/project.toml', '/work/one', 'linux'), '/work/one/cfg/project.toml');
  assert.equal(resolveWorkspacePath('/shared/project.toml', '/work/one', 'linux'), '/shared/project.toml');
  assert.equal(
    resolveWorkspacePath('${workspaceFolder}\\build\\mcutrace-mcp.exe', 'C:\\work\\one', 'win32'),
    'C:\\work\\one\\build\\mcutrace-mcp.exe'
  );
  assert.equal(bundledMcpPath('C:\\extension', 'win32'), 'C:\\extension\\bin\\mcutrace-mcp.exe');
  assert.equal(
    resolveWorkspacePath('${workspaceFolder}/mcutrace.toml', '/work/$&-project', 'linux'),
    '/work/$&-project/mcutrace.toml'
  );
});

test('creates one isolated direct stdio definition per eligible workspace folder [REQ-0180] [REQ-0182] [REQ-0183] [REQ-0184] [REQ-0186]', async () => {
  const folders = [folder('app', '/work/a'), folder('app', '/work/b'), folder('docs', '/work/docs')];
  const settings = new Map([
    ['/work/a', configuration({
      'agent.mcp.path': 'out/mcutrace-mcp',
      'agent.mcp.arguments': ['--tag', '${workspaceFolder}/tag'],
      'agent.mcp.configPath': 'config/trace.toml',
    })],
    ['/work/b', configuration()],
    ['/work/docs', configuration()],
  ]);
  const existing = new Set([
    '/work/a/out/mcutrace-mcp', '/work/a/config/trace.toml',
    '/extension/bin/mcutrace-mcp', '/work/b/mcutrace.toml',
  ]);
  const vscode = mockVscode({ folders, settings });
  const snapshot = await collectFolderStates(vscode, '/extension', {
    platform: 'linux', pathExists: async (filePath) => existing.has(filePath),
  });
  const eligible = snapshot.states.filter((state) => state.mcpEligible);
  assert.equal(eligible.length, 2);
  assert.deepEqual(eligible.map((state) => state.label), [
    'mcutrace (app: /work/a)', 'mcutrace (app: /work/b)',
  ]);
  assert.deepEqual(eligible[0].args, [
    '--tag', '/work/a/tag', '--config', '/work/a/config/trace.toml',
  ]);
  assert.deepEqual(eligible[1].args, ['--config', '/work/b/mcutrace.toml']);
  const definition = createMcpDefinition(vscode, eligible[0]);
  assert.equal(definition.command, '/work/a/out/mcutrace-mcp');
  assert.equal(definition.cwd, folders[0].uri);
  assert.equal(definition.version, MCP_SERVER_VERSION);
  assert.deepEqual(definition.env, {});
});

test('trust, absent APIs, and independent flags gate exposure without starting a process [REQ-0186] [REQ-0187] [REQ-0188] [REQ-0191]', async () => {
  const project = folder('app', '/work/app');
  const existing = new Set(['/extension/bin/mcutrace-mcp', '/work/app/mcutrace.toml']);
  const untrusted = mockVscode({ folders: [project], existing, trusted: false });
  const hidden = await collectFolderStates(untrusted, '/extension');
  assert.equal(hidden.states[0].skillEligible, false);
  assert.equal(hidden.states[0].mcpEligible, false);

  const settings = new Map([['/work/app', configuration({ 'agent.mcp.enabled': false })]]);
  const skillOnly = await collectFolderStates(mockVscode({ folders: [project], existing, settings }), '/extension');
  assert.equal(skillOnly.states[0].skillEligible, true);
  assert.equal(skillOnly.states[0].mcpEligible, false);

  const skillDisabledSettings = new Map([['/work/app', configuration({
    'agent.skill.enabled': false,
  })]]);
  const mcpOnly = await collectFolderStates(
    mockVscode({ folders: [project], existing, settings: skillDisabledSettings }), '/extension'
  );
  assert.equal(mcpOnly.states[0].skillEligible, false);
  assert.equal(mcpOnly.states[0].mcpEligible, true);

  const masterDisabledSettings = new Map([['/work/app', configuration({
    'agent.enabled': false,
  })]]);
  const disabled = await collectFolderStates(
    mockVscode({ folders: [project], existing, settings: masterDisabledSettings }), '/extension'
  );
  assert.equal(disabled.states[0].skillEligible, false);
  assert.equal(disabled.states[0].mcpEligible, false);

  const absentApi = mockVscode({ folders: [project], existing });
  delete absentApi.lm;
  delete absentApi.McpStdioServerDefinition;
  const unsupported = await collectFolderStates(absentApi, '/extension');
  assert.equal(unsupported.apiAvailable, false);
  assert.equal(unsupported.states[0].skillEligible, true);
  assert.equal(unsupported.states[0].mcpEligible, false);

  const empty = await collectFolderStates(mockVscode(), '/extension');
  assert.deepEqual(empty.states, []);
});

test('registers the provider, refreshes on changes, revalidates on resolve, and reports status [REQ-0185] [REQ-0187] [REQ-0188] [REQ-0189]', async () => {
  const project = folder('app', '/work/app');
  const existing = new Set(['/extension/bin/mcutrace-mcp', '/work/app/mcutrace.toml']);
  const settings = new Map([['/work/app', configuration()]]);
  const vscode = mockVscode({ folders: [project], existing, settings });
  const contexts = new Map();
  const commands = new Map();
  const listeners = {};
  const outputLines = [];
  class EventEmitter {
    constructor() { this.listeners = []; this.event = (callback) => { this.listeners.push(callback); return { dispose() {} }; }; }
    fire() { this.listeners.forEach((listener) => listener()); }
    dispose() {}
  }
  vscode.EventEmitter = EventEmitter;
  vscode.commands = {
    executeCommand: async (_command, key, value) => contexts.set(key, value),
    registerCommand: (id, callback) => { commands.set(id, callback); return { dispose() {} }; },
  };
  vscode.window = {
    createOutputChannel: () => ({
      appendLine: (line) => outputLines.push(line), clear: () => { outputLines.length = 0; },
      show() {}, dispose() {},
    }),
  };
  vscode.workspace.onDidChangeConfiguration = (callback) => { listeners.configuration = callback; return { dispose() {} }; };
  vscode.workspace.onDidChangeWorkspaceFolders = (callback) => { listeners.folders = callback; return { dispose() {} }; };
  vscode.workspace.onDidGrantWorkspaceTrust = (callback) => { listeners.trust = callback; return { dispose() {} }; };
  const watcher = {};
  for (const event of ['onDidCreate', 'onDidChange', 'onDidDelete']) {
    watcher[event] = (callback) => { listeners[event] = callback; return { dispose() {} }; };
  }
  const context = { extensionPath: '/extension', subscriptions: [] };

  const controller = await activateAgentIntegration(vscode, context, watcher);
  let providerChanges = 0;
  vscode.definitions.provider.onDidChangeMcpServerDefinitions(() => { providerChanges += 1; });
  assert.equal(vscode.definitions.id, MCP_PROVIDER_ID);
  assert.equal(contexts.get('mcutrace.agent.skillAvailable'), true);
  assert.equal(contexts.get('mcutrace.agent.mcpAvailable'), true);
  assert.equal((await vscode.definitions.provider.provideMcpServerDefinitions()).length, 1);

  settings.set('/work/app', configuration({ 'agent.mcp.enabled': false }));
  listeners.configuration({ affectsConfiguration: (key) => key === 'mcutrace.agent' });
  await controller.refresh(false);
  assert.equal((await vscode.definitions.provider.provideMcpServerDefinitions()).length, 0);
  assert.equal(await vscode.definitions.provider.resolveMcpServerDefinition({ label: 'mcutrace (app)' }), undefined);

  listeners.onDidChange({ fsPath: '/work/app/mcutrace.toml' });
  listeners.folders();
  listeners.trust();
  await controller.refresh(false);
  assert.ok(providerChanges >= 3);

  await commands.get('mcutrace.showAgentIntegrationStatus')();
  assert.match(outputLines.join('\n'), /Workspace trust: trusted/);
  assert.match(outputLines.join('\n'), /MCP enabled\/available: false\/false/);
  assert.ok(listeners.onDidCreate && listeners.onDidChange && listeners.onDidDelete);
  assert.ok(listeners.folders && listeners.trust);

  vscode.workspace.workspaceFolders = [];
  listeners.folders();
  await controller.refresh(false);
  assert.equal((await vscode.definitions.provider.provideMcpServerDefinitions()).length, 0);
  assert.equal(contexts.get('mcutrace.agent.skillAvailable'), false);
});

test('status output explains invalid settings, missing paths, and unavailable APIs without environment data [REQ-0189] [REQ-0191]', async () => {
  const project = folder('broken', '/work/broken');
  const settings = new Map([['/work/broken', configuration({
    'agent.mcp.path': 42,
    'agent.mcp.arguments': ['--flag', false],
  })]]);
  const vscode = mockVscode({ folders: [project], settings });
  delete vscode.lm;
  delete vscode.McpStdioServerDefinition;
  const snapshot = await collectFolderStates(vscode, '/extension', {
    pathExists: async () => false,
  });
  const status = formatStatus(snapshot).join('\n');
  assert.match(status, /VS Code MCP provider API: unavailable/);
  assert.match(status, /MCP executable: .* \(not found\)/);
  assert.match(status, /Configuration: .* \(not found\)/);
  assert.match(status, /Configuration issue: mcutrace\.agent\.mcp\.path must be a string/);
  assert.match(status, /Configuration issue: mcutrace\.agent\.mcp\.arguments must be an array of strings/);
  assert.doesNotMatch(status, /PATH=/);
});

test('provider registration failures remain inspectable and actionable [REQ-0189] [REQ-0191]', async () => {
  const vscode = mockVscode();
  const commands = new Map();
  const lines = [];
  class EventEmitter {
    constructor() { this.event = () => ({ dispose() {} }); }
    fire() {}
    dispose() {}
  }
  vscode.EventEmitter = EventEmitter;
  vscode.lm.registerMcpServerDefinitionProvider = () => { throw new Error('registration failed'); };
  vscode.commands = {
    executeCommand: async () => {},
    registerCommand: (id, callback) => { commands.set(id, callback); return { dispose() {} }; },
  };
  vscode.window = {
    createOutputChannel: () => ({
      appendLine: (line) => lines.push(line), clear: () => { lines.length = 0; },
      show() {}, dispose() {},
    }),
  };
  vscode.workspace.onDidChangeConfiguration = () => ({ dispose() {} });
  vscode.workspace.onDidChangeWorkspaceFolders = () => ({ dispose() {} });
  vscode.workspace.onDidGrantWorkspaceTrust = () => ({ dispose() {} });

  await activateAgentIntegration(vscode, { extensionPath: '/extension', subscriptions: [] });
  assert.match(lines.join('\n'), /Unable to register.*registration failed/);
  await commands.get('mcutrace.showAgentIntegrationStatus')();
  assert.match(lines.join('\n'), /registration issue: registration failed/);
  assert.match(lines.join('\n'), /Check VS Code compatibility and extension logs/);
});

test('repository fixture contains config, requirements, a plan, and source annotations [REQ-0191]', () => {
  const fixture = path.resolve(__dirname, '../../../tests/fixtures/mcp-project');
  const config = fs.readFileSync(path.join(fixture, 'mcutrace.toml'), 'utf8');
  assert.match(config, /requirements = \["requirements\.md"\]/);
  assert.match(config, /plans = \["plan\.md"\]/);
  assert.match(config, /sources = \["source\.cpp"\]/);
  assert.match(fs.readFileSync(path.join(fixture, 'requirements.md'), 'utf8'), /REQ-0139/);
  assert.match(fs.readFileSync(path.join(fixture, 'plan.md'), 'utf8'), /PLN-0014/);
  assert.match(fs.readFileSync(path.join(fixture, 'source.cpp'), 'utf8'), /@req REQ-0139/);
});
