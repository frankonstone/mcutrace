'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const test = require('node:test');

const { runtimeExecutableNames, stageRuntime } = require('../scripts/stage-server');

test('stages both runtimes and the canonical skill in extension package paths [REQ-0190] [REQ-0191]', (t) => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'mcutrace-vscode-stage-'));
  t.after(() => fs.rmSync(root, { recursive: true, force: true }));
  const sourceDirectory = path.join(root, 'runtime');
  const skillSource = path.join(root, 'canonical-skill');
  const extensionDirectory = path.join(root, 'extension');
  fs.mkdirSync(sourceDirectory, { recursive: true });
  fs.mkdirSync(path.join(skillSource, 'agents'), { recursive: true });
  for (const executable of runtimeExecutableNames('linux')) {
    fs.writeFileSync(path.join(sourceDirectory, executable), executable, { mode: 0o755 });
  }
  fs.writeFileSync(path.join(skillSource, 'SKILL.md'), '# canonical\n');
  fs.writeFileSync(path.join(skillSource, 'agents', 'openai.yaml'), 'interface: {}\n');

  const staged = stageRuntime({
    platform: 'linux', extensionDirectory, sourceDirectory, skillSource,
  });
  assert.equal(staged.executables.length, 2);
  assert.equal(fs.readFileSync(path.join(extensionDirectory, 'bin', 'mcutrace-lsp'), 'utf8'), 'mcutrace-lsp');
  assert.equal(fs.readFileSync(path.join(extensionDirectory, 'bin', 'mcutrace-mcp'), 'utf8'), 'mcutrace-mcp');
  assert.equal(fs.statSync(path.join(extensionDirectory, 'bin', 'mcutrace-mcp')).mode & 0o111, 0o111);
  assert.equal(fs.readFileSync(staged.skill, 'utf8'), '# canonical\n');
  assert.ok(fs.existsSync(path.join(extensionDirectory, 'skills', 'mcutrace', 'agents', 'openai.yaml')));
});

test('uses platform-correct executable names [REQ-0191]', () => {
  assert.deepEqual(runtimeExecutableNames('linux'), ['mcutrace-lsp', 'mcutrace-mcp']);
  assert.deepEqual(runtimeExecutableNames('darwin'), ['mcutrace-lsp', 'mcutrace-mcp']);
  assert.deepEqual(runtimeExecutableNames('win32'), ['mcutrace-lsp.exe', 'mcutrace-mcp.exe']);
});
