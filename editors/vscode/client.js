'use strict';

const path = require('node:path');

const DEFAULT_SERVER_PATH = 'mcutrace-lsp';
const DOCUMENT_SELECTOR = [{ scheme: 'file' }, { scheme: 'untitled' }];

function replaceWorkspaceFolder(value, workspaceFolder) {
  if (typeof value !== 'string' || value.length === 0) {
    return undefined;
  }
  return workspaceFolder
    ? value.replaceAll('${workspaceFolder}', workspaceFolder)
    : value;
}

function serverArguments(value, workspaceFolder) {
  if (!Array.isArray(value)) {
    return [];
  }
  return value
    .map((argument) => replaceWorkspaceFolder(argument, workspaceFolder))
    .filter((argument) => argument !== undefined);
}

function bundledServerPath(extensionPath, platform = process.platform) {
  if (typeof extensionPath !== 'string' || extensionPath.length === 0) {
    return DEFAULT_SERVER_PATH;
  }
  const executable = platform === 'win32' ? 'mcutrace-lsp.exe' : 'mcutrace-lsp';
  return path.join(extensionPath, 'bin', executable);
}

// @req REQ-0100 REQ-0124
function serverOptions(configuration, workspaceFolder, extensionPath) {
  const configuredPath = configuration.get('languageServer.path', '');
  const command = replaceWorkspaceFolder(configuredPath, workspaceFolder)
    || bundledServerPath(extensionPath);
  const argumentsValue = configuration.get('languageServer.arguments', []);
  const options = workspaceFolder ? { cwd: workspaceFolder } : {};
  return {
    command,
    args: serverArguments(argumentsValue, workspaceFolder),
    options,
  };
}

// @req REQ-0098 REQ-0099 REQ-0101 REQ-0102
function clientOptions(fileWatcher, outputChannel) {
  return {
    documentSelector: DOCUMENT_SELECTOR,
    outputChannel,
    synchronize: {
      configurationSection: 'mcutrace',
      fileEvents: fileWatcher,
    },
  };
}

module.exports = {
  bundledServerPath,
  clientOptions,
  replaceWorkspaceFolder,
  serverArguments,
  serverOptions,
};
