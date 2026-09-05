'use strict';

const EVIDENCE_GROUPS = [
  ['implementations', 'Implementations', 'symbol-method'],
  ['sources', 'Sources', 'file-code'],
  ['tests', 'Tests', 'beaker'],
  ['coverage', 'Coverage', 'pie-chart'],
  ['builds', 'Build evidence', 'tools'],
  ['findings', 'Static analysis', 'search'],
];

function sourceLocation(source) {
  return source && source.path ? { source } : undefined;
}

function escapeRegExp(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

// Resolve mcutest's suite/test names (and legacy suite.test names) back to their
// TEST(suite, name, ...) declaration when an artifact has no file/line metadata.
function findTestDeclaration(text, testName) {
  const separator = Math.max(testName.lastIndexOf('/'), testName.lastIndexOf('.'));
  if (separator <= 0 || separator === testName.length - 1) {
    return undefined;
  }
  const suite = escapeRegExp(testName.slice(0, separator));
  const name = escapeRegExp(testName.slice(separator + 1));
  const match = new RegExp(
    `\\b(?:TEST|TEST_F|TEST_P)\\s*\\(\\s*${suite}\\s*,\\s*${name}\\b`).exec(text);
  if (!match) {
    return undefined;
  }
  const lineStart = text.lastIndexOf('\n', match.index) + 1;
  const line = text.slice(0, lineStart).split('\n').length - 1;
  return {
    line,
    character: match.index - lineStart,
    endLine: line,
    endCharacter: match.index - lineStart + match[0].length,
  };
}

function evidenceIcon(node, fallback) {
  if (node.evidence_state === 'passed') {
    return 'pass';
  }
  if (node.evidence_state === 'failed') {
    return 'error';
  }
  if (node.finding_state === 'violation' || node.finding_state === 'failed') {
    return 'warning';
  }
  return fallback;
}

function evidenceDescription(node) {
  return [
    node.evidence_state && node.evidence_state !== 'unknown' ? node.evidence_state : undefined,
    node.evidence_detail,
    node.finding_state,
  ].filter(Boolean).join(' · ');
}

function evidenceGroups(trace = {}) {
  return EVIDENCE_GROUPS.flatMap(([key, label, icon]) => {
    const values = Array.isArray(trace[key]) ? trace[key] : [];
    if (values.length === 0) {
      return [];
    }
    return [{
      label,
      description: String(values.length),
      icon,
      contextValue: 'mcutrace.evidenceGroup',
      children: values.map((node) => {
        const label = node.label || node.id;
        const location = sourceLocation(node.source);
        return {
          label,
          description: evidenceDescription(node),
          tooltip: node.id,
          traceId: node.id,
          icon: evidenceIcon(node, icon),
          contextValue: `mcutrace.${node.kind || 'evidence'}`,
          location,
          test: node.kind === 'test' ? { name: label, location } : undefined,
          children: [],
        };
      }),
    }];
  });
}

function requirementNode(requirement) {
  return {
    label: requirement.title || requirement.id,
    description: requirement.title ? requirement.id : '',
    tooltip: requirement.title
      ? `${requirement.id} — ${requirement.title}`
      : requirement.id,
    icon: 'checklist',
    contextValue: 'mcutrace.requirement',
    requirementId: requirement.id,
    location: requirement.location,
    children: [],
    requirementChildren: [],
    evidenceChildren: evidenceGroups(requirement.trace),
  };
}

function sectionNode(heading) {
  return {
    label: heading.title,
    tooltip: heading.title,
    icon: 'list-tree',
    contextValue: 'mcutrace.requirementSection',
    location: heading.location,
    children: [],
    headingChildren: [],
    evidenceChildren: [],
  };
}

function headingHierarchy(headings = []) {
  const roots = [];
  const stack = [];
  for (const heading of headings) {
    const node = heading.kind === 'section' ? sectionNode(heading) : requirementNode(heading);
    node.headingChildren = node.requirementChildren || [];
    delete node.requirementChildren;
    const level = Number.isInteger(heading.headingLevel) ? heading.headingLevel : 6;
    while (stack.length > 0 && stack.at(-1).level >= level) {
      stack.pop();
    }
    const parent = stack.at(-1);
    (parent ? parent.node.headingChildren : roots).push(node);
    stack.push({ level, node });
  }

  const finish = (node) => {
    node.headingChildren.forEach(finish);
    node.children = [...node.headingChildren, ...node.evidenceChildren];
    delete node.headingChildren;
    delete node.evidenceChildren;
  };
  roots.forEach(finish);
  return roots;
}

const SEVERITY_RANK = { note: 1, warning: 2, error: 3 };

function locationIdentity(location) {
  if (!location) {
    return {};
  }
  const source = location.source || location;
  const path = source.path || source.uri;
  const line = source.range?.start?.line ??
    (Number.isInteger(source.line) && source.line > 0 ? source.line - 1 : undefined);
  return { path, line };
}

function collectNodes(roots) {
  const result = [];
  const visit = (node, depth) => {
    result.push({ node, depth, location: locationIdentity(node.location) });
    node.children.forEach((child) => visit(child, depth + 1));
  };
  roots.forEach((root) => visit(root, 0));
  return result;
}

function identifierMatch(diagnostic, candidates) {
  const text = `${diagnostic.code} ${diagnostic.message}`;
  return candidates
    .filter(({ node }) =>
      (node.requirementId && text.includes(node.requirementId)) ||
      (node.traceId && text.includes(node.traceId)))
    .sort((left, right) => right.depth - left.depth)[0];
}

function locationMatch(diagnostic, candidates) {
  const wanted = locationIdentity(diagnostic.source);
  if (!wanted.path) {
    return undefined;
  }
  return candidates
    .filter(({ location }) => location.path === wanted.path &&
      (wanted.line === undefined || location.line === wanted.line))
    .sort((left, right) => right.depth - left.depth)[0];
}

function severitySummary(diagnostics) {
  const counts = { error: 0, warning: 0, note: 0 };
  diagnostics.forEach((diagnostic) => {
    if (Object.hasOwn(counts, diagnostic.severity)) {
      counts[diagnostic.severity] += 1;
    }
  });
  return [
    counts.error ? `${counts.error} error${counts.error === 1 ? '' : 's'}` : undefined,
    counts.warning ? `${counts.warning} warning${counts.warning === 1 ? '' : 's'}` : undefined,
    counts.note ? `${counts.note} note${counts.note === 1 ? '' : 's'}` : undefined,
  ].filter(Boolean).join(', ');
}

function finishDecorations(node, path) {
  const directDiagnostics = node.directDiagnostics || [];
  node.directProblemSeverity = directDiagnostics.reduce((severity, diagnostic) =>
    SEVERITY_RANK[diagnostic.severity] > (SEVERITY_RANK[severity] || 0)
      ? diagnostic.severity : severity, undefined);
  const diagnostics = [...directDiagnostics];
  node.children.forEach((child, index) => {
    finishDecorations(child, `${path}/${index}`);
    diagnostics.push(...child.aggregatedDiagnostics);
  });
  node.aggregatedDiagnostics = diagnostics;
  node.decorationId = path;
  if (diagnostics.length > 0) {
    node.problemSeverity = diagnostics.reduce((severity, diagnostic) =>
      SEVERITY_RANK[diagnostic.severity] > SEVERITY_RANK[severity]
        ? diagnostic.severity : severity, 'note');
    node.problemBadge = node.problemSeverity === 'error' ? 'E'
      : node.problemSeverity === 'warning' ? 'W' : 'I';
    const summary = `${directDiagnostics.length ? 'Trace problems' : 'Problems inside'}: ${severitySummary(diagnostics)}`;
    const details = directDiagnostics.map(
      (diagnostic) => `${diagnostic.code}: ${diagnostic.message}`).join('\n');
    node.problemTooltip = details ? `${summary}\n${details}` : summary;
  }
  delete node.directDiagnostics;
}

function applyDiagnostics(roots, diagnostics = []) {
  const candidates = collectNodes(roots);
  for (const diagnostic of diagnostics) {
    const match = identifierMatch(diagnostic, candidates) ||
      locationMatch(diagnostic, candidates);
    if (match) {
      (match.node.directDiagnostics ||= []).push(diagnostic);
    }
  }
  roots.forEach((root, index) => finishDecorations(root, String(index)));
  return roots;
}

// @req REQ-0127
function buildTraceTree(payload = {}, relativePath = (value) => value) {
  const roots = [];
  const folders = new Map();
  for (const document of Array.isArray(payload.documents) ? payload.documents : []) {
    const path = relativePath(document.path).replace(/\\/g, '/');
    const parts = path.split('/').filter(Boolean);
    const filename = parts.pop();
    let children = roots;
    let folderPath = path.startsWith('/') ? '/' : '';
    for (const part of parts) {
      folderPath += `${part}/`;
      let folder = folders.get(folderPath);
      if (!folder) {
        folder = {
          id: `folder:${folderPath}`,
          label: part,
          tooltip: folderPath,
          icon: 'folder',
          contextValue: 'mcutrace.requirementFolder',
          children: [],
        };
        folders.set(folderPath, folder);
        children.push(folder);
      }
      children = folder.children;
    }
    children.push({
      id: `document:${document.uri || document.path}`,
      label: filename,
      tooltip: document.path,
      icon: 'markdown',
      contextValue: 'mcutrace.requirementDocument',
      location: { uri: document.uri },
      children: headingHierarchy(document.headings || document.requirements),
    });
  }
  applyDiagnostics(roots, Array.isArray(payload.diagnostics) ? payload.diagnostics : []);
  assignTreeIds(roots);
  return roots;
}

// Identity follows content, rather than sibling position, so refreshes preserve folds.
function assignTreeIds(nodes, parentId = '') {
  const occurrences = new Map();
  for (const node of nodes) {
    const key = JSON.stringify([node.contextValue, node.requirementId || node.traceId || node.label]);
    const occurrence = occurrences.get(key) || 0;
    occurrences.set(key, occurrence + 1);
    node.id ||= `${parentId}/${key}/${occurrence}`;
    node.decorationId = node.id;
    assignTreeIds(node.children, node.id);
  }
}

function requirementNodes(roots) {
  return collectNodes(roots).map(({ node }) => node).filter((node) => node.requirementId);
}

function requirementAtLocation(roots, uri, line) {
  // Include section boundaries so a cursor in a new section doesn't select the
  // last requirement in the previous section. Heading ranges cover the heading.
  const headings = collectNodes(roots).map(({ node }) => node).filter((node) =>
    (node.requirementId || node.contextValue === 'mcutrace.requirementSection') &&
    node.location?.uri === uri && node.location.range?.start.line <= line);
  headings.sort((a, b) => b.location.range.start.line - a.location.range.start.line ||
    b.location.range.start.character - a.location.range.start.character);
  return headings[0]?.requirementId ? headings[0] : undefined;
}

module.exports = {
  applyDiagnostics,
  buildTraceTree,
  evidenceGroups,
  findTestDeclaration,
  headingHierarchy,
  requirementAtLocation,
  requirementNodes,
};
