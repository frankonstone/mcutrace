'use strict';

const vscode = require('vscode');
const { LanguageClient } = require('vscode-languageclient/node');

const { clientOptions, serverOptions } = require('./client');

let languageClient;

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

// @req REQ-0098 REQ-0099 REQ-0100 REQ-0101 REQ-0102 REQ-0124
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

  context.subscriptions.push(
    output,
    watcher,
    { dispose: () => { void languageClient.stop(); } },
    vscode.commands.registerCommand('mcutrace.refreshRequirements', refreshAnalysis),
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
