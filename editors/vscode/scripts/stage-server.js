'use strict';

const fs = require('node:fs');
const path = require('node:path');

function runtimeExecutableNames(platform = process.platform) {
  const suffix = platform === 'win32' ? '.exe' : '';
  return [`mcutrace-lsp${suffix}`, `mcutrace-mcp${suffix}`];
}

// @req REQ-0179 REQ-0190 REQ-0191
function stageRuntime(options = {}) {
  const platform = options.platform || process.platform;
  const extensionDirectory = options.extensionDirectory || path.resolve(__dirname, '..');
  const sourceDirectory = options.sourceDirectory
    || path.resolve(extensionDirectory, '..', '..', 'build', 'host');
  const skillSource = options.skillSource
    || path.resolve(extensionDirectory, '..', '..', 'integrations', 'skills', 'mcutrace');
  const destinationDirectory = path.join(extensionDirectory, 'bin');
  const skillDestination = path.join(extensionDirectory, 'skills', 'mcutrace');

  fs.mkdirSync(destinationDirectory, { recursive: true });
  for (const executable of runtimeExecutableNames(platform)) {
    const source = path.join(sourceDirectory, executable);
    const destination = path.join(destinationDirectory, executable);
    if (!fs.existsSync(source)) {
      throw new Error(`${executable} not found at ${source}. Build the mcutrace runtime before packaging the extension.`);
    }
    fs.copyFileSync(source, destination);
    if (platform !== 'win32') {
      fs.chmodSync(destination, fs.statSync(source).mode);
    }
  }

  if (!fs.existsSync(path.join(skillSource, 'SKILL.md'))) {
    throw new Error(`Canonical mcutrace skill not found at ${skillSource}.`);
  }
  fs.rmSync(skillDestination, { recursive: true, force: true });
  fs.mkdirSync(path.dirname(skillDestination), { recursive: true });
  fs.cpSync(skillSource, skillDestination, { recursive: true });

  return {
    executables: runtimeExecutableNames(platform).map((name) => path.join(destinationDirectory, name)),
    skill: path.join(skillDestination, 'SKILL.md'),
  };
}

if (require.main === module) {
  const staged = stageRuntime({
    sourceDirectory: process.argv[2] || process.env.MCUTRACE_RUNTIME_DIRECTORY,
    skillSource: process.argv[3] || process.env.MCUTRACE_SKILL_DIRECTORY,
  });
  console.log(`Staged bundled runtimes: ${staged.executables.join(', ')}`);
  console.log(`Staged canonical Agent Skill: ${staged.skill}`);
}

module.exports = { runtimeExecutableNames, stageRuntime };
