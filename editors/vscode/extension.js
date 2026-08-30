'use strict';

const vscode = require('vscode');
const { LanguageClient } = require('vscode-languageclient/node');

const { clientOptions, serverOptions } = require('./client');
const { buildTraceTree } = require('./trace-view');

let languageClient;

const PROBLEM_COLORS = {
  error: 'problemsErrorIcon.foreground',
  warning: 'problemsWarningIcon.foreground',
  note: 'problemsInfoIcon.foreground',
};

class TraceDecorationProvider {
  constructor() {
    this.decorations = new Map();
    this.changed = new vscode.EventEmitter();
    this.onDidChangeFileDecorations = this.changed.event;
  }

  update(roots) {
    this.decorations.clear();
    const visit = (node) => {
      if (node.problemSeverity) {
        this.decorations.set(node.decorationId, {
          badge: node.problemBadge,
          tooltip: node.problemTooltip,
          color: new vscode.ThemeColor(PROBLEM_COLORS[node.problemSeverity]),
          propagate: false,
        });
      }
      node.children.forEach(visit);
    };
    roots.forEach(visit);
    this.changed.fire(undefined);
  }

  provideFileDecoration(uri) {
    return this.decorations.get(uri.path.slice(1));
  }

  dispose() {
    this.changed.dispose();
  }
}

class TraceTreeProvider {
  constructor(decorations, updateBadge) {
    this.roots = [];
    this.decorations = decorations;
    this.updateBadge = updateBadge;
    this.changed = new vscode.EventEmitter();
    this.onDidChangeTreeData = this.changed.event;
  }

  update(payload) {
    this.roots = buildTraceTree(payload, (path) => vscode.workspace.asRelativePath(path));
    this.decorations.update(this.roots);
    this.updateBadge(Array.isArray(payload.diagnostics) ? payload.diagnostics.length : 0);
    this.changed.fire();
  }

  getChildren(element) {
    return element ? element.children : this.roots;
  }

  getTreeItem(element) {
    const state = element.children.length > 0
      ? vscode.TreeItemCollapsibleState.Collapsed
      : vscode.TreeItemCollapsibleState.None;
    const item = new vscode.TreeItem(element.label, state);
    item.description = element.description;
    item.tooltip = element.tooltip;
    item.contextValue = element.contextValue;
    item.iconPath = new vscode.ThemeIcon(element.icon);
    item.resourceUri = vscode.Uri.parse(`mcutrace:/${element.decorationId}`);
    if (element.problemTooltip) {
      item.tooltip = [item.tooltip, element.problemTooltip].filter(Boolean).join('\n\n');
    }
    if (element.location) {
      item.command = {
        command: 'mcutrace.openTraceLocation',
        title: 'Open trace location',
        arguments: [element.location],
      };
    }
    return item;
  }

  dispose() {
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

// @req REQ-0098 REQ-0099 REQ-0100 REQ-0101 REQ-0102 REQ-0124 REQ-0127
async function activate(context) {
  const output = vscode.window.createOutputChannel('mcutrace Language Server');
  const configuration = vscode.workspace.getConfiguration('mcutrace');
  const watcher = vscode.workspace.createFileSystemWatcher('**/*');
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
  const traceSubscription = languageClient.onNotification(
    'mcutrace/publishTrace', (payload) => traceProvider.update(payload));

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
