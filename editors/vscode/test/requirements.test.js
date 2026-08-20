'use strict';

const assert = require('node:assert/strict');
const test = require('node:test');
const {
  parseRequirementDocument,
  requirementAtPosition,
  requirementRanges,
} = require('../requirements');
const { evidenceDetails, relatedEvidence, testDeclarations } = require('../trace');

test('parses requirement title, body, and source position [REQ-0098] [REQ-0100]', () => {
  const content = [
    '# Product',
    '',
    '### @req REQ-0046 Duplicate nodes @evidence(implementation,test)',
    'Conflicting identities shall be diagnosed.',
    '',
    '#### Rationale',
    'Nested headings remain in the requirement body.',
    '',
    '### Unrelated section',
    'This is not part of REQ-0046.',
    '',
    '### REQ-0047 Missing tests',
    'Missing tests shall be reported.',
  ].join('\n');

  const requirements = parseRequirementDocument(content);
  assert.equal(requirements.length, 2);
  assert.deepEqual(
    {
      id: requirements[0].id,
      title: requirements[0].title,
      line: requirements[0].line,
      character: requirements[0].character,
    },
    {
      id: 'REQ-0046',
      title: 'Duplicate nodes',
      line: 2,
      character: 9,
    });
  assert.match(requirements[0].body, /Nested headings remain/);
  assert.doesNotMatch(requirements[0].body, /Unrelated section/);
  assert.doesNotMatch(requirements[0].body, /Missing tests/);
});

test('ignores requirement examples inside fenced code [REQ-0100]', () => {
  const content = [
    '```markdown',
    '### REQ-9999 Example',
    '```',
    '### REQ-0001 Real requirement',
    'Body',
  ].join('\n');

  const requirements = parseRequirementDocument(content);
  assert.equal(requirements.length, 1);
  assert.equal(requirements[0].id, 'REQ-0001');
});

test('finds a canonical tag only when the cursor is on it [REQ-0098] [REQ-0099]', () => {
  const line = '// @req REQ-0001 REQ-0002';
  assert.deepEqual(requirementAtPosition(line, 10), {
    id: 'REQ-0001', start: 8, end: 16,
  });
  assert.equal(requirementAtPosition(line, 3), undefined);
  assert.equal(requirementAtPosition('XREQ-0001', 5), undefined);
});

test('finds every bounded requirement reference [REQ-0102]', () => {
  const content = [
    'REQ-0102 is a requirement reference.',
    'A second REQ-0102 reference is on this line.',
    'REQ-01020 and XREQ-0102 are invalid lookalikes.',
  ].join('\n');

  assert.deepEqual(requirementRanges(content, 'REQ-0102'), [
    {
      start: { line: 0, character: 0 },
      end: { line: 0, character: 8 },
    },
    {
      start: { line: 1, character: 9 },
      end: { line: 1, character: 17 },
    },
  ]);
});

test('relates coverage and findings through a requirement implementation source', () => {
  const report = {
    nodes: [
      { id: 'REQ-0001', kind: 'requirement' },
      { id: 'implementation:one', kind: 'implementation', label: 'function one', source: { path: '/project/src/one.cpp', line: 7 } },
      { id: 'source:/project/src/one.cpp', kind: 'source', label: '/project/src/one.cpp', source: { path: '/project/src/one.cpp' } },
      { id: 'test:one', kind: 'test', label: 'one', evidence_state: 'passed' },
      { id: 'coverage:one', kind: 'coverage', label: 'one.cpp', source: { path: '/project/src/one.cpp' } },
      { id: 'finding:one', kind: 'finding', label: 'A1: issue', finding_state: 'violation', source: { path: '/project/src/one.cpp', line: 12 } },
    ],
    relationships: [
      { source: 'implementation:one', target: 'REQ-0001', type: 'implements' },
      { source: 'test:one', target: 'REQ-0001', type: 'verifies' },
      { source: 'coverage:one', target: 'source:/project/src/one.cpp', type: 'covers' },
      { source: 'finding:one', target: 'source:/project/src/one.cpp', type: 'reports' },
    ],
  };

  const evidence = relatedEvidence(report, 'REQ-0001');
  assert.equal(evidence.implementations.length, 1);
  assert.equal(evidence.sources.length, 1);
  assert.equal(evidence.tests.length, 1);
  assert.equal(evidence.coverage.length, 1);
  assert.equal(evidence.findings.length, 1);
});

test('formats a coverage summary for the requirement hover', () => {
  assert.equal(
    evidenceDetails({ evidence_state: 'unknown', evidence_detail: '2/3 probes covered (66.7%)' }),
    '2/3 probes covered (66.7%)');
});

test('finds mcutest test declarations for hover navigation', () => {
  const declarations = testDeclarations([
    'TEST(output, writes_json, "REQ-0001") {',
    '}',
    '',
    'TEST(',
    '  validation,',
    '  rejects_invalid_input,',
    '  "REQ-0002") {',
    '}',
  ].join('\n'));

  assert.deepEqual(declarations, [
    { name: 'output/writes_json', line: 0, character: 0 },
    { name: 'validation/rejects_invalid_input', line: 3, character: 0 },
  ]);
});
