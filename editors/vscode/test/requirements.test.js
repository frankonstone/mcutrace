'use strict';

const assert = require('node:assert/strict');
const test = require('node:test');
const {
  parseRequirementDocument,
  requirementAtPosition,
} = require('../requirements');

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
