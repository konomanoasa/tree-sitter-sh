import assert from "node:assert/strict";

const continuationType = "line_continuation";

const lexicalTypes = new Set([
  "and_if",
  "arithmetic_number",
  "arithmetic_operator",
  "arithmetic_source_fragment",
  "arithmetic_variable",
  "bang",
  "clobber",
  "comment_text",
  "dgreat",
  "dless",
  "dlessdash",
  "dollar_single_quote_escape",
  "dollar_single_quote_text",
  "double_quote_escape",
  "double_quote_text",
  "dsemi",
  "escaped_character",
  "fname",
  "greatand",
  "here_document_end_text",
  "here_document_escape",
  "here_document_line_end",
  "here_document_text",
  "io_number",
  "lessand",
  "lessgreat",
  "literal",
  "name",
  "or_if",
  "parameter_length_operator",
  "parameter_pattern_operator",
  "parameter_value_operator",
  "pattern_bracket_character_source",
  "pattern_bracket_hyphen_source",
  "pattern_bracket_negation_source",
  "pattern_bracket_range_operator_source",
  "pattern_character_class_content_source",
  "pattern_collating_symbol_character_source",
  "pattern_equivalence_class_character_source",
  "pattern_question_source",
  "pattern_star_source",
  "positional_parameter",
  "quoted_here_document_text",
  "semi_and",
  "single_quote_content",
  "special_parameter",
  "variable_name",
]);

function parseCst(output) {
  const entries = [];
  for (const line of output.split("\n")) {
    const match = line.match(
      /^([0-9]+:[0-9]+)[ ]+-[ ]+([0-9]+:[0-9]+)([ ]+)([^\n]*)$/,
    );
    if (match === null) continue;
    let content = match[4];
    const rangeWidth = line.indexOf("-") - 1;
    let depth = match[3].length - Math.max(0, rangeWidth - match[2].length);
    const recovery = content.startsWith("•");
    if (recovery) {
      content = content.slice(1);
      depth += 1;
    }
    if (content.startsWith("`")) continue;
    entries.push({
      content,
      depth,
      line,
      range: `${match[1]}-${match[2]}`,
      recovery,
    });
  }
  return entries;
}

function nodeIdentity(entry) {
  const field = entry.content.match(/^([a-z_]+): /)?.[1];
  let type =
    field === undefined ? entry.content : entry.content.slice(field.length + 2);
  if (!type.startsWith('"')) type = type.replace(/[ ]+`[^\n]*$/, "");
  return { field, type, recovery: entry.recovery };
}

function isContinuation(entry) {
  return nodeIdentity(entry).type === continuationType;
}

function projectedCst(output, ranges) {
  const structure = [];
  const continuations = [];
  const entries = parseCst(output);
  const rootDepth = entries[0]?.depth;
  for (const entry of entries) {
    if (isContinuation(entry)) {
      continuations.push(entry.range);
    } else {
      const { field, type, recovery } = nodeIdentity(entry);
      structure.push([
        entry.depth - rootDepth,
        field ?? null,
        type,
        recovery,
        ...(ranges ? [entry.range] : []),
      ]);
    }
  }
  assert.notEqual(structure.length, 0, "CST projection is empty");
  return { structure, continuations };
}

function cstFingerprint(output) {
  return JSON.stringify(projectedCst(output, true));
}

function logicalProjection(output) {
  return JSON.stringify(projectedCst(output, false).structure);
}

function namedStructure(output, includeFields) {
  const depths = [];
  let result = "";
  for (const entry of parseCst(output)) {
    const { field, type } = nodeIdentity(entry);
    if (type.startsWith('"') || type === continuationType) continue;
    while (depths.at(-1) >= entry.depth) {
      result += ")";
      depths.pop();
    }
    if (result) result += " ";
    if (includeFields && field !== undefined) result += `${field}: `;
    result += `(${type}`;
    depths.push(entry.depth);
  }
  return result + ")".repeat(depths.length);
}

function continuationManifest(output) {
  return parseCst(output)
    .filter(isContinuation)
    .map((entry) => entry.range);
}

function lineOffsets(bytes) {
  const offsets = [0];
  for (const [index, byte] of bytes.entries()) {
    if (byte === 10) offsets.push(index + 1);
  }
  return offsets;
}

function pointOffset(bytes, offsets, point) {
  const [row, column] = point.split(":").map(Number);
  const offset = offsets[row];
  assert.notEqual(offset, undefined, `point exceeds source: ${point}`);
  const lineEnd =
    row + 1 < offsets.length ? offsets[row + 1] - 1 : bytes.length;
  assert.ok(column <= lineEnd - offset, `column exceeds source: ${point}`);
  return offset + column;
}

function sourceByteOffset(source, point) {
  const bytes = Buffer.from(source);
  return pointOffset(bytes, lineOffsets(bytes), point);
}

function assertCstSourceContract(output, source) {
  const bytes = Buffer.from(source);
  const offsets = lineOffsets(bytes);
  const entries = parseCst(output);
  const ancestors = [];
  const removedNewlines = new Set();
  let previousContinuation = -1;
  for (const entry of entries) {
    const { type } = nodeIdentity(entry);
    const [start, end] = entry.range
      .split("-")
      .map((point) => pointOffset(bytes, offsets, point));
    assert.ok(end >= start, `reversed source range: ${entry.range}`);
    if (type.startsWith('"') && !isContinuation(entry)) {
      const spelling = JSON.parse(type.replaceAll("\\`", "`"));
      const text = bytes.subarray(start, end).toString();
      assert.equal(text, spelling, `anonymous source differs: ${entry.line}`);
    }
    while (ancestors.at(-1)?.entry.depth >= entry.depth) ancestors.pop();
    const parent = ancestors.at(-1);
    if (parent !== undefined) {
      assert.ok(
        start >= parent.start && end <= parent.end,
        `child exceeds parent: ${entry.line}`,
      );
      assert.ok(
        !isContinuation(parent.entry),
        `continuation has a child: ${entry.line}`,
      );
      if (lexicalTypes.has(parent.type) || parent.type.endsWith("_keyword")) {
        assert.equal(
          type,
          continuationType,
          `internal lexical child is public: ${entry.line}`,
        );
      }
    }
    if (isContinuation(entry)) {
      assert.equal(
        end,
        start + 1,
        `continuation is not one byte: ${entry.range}`,
      );
      assert.equal(
        bytes[start],
        92,
        `continuation is not a backslash: ${entry.range}`,
      );
      assert.equal(
        bytes[end],
        10,
        `continuation has no physical newline: ${entry.range}`,
      );
      assert.ok(
        start > previousContinuation,
        `continuations are duplicated or out of order: ${entry.range}`,
      );
      previousContinuation = start;
      removedNewlines.add(end);
    }
    ancestors.push({ entry, type, start, end });
  }
  for (const entry of entries) {
    const [start, end] = entry.range
      .split("-")
      .map((point) => pointOffset(bytes, offsets, point));
    assert.ok(
      !(removedNewlines.has(start) && end === start + 1),
      `removed newline is public: ${entry.line}`,
    );
  }
}

function sourceNodes(output, source) {
  const bytes = Buffer.from(source);
  const offsets = lineOffsets(bytes);
  return parseCst(output).map((entry) => {
    const [start, end] = entry.range
      .split("-")
      .map((point) => pointOffset(bytes, offsets, point));
    assert.ok(end >= start, `reversed source range: ${entry.range}`);
    return {
      ...entry,
      ...nodeIdentity(entry),
      start,
      end,
      text: bytes.subarray(start, end).toString(),
    };
  });
}

function sourcePoint(source, byte) {
  const prefix = Buffer.from(source).subarray(0, byte);
  let row = 0;
  for (const value of prefix) if (value === 10) row += 1;
  return `${row}:${prefix.length - prefix.lastIndexOf(10) - 1}`;
}

function lexicalTexts(output, source, type) {
  return sourceNodes(output, source)
    .filter((entry) => entry.type === type)
    .map((entry) => entry.text.replaceAll("\\\n", ""));
}

function rootEndPoint(format, output) {
  const firstLine = output.split("\n", 1)[0];
  if (format === "tree") {
    const match = firstLine.match(
      /^\((program|ERROR) \[[0-9]+, [0-9]+\] - \[([0-9]+), ([0-9]+)\]/,
    );
    return match === null ? undefined : `${match[2]}:${match[3]}`;
  }
  if (format === "cst") {
    const match = firstLine.match(/^[0-9]+:[0-9]+[ ]+-[ ]+([0-9]+:[0-9]+)/);
    return match?.[1];
  }
  if (format === "summary") {
    const summary = JSON.parse(output.slice(output.indexOf("{\n")))
      .parse_summaries?.[0];
    return summary === undefined
      ? undefined
      : `${summary.end.row}:${summary.end.column}`;
  }
  throw new Error(`Unknown parse output format: ${format}`);
}

function assertContains(output, expected, description = expected) {
  assert.ok(
    output.includes(expected),
    `${description}: missing ${expected}\n${output}`,
  );
}

function assertNotContains(output, unexpected, description = unexpected) {
  assert.ok(
    !output.includes(unexpected),
    `${description}: unexpectedly contained ${unexpected}\n${output}`,
  );
}

function assertOccurrenceCount(output, expected, count) {
  assert.notEqual(expected, "", "expected substring is empty");
  assert.equal(
    output.split(expected).length - 1,
    count,
    `expected ${count} occurrences of ${expected}`,
  );
}

function assertNodeCount(output, type, count) {
  const actual = parseCst(output).filter(
    (entry) => nodeIdentity(entry).type === type,
  ).length;
  assert.equal(actual, count, `expected ${count} ${type} nodes`);
}

function normalizeRange(range) {
  return range.split(" ").join("");
}

function assertCstRange(output, expectedRange, expectedItem) {
  const range = normalizeRange(expectedRange);
  assert.ok(
    parseCst(output).some(
      (entry) => entry.range === range && entry.content.includes(expectedItem),
    ),
    `expected ${expectedItem} at ${range}\n${output}`,
  );
}

function assertCstDirectChildRange(
  output,
  parentRange,
  parentItem,
  childRange,
  childItem,
) {
  const entries = parseCst(output);
  const parentIndex = entries.findIndex(
    (entry) =>
      entry.range === normalizeRange(parentRange) &&
      entry.content === parentItem,
  );
  assert.notEqual(
    parentIndex,
    -1,
    `missing parent ${parentItem} at ${parentRange}`,
  );
  const parentDepth = entries[parentIndex].depth;
  for (const entry of entries.slice(parentIndex + 1)) {
    if (entry.depth <= parentDepth) {
      break;
    }
    if (
      entry.depth === parentDepth + 2 &&
      entry.range === normalizeRange(childRange) &&
      (childItem === "*" || entry.content === childItem)
    ) {
      return;
    }
  }
  assert.fail(
    `expected ${childItem} at ${childRange} directly under ${parentItem} at ${parentRange}\n${output}`,
  );
}

function assertCstOutputsEqual(name, left, right) {
  assert.equal(left.status, right.status, `${name}: parse statuses differ`);
  assert.equal(
    cstFingerprint(left.output),
    cstFingerprint(right.output),
    `${name}: public CST contracts differ`,
  );
}

function assertFreshOutputsEqual(name, left, right) {
  assert.equal(left.status, right.status, `${name}: parse statuses differ`);
  assert.deepEqual(
    parseCst(left.output),
    parseCst(right.output),
    `${name}: fresh CST is not deterministic`,
  );
}

function assertSameLogicalProjection(name, logicalOutput, physicalOutput) {
  assert.equal(
    logicalProjection(physicalOutput),
    logicalProjection(logicalOutput),
    `${name}: physical source changes the logical CST`,
  );
}

function lineContinuationManifest(queryOutput) {
  const manifest = [];
  for (const line of queryOutput.split("\n")) {
    const match = line.match(
      /capture: [0-9]+ - line[.]continuation, start: \(([0-9]+), ([0-9]+)\), end: \(([0-9]+), ([0-9]+)\), text:/,
    );
    if (match !== null) {
      manifest.push(`${match[1]}:${match[2]}-${match[3]}:${match[4]}`);
    }
  }
  return manifest;
}

function hasRecovery(cst) {
  return parseCst(cst).some((entry) => entry.recovery);
}

export {
  assertContains,
  assertCstDirectChildRange,
  assertCstOutputsEqual,
  assertCstRange,
  assertCstSourceContract,
  assertFreshOutputsEqual,
  assertNodeCount,
  assertNotContains,
  assertOccurrenceCount,
  assertSameLogicalProjection,
  continuationManifest,
  cstFingerprint,
  hasRecovery,
  lexicalTexts,
  lineContinuationManifest,
  logicalProjection,
  namedStructure,
  nodeIdentity,
  parseCst,
  rootEndPoint,
  sourceByteOffset,
  sourceNodes,
  sourcePoint,
};
