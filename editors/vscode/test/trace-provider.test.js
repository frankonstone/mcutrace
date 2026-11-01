'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { createRequire } = require('node:module');
const test = require('node:test');

// Exercise the registered provider and commands through the extension's public
// activation entry point, without starting a language server or editor window.
async function activateExtension() {
  const commands = new Map();
  const contexts = new Map();
  const notifications = new Map();
  const state = {};
  const disposable = { dispose() {} };
  class EventEmitter {
    constructor() { this.fireCount = 0; }
    event = () => disposable;
    fire() { this.fireCount += 1; }
    dispose() {}
  }
  const vscode = {
    EventEmitter,
    TreeItemCollapsibleState: { None: 0, Collapsed: 1, Expanded: 2 },
    TreeItem: class { constructor(label, collapsibleState) { Object.assign(this, { label, collapsibleState }); } },
    ThemeIcon: class { constructor(id, color) { Object.assign(this, { id, color }); } },
    ThemeColor: class { constructor(id) { this.id = id; } },
    Uri: {
      from: (parts) => ({ ...parts }),
      parse: (value) => ({ scheme: 'file', path: value.slice(7), toString: () => value }),
    },
    workspace: {
      workspaceFolders: [],
      getConfiguration: () => ({ get: (_key, fallback) => fallback }),
      createFileSystemWatcher: () => disposable,
      onDidChangeConfiguration: () => disposable,
      asRelativePath: (value) => (typeof value === 'string' ? value : value.path).replace('/work/', ''),
    },
    window: {
      createOutputChannel: () => ({ ...disposable, appendLine() {} }),
      createTreeView: (_id, options) => {
        state.provider = options.treeDataProvider;
        state.view = { ...disposable, reveal: async (node) => { state.revealed = node; } };
        return state.view;
      },
      registerFileDecorationProvider: (provider) => { state.decorations = provider; return disposable; },
      showQuickPick: async (items, options) => {
        state.pickerOptions = options;
        return items.find((item) => item.description === 'REQ-0002');
      },
      showInformationMessage: (message) => { state.message = message; },
    },
    commands: {
      registerCommand: (name, callback) => { commands.set(name, callback); return disposable; },
      executeCommand: async (_command, key, value) => { contexts.set(key, value); },
    },
  };
  class LanguageClient {
    onNotification(name, callback) { notifications.set(name, callback); return disposable; }
    async start() {}
    async stop() {}
  }
  const filename = path.resolve(__dirname, '../extension.js');
  const localRequire = createRequire(filename);
  const sandbox = {
    module: { exports: {} },
    require: (name) => name === 'vscode' ? vscode : name === 'vscode-languageclient/node'
      ? { LanguageClient } : localRequire(name),
    Buffer,
    clearTimeout,
    setTimeout,
  };
  vm.runInNewContext(fs.readFileSync(filename, 'utf8'), sandbox, { filename });
  await sandbox.module.exports.activate({ extensionPath: '/extension', subscriptions: [] });
  return {
    state, vscode, commands, contexts,
    publish: notifications.get('mcutrace/publishTrace'),
  };
}

test('renders neutral labels, filters problems, and reveals searched and active requirements [REQ-0127]', async () => {
  const { state, vscode, commands, contexts, publish } = await activateExtension();
  const uri = 'file:///work/docs/100%25%20ready.md';
  publish({
    documents: [{
      path: '/work/docs/100% ready.md', uri,
      headings: [1, 2].map((number) => ({
        kind: 'requirement', id: `REQ-000${number}`, title: `Feature ${number}`, headingLevel: 2,
        location: { uri, range: { start: { line: number * 3, character: 4 } } },
      })),
    }],
    diagnostics: [{ severity: 'warning', code: 'missing', message: 'REQ-0001 has no test' }],
  });
  state.provider.flushUpdates();
  const provider = state.provider;
  const [folder] = provider.getChildren();
  const [document] = provider.getChildren(folder);
  const [affected, clean] = provider.getChildren(document);
  const folderItem = provider.getTreeItem(folder);
  const affectedItem = provider.getTreeItem(affected);
  assert.equal(folderItem.collapsibleState, vscode.TreeItemCollapsibleState.Collapsed);
  assert.equal(folderItem.iconPath.id, 'folder');
  assert.equal(folderItem.iconPath.color, undefined);
  assert.equal(affectedItem.label, 'Feature 1');
  assert.equal(affectedItem.description, 'REQ-0001');
  assert.equal(affectedItem.id, affected.id);
  assert.equal(affectedItem.iconPath.id, 'warning');
  assert.equal(affectedItem.iconPath.color.id, 'problemsWarningIcon.foreground');
  assert.equal(affectedItem.command.command, 'mcutrace.openTraceLocation');
  const decoration = state.decorations.provideFileDecoration(affectedItem.resourceUri);
  assert.equal(decoration.badge, 'W');
  assert.equal(decoration.color, undefined);
  assert.equal(state.decorations.provideFileDecoration({ scheme: 'file', path: '/anything' }), undefined);
  assert.match(provider.getTreeItem(folder).tooltip, /Problems inside: 1 warning/);
  assert.equal(provider.getParent(affected), document);
  assert.equal(provider.getParent(document), folder);
  assert.equal(provider.getParent(folder), undefined);

  await commands.get('mcutrace.showTraceProblems')();
  assert.equal(contexts.get('mcutrace.traceProblemsOnly'), true);
  assert.deepEqual(provider.getChildren(document), [affected]);
  await commands.get('mcutrace.findRequirement')();
  assert.equal(state.pickerOptions.matchOnDescription, true);
  assert.equal(state.revealed, clean);
  assert.equal(contexts.get('mcutrace.traceProblemsOnly'), false);
  assert.equal(state.view.message, undefined);

  vscode.window.activeTextEditor = {
    document: { uri: { toString: () => uri } }, selection: { active: { line: 4, character: 0 } },
  };
  await commands.get('mcutrace.revealActiveRequirement')();
  assert.equal(state.revealed, affected);
  await commands.get('mcutrace.showTraceProblems')();
  publish({ documents: [] });
  state.provider.flushUpdates();
  assert.equal(provider.getChildren().length, 0);
  assert.equal(state.view.badge, undefined);
  assert.equal(state.decorations.provideFileDecoration(affectedItem.resourceUri), undefined);
});

test('renders warning icons on linked requirement copies [REQ-0127]', async () => {
  const { state, publish } = await activateExtension();
  const uri = 'file:///work/docs/requirements.md';
  publish({
    documents: [{
      path: '/work/docs/requirements.md', uri,
      headings: [{
        kind: 'requirement', id: 'REQ-0001', title: 'Affected', headingLevel: 2,
        location: { uri, range: { start: { line: 1, character: 0 } } },
      }, {
        kind: 'requirement', id: 'REQ-0002', title: 'Consumer', headingLevel: 2,
        trace: {
          prerequisites: [{ id: 'REQ-0001', kind: 'requirement', label: 'Affected' }],
        },
      }],
    }],
    diagnostics: [{ severity: 'warning', code: 'missing', message: 'REQ-0001 has no test' }],
  });
  state.provider.flushUpdates();

  const provider = state.provider;
  const [folder] = provider.getChildren();
  const [document] = provider.getChildren(folder);
  const [, consumer] = provider.getChildren(document);
  const [dependsOn] = provider.getChildren(consumer);
  const [linkedRequirement] = provider.getChildren(dependsOn);

  const linkedItem = provider.getTreeItem(linkedRequirement);
  assert.equal(linkedItem.iconPath.id, 'warning');
  assert.equal(linkedItem.iconPath.color.id, 'problemsWarningIcon.foreground');
  assert.equal(provider.getTreeItem(dependsOn).iconPath.id, 'references');
  assert.equal(provider.getTreeItem(consumer).iconPath.id, 'checklist');
});

test('excludes informative diagnostics from the tree problem count [REQ-0127]', async () => {
  const { state, publish } = await activateExtension();
  publish({
    documents: [],
    diagnostics: [
      { severity: 'note', code: 'info', message: 'Informative context' },
      { severity: 'warning', code: 'warning', message: 'Action required' },
    ],
  });
  state.provider.flushUpdates();

  assert.equal(state.view.badge.value, 1);
  assert.equal(state.view.badge.tooltip, '1 trace problem');

  publish({
    documents: [],
    diagnostics: [{ severity: 'note', code: 'info', message: 'Informative context' }],
  });
  state.provider.flushUpdates();
  assert.equal(state.view.badge, undefined);
});

test('renders an in-progress icon for plan items [REQ-0127]', async () => {
  const { state, publish } = await activateExtension();
  publish({
    documents: [{
      path: '/work/implementation-plan.md',
      uri: 'file:///work/implementation-plan.md',
      headings: [{
        kind: 'plan', id: 'PLN-0001', title: 'Implement feature',
        status: 'in_progress', headingLevel: 2,
      }],
    }],
  });
  state.provider.flushUpdates();

  const [document] = state.provider.getChildren();
  const [plan] = state.provider.getChildren(document);
  assert.equal(state.provider.getTreeItem(plan).iconPath.id, 'loading~spin');
});

test('coalesces trace bursts and skips identical refreshes [REQ-0220]', async () => {
  const { state, publish } = await activateExtension();
  const payload = (title, diagnostics = []) => ({
    documents: [{
      path: '/work/docs/requirements.md',
      uri: 'file:///work/docs/requirements.md',
      headings: [{
        kind: 'requirement', id: 'REQ-0001', title, headingLevel: 2,
      }],
    }],
    diagnostics,
  });
  const provider = state.provider;
  const treeRefreshes = provider.changed.fireCount;
  const decorationRefreshes = state.decorations.changed.fireCount;

  publish(payload('Intermediate'));
  publish(payload('Latest'));
  assert.equal(provider.flushUpdates(), true);
  assert.equal(provider.changed.fireCount, treeRefreshes + 1);
  assert.equal(provider.getChildren()[0].children[0].children[0].label, 'Latest');
  assert.equal(state.decorations.changed.fireCount, decorationRefreshes);

  publish(payload('Latest'));
  assert.equal(provider.flushUpdates(), false);
  assert.equal(provider.changed.fireCount, treeRefreshes + 1);
  assert.equal(state.decorations.changed.fireCount, decorationRefreshes);

  publish(payload('Latest', [{
    severity: 'warning', code: 'missing', message: 'REQ-0001 has no test',
  }]));
  assert.equal(provider.flushUpdates(), true);
  assert.equal(provider.changed.fireCount, treeRefreshes + 2);
  assert.equal(state.decorations.changed.fireCount, decorationRefreshes + 1);
});

test('applies a scheduled trace update after the bounded delay [REQ-0220]', async () => {
  const { state, publish } = await activateExtension();
  publish({
    documents: [{
      path: '/work/requirements.md',
      uri: 'file:///work/requirements.md',
      headings: [{
        kind: 'requirement', id: 'REQ-0001', title: 'Rendered', headingLevel: 2,
      }],
    }],
  });
  assert.equal(state.provider.getChildren().length, 0);
  await new Promise((resolve) => setTimeout(resolve, 125));
  assert.equal(state.provider.getChildren()[0].children[0].label, 'Rendered');
});
