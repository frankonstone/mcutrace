'use strict';

const CANONICAL_REQUIREMENT = /^REQ-\d{4}$/;

function sourceLines(content) {
  const lines = [];
  let offset = 0;
  let lineNumber = 0;
  while (offset <= content.length) {
    const newline = content.indexOf('\n', offset);
    const end = newline === -1 ? content.length : newline;
    lines.push({
      text: content.slice(offset, end),
      offset,
      nextOffset: newline === -1 ? content.length : newline + 1,
      line: lineNumber,
    });
    if (newline === -1) {
      break;
    }
    offset = newline + 1;
    lineNumber += 1;
  }
  return lines;
}

function fenceMarker(line) {
  const match = /^(`{3,}|~{3,})/.exec(line.trim());
  return match ? match[1][0] : undefined;
}

function headingFromLine(line) {
  const match = /^(#{1,6})[ \t]+(.*)$/.exec(line.text);
  if (!match) {
    return undefined;
  }
  let text = match[2].trim();
  while (text.endsWith('#')) {
    text = text.slice(0, -1).trim();
  }
  const candidate = /REQ-[A-Za-z0-9]*/.exec(text);
  const id = candidate && CANONICAL_REQUIREMENT.test(candidate[0])
    ? candidate[0]
    : undefined;
  return {
    id,
    title: id
      ? text
        .replace(/@evidence\([^)]*\)/, '')
        .replace(id, '')
        .replace('@req', '')
        .trim()
      : '',
    level: match[1].length,
    line: line.line,
    character: line.text.indexOf(id),
    offset: line.offset,
    bodyOffset: line.nextOffset,
  };
}

// @req REQ-0100
function parseRequirementDocument(content) {
  const headings = [];
  let fencedBy;
  for (const line of sourceLines(content)) {
    const marker = fenceMarker(line.text);
    if (marker) {
      if (!fencedBy) {
        fencedBy = marker;
        continue;
      }
      if (marker === fencedBy) {
        fencedBy = undefined;
      }
      continue;
    }
    if (fencedBy) {
      continue;
    }
    const heading = headingFromLine(line);
    if (heading) {
      headings.push(heading);
    }
  }

  return headings.flatMap((heading, index) => {
    if (!heading.id) {
      return [];
    }
    let bodyEnd = content.length;
    for (let next = index + 1; next < headings.length; next += 1) {
      if (headings[next].level <= heading.level) {
        bodyEnd = headings[next].offset;
        break;
      }
    }
    return [{
      id: heading.id,
      title: heading.title,
      body: content.slice(heading.bodyOffset, bodyEnd)
        .replace(/^[\r\n]+|[\r\n]+$/g, ''),
      line: heading.line,
      character: heading.character,
      headingLevel: heading.level,
    }];
  });
}

function requirementAtPosition(line, character) {
  const pattern = /REQ-\d{4}/g;
  let match;
  while ((match = pattern.exec(line)) !== null) {
    const before = match.index === 0 ? '' : line[match.index - 1];
    const afterIndex = match.index + match[0].length;
    const after = afterIndex === line.length ? '' : line[afterIndex];
    const bounded = !/[A-Za-z0-9]/.test(before) && !/[A-Za-z0-9]/.test(after);
    if (bounded && character >= match.index && character < afterIndex) {
      return { id: match[0], start: match.index, end: afterIndex };
    }
  }
  return undefined;
}

function requirementRanges(content, id) {
  if (!CANONICAL_REQUIREMENT.test(id)) {
    return [];
  }

  const lines = sourceLines(content);
  const ranges = [];
  const pattern = /REQ-\d{4}/g;
  let lineIndex = 0;
  let match;
  while ((match = pattern.exec(content)) !== null) {
    while (lineIndex + 1 < lines.length && match.index >= lines[lineIndex].nextOffset) {
      lineIndex += 1;
    }
    const before = match.index === 0 ? '' : content[match.index - 1];
    const afterIndex = match.index + match[0].length;
    const after = afterIndex === content.length ? '' : content[afterIndex];
    const bounded = !/[A-Za-z0-9]/.test(before) && !/[A-Za-z0-9]/.test(after);
    if (match[0] !== id || !bounded) {
      continue;
    }
    const character = match.index - lines[lineIndex].offset;
    ranges.push({
      start: { line: lines[lineIndex].line, character },
      end: { line: lines[lineIndex].line, character: character + match[0].length },
    });
  }
  return ranges;
}

module.exports = {
  parseRequirementDocument,
  requirementAtPosition,
  requirementRanges,
};
