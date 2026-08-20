'use strict';

const fs = require('node:fs');
const path = require('node:path');

const executable = process.platform === 'win32' ? 'mcutrace-lsp.exe' : 'mcutrace-lsp';
const defaultSource = path.resolve(__dirname, '..', '..', '..', 'build', 'host', executable);
const source = process.argv[2] || process.env.MCUTRACE_LSP_BINARY || defaultSource;
const destinationDirectory = path.resolve(__dirname, '..', 'bin');
const destination = path.join(destinationDirectory, executable);

if (!fs.existsSync(source)) {
  throw new Error(
    `mcutrace language server not found at ${source}. Build mcutrace-lsp before packaging the extension.`);
}

fs.mkdirSync(destinationDirectory, { recursive: true });
fs.copyFileSync(source, destination);
if (process.platform !== 'win32') {
  fs.chmodSync(destination, fs.statSync(source).mode);
}
console.log(`Staged bundled language server: ${destination}`);
