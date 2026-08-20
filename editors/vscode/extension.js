'use strict';

const vscode = require('vscode');
const {
  parseRequirementDocument,
  requirementAtPosition,
  requirementRanges,
} = require('./requirements');
const { evidenceDetails, relatedEvidence, testDeclarations } = require('./trace');

const DEFAULT_GLOBS = ['**/requirements*.md'];
const DEFAULT_EXCLUDE = '**/{.git,build,node_modules}/**';
const DEFAULT_TRACE_REPORT = 'build/mcutrace-report.json';
const TEST_SOURCE_GLOB = '**/*.{c,cc,cpp,cxx}';
const OPEN_TRACE_LOCATION = 'mcutrace.openTraceLocation';

// @req REQ-0100
class RequirementIndex {
  constructor(output) {
    this.output = output;
    this.byId = new Map();
    this.refreshVersion = 0;
    this.refreshTimer = undefined;
  }

  get(id) {
    return this.byId.get(id) || [];
  }

  scheduleRefresh() {
    if (this.refreshTimer) {
      clearTimeout(this.refreshTimer);
    }
    this.refreshTimer = setTimeout(() => {
      this.refresh().catch((error) => {
        this.output.appendLine(`Failed to refresh requirements: ${error.message}`);
      });
    }, 150);
  }

  async refresh() {
    const version = ++this.refreshVersion;
    const configuration = vscode.workspace.getConfiguration('mcutrace');
    const configured = configuration.get('requirementFiles', DEFAULT_GLOBS);
    const globs = Array.isArray(configured) && configured.length > 0
      ? configured.filter((value) => typeof value === 'string' && value.length > 0)
      : DEFAULT_GLOBS;
    const exclude = configuration.get('exclude', DEFAULT_EXCLUDE);

    const discovered = new Map();
    for (const glob of globs) {
      const uris = await vscode.workspace.findFiles(glob, exclude);
      for (const uri of uris) {
        discovered.set(uri.toString(), uri);
      }
    }

    const next = new Map();
    const uris = [...discovered.values()].sort((left, right) =>
      left.toString().localeCompare(right.toString()));
    for (const uri of uris) {
      const open = vscode.workspace.textDocuments.find(
        (document) => document.uri.toString() === uri.toString());
      const content = open
        ? open.getText()
        : Buffer.from(await vscode.workspace.fs.readFile(uri)).toString('utf8');
      for (const parsed of parseRequirementDocument(content)) {
        const definition = { ...parsed, uri };
        const definitions = next.get(definition.id) || [];
        definitions.push(definition);
        next.set(definition.id, definitions);
      }
    }

    if (version !== this.refreshVersion) {
      return;
    }
    this.byId = next;
    const definitionCount = [...next.values()]
      .reduce((count, definitions) => count + definitions.length, 0);
    this.output.appendLine(
      `Indexed ${definitionCount} requirement definitions from ${uris.length} files.`);
  }

  dispose() {
    if (this.refreshTimer) {
      clearTimeout(this.refreshTimer);
    }
  }
}

class TestLocationIndex {
  constructor() {
    this.byName = new Map();
  }

  get(name) {
    return this.byName.get(name);
  }

  async refresh() {
    const uris = await vscode.workspace.findFiles(TEST_SOURCE_GLOB, DEFAULT_EXCLUDE);
    const next = new Map();
    const sorted = [...uris].sort((left, right) => left.toString().localeCompare(right.toString()));
    for (const uri of sorted) {
      const content = Buffer.from(await vscode.workspace.fs.readFile(uri)).toString('utf8');
      for (const declaration of testDeclarations(content)) {
        if (!next.has(declaration.name)) {
          next.set(declaration.name, { ...declaration, uri });
        }
      }
    }
    this.byName = next;
  }
}

class TraceIndex {
  constructor(output) {
    this.output = output;
    this.reports = [];
    this.watchers = [];
    this.testLocations = new TestLocationIndex();
  }

  dispose() {
    for (const watcher of this.watchers) {
      watcher.dispose();
    }
    this.watchers = [];
  }

  evidence(id) {
    const combined = {
      implementations: [], sources: [], tests: [], coverage: [], findings: [],
    };
    for (const report of this.reports) {
      const evidence = relatedEvidence(report, id);
      if (!evidence) {
        continue;
      }
      for (const key of Object.keys(combined)) {
        for (const node of evidence[key]) {
          const resolved = key === 'tests' ? this.withTestLocation(node) : node;
          if (!combined[key].some((candidate) => candidate.id === resolved.id)) {
            combined[key].push(resolved);
          }
        }
      }
    }
    return combined;
  }

  withTestLocation(node) {
    if (node.source || typeof node.label !== 'string') {
      return node;
    }
    const location = this.testLocations.get(node.label);
    if (!location) {
      return node;
    }
    return {
      ...node,
      source: {
        path: location.uri.fsPath,
        line: location.line + 1,
        column: location.character,
      },
    };
  }

  async refresh() {
    const configuration = vscode.workspace.getConfiguration('mcutrace');
    const reportPath = configuration.get('traceReport', DEFAULT_TRACE_REPORT);
    const folders = vscode.workspace.workspaceFolders || [];
    const reports = [];
    try {
      await this.testLocations.refresh();
    } catch (error) {
      this.output.appendLine(`Failed to index test locations: ${error.message}`);
    }
    for (const folder of folders) {
      const reportUri = vscode.Uri.joinPath(folder.uri, ...reportPath.split('/'));
      try {
        const content = Buffer.from(await vscode.workspace.fs.readFile(reportUri)).toString('utf8');
        const report = JSON.parse(content);
        if (!Array.isArray(report.nodes) || !Array.isArray(report.relationships)) {
          throw new Error('expected nodes and relationships arrays');
        }
        reports.push(report);
      } catch (error) {
        if (error.code !== 'FileNotFound') {
          this.output.appendLine(`Failed to load trace report ${reportUri.fsPath}: ${error.message}`);
        }
      }
    }
    this.reports = reports;
    this.output.appendLine(`Loaded ${reports.length} mcutrace trace report${reports.length === 1 ? '' : 's'}.`);
  }

  resetWatchers(context) {
    this.dispose();
    const configuration = vscode.workspace.getConfiguration('mcutrace');
    const reportPath = configuration.get('traceReport', DEFAULT_TRACE_REPORT);
    for (const folder of vscode.workspace.workspaceFolders || []) {
      const watcher = vscode.workspace.createFileSystemWatcher(
        new vscode.RelativePattern(folder, reportPath));
      watcher.onDidCreate(() => this.refresh());
      watcher.onDidChange(() => this.refresh());
      watcher.onDidDelete(() => this.refresh());
      this.watchers.push(watcher);
      context.subscriptions.push(watcher);
    }
  }
}

function tagAt(document, position) {
  const line = document.lineAt(position.line).text;
  const tag = requirementAtPosition(line, position.character);
  if (!tag) {
    return undefined;
  }
  return {
    ...tag,
    range: new vscode.Range(
      position.line,
      tag.start,
      position.line,
      tag.end),
  };
}

function escapedInline(value) {
  return value.replace(/([\\`*_{}\[\]()<>#+.!|-])/g, '\\$1');
}

function definitionLabel(definition) {
  return `${vscode.workspace.asRelativePath(definition.uri, false)}:${definition.line + 1}`;
}

function definitionLink(definition) {
  const node = {
    source: {
      path: definition.uri.fsPath,
      line: definition.line + 1,
      column: definition.character,
    },
  };
  const text = inlineCode(definitionLabel(definition));
  const target = traceLocationCommand(node);
  return target ? markdownLink(text, target) : text;
}

function inlineCode(value) {
  const longestRun = Math.max(0, ...[...value.matchAll(/`+/g)]
    .map((match) => match[0].length));
  const fence = '`'.repeat(longestRun + 1);
  return `${fence}${value}${fence}`;
}

function sourceDisplay(node) {
  if (!node.source || typeof node.source.path !== 'string') {
    return undefined;
  }
  const uri = vscode.Uri.file(node.source.path);
  const relative = vscode.workspace.asRelativePath(uri, false);
  const line = Number.isInteger(node.source.line) && node.source.line > 0
    ? `:${node.source.line}`
    : '';
  return `${relative}${line}`;
}

function traceLocationCommand(node) {
  if (!node.source || typeof node.source.path !== 'string') {
    return undefined;
  }
  const uri = vscode.Uri.file(node.source.path);
  if (!vscode.workspace.getWorkspaceFolder(uri)) {
    return undefined;
  }
  const target = {
    path: uri.fsPath,
    line: Number.isInteger(node.source.line) ? node.source.line : 0,
    column: Number.isInteger(node.source.column) ? node.source.column : 0,
  };
  const args = encodeURIComponent(JSON.stringify([target]));
  return vscode.Uri.parse(`command:${OPEN_TRACE_LOCATION}?${args}`);
}

function markdownLink(text, target) {
  return `[${text}](${target.toString()})`;
}

function evidenceLabel(node) {
  const source = sourceDisplay(node);
  const label = node.kind === 'source' && source ? source : node.label || node.id;
  const text = inlineCode(label);
  const target = traceLocationCommand(node);
  return target ? markdownLink(text, target) : text;
}

function traceLocation(node) {
  if (node.kind === 'source') {
    return '';
  }
  const source = sourceDisplay(node);
  if (!source) {
    return '';
  }
  const text = inlineCode(source);
  const target = traceLocationCommand(node);
  return target ? ` (${markdownLink(text, target)})` : ` (${text})`;
}

function evidenceState(node) {
  if (node.evidence_state && node.evidence_state !== 'unknown') {
    return node.evidence_state;
  }
  return node.finding_state;
}

function evidenceIcon(state) {
  switch (state) {
    case 'passed': return '$(pass)';
    case 'failed':
    case 'violation': return '$(error)';
    case 'unavailable':
    case 'limited': return '$(warning)';
    case 'informational': return '$(info)';
    case 'suppressed':
    case 'deviated':
    case 'baselined': return '$(circle-slash)';
    default: return '';
  }
}

function appendEvidenceGroup(markdown, title, nodes) {
  if (nodes.length === 0) {
    return;
  }
  markdown.appendMarkdown(`\n\n**${title}**\n`);
  for (const node of nodes) {
    const value = evidenceDetails(node);
    const state = value ? ` — ${escapedInline(value)}` : '';
    const icon = evidenceIcon(evidenceState(node));
    markdown.appendMarkdown(`- ${icon ? `${icon} ` : ''}${evidenceLabel(node)}${state}${traceLocation(node)}\n`);
  }
}

function appendEvidence(markdown, evidence) {
  appendEvidenceGroup(markdown, 'Implementations', evidence.implementations);
  appendEvidenceGroup(markdown, 'Sources', evidence.sources);
  appendEvidenceGroup(markdown, 'Tests', evidence.tests);
  appendEvidenceGroup(markdown, 'Coverage', evidence.coverage);
  appendEvidenceGroup(markdown, 'Static analysis', evidence.findings);
}

// @req REQ-0098 REQ-0101
class RequirementHoverProvider {
  constructor(index, trace) {
    this.index = index;
    this.trace = trace;
  }

  provideHover(document, position) {
    const tag = tagAt(document, position);
    if (!tag) {
      return undefined;
    }
    const definitions = this.index.get(tag.id);
    if (definitions.length === 0) {
      return undefined;
    }

    const markdown = new vscode.MarkdownString();
    markdown.isTrusted = { enabledCommands: [OPEN_TRACE_LOCATION] };
    markdown.supportThemeIcons = true;
    if (definitions.length > 1) {
      markdown.appendMarkdown(
        `**Warning:** ${definitions.length} definitions found for \`${tag.id}\`.\n\n`);
    }
    definitions.forEach((definition, index) => {
      if (index > 0) {
        markdown.appendMarkdown('\n\n---\n\n');
      }
      const title = definition.title ? ` — ${escapedInline(definition.title)}` : '';
      markdown.appendMarkdown(`### ${definition.id}${title}\n\n`);
      if (definition.body) {
        markdown.appendMarkdown(definition.body);
        markdown.appendMarkdown('\n\n');
      }
      markdown.appendMarkdown(`_Defined at ${definitionLink(definition)}_`);
    });
    appendEvidence(markdown, this.trace.evidence(tag.id));
    return new vscode.Hover(markdown, tag.range);
  }
}

// @req REQ-0099 REQ-0101
class RequirementDefinitionProvider {
  constructor(index) {
    this.index = index;
  }

  provideDefinition(document, position) {
    const tag = tagAt(document, position);
    if (!tag) {
      return undefined;
    }
    const definitions = this.index.get(tag.id);
    if (definitions.length === 0) {
      return undefined;
    }
    return definitions.map((definition) => new vscode.Location(
      definition.uri,
      new vscode.Position(definition.line, definition.character)));
  }
}

// @req REQ-0102
class RequirementReferenceProvider {
  constructor(output) {
    this.output = output;
  }

  async provideReferences(document, position, _context, token) {
    const tag = tagAt(document, position);
    if (!tag) {
      return undefined;
    }

    const configuration = vscode.workspace.getConfiguration('mcutrace');
    const exclude = configuration.get('exclude', DEFAULT_EXCLUDE);
    const discovered = await vscode.workspace.findFiles('**/*', exclude, undefined, token);
    const candidates = new Map(discovered.map((uri) => [uri.toString(), uri]));
    candidates.set(document.uri.toString(), document.uri);

    const locations = [];
    const uris = [...candidates.values()].sort((left, right) =>
      left.toString().localeCompare(right.toString()));
    for (const uri of uris) {
      if (token.isCancellationRequested) {
        return [];
      }
      try {
        const open = vscode.workspace.textDocuments.find(
          (candidate) => candidate.uri.toString() === uri.toString());
        const content = open
          ? open.getText()
          : Buffer.from(await vscode.workspace.fs.readFile(uri)).toString('utf8');
        for (const range of requirementRanges(content, tag.id)) {
          locations.push(new vscode.Location(uri, new vscode.Range(
            range.start.line,
            range.start.character,
            range.end.line,
            range.end.character)));
        }
      } catch (error) {
        this.output.appendLine(`Skipped ${uri.fsPath} while finding references: ${error.message}`);
      }
    }
    return locations;
  }
}

async function openTraceLocation(location) {
  if (!location || typeof location.path !== 'string') {
    return;
  }
  const uri = vscode.Uri.file(location.path);
  if (!vscode.workspace.getWorkspaceFolder(uri)) {
    return;
  }
  const document = await vscode.workspace.openTextDocument(uri);
  const line = Math.min(Math.max(0, (Number(location.line) || 1) - 1), document.lineCount - 1);
  const maximumColumn = document.lineAt(line).range.end.character;
  const column = Math.min(Math.max(0, (Number(location.column) || 0) - 1), maximumColumn);
  const position = new vscode.Position(line, column);
  return vscode.window.showTextDocument(document, {
    selection: new vscode.Range(position, position),
  });
}

async function activate(context) {
  const output = vscode.window.createOutputChannel('mcutrace Requirements');
  const index = new RequirementIndex(output);
  const trace = new TraceIndex(output);
  const selector = [{ scheme: 'file' }, { scheme: 'untitled' }];
  const markdownWatcher = vscode.workspace.createFileSystemWatcher('**/*.md');

  context.subscriptions.push(
    output,
    index,
    trace,
    vscode.languages.registerHoverProvider(
      selector,
      new RequirementHoverProvider(index, trace)),
    vscode.languages.registerDefinitionProvider(
      selector,
      new RequirementDefinitionProvider(index)),
    vscode.languages.registerReferenceProvider(
      selector,
      new RequirementReferenceProvider(output)),
    vscode.commands.registerCommand('mcutrace.refreshRequirements', async () => {
      await index.refresh();
      vscode.window.setStatusBarMessage('mcutrace requirements refreshed', 2000);
    }),
    vscode.commands.registerCommand('mcutrace.refreshTraceData', async () => {
      await trace.refresh();
      vscode.window.setStatusBarMessage('mcutrace trace data refreshed', 2000);
    }),
    vscode.commands.registerCommand(OPEN_TRACE_LOCATION, openTraceLocation),
    markdownWatcher,
    markdownWatcher.onDidCreate(() => index.scheduleRefresh()),
    markdownWatcher.onDidChange(() => index.scheduleRefresh()),
    markdownWatcher.onDidDelete(() => index.scheduleRefresh()),
    vscode.workspace.onDidChangeTextDocument((event) => {
      if (event.document.uri.path.endsWith('.md')) {
        index.scheduleRefresh();
      }
    }),
    vscode.workspace.onDidChangeConfiguration((event) => {
      if (event.affectsConfiguration('mcutrace.requirementFiles') ||
          event.affectsConfiguration('mcutrace.exclude')) {
        index.scheduleRefresh();
      }
      if (event.affectsConfiguration('mcutrace.traceReport')) {
        trace.resetWatchers(context);
        trace.refresh();
      }
    }),
  );

  await index.refresh();
  trace.resetWatchers(context);
  await trace.refresh();
}

function deactivate() {}

module.exports = { activate, deactivate };
