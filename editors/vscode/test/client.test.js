'use strict';

const assert = require('node:assert/strict');
const test = require('node:test');

const {
  bundledServerPath,
  clientOptions,
  replaceWorkspaceFolder,
  serverArguments,
  serverOptions,
} = require('../client');

test('expands workspace folder placeholders in language-server settings [REQ-0124]', () => {
  assert.equal(
    replaceWorkspaceFolder('${workspaceFolder}/build/mcutrace-lsp', '/project'),
    '/project/build/mcutrace-lsp');
  assert.equal(replaceWorkspaceFolder('', '/project'), undefined);
  assert.equal(replaceWorkspaceFolder('${workspaceFolder}/server'), '${workspaceFolder}/server');
});

test('builds server options without embedding requirement semantics [REQ-0100] [REQ-0124]', () => {
  const values = new Map([
    ['languageServer.path', '${workspaceFolder}/build/trace/mcutrace-lsp'],
    ['languageServer.arguments', ['--stdio', '${workspaceFolder}/mcutrace.toml', 42]],
  ]);
  const configuration = {
    get(key, fallback) {
      return values.has(key) ? values.get(key) : fallback;
    },
  };

  assert.deepEqual(serverOptions(configuration, '/project', '/extension'), {
    command: '/project/build/trace/mcutrace-lsp',
    args: ['--stdio', '/project/mcutrace.toml'],
    options: { cwd: '/project' },
  });
  assert.deepEqual(serverArguments('not-an-array', '/project'), []);
});

test('uses the bundled language server when no override is configured [REQ-0100] [REQ-0124]', () => {
  const configuration = {
    get(_key, fallback) {
      return fallback;
    },
  };

  assert.equal(bundledServerPath('/extension'), '/extension/bin/mcutrace-lsp');
  assert.equal(bundledServerPath('/extension', 'win32'), '/extension/bin/mcutrace-lsp.exe');
  assert.deepEqual(serverOptions(configuration, '/project', '/extension'), {
    command: '/extension/bin/mcutrace-lsp',
    args: [],
    options: { cwd: '/project' },
  });
});

test('delegates editor synchronization, diagnostics, and navigation to the language server [REQ-0098] [REQ-0099] [REQ-0101] [REQ-0102]', () => {
  const watcher = { dispose() {} };
  const output = { appendLine() {} };
  assert.deepEqual(clientOptions(watcher, output), {
    documentSelector: [{ scheme: 'file' }, { scheme: 'untitled' }],
    outputChannel: output,
    synchronize: {
      configurationSection: 'mcutrace',
      fileEvents: watcher,
    },
  });
});
