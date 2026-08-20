'use strict';

function addOnce(nodes, node) {
  if (!nodes.some((candidate) => candidate.id === node.id)) {
    nodes.push(node);
  }
}

function validNode(node) {
  return node && typeof node.id === 'string' && typeof node.kind === 'string';
}

function sourcePath(node) {
  return node && node.source && typeof node.source.path === 'string'
    ? node.source.path
    : undefined;
}

function evidenceDetails(node) {
  const details = [];
  const state = node && node.evidence_state && node.evidence_state !== 'unknown'
    ? node.evidence_state
    : node && node.finding_state;
  if (typeof state === 'string' && state.length > 0) {
    details.push(state);
  }
  if (node && typeof node.evidence_detail === 'string' && node.evidence_detail.length > 0) {
    details.push(node.evidence_detail);
  }
  return details.join(' — ');
}

// mcutest result JSON can omit source metadata to remain small for embedded
// targets. The extension resolves host-test declarations in the workspace so
// test evidence can still navigate directly to its TEST(...) method.
function testDeclarations(content) {
  if (typeof content !== 'string') {
    return [];
  }
  const declarations = [];
  const pattern = /\bTEST(?:_DISABLED)?\s*\(\s*([A-Za-z_]\w*)\s*,\s*([A-Za-z_]\w*)/g;
  for (let match = pattern.exec(content); match !== null; match = pattern.exec(content)) {
    const before = content.slice(0, match.index);
    const lastNewline = before.lastIndexOf('\n');
    declarations.push({
      name: `${match[1]}/${match[2]}`,
      line: before.split('\n').length - 1,
      character: match.index - lastNewline - 1,
    });
  }
  return declarations;
}

function emptyEvidence() {
  return {
    implementations: [],
    sources: [],
    tests: [],
    coverage: [],
    findings: [],
  };
}

function addRelated(evidence, node) {
  if (node.kind === 'implementation') {
    addOnce(evidence.implementations, node);
  } else if (node.kind === 'source') {
    addOnce(evidence.sources, node);
  } else if (node.kind === 'test') {
    addOnce(evidence.tests, node);
  } else if (node.kind === 'coverage') {
    addOnce(evidence.coverage, node);
  } else if (node.kind === 'finding') {
    addOnce(evidence.findings, node);
  }
}

function relationshipEndpoints(report, requirementId) {
  const result = new Set();
  if (!Array.isArray(report.relationships)) {
    return result;
  }
  for (const relationship of report.relationships) {
    if (!relationship || typeof relationship.source !== 'string' ||
        typeof relationship.target !== 'string') {
      continue;
    }
    if (relationship.source === requirementId) {
      result.add(relationship.target);
    } else if (relationship.target === requirementId) {
      result.add(relationship.source);
    }
  }
  return result;
}

// Builds the evidence sections shown in a requirement hover. Coverage and
// static-analysis findings are associated through the source implementation
// path when their producer does not link them to a requirement directly.
function relatedEvidence(report, requirementId) {
  const nodes = Array.isArray(report && report.nodes) ? report.nodes.filter(validNode) : [];
  const byId = new Map(nodes.map((node) => [node.id, node]));
  const requirement = byId.get(requirementId);
  if (!requirement || requirement.kind !== 'requirement') {
    return undefined;
  }

  const evidence = emptyEvidence();
  const paths = new Set();
  for (const id of relationshipEndpoints(report, requirementId)) {
    const node = byId.get(id);
    if (!node) {
      continue;
    }
    addRelated(evidence, node);
    const path = sourcePath(node);
    if (path) {
      paths.add(path);
    }
  }

  for (const implementation of evidence.implementations) {
    const path = sourcePath(implementation);
    if (path) {
      paths.add(path);
      const source = byId.get(`source:${path}`);
      if (source && source.kind === 'source') {
        addOnce(evidence.sources, source);
      }
    }
  }

  for (const node of nodes) {
    const path = sourcePath(node);
    if (path && paths.has(path) && (node.kind === 'coverage' || node.kind === 'finding')) {
      addRelated(evidence, node);
    }
  }
  return evidence;
}

module.exports = { evidenceDetails, relatedEvidence, testDeclarations };
