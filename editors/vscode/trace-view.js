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
      children: values.map((node) => ({
        label: node.label || node.id,
        description: evidenceDescription(node),
        tooltip: node.id,
        icon: evidenceIcon(node, icon),
        contextValue: `mcutrace.${node.kind || 'evidence'}`,
        location: sourceLocation(node.source),
        children: [],
      })),
    }];
  });
}

function requirementNode(requirement) {
  return {
    label: requirement.id,
    description: requirement.title || '',
    tooltip: requirement.title
      ? `${requirement.id} — ${requirement.title}`
      : requirement.id,
    icon: 'symbol-key',
    contextValue: 'mcutrace.requirement',
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
    icon: 'symbol-namespace',
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

function diagnosticGroups(diagnostics = []) {
  if (diagnostics.length === 0) {
    return [];
  }
  const definitions = [
    ['error', 'Errors', 'error'],
    ['warning', 'Warnings', 'warning'],
    ['note', 'Notes', 'info'],
  ];
  const children = definitions.flatMap(([severity, label, icon]) => {
    const issues = diagnostics.filter((diagnostic) => diagnostic.severity === severity);
    if (issues.length === 0) {
      return [];
    }
    return [{
      label,
      description: String(issues.length),
      icon,
      contextValue: `mcutrace.${severity}Group`,
      children: issues.map((diagnostic) => ({
        label: diagnostic.message,
        description: diagnostic.code,
        tooltip: `${diagnostic.code}: ${diagnostic.message}`,
        icon,
        contextValue: 'mcutrace.traceDiagnostic',
        location: diagnostic.source,
        children: [],
      })),
    }];
  });
  return [{
    label: 'Trace problems',
    description: String(diagnostics.length),
    icon: 'issues',
    contextValue: 'mcutrace.traceProblems',
    children,
  }];
}

// @req REQ-0127
function buildTraceTree(payload = {}, relativePath = (value) => value) {
  const issues = diagnosticGroups(Array.isArray(payload.diagnostics) ? payload.diagnostics : []);
  const documents = (Array.isArray(payload.documents) ? payload.documents : []).map((document) => ({
    label: relativePath(document.path),
    tooltip: document.path,
    icon: 'markdown',
    contextValue: 'mcutrace.requirementDocument',
    location: { uri: document.uri },
    children: headingHierarchy(document.headings || document.requirements),
  }));
  return [...issues, ...documents];
}

module.exports = { buildTraceTree, diagnosticGroups, evidenceGroups, headingHierarchy };
