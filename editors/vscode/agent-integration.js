'use strict';

const fs = require('node:fs/promises');
const path = require('node:path');

const MCP_PROVIDER_ID = 'mcutrace.agent.mcp';
const MCP_SERVER_VERSION = '0.1.0';
const SKILL_CONTEXT_KEY = 'mcutrace.agent.skillAvailable';
const MCP_CONTEXT_KEY = 'mcutrace.agent.mcpAvailable';
const SETUP_CODEX_COMMAND = 'mcutrace.setupCodex';
const WORKSPACE_SKILL_PATH = '.agents/skills/mcutrace';
const CODEX_CONFIG_PATH = '.codex/config.toml';
const MANAGED_MCP_BEGIN = '# BEGIN mcutrace VS Code managed MCP';
const MANAGED_MCP_END = '# END mcutrace VS Code managed MCP';

function pathModule(platform) {
  return platform === 'win32' ? path.win32 : path.posix;
}

function readSetting(configuration, key, fallback, predicate, expected, diagnostics) {
  const value = configuration.get(`agent.${key}`, fallback);
  if (predicate(value)) {
    return value;
  }

  diagnostics.push(`mcutrace.agent.${key} must be ${expected}; using the default value.`);
  return fallback;
}

// @req REQ-0181
function readAgentSettings(configuration) {
  const diagnostics = [];
  const isBoolean = (value) => typeof value === 'boolean';
  const isString = (value) => typeof value === 'string';
  const isStringArray = (value) => Array.isArray(value)
    && value.every((entry) => typeof entry === 'string');

  return {
    enabled: readSetting(configuration, 'enabled', true, isBoolean, 'a boolean', diagnostics),
    skillEnabled: readSetting(configuration, 'skill.enabled', true, isBoolean, 'a boolean', diagnostics),
    mcpEnabled: readSetting(configuration, 'mcp.enabled', true, isBoolean, 'a boolean', diagnostics),
    mcpPath: readSetting(configuration, 'mcp.path', '', isString, 'a string', diagnostics),
    arguments: readSetting(configuration, 'mcp.arguments', [], isStringArray, 'an array of strings', diagnostics),
    configPath: readSetting(configuration, 'mcp.configPath', '', isString, 'a string', diagnostics),
    diagnostics,
  };
}

// @req REQ-0181 REQ-0182 REQ-0183
function substituteWorkspaceFolder(value, workspaceFolder) {
  return value.replaceAll('${workspaceFolder}', () => workspaceFolder);
}

function resolveWorkspacePath(value, workspaceFolder, platform = process.platform) {
  const platformPath = pathModule(platform);
  const expanded = substituteWorkspaceFolder(value, workspaceFolder);
  return platformPath.normalize(platformPath.isAbsolute(expanded)
    ? expanded
    : platformPath.resolve(workspaceFolder, expanded));
}

function bundledMcpPath(extensionPath, platform = process.platform) {
  const platformPath = pathModule(platform);
  const executable = platform === 'win32' ? 'mcutrace-mcp.exe' : 'mcutrace-mcp';
  return platformPath.join(extensionPath, 'bin', executable);
}

function assignLabels(states) {
  const totals = new Map();
  for (const state of states) {
    totals.set(state.folder.name, (totals.get(state.folder.name) || 0) + 1);
  }

  for (const state of states) {
    const name = state.folder.name;
    state.label = totals.get(name) === 1
      ? `mcutrace (${name})`
      : `mcutrace (${name}: ${state.workspaceFolder})`;
  }
}

// @req REQ-0182 REQ-0183 REQ-0184 REQ-0186
function resolveFolderState(folder, configuration, extensionPath, platform = process.platform) {
  const platformPath = pathModule(platform);
  const workspaceFolder = folder.uri.fsPath;
  const settings = readAgentSettings(configuration);
  const command = settings.mcpPath
    ? resolveWorkspacePath(settings.mcpPath, workspaceFolder, platform)
    : bundledMcpPath(extensionPath, platform);
  const configPath = settings.configPath
    ? resolveWorkspacePath(settings.configPath, workspaceFolder, platform)
    : platformPath.join(workspaceFolder, 'mcutrace.toml');
  const args = settings.arguments.map((argument) => (
    substituteWorkspaceFolder(argument, workspaceFolder)
  ));

  args.push('--config', configPath);
  return {
    folder,
    workspaceFolder,
    settings,
    command,
    configPath,
    args,
    label: '',
    configExists: false,
    executableExists: false,
    skillEligible: false,
    mcpEligible: false,
  };
}

async function pathExists(vscode, filePath) {
  try {
    await vscode.workspace.fs.stat(vscode.Uri.file(filePath));
    return true;
  } catch (_error) {
    return false;
  }
}

async function uriExists(vscode, uri) {
  try {
    await vscode.workspace.fs.stat(uri);
    return true;
  } catch (_error) {
    return false;
  }
}

function bundledSkillUri(vscode, context) {
  const extensionUri = context.extensionUri || vscode.Uri.file(context.extensionPath);
  return vscode.Uri.joinPath(extensionUri, 'skills', 'mcutrace');
}

function workspaceSkillUri(vscode, folder) {
  return vscode.Uri.joinPath(folder.uri, '.agents', 'skills', 'mcutrace');
}

function codexConfigUri(vscode, folder) {
  return vscode.Uri.joinPath(folder.uri, '.codex', 'config.toml');
}

// @req REQ-0221
async function installWorkspaceSkill(vscode, context, folder) {
  const source = bundledSkillUri(vscode, context);
  const target = workspaceSkillUri(vscode, folder);
  if (!await uriExists(vscode, source)) {
    throw new Error(`the bundled mcutrace Agent Skill is missing at ${source.fsPath || source.toString()}`);
  }
  if (await uriExists(vscode, target)) {
    return { status: 'exists', target };
  }

  await vscode.workspace.fs.createDirectory(vscode.Uri.joinPath(folder.uri, '.agents', 'skills'));
  try {
    await vscode.workspace.fs.copy(source, target, { overwrite: false });
  } catch (error) {
    // Another installer may have won the race after the existence check.
    if (await uriExists(vscode, target)) {
      return { status: 'exists', target };
    }
    throw error;
  }
  return { status: 'installed', target };
}

async function selectWorkspaceFolder(vscode) {
  const folders = vscode.workspace.workspaceFolders || [];
  if (folders.length === 0) {
    return undefined;
  }
  if (folders.length === 1) {
    return folders[0];
  }
  return vscode.window.showWorkspaceFolderPick({
    placeHolder: 'Select the workspace folder where the mcutrace Agent Skill should be installed',
  });
}

function tomlString(value) {
  return JSON.stringify(value);
}

function renderMcpConfigBlock(state) {
  return [
    MANAGED_MCP_BEGIN,
    '[mcp_servers.mcutrace]',
    `command = ${tomlString(state.command)}`,
    `args = [${state.args.map(tomlString).join(', ')}]`,
    `cwd = ${tomlString(state.workspaceFolder)}`,
    MANAGED_MCP_END,
  ].join('\n');
}

function hasUserManagedMcpConfig(content) {
  const name = String.raw`(?:mcutrace|"mcutrace"|'mcutrace')`;
  if (new RegExp(String.raw`^\s*\[\s*mcp_servers\s*\.\s*${name}\s*\]\s*(?:#.*)?$`, 'm').test(content)) {
    return true;
  }
  if (new RegExp(String.raw`^\s*mcp_servers\s*\.\s*${name}\s*(?:\.|=)`, 'm').test(content)) {
    return true;
  }

  const parent = /^\s*\[\s*mcp_servers\s*\]\s*(?:#.*)?$/m.exec(content);
  if (!parent) {
    return false;
  }
  const bodyStart = parent.index + parent[0].length;
  const nextTable = /^\s*\[/m.exec(content.slice(bodyStart));
  const body = content.slice(bodyStart, nextTable ? bodyStart + nextTable.index : undefined);
  return new RegExp(String.raw`^\s*${name}\s*=`, 'm').test(body);
}

function mergeMcpConfig(content, block) {
  const begin = content.indexOf(MANAGED_MCP_BEGIN);
  if (begin >= 0) {
    const endStart = content.indexOf(MANAGED_MCP_END, begin);
    if (endStart < 0) {
      throw new Error(`the managed mcutrace block in ${CODEX_CONFIG_PATH} is missing its end marker`);
    }
    const end = endStart + MANAGED_MCP_END.length;
    const text = `${content.slice(0, begin)}${block}${content.slice(end)}`;
    return { status: text === content ? 'current' : 'updated', text };
  }
  if (content.includes(MANAGED_MCP_END)) {
    throw new Error(`the managed mcutrace block in ${CODEX_CONFIG_PATH} is missing its begin marker`);
  }
  if (hasUserManagedMcpConfig(content)) {
    return { status: 'existing', text: content };
  }

  let prefix = content;
  if (prefix && !prefix.endsWith('\n')) {
    prefix += '\n';
  }
  if (prefix && !prefix.endsWith('\n\n')) {
    prefix += '\n';
  }
  return { status: 'installed', text: `${prefix}${block}\n` };
}

// @req REQ-0222
async function stageMcpExecutable(vscode, context, options = {}) {
  const platform = options.platform || process.platform;
  const executable = platform === 'win32' ? 'mcutrace-mcp.exe' : 'mcutrace-mcp';
  const extensionUri = context.extensionUri || vscode.Uri.file(context.extensionPath);
  if (!context.globalStorageUri) {
    throw new Error('VS Code did not provide an extension storage location for mcutrace-mcp');
  }
  const source = vscode.Uri.joinPath(extensionUri, 'bin', executable);
  const destinationDirectory = vscode.Uri.joinPath(context.globalStorageUri, 'bin');
  const target = vscode.Uri.joinPath(destinationDirectory, executable);
  if (!await uriExists(vscode, source)) {
    throw new Error(`the bundled mcutrace MCP executable is missing at ${source.fsPath || source.toString()}`);
  }

  await vscode.workspace.fs.createDirectory(destinationDirectory);
  await vscode.workspace.fs.copy(source, target, { overwrite: true });
  if (platform !== 'win32') {
    const chmod = options.chmod || fs.chmod;
    await chmod(target.fsPath, 0o755);
  }
  return target;
}

// @req REQ-0222
async function configureWorkspaceMcp(vscode, folder, state) {
  const target = codexConfigUri(vscode, folder);
  let content = '';
  if (await uriExists(vscode, target)) {
    content = Buffer.from(await vscode.workspace.fs.readFile(target)).toString('utf8');
  }
  const merged = mergeMcpConfig(content, renderMcpConfigBlock(state));
  if (merged.status === 'existing' || merged.status === 'current') {
    return { status: merged.status, target };
  }

  await vscode.workspace.fs.createDirectory(vscode.Uri.joinPath(folder.uri, '.codex'));
  await vscode.workspace.fs.writeFile(target, Buffer.from(merged.text, 'utf8'));
  return { status: merged.status, target };
}

function setupStateLabel(status) {
  return ({
    installed: 'installed',
    existing: 'kept existing user-managed configuration',
    updated: 'updated',
    current: 'already current',
  })[status] || status;
}

// @req REQ-0221 REQ-0222
async function runSetupCodexCommand(vscode, context) {
  if (vscode.workspace.isTrusted === false) {
    await vscode.window.showWarningMessage(
      'Trust this workspace before installing the mcutrace Agent Skill.'
    );
    return;
  }

  const folders = vscode.workspace.workspaceFolders || [];
  if (folders.length === 0) {
    await vscode.window.showInformationMessage(
      'Open a workspace folder before installing the mcutrace Agent Skill.'
    );
    return;
  }

  const folder = await selectWorkspaceFolder(vscode);
  if (!folder) {
    return;
  }

  try {
    const extensionPath = context.extensionPath || context.extensionUri.fsPath;
    const state = resolveFolderState(
      folder,
      vscode.workspace.getConfiguration('mcutrace', folder.uri),
      extensionPath
    );
    if (!await pathExists(vscode, state.configPath)) {
      throw new Error(`the selected mcutrace configuration does not exist at ${state.configPath}`);
    }

    const skill = await installWorkspaceSkill(vscode, context, folder);
    const executable = await stageMcpExecutable(vscode, context, {
      chmod: context.chmodExecutable,
    });
    state.command = executable.fsPath;
    const mcp = await configureWorkspaceMcp(vscode, folder, state);
    const skillState = skill.status === 'installed' ? 'installed' : 'already present';
    const mcpState = setupStateLabel(mcp.status);
    await vscode.window.showInformationMessage(
      `Codex setup completed in ${folder.name}: skill ${skillState}; MCP ${mcpState}. `
      + 'Start a new Codex session in this workspace to load both.'
    );
  } catch (error) {
    await vscode.window.showErrorMessage(
      `Codex setup is incomplete in ${folder.name}: ${errorMessage(error)}. Fix the issue and run the setup command again.`
    );
  }
}

// @req REQ-0184 REQ-0187 REQ-0188 REQ-0189
async function collectFolderStates(vscode, extensionPath, options = {}) {
  const folders = vscode.workspace.workspaceFolders || [];
  const trusted = vscode.workspace.isTrusted !== false;
  const apiAvailable = Boolean(
    vscode.lm
    && typeof vscode.lm.registerMcpServerDefinitionProvider === 'function'
    && typeof vscode.McpStdioServerDefinition === 'function'
  );
  const platform = options.platform || process.platform;
  const exists = options.pathExists || ((filePath) => pathExists(vscode, filePath));
  const states = folders.map((folder) => resolveFolderState(
    folder,
    vscode.workspace.getConfiguration('mcutrace', folder.uri),
    extensionPath,
    platform
  ));

  assignLabels(states);
  await Promise.all(states.map(async (state) => {
    state.configExists = await exists(state.configPath);
    state.executableExists = await exists(state.command);
    state.skillEligible = trusted
      && state.settings.enabled
      && state.settings.skillEnabled
      && state.configExists;
    state.mcpEligible = trusted
      && apiAvailable
      && state.settings.enabled
      && state.settings.mcpEnabled
      && state.configExists
      && state.executableExists;
  }));

  return { states, trusted, apiAvailable };
}

// @req REQ-0180 REQ-0184 REQ-0186 REQ-0189
function createMcpDefinition(vscode, state) {
  const definition = new vscode.McpStdioServerDefinition(
    state.label,
    state.command,
    state.args,
    {},
    MCP_SERVER_VERSION
  );
  definition.cwd = state.folder.uri;
  return definition;
}

function samePath(left, right, platform = process.platform) {
  if (!left || !right) {
    return false;
  }
  const platformPath = pathModule(platform);
  const normalizedLeft = platformPath.normalize(left);
  const normalizedRight = platformPath.normalize(right);
  return platform === 'win32'
    ? normalizedLeft.toLowerCase() === normalizedRight.toLowerCase()
    : normalizedLeft === normalizedRight;
}

function formatStatus(snapshot) {
  const lines = [
    'mcutrace agent integration status',
    `Workspace trust: ${snapshot.trusted ? 'trusted' : 'untrusted'}`,
    `VS Code MCP provider API: ${snapshot.apiAvailable ? 'available' : 'unavailable'}`,
  ];

  if (snapshot.states.length === 0) {
    lines.push('', 'No workspace folders are open.');
    return lines;
  }

  for (const state of snapshot.states) {
    lines.push(
      '',
      `[${state.folder.name}] ${state.workspaceFolder}`,
      `  Integration enabled: ${state.settings.enabled}`,
      `  Skill enabled/available: ${state.settings.skillEnabled}/${state.skillEligible}`,
      `  MCP enabled/available: ${state.settings.mcpEnabled}/${state.mcpEligible}`,
      `  MCP executable: ${state.command}${state.executableExists ? '' : ' (not found)'}`,
      `  Configuration: ${state.configPath}${state.configExists ? '' : ' (not found)'}`,
      `  Arguments: ${JSON.stringify(state.args)}`
    );
    for (const diagnostic of state.settings.diagnostics) {
      lines.push(`  Configuration issue: ${diagnostic}`);
    }
    if (!snapshot.trusted) {
      lines.push('  Action: Trust the workspace to enable agent integration.');
    } else if (!state.configExists) {
      lines.push('  Action: Add mcutrace.toml or configure mcutrace.agent.mcp.configPath.');
    } else if (state.settings.mcpEnabled && !state.executableExists) {
      lines.push('  Action: Set mcutrace.agent.mcp.path to an existing mcutrace-mcp executable.');
    } else if (state.settings.mcpEnabled && !snapshot.apiAvailable) {
      lines.push('  Action: Upgrade VS Code to a release with the stable MCP provider API.');
    }
  }
  return lines;
}

function errorMessage(error) {
  return error instanceof Error ? error.message : String(error);
}

// @req REQ-0178 REQ-0185 REQ-0187 REQ-0188 REQ-0189
async function activateAgentIntegration(vscode, context, watcher) {
  const output = vscode.window.createOutputChannel('mcutrace Agent Integration');
  const emitter = typeof vscode.EventEmitter === 'function' ? new vscode.EventEmitter() : undefined;
  let snapshot = { states: [], trusted: vscode.workspace.isTrusted !== false, apiAvailable: false };
  let refreshQueue = Promise.resolve();
  let providerRegistrationError;

  const performRefresh = async (notifyProvider) => {
    snapshot = await collectFolderStates(vscode, context.extensionPath);
    await Promise.resolve(vscode.commands.executeCommand(
      'setContext',
      SKILL_CONTEXT_KEY,
      snapshot.states.some((state) => state.skillEligible)
    ));
    await Promise.resolve(vscode.commands.executeCommand(
      'setContext',
      MCP_CONTEXT_KEY,
      snapshot.states.some((state) => state.mcpEligible)
    ));
    if (notifyProvider && emitter) {
      emitter.fire();
    }
    return snapshot;
  };

  const refresh = (notifyProvider = true) => {
    refreshQueue = refreshQueue
      .then(() => performRefresh(notifyProvider))
      .catch((error) => {
        output.appendLine(
          `Unable to refresh agent integration: ${errorMessage(error)}. Check the agent settings and resolved paths.`
        );
        return snapshot;
      });
    return refreshQueue;
  };

  const provider = {
    onDidChangeMcpServerDefinitions: emitter ? emitter.event : undefined,
    provideMcpServerDefinitions: async () => {
      const current = await refresh(false);
      return current.states
        .filter((state) => state.mcpEligible)
        .map((state) => createMcpDefinition(vscode, state));
    },
    resolveMcpServerDefinition: async (definition) => {
      const current = await refresh(false);
      if (!current.trusted) {
        return undefined;
      }
      const state = current.states.find((candidate) => (
        candidate.mcpEligible && candidate.label === definition.label
      ));
      return state ? createMcpDefinition(vscode, state) : undefined;
    },
  };

  const disposables = [output];
  if (emitter) {
    disposables.push(emitter);
  }
  if (vscode.lm && typeof vscode.lm.registerMcpServerDefinitionProvider === 'function') {
    try {
      disposables.push(vscode.lm.registerMcpServerDefinitionProvider(MCP_PROVIDER_ID, provider));
    } catch (error) {
      providerRegistrationError = errorMessage(error);
      output.appendLine(
        `Unable to register the mcutrace MCP provider: ${providerRegistrationError}. Check VS Code compatibility and extension logs.`
      );
    }
  }

  if (typeof vscode.workspace.onDidChangeConfiguration === 'function') {
    disposables.push(vscode.workspace.onDidChangeConfiguration((event) => {
      if (event.affectsConfiguration('mcutrace.agent')) {
        void refresh();
      }
    }));
  }
  if (typeof vscode.workspace.onDidChangeWorkspaceFolders === 'function') {
    disposables.push(vscode.workspace.onDidChangeWorkspaceFolders(() => void refresh()));
  }
  if (typeof vscode.workspace.onDidGrantWorkspaceTrust === 'function') {
    disposables.push(vscode.workspace.onDidGrantWorkspaceTrust(() => void refresh()));
  }
  if (watcher && typeof watcher.onDidCreate === 'function') {
    const configChanged = (uri) => {
      if (snapshot.states.some((state) => samePath(state.configPath, uri.fsPath))) {
        void refresh();
      }
    };
    disposables.push(watcher.onDidCreate(configChanged));
    disposables.push(watcher.onDidChange(configChanged));
    disposables.push(watcher.onDidDelete(configChanged));
  }

  const showStatus = vscode.commands.registerCommand('mcutrace.showAgentIntegrationStatus', async () => {
    const current = await refresh(false);
    output.clear();
    for (const line of formatStatus(current)) {
      output.appendLine(line);
    }
    if (providerRegistrationError) {
      output.appendLine(
        `MCP provider registration issue: ${providerRegistrationError}. Check VS Code compatibility and extension logs.`
      );
    }
    output.show(true);
  });
  disposables.push(showStatus);
  disposables.push(vscode.commands.registerCommand(
    SETUP_CODEX_COMMAND,
    () => runSetupCodexCommand(vscode, context)
  ));
  context.subscriptions.push(...disposables);

  await refresh(false);
  return { refresh, provider, output, getSnapshot: () => snapshot };
}

module.exports = {
  MCP_PROVIDER_ID,
  MCP_SERVER_VERSION,
  SKILL_CONTEXT_KEY,
  MCP_CONTEXT_KEY,
  SETUP_CODEX_COMMAND,
  WORKSPACE_SKILL_PATH,
  CODEX_CONFIG_PATH,
  readAgentSettings,
  substituteWorkspaceFolder,
  resolveWorkspacePath,
  bundledMcpPath,
  resolveFolderState,
  collectFolderStates,
  createMcpDefinition,
  samePath,
  formatStatus,
  bundledSkillUri,
  workspaceSkillUri,
  codexConfigUri,
  installWorkspaceSkill,
  selectWorkspaceFolder,
  renderMcpConfigBlock,
  hasUserManagedMcpConfig,
  mergeMcpConfig,
  stageMcpExecutable,
  configureWorkspaceMcp,
  runSetupCodexCommand,
  activateAgentIntegration,
};
