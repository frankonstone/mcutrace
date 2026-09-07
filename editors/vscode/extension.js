'use strict';

const vscode = require('vscode');
const { LanguageClient } = require('vscode-languageclient/node');

const { clientOptions, serverOptions } = require('./client');
const { activateAgentIntegration } = require('./agent-integration');
const {
  buildTraceTree, findTestDeclaration, requirementAtLocation, requirementNodes,
} = require('./trace-view');

let languageClient;

const PROBLEM_COLORS = {
  error: 'problemsErrorIcon.foreground',
  warning: 'problemsWarningIcon.foreground',
  note: 'problemsInfoIcon.foreground',
};
const TRACE_UPDATE_DELAY_MS = 100;

function sameDecoration(left, right) {
  return left?.badge === right?.badge && left?.tooltip === right?.tooltip &&
    left?.propagate === right?.propagate;
}

class TraceDecorationProvider {
  constructor() {
    this.decorations = new Map();
    this.changed = new vscode.EventEmitter();
    this.onDidChangeFileDecorations = this.changed.event;
  }

  update(roots) {
    const next = new Map();
    const visit = (node) => {
      if (node.problemSeverity) {
        next.set(node.decorationId, {
          badge: node.problemBadge,
          tooltip: node.problemTooltip,
          propagate: false,
        });
      }
      node.children.forEach(visit);
    };
    roots.forEach(visit);
    const changed = this.decorations.size !== next.size ||
      [...next].some(([id, decoration]) =>
        !sameDecoration(this.decorations.get(id), decoration));
    this.decorations = next;
    if (changed) {
      this.changed.fire(undefined);
    }
  }

  provideFileDecoration(uri) {
    return uri.scheme === 'mcutrace'
      ? this.decorations.get(decodeURIComponent(uri.path.slice(1))) : undefined;
  }

  dispose() {
    this.changed.dispose();
  }
}

class TraceTreeProvider {
  constructor(decorations, updateBadge) {
    this.roots = [];
    this.parents = new Map();
    this.problemsOnly = false;
    this.decorations = decorations;
    this.updateBadge = updateBadge;
    this.changed = new vscode.EventEmitter();
    this.onDidChangeTreeData = this.changed.event;
    this.pendingPayload = undefined;
    this.updateTimer = undefined;
    this.lastPayload = undefined;
  }

  // @req REQ-0220
  scheduleUpdate(payload) {
    this.pendingPayload = payload;
    if (this.updateTimer === undefined) {
      this.updateTimer = setTimeout(() => this.flushUpdates(), TRACE_UPDATE_DELAY_MS);
    }
  }

  flushUpdates() {
    if (this.updateTimer !== undefined) {
      clearTimeout(this.updateTimer);
      this.updateTimer = undefined;
    }
    if (this.pendingPayload === undefined) {
      return false;
    }
    const payload = this.pendingPayload;
    this.pendingPayload = undefined;
    const serialized = JSON.stringify(payload);
    if (serialized === this.lastPayload) {
      return false;
    }
    this.roots = buildTraceTree(payload, (path) => vscode.workspace.asRelativePath(path));
    this.parents.clear();
    const visit = (nodes, parent) => nodes.forEach((node) => {
      this.parents.set(node, parent);
      visit(node.children, node);
    });
    visit(this.roots);
    this.decorations.update(this.roots);
    this.updateBadge(Array.isArray(payload.diagnostics) ? payload.diagnostics.length : 0);
    this.lastPayload = serialized;
    this.changed.fire();
    return true;
  }

  getChildren(element) {
    const children = element ? element.children : this.roots;
    return this.problemsOnly ? children.filter((node) => node.problemSeverity) : children;
  }

  getParent(element) {
    return this.parents.get(element);
  }

  setProblemsOnly(enabled) {
    this.problemsOnly = enabled;
    this.changed.fire();
  }

  getTreeItem(element) {
    const state = this.getChildren(element).length > 0
      ? vscode.TreeItemCollapsibleState.Collapsed
      : vscode.TreeItemCollapsibleState.None;
    const item = new vscode.TreeItem(element.label, state);
    item.id = element.id;
    item.description = element.description;
    item.tooltip = element.tooltip;
    item.contextValue = element.contextValue;
    const severity = element.problemIconSeverity;
    item.iconPath = severity
      ? new vscode.ThemeIcon(severity === 'note' ? 'info' : severity,
        new vscode.ThemeColor(PROBLEM_COLORS[severity]))
      : new vscode.ThemeIcon(element.icon);
    item.resourceUri = vscode.Uri.from({
      scheme: 'mcutrace', path: `/${encodeURIComponent(element.decorationId)}`,
    });
    if (element.problemTooltip) {
      item.tooltip = [item.tooltip, element.problemTooltip].filter(Boolean).join('\n\n');
    }
    if (element.test) {
      item.command = {
        command: 'mcutrace.openTestLocation',
        title: 'Open test declaration',
        arguments: [element.test],
      };
    } else if (element.location) {
      item.command = {
        command: 'mcutrace.openTraceLocation',
        title: 'Open trace location',
        arguments: [element.location],
      };
    }
    return item;
  }

  dispose() {
    if (this.updateTimer !== undefined) {
      clearTimeout(this.updateTimer);
      this.updateTimer = undefined;
    }
    this.changed.dispose();
  }
}

function sourceSelection(source) {
  const start = new vscode.Position(
    Math.max(0, (source.line || 1) - 1),
    Math.max(0, (source.column || 1) - 1));
  const end = new vscode.Position(
    Math.max(start.line, (source.end_line || source.line || 1) - 1),
    Math.max(0, (source.end_column || source.column || 1) - 1));
  return new vscode.Range(start, end);
}

async function openTraceLocation(location) {
  const source = location.source;
  const uri = source ? vscode.Uri.file(source.path) : vscode.Uri.parse(location.uri);
  let selection;
  if (source) {
    selection = sourceSelection(source);
  } else if (location.range) {
    selection = new vscode.Range(
      new vscode.Position(location.range.start.line, location.range.start.character),
      new vscode.Position(location.range.end.line, location.range.end.character));
  }
  await vscode.window.showTextDocument(uri, { selection });
}

// @req REQ-0128
async function openTestLocation(test) {
  if (test.location) {
    await openTraceLocation(test.location);
    return;
  }

  const files = await vscode.workspace.findFiles(
    '**/*.{c,cc,cpp,cxx,h,hh,hpp,hxx}',
    '**/{.git,node_modules,build,out}/**');
  files.sort((left, right) => {
    const leftIsTest = /(^|[/\\])tests?([/\\]|$)/i.test(left.fsPath) ? 0 : 1;
    const rightIsTest = /(^|[/\\])tests?([/\\]|$)/i.test(right.fsPath) ? 0 : 1;
    return leftIsTest - rightIsTest || left.fsPath.localeCompare(right.fsPath);
  });

  for (const uri of files) {
    let source;
    try {
      source = Buffer.from(await vscode.workspace.fs.readFile(uri)).toString('utf8');
    } catch {
      continue;
    }
    const declaration = findTestDeclaration(source, test.name);
    if (declaration) {
      const document = await vscode.workspace.openTextDocument(uri);
      const selection = new vscode.Range(
        new vscode.Position(declaration.line, declaration.character),
        new vscode.Position(declaration.endLine, declaration.endCharacter));
      await vscode.window.showTextDocument(document, { selection });
      return;
    }
  }

  vscode.window.showWarningMessage(`Unable to find test declaration: ${test.name}`);
}

function workspaceFolderPath() {
  const [folder] = vscode.workspace.workspaceFolders || [];
  return folder ? folder.uri.fsPath : undefined;
}

async function refreshAnalysis() {
  if (!languageClient) {
    return;
  }
  await languageClient.sendNotification('workspace/didChangeWatchedFiles', { changes: [] });
  vscode.window.setStatusBarMessage('mcutrace analysis refreshed', 2000);
}

function reportServerSettingChange(event) {
  if (event.affectsConfiguration('mcutrace.languageServer')) {
    vscode.window.showInformationMessage(
      'mcutrace language-server settings changed. Reload the window to apply them.');
  }
}

// @req REQ-0098 REQ-0099 REQ-0100 REQ-0101 REQ-0102 REQ-0124 REQ-0127 REQ-0128
async function activate(context) {
  const output = vscode.window.createOutputChannel('mcutrace Language Server');
  const configuration = vscode.workspace.getConfiguration('mcutrace');
  const watcher = vscode.workspace.createFileSystemWatcher('**/*');
  await activateAgentIntegration(vscode, context, watcher);
  const server = serverOptions(configuration, workspaceFolderPath(), context.extensionPath);
  languageClient = new LanguageClient(
    'mcutraceLanguageServer',
    'mcutrace Language Server',
    server,
    clientOptions(watcher, output));
  const traceDecorations = new TraceDecorationProvider();
  let traceView;
  const traceProvider = new TraceTreeProvider(traceDecorations, (count) => {
    traceView.badge = count > 0
      ? { value: count, tooltip: `${count} trace problem${count === 1 ? '' : 's'}` }
      : undefined;
  });
  traceView = vscode.window.createTreeView('mcutrace.traceView', {
    treeDataProvider: traceProvider,
    showCollapseAll: true,
  });
  const setProblemsOnly = async (enabled) => {
    traceProvider.setProblemsOnly(enabled);
    traceView.message = enabled ? 'Showing items with trace problems' : undefined;
    await vscode.commands.executeCommand('setContext', 'mcutrace.traceProblemsOnly', enabled);
  };
  await setProblemsOnly(false);
  const revealRequirement = async (node) => {
    if (traceProvider.problemsOnly && !node.problemSeverity) {
      await setProblemsOnly(false);
    }
    await traceView.reveal(node, { select: true, focus: true });
  };
  const findRequirement = async () => {
    const items = requirementNodes(traceProvider.roots).map((node) => ({
      label: node.label,
      description: node.requirementId,
      detail: node.location?.uri ? vscode.workspace.asRelativePath(vscode.Uri.parse(node.location.uri)) : undefined,
      node,
    }));
    const picked = await vscode.window.showQuickPick(items, {
      placeHolder: 'Find requirement by title or ID',
      matchOnDescription: true,
    });
    if (picked) {
      // Analysis may have refreshed while the picker was open.
      const node = requirementNodes(traceProvider.roots).find((entry) => entry.id === picked.node.id);
      if (node) await revealRequirement(node);
    }
  };
  const revealActiveRequirement = async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor) return;
    const position = editor.selection.active;
    const node = requirementAtLocation(traceProvider.roots,
      editor.document.uri.toString(), position.line);
    if (node) {
      await revealRequirement(node);
    } else {
      vscode.window.showInformationMessage('No requirement at the current cursor position.');
    }
  };
  const traceSubscription = languageClient.onNotification(
    'mcutrace/publishTrace', (payload) => traceProvider.scheduleUpdate(payload));

  context.subscriptions.push(
    output,
    watcher,
    traceProvider,
    traceDecorations,
    vscode.window.registerFileDecorationProvider(traceDecorations),
    traceView,
    traceSubscription,
    { dispose: () => { void languageClient.stop(); } },
    vscode.commands.registerCommand('mcutrace.refreshRequirements', refreshAnalysis),
    vscode.commands.registerCommand('mcutrace.openTraceLocation', openTraceLocation),
    vscode.commands.registerCommand('mcutrace.openTestLocation', openTestLocation),
    vscode.commands.registerCommand('mcutrace.findRequirement', findRequirement),
    vscode.commands.registerCommand('mcutrace.revealActiveRequirement', revealActiveRequirement),
    vscode.commands.registerCommand('mcutrace.showTraceProblems', () => setProblemsOnly(true)),
    vscode.commands.registerCommand('mcutrace.showAllTraceItems', () => setProblemsOnly(false)),
    vscode.workspace.onDidChangeConfiguration(reportServerSettingChange),
  );

  try {
    await languageClient.start();
    output.appendLine(`Started mcutrace language server: ${server.command}`);
  } catch (error) {
    const detail = error instanceof Error ? error.message : String(error);
    output.appendLine(`Unable to start mcutrace language server: ${detail}`);
    vscode.window.showErrorMessage(`Unable to start mcutrace language server: ${detail}`);
    throw error;
  }
}

async function deactivate() {
  if (languageClient) {
    await languageClient.stop();
    languageClient = undefined;
  }
}

module.exports = { activate, deactivate };
