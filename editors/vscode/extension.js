'use strict';

const vscode = require('vscode');
const {
  parseRequirementDocument,
  requirementAtPosition,
} = require('./requirements');

const DEFAULT_GLOBS = ['**/requirements*.md'];
const DEFAULT_EXCLUDE = '**/{.git,build,node_modules}/**';

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

// @req REQ-0098 REQ-0101
class RequirementHoverProvider {
  constructor(index) {
    this.index = index;
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
    markdown.isTrusted = false;
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
      markdown.appendMarkdown(`_Defined at \`${escapedInline(definitionLabel(definition))}\`_`);
    });
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

async function activate(context) {
  const output = vscode.window.createOutputChannel('mcutrace Requirements');
  const index = new RequirementIndex(output);
  const selector = [{ scheme: 'file' }, { scheme: 'untitled' }];
  const markdownWatcher = vscode.workspace.createFileSystemWatcher('**/*.md');

  context.subscriptions.push(
    output,
    index,
    vscode.languages.registerHoverProvider(
      selector,
      new RequirementHoverProvider(index)),
    vscode.languages.registerDefinitionProvider(
      selector,
      new RequirementDefinitionProvider(index)),
    vscode.commands.registerCommand('mcutrace.refreshRequirements', async () => {
      await index.refresh();
      vscode.window.setStatusBarMessage('mcutrace requirements refreshed', 2000);
    }),
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
    }),
  );

  await index.refresh();
}

function deactivate() {}

module.exports = { activate, deactivate };
