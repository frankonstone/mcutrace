'use strict';

const assert = require('node:assert/strict');
const test = require('node:test');

const { buildTraceTree, findTestDeclaration, requirementAtLocation, requirementNodes } = require('../trace-view');

test('builds a document and Markdown heading hierarchy for requirements', () => {
  const tree = buildTraceTree({
    documents: [{
      path: '/project/docs/requirements.md',
      uri: 'file:///project/docs/requirements.md',
      headings: [
        { kind: 'section', title: 'Feature group', headingLevel: 2 },
        { kind: 'requirement', id: 'REQ-0001', title: 'Parent', headingLevel: 3, trace: {} },
        { kind: 'requirement', id: 'REQ-0002', title: 'Child', headingLevel: 4, trace: {} },
        { kind: 'section', title: 'Other group', headingLevel: 2 },
        { kind: 'requirement', id: 'REQ-0003', title: 'Sibling', headingLevel: 3, trace: {} },
      ],
    }],
  }, (path) => path.replace('/project/', ''));

  assert.equal(tree[0].label, 'docs');
  const document = tree[0].children[0];
  assert.equal(document.label, 'requirements.md');
  assert.deepEqual(document.children.map((node) => node.label), ['Feature group', 'Other group']);
  assert.equal(document.children[0].children[0].label, 'Parent');
  assert.equal(document.children[0].children[0].description, 'REQ-0001');
  assert.equal(document.children[0].children[0].children[0].label, 'Child');
});

test('shows trace diagnostics and typed evidence without using Problems', () => {
  const tree = buildTraceTree({
    diagnostics: [{
      severity: 'error',
      code: 'validation.failed_test_evidence',
      message: 'linked test evidence is failing',
      source: {
        path: '/project/tests/component.cpp',
        uri: 'file:///project/tests/component.cpp',
        range: { start: { line: 3, character: 0 }, end: { line: 3, character: 1 } },
      },
    }],
    documents: [{
      path: '/project/docs/requirements.md',
      uri: 'file:///project/docs/requirements.md',
      headings: [{
        kind: 'requirement',
        id: 'REQ-0001',
        title: 'Feature',
        headingLevel: 2,
        trace: {
          tests: [{
            id: 'test:component',
            kind: 'test',
            label: 'component works',
            evidence_state: 'failed',
            source: { path: '/project/tests/component.cpp', line: 4, column: 1 },
          }],
        },
      }],
    }],
  });

  assert.equal(tree.length, 1);
  assert.equal(tree[0].label, 'project');
  assert.equal(tree[0].problemSeverity, 'error');
  assert.equal(tree[0].problemBadge, 'E');
  const [requirement] = requirementNodes(tree);
  assert.equal(requirement.problemSeverity, 'error');
  const tests = requirement.children.find((node) => node.label === 'Tests');
  assert.equal(tests.children[0].icon, 'error');
  assert.equal(tests.children[0].description, 'failed');
  assert.deepEqual(tests.children[0].test, {
    name: 'component works',
    location: {
      source: { path: '/project/tests/component.cpp', line: 4, column: 1 },
    },
  });
  assert.equal(tests.children[0].problemSeverity, 'error');
  assert.equal(tests.children[0].problemTooltip,
    'Trace problems: 1 error\nvalidation.failed_test_evidence: linked test evidence is failing');
  assert.equal(tree.flatMap((node) => node.children).some(
    (node) => node.label === 'Trace problems'), false);
});

test('makes tests without artifact locations navigable by their mcutest name', () => {
  const tree = buildTraceTree({
    documents: [{
      path: '/project/docs/requirements.md',
      uri: 'file:///project/docs/requirements.md',
      headings: [{
        kind: 'requirement',
        id: 'REQ-0001',
        title: 'Feature',
        headingLevel: 2,
        trace: {
          tests: [{ id: 'test:mcutest:component.works', kind: 'test', label: 'component.works' }],
        },
      }],
    }],
  });

  const tests = requirementNodes(tree)[0].children.find((node) => node.label === 'Tests');
  assert.deepEqual(tests.children[0].test, { name: 'component.works', location: undefined });
});

test('shows dependency and plan groups and standalone plan documents', () => {
  const tree = buildTraceTree({ documents: [
    {
      path: '/project/docs/requirements.md',
      uri: 'file:///project/docs/requirements.md',
      headings: [{
        kind: 'requirement', id: 'REQ-0001', title: 'Feature', headingLevel: 2,
        trace: {
          prerequisites: [{ id: 'REQ-0002', kind: 'requirement', label: 'Foundation' }],
          dependents: [{ id: 'REQ-0003', kind: 'requirement', label: 'Consumer' }],
          plans: [{ id: 'PLN-0001', kind: 'plan', label: 'Implement feature', status: 'in_progress' }],
        },
      }],
    },
    {
      path: '/project/docs/implementation-plan.md',
      uri: 'file:///project/docs/implementation-plan.md',
      headings: [{
        kind: 'plan', id: 'PLN-0001', title: 'Implement feature', status: 'in_progress',
        headingLevel: 2,
        links: [{
          id: 'REQ-0001', title: 'Feature',
          location: { uri: 'file:///project/docs/requirements.md' },
        }, {
          id: 'REQ-9999',
          location: { uri: 'file:///project/docs/implementation-plan.md' },
        }],
      }],
    },
  ] });

  const requirement = requirementNodes(tree)[0];
  assert.deepEqual(requirement.children.map((node) => node.label),
    ['Depends on', 'Required by', 'Implementation plans']);
  assert.equal(requirement.children[2].children[0].description, 'in_progress');
  const planDocument = tree[0].children[0].children.find(
    (node) => node.label === 'implementation-plan.md');
  assert.equal(planDocument.children[0].contextValue, 'mcutrace.plan');
  assert.equal(planDocument.children[0].description, 'PLN-0001 · in_progress');
  const linkedRequirements = planDocument.children[0].children[0];
  assert.equal(linkedRequirements.label, 'Linked requirements');
  assert.equal(linkedRequirements.description, '2');
  assert.equal(linkedRequirements.children[0].label, 'Feature');
  assert.equal(linkedRequirements.children[0].description, 'REQ-0001');
  assert.equal(linkedRequirements.children[0].traceId, 'REQ-0001');
  assert.equal(linkedRequirements.children[0].contextValue, 'mcutrace.requirementLink');
  assert.deepEqual(linkedRequirements.children[0].location,
    { uri: 'file:///project/docs/requirements.md' });
  assert.equal(linkedRequirements.children[1].label, 'REQ-9999');
  assert.equal(linkedRequirements.children[1].description, '');
  assert.equal(requirementNodes(tree).length, 1);
});

test('finds a mcutest TEST declaration from its dotted artifact name', () => {
  const source = [
    '#include <mcutest/mcutest.hpp>',
    '',
    'TEST(component, works, "REQ-0001") {',
    '}',
  ].join('\n');

  assert.deepEqual(findTestDeclaration(source, 'component.works'), {
    line: 2,
    character: 0,
    endLine: 2,
    endCharacter: 'TEST(component, works'.length,
  });
  assert.equal(findTestDeclaration(source, 'component.missing'), undefined);
});

test('finds a mcutest TEST declaration from its slash-separated artifact name', () => {
  const source = 'TEST(cli, exposes_evidence_query_help, "REQ-0055", "REQ-0064") {}';

  assert.deepEqual(findTestDeclaration(source, 'cli/exposes_evidence_query_help'), {
    line: 0,
    character: 0,
    endLine: 0,
    endCharacter: 'TEST(cli, exposes_evidence_query_help'.length,
  });
});

test('maps requirement diagnostics by identifier and propagates warning decoration', () => {
  const tree = buildTraceTree({
    diagnostics: [{
      severity: 'warning',
      code: 'validation.missing_test_evidence',
      message: 'requirement has no linked test evidence: REQ-0042',
    }],
    documents: [{
      path: '/project/requirements.md',
      uri: 'file:///project/requirements.md',
      headings: [{
        kind: 'requirement', id: 'REQ-0042', title: 'Feature', headingLevel: 2, trace: {},
      }],
    }],
  });

  assert.equal(requirementNodes(tree)[0].problemSeverity, 'warning');
  assert.equal(requirementNodes(tree)[0].problemBadge, 'W');
  assert.equal(tree[0].problemSeverity, 'warning');
});

test('groups nested folders without merging equal filenames or workspace roots [REQ-0127]', () => {
  const paths = ['alpha/docs/a.md', 'alpha/docs/nested/a.md', 'alpha/docs/b.md', 'beta/docs/a.md', 'root.md'];
  const tree = buildTraceTree({ documents: paths.map((path) => ({
    path, uri: `file:///work/${path}`, requirements: [],
  })) });
  assert.deepEqual(tree.map((node) => node.label), ['alpha', 'beta', 'root.md']);
  const docs = tree[0].children[0];
  assert.equal(docs.label, 'docs');
  assert.equal(docs.contextValue, 'mcutrace.requirementFolder');
  assert.equal(docs.location, undefined);
  assert.deepEqual(docs.children.map((node) => node.label), ['a.md', 'nested', 'b.md']);
  assert.equal(docs.children[1].children[0].label, 'a.md');
  assert.notEqual(docs.children[0].id, tree[1].children[0].children[0].id);
  const windows = buildTraceTree({ documents: [{ path: 'docs\\nested\\a.md' }] });
  assert.equal(windows[0].children[0].children[0].label, 'a.md');
});

test('retains identities across inserted siblings and changed requirement titles [REQ-0127]', () => {
  const document = (requirements) => ({
    path: 'docs/req.md', uri: 'file:///work/docs/req.md', requirements,
  });
  const original = buildTraceTree({ documents: [document([
    { id: 'REQ-0001', title: 'Same title' }, { id: 'REQ-0002', title: 'Same title' },
  ])] });
  const refreshed = buildTraceTree({ documents: [
    { path: 'new.md', uri: 'file:///work/new.md' },
    document([{ id: 'REQ-0003' }, { id: 'REQ-0001', title: 'Renamed' }, { id: 'REQ-0002', title: 'Same title' }]),
  ] });
  const before = requirementNodes(original);
  const after = requirementNodes(refreshed);
  assert.notEqual(before[0].id, before[1].id);
  assert.equal(before[0].id, after[1].id);
  assert.equal(before[1].id, after[2].id);
  assert.equal(original[0].id, refreshed[1].id);
  assert.equal(original[0].children[0].id, refreshed[1].children[0].id);
  assert.equal(after[0].label, 'REQ-0003');
  assert.equal(after[0].description, '');
});

test('distinguishes direct problems from descendant summaries [REQ-0127]', () => {
  const tree = buildTraceTree({
    documents: [{ path: 'docs/req.md', uri: 'file:///docs/req.md', requirements: [{ id: 'REQ-0042' }] }],
    diagnostics: [{ severity: 'warning', code: 'missing', message: 'REQ-0042 has no test' }],
  });
  assert.equal(tree[0].directProblemSeverity, undefined);
  assert.equal(tree[0].problemTooltip, 'Problems inside: 1 warning');
  assert.equal(requirementNodes(tree)[0].directProblemSeverity, 'warning');
  assert.match(requirementNodes(tree)[0].problemTooltip, /missing: REQ-0042 has no test/);
});

test('reveals requirements on heading and body lines without crossing section or document boundaries [REQ-0127]', () => {
  const uri = 'file:///work/docs/req.md';
  const heading = (kind, line, id) => ({
    kind, id, title: id || 'Section', headingLevel: kind === 'section' ? 1 : 2,
    location: { uri, range: { start: { line, character: 4 }, end: { line, character: 12 } } },
  });
  const tree = buildTraceTree({ documents: [{ path: 'docs/req.md', uri, headings: [
    heading('section', 0), heading('requirement', 2, 'REQ-0001'),
    heading('requirement', 5, 'REQ-0002'), heading('section', 8),
  ] }] });
  assert.equal(requirementAtLocation(tree, uri, 1), undefined);
  assert.equal(requirementAtLocation(tree, uri, 2).requirementId, 'REQ-0001');
  assert.equal(requirementAtLocation(tree, uri, 4).requirementId, 'REQ-0001');
  assert.equal(requirementAtLocation(tree, uri, 7).requirementId, 'REQ-0002');
  assert.equal(requirementAtLocation(tree, uri, 9), undefined);
  assert.equal(requirementAtLocation(tree, 'file:///other.md', 4), undefined);
});
