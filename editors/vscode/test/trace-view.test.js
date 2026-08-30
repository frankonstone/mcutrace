'use strict';

const assert = require('node:assert/strict');
const test = require('node:test');

const { buildTraceTree, findTestDeclaration } = require('../trace-view');

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

  assert.equal(tree[0].label, 'docs/requirements.md');
  assert.deepEqual(tree[0].children.map((node) => node.label), ['Feature group', 'Other group']);
  assert.equal(tree[0].children[0].children[0].label, 'REQ-0001');
  assert.equal(tree[0].children[0].children[0].children[0].label, 'REQ-0002');
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
  assert.equal(tree[0].label, '/project/docs/requirements.md');
  assert.equal(tree[0].problemSeverity, 'error');
  assert.equal(tree[0].problemBadge, 'E');
  const requirement = tree[0].children[0];
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

  const tests = tree[0].children[0].children.find((node) => node.label === 'Tests');
  assert.deepEqual(tests.children[0].test, { name: 'component.works', location: undefined });
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

  assert.equal(tree[0].children[0].problemSeverity, 'warning');
  assert.equal(tree[0].children[0].problemBadge, 'W');
  assert.equal(tree[0].problemSeverity, 'warning');
});
