'use strict';

const assert = require('node:assert/strict');
const test = require('node:test');

const { buildTraceTree } = require('../trace-view');

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
      source: { uri: 'file:///project/tests/component.cpp', range: {} },
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

  assert.equal(tree[0].label, 'Trace problems');
  assert.equal(tree[0].children[0].children[0].description, 'validation.failed_test_evidence');
  const tests = tree[1].children[0].children.find((node) => node.label === 'Tests');
  assert.equal(tests.children[0].icon, 'error');
  assert.equal(tests.children[0].description, 'failed');
});
