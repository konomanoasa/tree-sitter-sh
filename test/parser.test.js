const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const { after, before, test } = require("node:test");

const {
  createEnvironmentDirectory,
  grammarDirectory,
  grammarName,
  runTreeSitter,
} = require("../scripts/tree-sitter");

const contractsQuerySource = `(line_continuation) @line.continuation

(function_definition
  name: (fname) @function)

(if_clause
  recovery: (compound_command_recovery) @compound.recovery) @compound.owner

(do_group
  recovery: (compound_command_recovery) @compound.recovery) @compound.owner

(case_clause
  recovery: (compound_command_recovery) @compound.recovery) @compound.owner

(for_clause
  recovery: (compound_command_recovery) @compound.recovery) @compound.owner

(while_clause
  recovery: (compound_command_recovery) @compound.recovery) @compound.owner

(until_clause
  recovery: (compound_command_recovery) @compound.recovery) @compound.owner

(brace_group
  recovery: (compound_command_recovery) @group.recovery) @group.owner

(subshell
  recovery: (compound_command_recovery) @group.recovery) @group.owner

(parameter_expansion
  recovery: (parameter_expansion_recovery) @parameter.recovery) @parameter.owner
`;
const defaultParseTimeout = 10_000_000;
let contractsQuery;
let runtimeDirectory;
let parserLibrary;
let sourceSequence = 0;

before(() => {
  runtimeDirectory = createEnvironmentDirectory("tree-sitter-posix-sh-parser");
  contractsQuery = path.join(runtimeDirectory, "contracts.scm");
  fs.writeFileSync(contractsQuery, contractsQuerySource);
  parserLibrary = path.join(runtimeDirectory, "parser");
  runTreeSitter(["build", "--output", parserLibrary, grammarDirectory], {
    environmentDirectory: runtimeDirectory,
  });
});

after(() => {
  fs.rmSync(runtimeDirectory, { force: true, recursive: true });
});

function lines(...sourceLines) {
  return `${sourceLines.join("\n")}\n`;
}

function writeSource(name, contents) {
  sourceSequence += 1;
  const sourcePath = path.join(
    runtimeDirectory,
    `${sourceSequence}-${name.replace(/[^A-Za-z0-9_.-]/g, "-")}.sh`,
  );
  fs.writeFileSync(sourcePath, contents);
  return sourcePath;
}

function sourceEndPoint(sourcePath) {
  const source = fs.readFileSync(sourcePath);
  let row = 0;
  let finalNewline = -1;
  for (let index = 0; index < source.length; index += 1) {
    if (source[index] === 10) {
      row += 1;
      finalNewline = index;
    }
  }
  return `${row}:${source.length - finalNewline - 1}`;
}

function applyEdits(source, edits) {
  let edited = Buffer.from(source);
  for (const edit of edits) {
    const firstSeparator = edit.indexOf(" ");
    const secondSeparator = edit.indexOf(" ", firstSeparator + 1);
    assert.notEqual(firstSeparator, -1, `invalid edit: ${edit}`);
    const start = Number(edit.slice(0, firstSeparator));
    const removedLength = Number(
      edit.slice(
        firstSeparator + 1,
        secondSeparator === -1 ? undefined : secondSeparator,
      ),
    );
    assert.ok(
      Number.isSafeInteger(start) && start >= 0,
      `invalid edit offset: ${edit}`,
    );
    assert.ok(
      Number.isSafeInteger(removedLength) && removedLength >= 0,
      `invalid edit removal length: ${edit}`,
    );
    assert.ok(
      start + removedLength <= edited.length,
      `edit exceeds source byte length: ${edit}`,
    );
    const inserted = Buffer.from(
      secondSeparator === -1 ? "" : edit.slice(secondSeparator + 1),
    );
    edited = Buffer.concat([
      edited.subarray(0, start),
      inserted,
      edited.subarray(start + removedLength),
    ]);
  }
  return edited;
}

function rootEndPoint(format, output) {
  const firstLine = output.split("\n", 1)[0];
  if (format === "tree") {
    const match = firstLine.match(
      /^\(program \[[0-9]+, [0-9]+\] - \[([0-9]+), ([0-9]+)\]/,
    );
    return match === null ? undefined : `${match[1]}:${match[2]}`;
  }
  if (format === "cst") {
    const match = firstLine.match(/^[0-9]+:[0-9]+[ ]+-[ ]+([0-9]+:[0-9]+)/);
    return match?.[1];
  }
  if (format === "summary") {
    const summary = JSON.parse(output).parse_summaries?.[0];
    return summary === undefined
      ? undefined
      : `${summary.end.row}:${summary.end.column}`;
  }
  throw new Error(`Unknown parse output format: ${format}`);
}

function runParse({
  description,
  edits = [],
  format = "cst",
  mode = "valid",
  source,
  expectedSource = source,
  timeout = defaultParseTimeout,
}) {
  const arguments_ = [
    "parse",
    "--lib-path",
    parserLibrary,
    "--lang-name",
    grammarName,
  ];
  if (format === "cst") {
    arguments_.push("--cst");
  } else if (format === "summary") {
    assert.equal(
      mode,
      "valid",
      `${description}: summaries require a valid parse`,
    );
    arguments_.push("--quiet", "--json-summary");
  } else {
    assert.equal(format, "tree", `${description}: unknown parse output format`);
  }
  arguments_.push("--timeout", String(timeout));
  if (edits.length > 0) {
    arguments_.push("--edits", ...edits);
  }
  arguments_.push("--", source);

  const result = runTreeSitter(arguments_, {
    allowedStatuses: [0, 1],
    environmentDirectory: runtimeDirectory,
  });
  const output = result.stdout;
  assert.ok(
    output.length > 0,
    `${description}: parser produced no ${format} output`,
  );

  if (mode === "valid") {
    assert.equal(
      result.status,
      0,
      `${description}: expected a valid parse\n${output}`,
    );
  } else if (mode === "resource") {
    assert.equal(
      format,
      "cst",
      `${description}: resource checks require CST output`,
    );
    const root = parseCst(output)[0]?.content;
    assert.ok(
      root === "program" || root === "ERROR",
      `${description}: resource-bounded parse has no complete program or ERROR root\n${output}`,
    );
  } else {
    assert.equal(mode, "recovery", `${description}: unknown parse mode`);
  }
  if (format === "cst" && mode !== "resource") {
    assert.equal(
      parseCst(output)[0]?.content,
      "program",
      `${description}: parser produced no complete program root\n${output}`,
    );
  }

  const expectedEnd = sourceEndPoint(expectedSource);
  const actualEnd = rootEndPoint(format, output);
  assert.notEqual(
    actualEnd,
    undefined,
    `${description}: parser root range is unreadable`,
  );
  assert.equal(
    actualEnd,
    expectedEnd,
    `${description}: parser stopped at ${actualEnd} before source EOF ${expectedEnd}`,
  );
  return { output, status: result.status };
}

function runQuery(source, queryPath = contractsQuery) {
  return runTreeSitter(
    [
      "query",
      "--lib-path",
      parserLibrary,
      "--lang-name",
      grammarName,
      "--captures",
      queryPath,
      "--",
      source,
    ],
    { environmentDirectory: runtimeDirectory },
  ).stdout;
}

function parseCst(output) {
  const entries = [];
  for (const line of output.split("\n")) {
    const match = line.match(
      /^([0-9]+:[0-9]+)[ ]+-[ ]+([0-9]+:[0-9]+)([ ]+)(.*)$/,
    );
    if (match === null) {
      continue;
    }
    let content = match[4];
    let depth = line.length - content.length;
    if (content.startsWith("•")) {
      content = content.slice(1);
      depth += 1;
    }
    entries.push({
      content,
      depth,
      line,
      range: `${match[1]}-${match[2]}`,
    });
  }
  return entries;
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
  let actual = 0;
  let offset = 0;
  while (true) {
    const found = output.indexOf(expected, offset);
    if (found === -1) {
      break;
    }
    actual += 1;
    offset = found + expected.length;
  }
  assert.equal(actual, count, `expected ${count} occurrences of ${expected}`);
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

function assertValid(source, description = path.basename(source)) {
  return runParse({ description, format: "summary", source });
}

function parseValidTree(source, description = path.basename(source)) {
  return runParse({ description, format: "tree", source }).output;
}

function parseValidCst(source, description = path.basename(source)) {
  return runParse({ description, source }).output;
}

function parseRecovery(source, description = path.basename(source)) {
  return runParse({ description, format: "tree", mode: "recovery", source })
    .output;
}

function parseContainsAll(source, description, ...expectedItems) {
  const output = parseRecovery(source, description);
  for (const expectedItem of expectedItems) {
    assertContains(output, expectedItem, description);
  }
  return output;
}

function cstFingerprint(output) {
  const fingerprint = parseCst(output)
    .map((entry) => entry.line)
    .join("\n");
  assert.notEqual(fingerprint, "", "CST fingerprint is empty");
  return fingerprint;
}

function assertCstOutputsEqual(name, left, right) {
  assert.equal(left.status, right.status, `${name}: parse statuses differ`);
  assert.equal(
    cstFingerprint(left.output),
    cstFingerprint(right.output),
    `${name}: complete CSTs differ`,
  );
}

function assertRepeatedColdParse(
  mode,
  source,
  name,
  timeout = defaultParseTimeout,
) {
  const first = runParse({
    description: `${name} first cold parse`,
    mode,
    source,
    timeout,
  });
  const second = runParse({
    description: `${name} second cold parse`,
    mode,
    source,
    timeout,
  });
  assertCstOutputsEqual(`${name} repeated cold parses`, first, second);
}

function compareIncrementalAndFresh(
  mode,
  initialSource,
  finalSource,
  name,
  ...edits
) {
  assert.ok(
    !fs.readFileSync(initialSource).equals(fs.readFileSync(finalSource)),
    `${name}: incremental inputs are identical`,
  );
  assert.ok(
    applyEdits(fs.readFileSync(initialSource), edits).equals(
      fs.readFileSync(finalSource),
    ),
    `${name}: edit sequence does not produce the final source`,
  );
  const incremental = runParse({
    description: `${name} incremental`,
    edits,
    expectedSource: finalSource,
    mode,
    source: initialSource,
  });
  const fresh = runParse({
    description: `${name} fresh`,
    mode,
    source: finalSource,
  });
  assertCstOutputsEqual(
    `${name} incremental and fresh parses`,
    incremental,
    fresh,
  );
  return [incremental.output, fresh.output];
}

function assertIncrementalEqualsFresh(
  initialSource,
  finalSource,
  name,
  ...edits
) {
  return compareIncrementalAndFresh(
    "valid",
    initialSource,
    finalSource,
    name,
    ...edits,
  );
}

function parseIncrementalAndFresh(initialSource, finalSource, name, ...edits) {
  return compareIncrementalAndFresh(
    "recovery",
    initialSource,
    finalSource,
    name,
    ...edits,
  );
}

function logicalProjection(output) {
  const projection = [];
  let rootDepth;
  for (const entry of parseCst(output)) {
    let { content, depth } = entry;
    if (entry.line.includes("•")) {
      content = `!${content}`;
    }
    if (content.startsWith('"') || content.startsWith("`")) {
      continue;
    }
    content = content.replace(/[ ]+`.*`$/, "");
    if (
      content === "line_continuation" ||
      content.endsWith(": line_continuation")
    ) {
      continue;
    }
    rootDepth ??= depth;
    projection.push(`${depth - rootDepth}:${content}`);
  }
  assert.ok(projection.length > 0, "logical CST projection is empty");
  return projection.join("\n");
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

function assertManifestSourceOrder(manifest, name) {
  let previous;
  for (const range of manifest) {
    const match = range.match(/^([0-9]+):([0-9]+)-([0-9]+):([0-9]+)$/);
    assert.notEqual(
      match,
      null,
      `${name}: invalid line-continuation range ${range}`,
    );
    const current = match.slice(1).map(Number);
    if (previous !== undefined) {
      assert.ok(
        current[0] > previous[0] ||
          (current[0] === previous[0] && current[1] > previous[1]),
        `${name}: line continuations are duplicated or out of order`,
      );
    }
    assert.equal(
      current[2],
      current[0] + 1,
      `${name}: invalid continuation row`,
    );
    assert.equal(current[3], 0, `${name}: invalid continuation end column`);
    previous = current;
  }
}

function assertLineContinuationManifest(
  name,
  physicalSource,
  logicalSource,
  expected,
) {
  const physicalOutput = parseValidCst(physicalSource, `${name} physical`);
  const logicalOutput = parseValidCst(logicalSource, `${name} logical`);
  const actual = lineContinuationManifest(runQuery(physicalSource));
  assertManifestSourceOrder(actual, name);
  assert.deepEqual(actual, expected, `${name}: continuation ranges differ`);
  assertSameLogicalProjection(name, logicalOutput, physicalOutput);
}

function assertNoLineContinuations(name, source) {
  parseValidCst(source, name);
  assert.deepEqual(
    lineContinuationManifest(runQuery(source)),
    [],
    `${name}: literal backslash-newline became line_continuation`,
  );
}

test("parser rejects a timeout as structural recovery", () => {
  const source = writeSource("timeout-guard", `${":\n".repeat(10_000)}`);
  assert.throws(() =>
    runParse({
      description: "timeout guard",
      mode: "recovery",
      source,
      timeout: 1,
    }),
  );
});

test("CST fingerprints distinguish anonymous tokens", () => {
  const semicolon = writeSource("semicolon-fingerprint", "a;b\n");
  const ampersand = writeSource("ampersand-fingerprint", "a&b\n");
  assert.notEqual(
    cstFingerprint(parseValidCst(semicolon)),
    cstFingerprint(parseValidCst(ampersand)),
  );
});

test("line-continuation and comment contracts", () => {
  const syntaxPhysical = writeSource(
    "syntax-continuation-physical",
    lines('echo "$(printf x\\', ')"', "echo `printf x\\", "`"),
  );
  const syntaxLogical = writeSource(
    "syntax-continuation-logical",
    lines('echo "$(printf x)"', "echo `printf x`"),
  );
  assertLineContinuationManifest(
    "line-continuation-syntax-contract",
    syntaxPhysical,
    syntaxLogical,
    ["0:16-1:0", "2:14-3:0"],
  );

  const arithmeticPhysical = writeSource(
    "arithmetic-continuation-physical",
    lines(': "$((1 + \\', '2))"', ': "$((1 + 2\\', '))"'),
  );
  const arithmeticLogical = writeSource(
    "arithmetic-continuation-logical",
    lines(': "$((1 + 2))"', ': "$((1 + 2))"'),
  );
  assertLineContinuationManifest(
    "line-continuation-arithmetic-contract",
    arithmeticPhysical,
    arithmeticLogical,
    ["0:10-1:0", "2:11-3:0"],
  );

  const literalContinuations = writeSource(
    "literal-line-continuations",
    lines(
      "printf '%s\\n' 'single\\",
      "quote' $'dollar\\",
      "quote'",
      "cat <<'EOF'",
      "quoted\\",
      "body",
      "EOF",
    ),
  );
  assertNoLineContinuations("line-continuation-literals", literalContinuations);

  const terminalAssignmentPhysical = writeSource(
    "terminal-assignment-physical",
    "A=\\\n",
  );
  const terminalAssignmentLogical = writeSource(
    "terminal-assignment-logical",
    "A=",
  );
  const terminalPhysicalOutput = parseValidCst(terminalAssignmentPhysical);
  const terminalLogicalOutput = parseValidCst(terminalAssignmentLogical);
  assertCstRange(terminalPhysicalOutput, "0:2-1:0", "line_continuation");
  assertOccurrenceCount(terminalPhysicalOutput, "line_continuation", 1);
  assertSameLogicalProjection(
    "terminal-assignment-continuation",
    terminalLogicalOutput,
    terminalPhysicalOutput,
  );

  const comments = writeSource(
    "comments",
    lines(
      "  # leading",
      "command # trailing",
      "cat <<EOF # declaration",
      "body",
      "EOF",
    ),
  );
  const commentsOutput = parseValidTree(comments);
  assertContains(commentsOutput, "(comment [0, 2] - [0, 11])");
  assertContains(commentsOutput, "(comment [1, 8] - [1, 18])");
  assertContains(commentsOutput, "comment: (comment [2, 10] - [2, 23])");

  const continuedComments = writeSource(
    "continued-comments",
    lines(
      "\\",
      "# leading",
      "first && \\",
      "# operator",
      "second",
      "cat <<EOF \\",
      "# declaration",
      "body",
      "EOF",
    ),
  );
  const continuedCommentsOutput = parseValidTree(continuedComments);
  for (const expected of [
    "(line_continuation [0, 0] - [1, 0])",
    "(comment [1, 0] - [1, 9])",
    "(line_continuation [2, 9] - [3, 0])",
    "(comment [3, 0] - [3, 10])",
    "(line_continuation [5, 10] - [6, 0])",
    "comment: (comment [6, 0] - [6, 13])",
  ]) {
    assertContains(continuedCommentsOutput, expected);
  }
  assertNotContains(continuedCommentsOutput, "ERROR");

  const nulComment = writeSource(
    "nul-comment",
    Buffer.from("#a\0b\nnext\n", "utf8"),
  );
  const nulCommentOutput = runParse({
    description: "NUL inside comment",
    mode: "recovery",
    source: nulComment,
  }).output;
  assertCstRange(nulCommentOutput, "0:0-0:4", "comment");
  assertCstRange(nulCommentOutput, "1:0-1:4", "command: complete_command");
  assertCstRange(nulCommentOutput, "0:0-2:0", "program");

  const continuedCommentInitial = writeSource(
    "continued-comment-initial",
    lines("first && # comment", "second"),
  );
  const continuedCommentFinal = writeSource(
    "continued-comment-final",
    lines("first && \\", "# comment", "second"),
  );
  assertIncrementalEqualsFresh(
    continuedCommentInitial,
    continuedCommentFinal,
    "insert-comment-boundary-continuation",
    "9 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    continuedCommentFinal,
    continuedCommentInitial,
    "delete-comment-boundary-continuation",
    "9 2",
  );
});

test("list recovery stops before raw command separators", () => {
  const rawBoundary = writeSource(
    "list-recovery-raw-boundary",
    lines("printf )", "printf hi"),
  );
  const rawOutput = runParse({
    description: "ownerless right parenthesis before a raw newline",
    mode: "recovery",
    source: rawBoundary,
  }).output;
  assertCstRange(rawOutput, "0:7-0:7", "separator: separator_recovery");
  assertCstRange(rawOutput, "0:7-0:8", "recovery: command_recovery");
  assertCstRange(rawOutput, "1:0-1:9", "command: complete_command");
  assertOccurrenceCount(rawOutput, "command: complete_command", 2);
  assertNotContains(rawOutput, "ERROR");
  assertRepeatedColdParse(
    "recovery",
    rawBoundary,
    "ownerless right parenthesis raw boundary",
  );

  const hierarchyBoundaries = writeSource(
    "list-recovery-command-hierarchies",
    lines(
      "printf x | printf )",
      "true && printf )",
      "if :; then :; fi )",
      "after",
    ),
  );
  const hierarchyOutput = runParse({
    description: "ownerless right parentheses after command hierarchies",
    mode: "recovery",
    source: hierarchyBoundaries,
  }).output;
  for (const range of ["0:18-0:19", "1:15-1:16", "2:17-2:18"]) {
    assertCstRange(hierarchyOutput, range, "recovery: command_recovery");
  }
  assertOccurrenceCount(hierarchyOutput, "recovery: command_recovery", 3);
  assertCstRange(hierarchyOutput, "3:0-3:5", "command: complete_command");
  assertNotContains(hierarchyOutput, "ERROR");

  const commentInitial = writeSource(
    "list-recovery-comment-initial",
    lines("printf )", "printf hi", "after"),
  );
  const commentFinal = writeSource(
    "list-recovery-comment-final",
    lines("printf )", "# printf hi", "after"),
  );
  for (const output of parseIncrementalAndFresh(
    commentInitial,
    commentFinal,
    "insert-comment-after-ownerless-right-parenthesis",
    "9 0 # ",
  )) {
    assertCstRange(output, "0:7-0:8", "recovery: command_recovery");
    assertCstRange(output, "1:0-1:11", "comment");
    assertCstRange(output, "2:0-2:5", "command: complete_command");
    assertNotContains(output, "ERROR");
  }

  const escapedBoundary = writeSource(
    "list-recovery-escaped-boundary",
    lines("printf \\", ")", "after"),
  );
  const escapedOutput = runParse({
    description: "ownerless right parenthesis after a line continuation",
    mode: "recovery",
    source: escapedBoundary,
  }).output;
  assertCstRange(escapedOutput, "0:7-1:0", "line_continuation");
  assertOccurrenceCount(escapedOutput, "line_continuation", 1);
  assertCstRange(escapedOutput, "1:0-1:1", "recovery: command_recovery");
  assertCstRange(escapedOutput, "2:0-2:5", "command: complete_command");
  assertNotContains(escapedOutput, "ERROR");

  const nestedBoundary = writeSource(
    "list-recovery-nested-boundary",
    lines("echo $(printf x\\", ") )", "after"),
  );
  const nestedOutput = runParse({
    description: "formal and ownerless right parentheses retain owners",
    mode: "recovery",
    source: nestedBoundary,
  }).output;
  assertCstRange(nestedOutput, "0:5-1:1", "command_substitution");
  assertCstRange(nestedOutput, "0:15-1:0", "line_continuation");
  assertCstRange(nestedOutput, "1:2-1:3", "recovery: command_recovery");
  assertCstRange(nestedOutput, "2:0-2:5", "command: complete_command");
  assertNotContains(nestedOutput, "ERROR");
});

test("formal right-parenthesis ownership survives boundary continuations", () => {
  const logical = writeSource(
    "formal-right-parenthesis-logical",
    lines("(printf )", "after"),
  );
  const physical = writeSource(
    "formal-right-parenthesis-physical",
    lines("(printf \\", ")", "after"),
  );
  const logicalOutput = parseValidCst(logical);
  const physicalOutput = parseValidCst(physical);
  assertSameLogicalProjection(
    "formal right parenthesis boundary continuation",
    logicalOutput,
    physicalOutput,
  );
  assertCstRange(physicalOutput, "0:0-1:1", "subshell");
  assertCstRange(physicalOutput, "0:8-1:0", "line_continuation");
  assertOccurrenceCount(physicalOutput, "line_continuation", 1);
  assertCstDirectChildRange(
    physicalOutput,
    "0:0-1:1",
    "subshell",
    "1:0-1:1",
    '")"',
  );
  assertCstRange(physicalOutput, "2:0-2:5", "command: complete_command");
  assertNotContains(physicalOutput, "ERROR");
  assertNotContains(physicalOutput, "command_recovery");
  assertIncrementalEqualsFresh(
    logical,
    physical,
    "insert-formal-right-parenthesis-boundary-continuation",
    "8 0 \\\n",
  );

  const repeatedLogical = writeSource(
    "formal-right-parenthesis-repeated-logical",
    lines("(printf  )", "after"),
  );
  const repeatedPhysical = writeSource(
    "formal-right-parenthesis-repeated-physical",
    lines("(printf \\", " \\", ")", "after"),
  );
  const repeatedLogicalOutput = parseValidCst(repeatedLogical);
  const repeatedPhysicalOutput = parseValidCst(repeatedPhysical);
  assertSameLogicalProjection(
    "formal right parenthesis repeated boundary continuations",
    repeatedLogicalOutput,
    repeatedPhysicalOutput,
  );
  assertCstRange(repeatedPhysicalOutput, "0:8-1:0", "line_continuation");
  assertCstRange(repeatedPhysicalOutput, "1:1-2:0", "line_continuation");
  assertOccurrenceCount(repeatedPhysicalOutput, "line_continuation", 2);
  assertCstDirectChildRange(
    repeatedPhysicalOutput,
    "0:0-2:1",
    "subshell",
    "2:0-2:1",
    '")"',
  );
  assertCstRange(
    repeatedPhysicalOutput,
    "3:0-3:5",
    "command: complete_command",
  );
  assertNotContains(repeatedPhysicalOutput, "ERROR");
  assertNotContains(repeatedPhysicalOutput, "command_recovery");
});

test("structural recovery preserves closer ownership", () => {
  const redirectionRecovery = writeSource(
    "recovery-redirection",
    lines("before >;", "after"),
  );
  const redirectionOutput = parseValidTree(redirectionRecovery);
  assertContains(
    redirectionOutput,
    "recovery: (redirection_target_recovery [0, 8] - [0, 8])",
  );
  assertContains(redirectionOutput, "(complete_command [1, 0] - [1, 5]");

  const andOrRecovery = writeSource(
    "recovery-and-or",
    lines("before", "after &&"),
  );
  const andOrOutput = parseValidTree(andOrRecovery);
  assertContains(andOrOutput, "(complete_command [0, 0] - [0, 6]");
  assertContains(andOrOutput, "operator: (and_if [1, 6] - [1, 8])");
  assertContains(andOrOutput, "recovery: (command_recovery [2, 0] - [2, 0])");

  const compoundRecovery = writeSource(
    "recovery-compound",
    lines("before", "if condition; then", "  inside"),
  );
  const compoundOutput = parseValidTree(compoundRecovery);
  assertContains(compoundOutput, "(complete_command [0, 0] - [0, 6]");
  assertContains(compoundOutput, "(and_or [2, 2] - [2, 8]");
  assertContains(
    compoundOutput,
    "recovery: (compound_command_recovery [3, 0] - [3, 0])",
  );
  const compoundQuery = runQuery(compoundRecovery);
  assertContains(compoundQuery, "compound.owner");
  assertContains(compoundQuery, "compound.recovery");

  const nestedCompoundInitial = writeSource(
    "recovery-nested-compound-initial",
    lines("if x; then while y; do z; done; fi"),
  );
  const nestedCompoundFinal = writeSource(
    "recovery-nested-compound-final",
    lines("if x; then while y; do z; fi"),
  );
  for (const output of parseIncrementalAndFresh(
    nestedCompoundInitial,
    nestedCompoundFinal,
    "nested-compound-outer-closer",
    "26 6",
  )) {
    assertCstRange(output, "0:11-0:26", "while_clause");
    assertCstRange(output, "0:26-0:26", "recovery: compound_command_recovery");
    assertCstRange(output, "0:26-0:28", "fi_keyword");
  }

  const nestedEofInitial = writeSource(
    "recovery-nested-eof-initial",
    lines("worker() {", " if true", " then", " printf yes", " fi", "}"),
  );
  const nestedEofFinal = writeSource(
    "recovery-nested-eof-final",
    lines("worker() {", " if true", " then", " printf yes"),
  );
  for (const output of parseIncrementalAndFresh(
    nestedEofInitial,
    nestedEofFinal,
    "delete-nested-compound-closers",
    "38 6",
  )) {
    assertCstRange(output, "0:0-4:0", "function_definition");
    assertCstRange(output, "0:9-4:0", "brace_group");
    assertCstRange(output, "1:1-4:0", "if_clause");
    assertOccurrenceCount(output, "recovery: compound_command_recovery", 2);
    assertCstRange(output, "4:0-4:0", "recovery: compound_command_recovery");
    assertNotContains(output, "ERROR");
  }

  const nestedCase = writeSource(
    "recovery-nested-case",
    lines("case x in x) if y; then z;; esac"),
  );
  const nestedCaseOutput = parseRecovery(nestedCase);
  for (const expected of [
    "(if_clause [0, 13] - [0, 25]",
    "(separator_recovery [0, 25] - [0, 25])",
    "recovery: (compound_command_recovery [0, 25] - [0, 25])",
    "terminator: (dsemi [0, 25] - [0, 27])",
    "(esac_keyword [0, 28] - [0, 32])",
  ]) {
    assertContains(nestedCaseOutput, expected);
  }

  const sameKindIf = writeSource(
    "recovery-same-kind-if",
    lines("if x; then if y; then z; fi"),
  );
  const sameKindIfOutput = parseRecovery(sameKindIf);
  assertContains(sameKindIfOutput, "(fi_keyword [0, 25] - [0, 27])");
  assertContains(
    sameKindIfOutput,
    "recovery: (compound_command_recovery [1, 0] - [1, 0])",
  );

  const sameKindWhile = writeSource(
    "recovery-same-kind-while",
    lines("while x; do while y; do z; done"),
  );
  const sameKindWhileOutput = parseRecovery(sameKindWhile);
  assertContains(sameKindWhileOutput, "(done_keyword [0, 27] - [0, 31])");
  assertContains(
    sameKindWhileOutput,
    "recovery: (compound_command_recovery [1, 0] - [1, 0])",
  );

  const reservedWordArguments = writeSource(
    "recovery-reserved-word-arguments",
    lines(
      "if x; then echo fi done esac; fi",
      "case x in x) echo fi done esac;; esac",
      "case fi in fi) :;; esac",
      "case done in done) :;; esac",
      "if x; then case fi in fi) :;; esac; fi",
      "while x; do case done in done) :;; esac; done",
    ),
  );
  assertValid(reservedWordArguments);

  const caseEsacPattern = writeSource(
    "case-esac-pattern",
    lines("case esac in esac) :;; esac"),
  );
  assertContains(
    parseRecovery(caseEsacPattern),
    "(esac_keyword [0, 13] - [0, 17])",
  );

  const parameterInitial = writeSource(
    "recovery-parameter-initial",
    lines("echo $" + "{x}; next"),
  );
  const parameterFinal = writeSource(
    "recovery-parameter-final",
    lines("echo ${x; next"),
  );
  for (const output of parseIncrementalAndFresh(
    parameterInitial,
    parameterFinal,
    "parameter-tail-recovery",
    "8 1",
  )) {
    assertCstRange(output, "0:8-0:8", "recovery: parameter_expansion_recovery");
    assertCstRange(output, "0:10-0:14", "and_or");
  }
  const parameterQuery = runQuery(parameterFinal);
  assertContains(parameterQuery, "parameter.owner");
  assertContains(parameterQuery, "parameter.recovery");

  const parameterBoundaries = writeSource(
    "recovery-parameter-boundaries",
    lines(
      "echo ${x",
      "next",
      'echo "${x"; next',
      "echo $(printf ${x); next",
      "echo ${00& next",
    ),
  );
  const parameterBoundariesOutput = parseRecovery(parameterBoundaries);
  for (const expected of [
    "recovery: (parameter_expansion_recovery [0, 8] - [0, 8])",
    "(complete_command [1, 0] - [1, 4]",
    "recovery: (parameter_expansion_recovery [2, 9] - [2, 9])",
    "recovery: (parameter_expansion_recovery [3, 17] - [3, 17])",
    "recovery: (parameter_expansion_recovery [4, 9] - [4, 9])",
  ]) {
    assertContains(parameterBoundariesOutput, expected);
  }

  const parameterMetacharacters = writeSource(
    "parameter-operator-metacharacters",
    lines("echo $" + "{x:-a;b} $" + "{x:-a&b}"),
  );
  assertNotContains(
    parseValidTree(parameterMetacharacters),
    "parameter_expansion_recovery",
  );

  const subshellInitial = writeSource(
    "recovery-subshell-initial",
    lines("(inside)"),
  );
  const subshellFinal = writeSource(
    "recovery-subshell-final",
    lines("(inside"),
  );
  for (const output of parseIncrementalAndFresh(
    subshellInitial,
    subshellFinal,
    "unclosed-subshell",
    "7 1",
  )) {
    assertCstRange(output, "0:0-1:0", "subshell");
    assertCstRange(output, "0:1-0:7", "and_or");
    assertCstRange(output, "1:0-1:0", "recovery: compound_command_recovery");
  }
  const groupQuery = runQuery(subshellFinal);
  assertContains(groupQuery, "group.owner");
  assertContains(groupQuery, "group.recovery");

  const braceInitial = writeSource(
    "recovery-brace-group-initial",
    lines("{ inside; }"),
  );
  const braceFinal = writeSource(
    "recovery-brace-group-final",
    lines("{ inside;"),
  );
  for (const output of parseIncrementalAndFresh(
    braceInitial,
    braceFinal,
    "unclosed-brace-group",
    "9 2",
  )) {
    assertCstRange(output, "0:0-1:0", "brace_group");
    assertCstRange(output, "0:2-0:8", "and_or");
    assertCstRange(output, "1:0-1:0", "recovery: compound_command_recovery");
  }

  const nestedGroups = writeSource(
    "recovery-nested-groups",
    lines("{ (inside; }", "( { inside; )"),
  );
  const nestedGroupsOutput = parseRecovery(nestedGroups);
  for (const expected of [
    "(subshell [0, 2] - [0, 11]",
    "recovery: (compound_command_recovery [0, 11] - [0, 11])",
    "(brace_group [1, 2] - [1, 12]",
    "recovery: (compound_command_recovery [1, 12] - [1, 12])",
  ]) {
    assertContains(nestedGroupsOutput, expected);
  }

  const nestedCompoundGroups = writeSource(
    "recovery-nested-compound-groups",
    lines("{ if x; then y; }", "(if x; then y;)"),
  );
  const nestedCompoundGroupsOutput = parseRecovery(nestedCompoundGroups);
  for (const expected of [
    "(brace_group [0, 0] - [0, 17]",
    "(if_clause [0, 2] - [0, 16]",
    "recovery: (compound_command_recovery [0, 16] - [0, 16])",
    "(subshell [1, 0] - [1, 15]",
    "(if_clause [1, 1] - [1, 14]",
    "recovery: (compound_command_recovery [1, 14] - [1, 14])",
  ]) {
    assertContains(nestedCompoundGroupsOutput, expected);
  }

  const unclosedFunction = writeSource(
    "recovery-function-group",
    lines("f() { inside;"),
  );
  const unclosedFunctionOutput = parseRecovery(unclosedFunction);
  for (const expected of [
    "(function_definition [0, 0] - [1, 0]",
    "(brace_group [0, 4] - [1, 0]",
    "recovery: (compound_command_recovery [1, 0] - [1, 0])",
  ]) {
    assertContains(unclosedFunctionOutput, expected);
  }

  const emptyGroups = writeSource("recovery-empty-groups", lines("()", "{ }"));
  const emptyGroupsOutput = parseRecovery(emptyGroups);
  for (const expected of [
    "(subshell [0, 0] - [0, 2]",
    "recovery: (compound_command_recovery [0, 1] - [0, 1])",
    "(brace_group [1, 0] - [1, 3]",
    "recovery: (compound_command_recovery [1, 2] - [1, 2])",
  ]) {
    assertContains(emptyGroupsOutput, expected);
  }

  const longBraceCloser = writeSource(
    "recovery-long-brace-closer",
    lines("holder() { first; }suffix"),
  );
  const longBraceOutput = parseRecovery(longBraceCloser);
  assertContains(longBraceOutput, "(brace_group [0, 9] - [1, 0]");
  assertContains(
    longBraceOutput,
    "recovery: (compound_command_recovery [1, 0] - [1, 0])",
  );

  const rightBrace = writeSource("recovery-right-brace", lines("first; }"));
  assertNotContains(parseRecovery(rightBrace), "(word [0, 7] - [0, 8]");
});

test("structural recovery preserves empty, missing, and stray structures", () => {
  const parameterPatternInitial = writeSource(
    "recovery-parameter-pattern-initial",
    lines('echo "$' + '{x%foo}"; next'),
  );
  const parameterPatternFinal = writeSource(
    "recovery-parameter-pattern-final",
    lines('echo "${x%foo"; next'),
  );
  for (const output of parseIncrementalAndFresh(
    parameterPatternInitial,
    parameterPatternFinal,
    "parameter-pattern-tail-recovery",
    "13 1",
  )) {
    assertCstRange(output, "0:6-0:13", "parameter_expansion");
    assertCstRange(
      output,
      "0:13-0:13",
      "recovery: parameter_expansion_recovery",
    );
    assertCstRange(output, "0:16-0:20", "and_or");
  }
  assertCstDirectChildRange(
    parseValidCst(parameterPatternFinal),
    "0:5-0:14",
    "double_quoted",
    "0:13-0:14",
    "*",
  );

  const emptyBodies = writeSource(
    "recovery-empty-bodies",
    lines(
      "if x; then fi",
      "after_if",
      "while x; do done",
      "after_while",
      "for x; do done",
      "after_for",
      "{ }",
      "after_brace",
      "()",
      "after_subshell",
    ),
  );
  const emptyBodiesOutput = parseContainsAll(
    emptyBodies,
    "empty compound-command bodies",
    "(if_clause [0, 0] - [0, 13]",
    "recovery: (compound_command_recovery [0, 11] - [0, 11])",
    "(fi_keyword [0, 11] - [0, 13])",
    "(complete_command [1, 0] - [1, 8]",
    "(while_clause [2, 0] - [2, 16]",
    "recovery: (compound_command_recovery [2, 12] - [2, 12])",
    "(done_keyword [2, 12] - [2, 16])",
    "(complete_command [3, 0] - [3, 11]",
    "(for_clause [4, 0] - [4, 14]",
    "recovery: (compound_command_recovery [4, 10] - [4, 10])",
    "(done_keyword [4, 10] - [4, 14])",
    "(complete_command [5, 0] - [5, 9]",
    "(brace_group [6, 0] - [6, 3]",
    "recovery: (compound_command_recovery [6, 2] - [6, 2])",
    "(complete_command [7, 0] - [7, 11]",
    "(subshell [8, 0] - [8, 2]",
    "recovery: (compound_command_recovery [8, 1] - [8, 1])",
    "(complete_command [9, 0] - [9, 14]",
  );
  assert.ok(emptyBodiesOutput.length > 0);
  const emptyBodiesCst = parseValidCst(emptyBodies);
  assertCstDirectChildRange(
    emptyBodiesCst,
    "6:0-6:3",
    "brace_group",
    "6:2-6:3",
    '"}"',
  );
  assertCstDirectChildRange(
    emptyBodiesCst,
    "8:0-8:2",
    "subshell",
    "8:1-8:2",
    '")"',
  );

  const emptyIfInitial = writeSource(
    "recovery-empty-if-initial",
    lines("if x; then :; fi", "next"),
  );
  const emptyIfFinal = writeSource(
    "recovery-empty-if-final",
    lines("if x; then fi", "next"),
  );
  parseIncrementalAndFresh(
    emptyIfInitial,
    emptyIfFinal,
    "delete-if-body",
    "11 3",
  );

  const emptyDoInitial = writeSource(
    "recovery-empty-do-initial",
    lines("while x; do :; done", "next"),
  );
  const emptyDoFinal = writeSource(
    "recovery-empty-do-final",
    lines("while x; do done", "next"),
  );
  parseIncrementalAndFresh(
    emptyDoInitial,
    emptyDoFinal,
    "delete-do-group-body",
    "12 3",
  );

  const emptyBraceInitial = writeSource(
    "recovery-empty-brace-initial",
    lines("{ :; }", "next"),
  );
  const emptyBraceFinal = writeSource(
    "recovery-empty-brace-final",
    lines("{ }", "next"),
  );
  parseIncrementalAndFresh(
    emptyBraceInitial,
    emptyBraceFinal,
    "delete-brace-body",
    "2 3",
  );

  const missingOwners = writeSource(
    "recovery-missing-owners",
    lines(
      "case",
      "after_case",
      "case # comment",
      "after_commented_case",
      "for",
      "after_for",
      "for # comment",
      "after_commented_for",
      "f()",
      "after_function",
      "g() # comment",
      "after_commented_function",
    ),
  );
  parseContainsAll(
    missingOwners,
    "missing compound and function owners",
    "(case_clause [0, 0] - [0, 4]",
    "(complete_command [1, 0] - [1, 10]",
    "(case_clause [2, 0] - [2, 5]",
    "(complete_command [3, 0] - [3, 20]",
    "(for_clause [4, 0] - [4, 3]",
    "(complete_command [5, 0] - [5, 9]",
    "(for_clause [6, 0] - [6, 4]",
    "(complete_command [7, 0] - [7, 19]",
    "(function_definition [8, 0] - [8, 3]",
    "name: (fname [8, 0] - [8, 1])",
    "(complete_command [9, 0] - [9, 14]",
    "(function_definition [10, 0] - [10, 3]",
    "name: (fname [10, 0] - [10, 1])",
    "(complete_command [11, 0] - [11, 24]",
  );

  const functionBodyInitial = writeSource(
    "recovery-function-body-initial",
    lines("f()", "{ :; }"),
  );
  const functionBodyFinal = writeSource(
    "recovery-function-body-final",
    lines("f()", "next"),
  );
  parseIncrementalAndFresh(
    functionBodyInitial,
    functionBodyFinal,
    "replace-function-body",
    "4 6 next",
  );

  const strayClosers = writeSource(
    "recovery-stray-closers",
    lines(
      "first; fi",
      "after",
      "first; }",
      "after_brace",
      "first; )",
      "after_parenthesis",
      "first; ;;",
      "after_dsemi",
      "first; ;&",
      "after_semi_and",
    ),
  );
  const strayOutput = parseContainsAll(
    strayClosers,
    "stray closers",
    "recovery: (command_recovery [0, 7] - [0, 9]",
    "(fi_keyword [0, 7] - [0, 9])",
    "(complete_command [1, 0] - [1, 5]",
    "recovery: (command_recovery [2, 7] - [2, 8])",
    "(complete_command [3, 0] - [3, 11]",
    "recovery: (command_recovery [4, 7] - [4, 8])",
    "(complete_command [5, 0] - [5, 17]",
    "recovery: (command_recovery [6, 7] - [6, 9]",
    "(dsemi [6, 7] - [6, 9])",
    "(complete_command [7, 0] - [7, 11]",
    "recovery: (command_recovery [8, 7] - [8, 9]",
    "(semi_and [8, 7] - [8, 9])",
    "(complete_command [9, 0] - [9, 14]",
  );
  for (const unexpected of [
    "(word [0, 7] - [0, 9]",
    "(word [2, 7] - [2, 8]",
    "(word [4, 7] - [4, 8]",
    "(word [6, 7] - [6, 9]",
    "(word [8, 7] - [8, 9]",
  ]) {
    assertNotContains(strayOutput, unexpected);
  }

  const strayInitial = writeSource(
    "recovery-stray-initial",
    lines("first", "after"),
  );
  const strayReservedFinal = writeSource(
    "recovery-stray-final",
    lines("first; fi", "after"),
  );
  parseIncrementalAndFresh(
    strayInitial,
    strayReservedFinal,
    "insert-stray-reserved-closer",
    "5 0 ; fi",
  );
  const strayParenthesisFinal = writeSource(
    "recovery-stray-parenthesis-final",
    lines("first; )", "after"),
  );
  parseIncrementalAndFresh(
    strayInitial,
    strayParenthesisFinal,
    "insert-stray-right-parenthesis",
    "5 0 ; )",
  );

  const andIfSemicolon = writeSource(
    "recovery-and-if-semicolon",
    lines("broken() {", "  first &&;", "}", "after=$(ok)"),
  );
  parseContainsAll(
    andIfSemicolon,
    "and-if before semicolon",
    "(function_definition [0, 0] - [2, 1]",
    "recovery: (command_recovery [1, 10] - [1, 10]",
    "(complete_command [3, 0] - [3, 11]",
  );

  const andIfNewlineInitial = writeSource(
    "recovery-and-if-newline-initial",
    lines("broken() {", "  first && :", "}", "after=$(ok)"),
  );
  const andIfNewlineFinal = writeSource(
    "recovery-and-if-newline-final",
    lines("broken() {", "  first && ", "}", "after=$(ok)"),
  );
  for (const output of parseIncrementalAndFresh(
    andIfNewlineInitial,
    andIfNewlineFinal,
    "delete-command-after-and-if",
    "22 1",
  )) {
    assertCstRange(output, "2:0-2:0", "recovery: command_recovery");
    assertCstRange(output, "3:0-3:11", "complete_command");
  }
});

test("substitution, redirection, and token boundaries retain ownership", () => {
  const backquoteOpeners = writeSource(
    "backquote-opener-boundaries",
    lines(
      "{ `printf brace`; }",
      ": && `printf and-or`",
      "case `printf selector` in",
      "  selector) : ;;",
      "esac",
    ),
  );
  const backquoteOpenersOutput = parseValidTree(backquoteOpeners);
  for (const expected of [
    "(backquote_substitution [0, 2] - [0, 16]",
    "(backquote_substitution [1, 5] - [1, 20]",
    "(backquote_substitution [2, 5] - [2, 22]",
  ]) {
    assertContains(backquoteOpenersOutput, expected);
  }
  assertNotContains(backquoteOpenersOutput, "recovery");

  const assignmentInitial = writeSource(
    "backquote-assignment-initial",
    lines("worker() {", "  :", "}", 'result="`printf nested`"'),
  );
  const assignmentFinal = writeSource(
    "backquote-assignment-final",
    lines("worker() {", "  :", "}", "result=`printf nested`"),
  );
  assertIncrementalEqualsFresh(
    assignmentInitial,
    assignmentFinal,
    "remove-double-quotes-around-assignment-backquote",
    "24 1",
    "39 1",
  );

  const substitutionLayoutInitial = writeSource(
    "command-substitution-layout-initial",
    lines("echo $(first;)"),
  );
  const substitutionLayoutFinal = writeSource(
    "command-substitution-layout-final",
    lines("echo $(first; )"),
  );
  assertIncrementalEqualsFresh(
    substitutionLayoutInitial,
    substitutionLayoutFinal,
    "insert-layout-before-command-substitution-closer",
    "13 0  ",
  );

  const backquoteLayoutInitial = writeSource(
    "backquote-layout-initial",
    lines("echo `first;`"),
  );
  const backquoteLayoutFinal = writeSource(
    "backquote-layout-final",
    lines("echo `first; `"),
  );
  assertIncrementalEqualsFresh(
    backquoteLayoutInitial,
    backquoteLayoutFinal,
    "insert-layout-before-backquote-closer",
    "12 0  ",
  );

  const redirectedCompoundWithout = writeSource(
    "redirected-compound-without-separator",
    lines(': "$({ :; }>g)"'),
  );
  const redirectedCompoundWith = writeSource(
    "redirected-compound-with-separator",
    lines(': "$({ :; }>g;)"'),
  );
  assertIncrementalEqualsFresh(
    redirectedCompoundWithout,
    redirectedCompoundWith,
    "insert-separator-after-redirected-compound-command",
    "13 0 ;",
  );
  const [, redirectedCompoundOutput] = assertIncrementalEqualsFresh(
    redirectedCompoundWith,
    redirectedCompoundWithout,
    "delete-separator-after-redirected-compound-command",
    "13 1",
  );
  assertCstRange(redirectedCompoundOutput, "0:3-0:14", "command_substitution");
  assertCstRange(
    redirectedCompoundOutput,
    "0:5-0:13",
    "command: complete_command",
  );
  assertCstRange(
    redirectedCompoundOutput,
    "0:5-0:11",
    "body: compound_command",
  );
  assertCstRange(
    redirectedCompoundOutput,
    "0:11-0:13",
    "redirects: redirect_list",
  );
  assertCstDirectChildRange(
    redirectedCompoundOutput,
    "0:3-0:14",
    "command_substitution",
    "0:13-0:14",
    '")"',
  );
  assertNotContains(redirectedCompoundOutput, "recovery");

  const redirectedFunctionWithout = writeSource(
    "redirected-function-without-separator",
    lines(': "$(f(){ :; }>g)"'),
  );
  const redirectedFunctionWith = writeSource(
    "redirected-function-with-separator",
    lines(': "$(f(){ :; }>g;)"'),
  );
  assertIncrementalEqualsFresh(
    redirectedFunctionWithout,
    redirectedFunctionWith,
    "insert-separator-after-redirected-function",
    "16 0 ;",
  );
  const [, redirectedFunctionOutput] = assertIncrementalEqualsFresh(
    redirectedFunctionWith,
    redirectedFunctionWithout,
    "delete-separator-after-redirected-function",
    "16 1",
  );
  for (const [range, item] of [
    ["0:3-0:17", "command_substitution"],
    ["0:5-0:16", "body: function_definition"],
    ["0:8-0:16", "body: function_body"],
    ["0:8-0:14", "body: compound_command"],
    ["0:14-0:16", "redirects: redirect_list"],
  ]) {
    assertCstRange(redirectedFunctionOutput, range, item);
  }
  assertCstDirectChildRange(
    redirectedFunctionOutput,
    "0:3-0:17",
    "command_substitution",
    "0:16-0:17",
    '")"',
  );
  assertNotContains(redirectedFunctionOutput, "recovery");

  const redirectTargetInitial = writeSource(
    "redirect-target-word-initial",
    lines("<2>x"),
  );
  const redirectTargetFinal = writeSource(
    "redirect-target-word-final",
    lines('<""2>x'),
  );
  assertIncrementalEqualsFresh(
    redirectTargetInitial,
    redirectTargetFinal,
    "insert-empty-quote-before-redirect-target-digit",
    '1 0 ""',
  );

  const locationCloserInitial = writeSource(
    "io-location-closer-initial",
    lines("{a}>x"),
  );
  const locationCloserFinal = writeSource(
    "io-location-closer-final",
    lines("{a}}>x"),
  );
  assertIncrementalEqualsFresh(
    locationCloserInitial,
    locationCloserFinal,
    "extend-io-location-through-right-brace",
    "3 0 }",
  );

  const locationContinuationInitial = writeSource(
    "io-location-continuation-initial",
    lines("{a}>x"),
  );
  const locationContinuationFinal = writeSource(
    "io-location-continuation-final",
    lines("{a}\\", ">x"),
  );
  const locationContinuationOutput = parseValidCst(locationContinuationFinal);
  assertCstRange(
    locationContinuationOutput,
    "0:0-0:3",
    "location: io_location",
  );
  assertCstRange(locationContinuationOutput, "0:3-1:0", "line_continuation");
  assertCstDirectChildRange(
    locationContinuationOutput,
    "1:0-1:2",
    "body: io_file",
    "1:0-1:1",
    '">"',
  );
  assertIncrementalEqualsFresh(
    locationContinuationInitial,
    locationContinuationFinal,
    "insert-continuation-after-io-location",
    "3 0 \\\n",
  );

  const functionCloserInitial = writeSource(
    "function-outer-closer-initial",
    lines("{ f()(:); }"),
  );
  const functionCloserFinal = writeSource(
    "function-outer-closer-final",
    lines("{ f()(:) }"),
  );
  assertIncrementalEqualsFresh(
    functionCloserInitial,
    functionCloserFinal,
    "delete-separator-before-function-outer-closer",
    "8 1",
  );

  const enclosingClosers = writeSource(
    "recovery-enclosing-closers",
    lines(
      "(if x; then y); next",
      "echo `if x; then y`; next",
      "echo `printf ${x`; next",
    ),
  );
  parseContainsAll(
    enclosingClosers,
    "enclosing closers",
    "(subshell [0, 0] - [0, 14]",
    "terminator: (separator_recovery [0, 13] - [0, 13])",
    "recovery: (compound_command_recovery [0, 13] - [0, 13])",
    "(and_or [0, 16] - [0, 20]",
    "(backquote_substitution [1, 5] - [1, 19]",
    "terminator: (separator_recovery [1, 18] - [1, 18])",
    "recovery: (compound_command_recovery [1, 18] - [1, 18])",
    "(and_or [1, 21] - [1, 25]",
    "(backquote_substitution [2, 5] - [2, 17]",
    "(parameter_expansion_recovery [2, 16] - [2, 16])",
    "(and_or [2, 19] - [2, 23]",
  );
  const enclosingCst = parseValidCst(enclosingClosers);
  for (const contract of [
    ["0:0-0:14", "subshell", "0:13-0:14", '")"'],
    ["1:5-1:19", "backquote_substitution", "1:18-1:19", "*"],
    ["2:5-2:17", "backquote_substitution", "2:16-2:17", "*"],
  ]) {
    assertCstDirectChildRange(enclosingCst, ...contract);
  }

  const missingCommand = writeSource(
    "recovery-missing-command-parenthesis",
    lines("(first && )", "after_missing_command"),
  );
  parseContainsAll(
    missingCommand,
    "missing command before parenthesis",
    "(subshell [0, 0] - [0, 11]",
    "recovery: (command_recovery [0, 10] - [0, 10])",
    "(complete_command [1, 0] - [1, 21]",
  );
  const missingCommandCst = parseValidCst(missingCommand);
  assertCstDirectChildRange(
    missingCommandCst,
    "0:0-0:11",
    "subshell",
    "0:10-0:11",
    '")"',
  );
  const continuedMissingCommand = writeSource(
    "recovery-missing-command-parenthesis-continuation",
    lines("(first && \\", ")", "after_missing_command"),
  );
  const continuedMissingCommandCst = parseValidCst(continuedMissingCommand);
  assertSameLogicalProjection(
    "missing command before a continued parenthesis",
    missingCommandCst,
    continuedMissingCommandCst,
  );
  assertCstRange(continuedMissingCommandCst, "0:10-1:0", "line_continuation");
  assertCstRange(continuedMissingCommandCst, "1:0-1:0", "command_recovery");
  assertCstDirectChildRange(
    continuedMissingCommandCst,
    "0:0-1:1",
    "subshell",
    "1:0-1:1",
    '")"',
  );
  assertIncrementalEqualsFresh(
    missingCommand,
    continuedMissingCommand,
    "insert-missing-command-boundary-continuation",
    "10 0 \\\n",
  );

  const outerSubshellInitial = writeSource(
    "recovery-enclosing-subshell-initial",
    lines("(if x; then y; fi); next"),
  );
  const outerSubshellFinal = writeSource(
    "recovery-enclosing-subshell-final",
    lines("(if x; then y); next"),
  );
  parseIncrementalAndFresh(
    outerSubshellInitial,
    outerSubshellFinal,
    "delete-inner-fi-before-subshell-closer",
    "13 4",
  );

  const outerBackquoteInitial = writeSource(
    "recovery-enclosing-backquote-initial",
    lines("echo `if x; then y; fi`; next"),
  );
  const outerBackquoteFinal = writeSource(
    "recovery-enclosing-backquote-final",
    lines("echo `if x; then y`; next"),
  );
  parseIncrementalAndFresh(
    outerBackquoteInitial,
    outerBackquoteFinal,
    "delete-inner-fi-before-backquote-closer",
    "18 4",
  );

  const casePatternRecovery = writeSource(
    "recovery-case-pattern",
    lines("case x in fi", "next"),
  );
  parseContainsAll(
    casePatternRecovery,
    "case pattern recovery",
    "(case_clause [0, 0] - [0, 12]",
    "patterns: (pattern_list [0, 10] - [0, 12]",
    "word: (word [0, 10] - [0, 12]",
    "recovery: (compound_command_recovery [0, 12] - [0, 12])",
    "(complete_command [1, 0] - [1, 4]",
  );
  const casePatternInitial = writeSource(
    "recovery-case-pattern-initial",
    lines("case x in fi) :;; esac", "next"),
  );
  parseIncrementalAndFresh(
    casePatternInitial,
    casePatternRecovery,
    "delete-case-pattern-tail",
    "12 10",
  );

  const invalidLocation = writeSource(
    "recovery-io-location",
    lines("printf {x'}>file"),
  );
  assertNotContains(parseRecovery(invalidLocation), "(io_location ");

  const delimiters = writeSource(
    "source-delimiters",
    lines("printf x$x", "echo `inner`", "echo `printf \\`nested\\` \\$name`"),
  );
  const delimiterOutput = parseValidCst(delimiters);
  const backquoteToken = '"\\`"';
  const backslashToken = '"\\\\"';
  for (const [range, item] of [
    ["0:8-0:9", '"$"'],
    ["1:5-1:6", backquoteToken],
    ["1:11-1:12", backquoteToken],
    ["2:5-2:6", backquoteToken],
    ["2:13-2:14", backslashToken],
    ["2:14-2:15", backquoteToken],
    ["2:21-2:22", backslashToken],
    ["2:22-2:23", backquoteToken],
    ["2:24-2:25", backslashToken],
    ["2:25-2:26", '"$"'],
    ["2:30-2:31", backquoteToken],
  ]) {
    assertCstRange(delimiterOutput, range, item);
  }

  const parameterDelimiterInitial = writeSource(
    "parameter-delimiter-initial",
    lines("printf x%"),
  );
  const parameterDelimiterFinal = writeSource(
    "parameter-delimiter-final",
    lines("printf x$x"),
  );
  assertIncrementalEqualsFresh(
    parameterDelimiterInitial,
    parameterDelimiterFinal,
    "parameter-delimiter",
    "8 1 $x",
  );

  const backquoteDelimiterInitial = writeSource(
    "backquote-delimiter-initial",
    lines("echo [inner]"),
  );
  const backquoteDelimiterFinal = writeSource(
    "backquote-delimiter-final",
    lines("echo `inner`"),
  );
  assertIncrementalEqualsFresh(
    backquoteDelimiterInitial,
    backquoteDelimiterFinal,
    "backquote-delimiter",
    "5 1 `",
    "11 1 `",
  );
});

test("here-document state, delimiters, and bodies remain deterministic", () => {
  const boundary = writeSource(
    "here-document-boundary",
    lines("cat <<EOF", "$(", "EOF", ")", "EOF", "after"),
  );
  const boundaryOutput = parseRecovery(boundary);
  assertContains(
    boundaryOutput,
    "(here_document_end_recovery [1, 2] - [2, 0])",
  );
  assertContains(boundaryOutput, "end: (here_document_end [2, 0] - [3, 0])");
  assertContains(boundaryOutput, "(complete_command [5, 0] - [5, 5]");

  const backquoteDocument = writeSource(
    "backquote-here-document",
    lines("cat <<\"`printf '%s' END`\"", "body", "`printf %s END`", "after"),
  );
  const backquoteDocumentOutput = parseValidCst(backquoteDocument);
  assertCstRange(backquoteDocumentOutput, "2:0-3:0", "end: here_document_end");
  assertCstRange(
    backquoteDocumentOutput,
    "3:0-3:5",
    "command: complete_command",
  );
  assertNotContains(backquoteDocumentOutput, "here_document_end_recovery");

  const backquoteHashInitial = writeSource(
    "backquote-hash-initial",
    lines(
      'cat <<"`printf $' + '{x}X#tag END`"',
      "body",
      "`printf $" + "{x}X#tag END`",
      "after",
    ),
  );
  const backquoteHashFinal = writeSource(
    "backquote-hash-final",
    lines(
      'cat <<"`printf $' + '{x}#tag END`"',
      "body",
      "`printf $" + "{x}#tag END`",
      "after",
    ),
  );
  const backquoteHashOutput = parseValidCst(backquoteHashFinal);
  assertCstRange(backquoteHashOutput, "0:6-0:29", "end: here_end");
  assertCstRange(backquoteHashOutput, "2:0-3:0", "end: here_document_end");
  assertCstRange(backquoteHashOutput, "3:0-3:5", "command: complete_command");
  assertNotContains(backquoteHashOutput, "ERROR");
  assertNotContains(backquoteHashOutput, "here_document_end_recovery");
  assertIncrementalEqualsFresh(
    backquoteHashInitial,
    backquoteHashFinal,
    "delete-backquote-delimiter-word-markers",
    "48 1",
    "19 1",
  );
  assertIncrementalEqualsFresh(
    backquoteHashFinal,
    backquoteHashInitial,
    "insert-backquote-delimiter-word-markers",
    "47 0 X",
    "19 0 X",
  );

  const slash = "\\";
  const byteDocument = writeSource(
    "byte-here-document",
    Buffer.concat([
      Buffer.from(`cat <<$'${slash}c?'\nbody\n`),
      Buffer.from([0x7f]),
      Buffer.from(`\ncat <<$'${slash}xC3${slash}xBF'\nbody\n`),
      Buffer.from([0xc3, 0xbf]),
      Buffer.from("\nafter\n"),
    ]),
  );
  const byteDocumentOutput = parseValidCst(byteDocument);
  assertCstRange(byteDocumentOutput, "2:0-3:0", "end: here_document_end");
  assertCstRange(byteDocumentOutput, "5:0-6:0", "end: here_document_end");
  assertCstRange(byteDocumentOutput, "6:0-6:5", "command: complete_command");
  assertNotContains(byteDocumentOutput, "here_document_end_recovery");

  const nulDocument = writeSource(
    "nul-here-document",
    Buffer.from("cat <<EOF #a\0b\nbody\nEOF\nafter\n"),
  );
  const nulDocumentOutput = runParse({
    description: "NUL inside here-document declaration comment",
    source: nulDocument,
  }).output;
  assertCstRange(nulDocumentOutput, "0:10-0:14", "comment: comment");
  assertCstRange(nulDocumentOutput, "2:0-3:0", "end: here_document_end");
  assertCstRange(nulDocumentOutput, "3:0-3:5", "command: complete_command");
  assertCstRange(nulDocumentOutput, "0:0-4:0", "program");

  const byteMismatch = writeSource(
    "byte-here-document-mismatch",
    Buffer.concat([
      Buffer.from(`cat <<$'${slash}xFF'\nbody\n`),
      Buffer.from([0x07]),
      Buffer.from("\nafter\n"),
    ]),
  );
  assertCstRange(
    parseValidCst(byteMismatch),
    "4:0-4:0",
    "end: here_document_end_recovery",
  );

  const delimiterBoundaryInitial = writeSource(
    "delimiter-boundary-initial",
    lines("cat <<EOF", "body", "EOF", "after"),
  );
  const delimiterBoundaryFinal = writeSource(
    "delimiter-boundary-final",
    lines("cat <<\\", "EOF", "body", "EOF", "after"),
  );
  const delimiterBoundaryOutput = parseValidCst(delimiterBoundaryFinal);
  for (const [range, item] of [
    ["0:4-0:6", "operator: dless"],
    ["0:6-1:0", "line_continuation"],
    ["1:0-1:3", "end: here_end"],
    ["1:0-1:3", "word: word"],
    ["3:0-4:0", "end: here_document_end"],
    ["4:0-4:5", "command: complete_command"],
  ]) {
    assertCstRange(delimiterBoundaryOutput, range, item);
  }
  assertNotContains(delimiterBoundaryOutput, "ERROR");
  assertIncrementalEqualsFresh(
    delimiterBoundaryInitial,
    delimiterBoundaryFinal,
    "insert-here-document-delimiter-boundary-continuation",
    "6 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    delimiterBoundaryFinal,
    delimiterBoundaryInitial,
    "delete-here-document-delimiter-boundary-continuation",
    "6 2",
  );

  const missingDelimiterGreat = writeSource(
    "missing-delimiter-great",
    lines("command << >out"),
  );
  const missingDelimiterLess = writeSource(
    "missing-delimiter-less",
    lines("command << <in"),
  );
  for (const output of parseIncrementalAndFresh(
    missingDelimiterGreat,
    missingDelimiterLess,
    "replace-redirection-after-missing-here-document-delimiter",
    "11 4 <in",
  )) {
    assertCstRange(output, "0:11-0:11", "recovery: missing_here_end");
    assertCstRange(output, "0:11-0:12", '"<"');
  }

  const continuedTabInitial = writeSource(
    "continued-tab-initial",
    "cat <<-AB\nA\\\nB\nAB\nafter\n",
  );
  const continuedTabFinal = writeSource(
    "continued-tab-final",
    "cat <<-AB\nA\\\n\tB\nAB\nafter\n",
  );
  const continuedTabOutput = parseValidCst(continuedTabFinal);
  assertCstRange(continuedTabOutput, "1:1-2:0", "line_continuation");
  assertCstRange(continuedTabOutput, "3:0-4:0", "end: here_document_end");
  assertIncrementalEqualsFresh(
    continuedTabInitial,
    continuedTabFinal,
    "insert-tab-after-here-document-continuation",
    "13 0 \t",
  );

  const continuedDollarInitial = writeSource(
    "continued-dollar-body-initial",
    lines("cat <<EOF", "$'text'", "EOF"),
  );
  const continuedDollarFinal = writeSource(
    "continued-dollar-body-final",
    lines("cat <<EOF", "$\\", "'text'", "EOF"),
  );
  const continuedDollarOutput = parseValidCst(continuedDollarFinal);
  assertCstRange(continuedDollarOutput, "1:1-2:0", "line_continuation");
  assertNotContains(continuedDollarOutput, "dollar_single_quoted");
  assertIncrementalEqualsFresh(
    continuedDollarInitial,
    continuedDollarFinal,
    "insert-continuation-in-here-document-body",
    "11 0 \\\n",
  );

  const nestedInitial = writeSource(
    "nested-initial",
    lines(
      "cat <<OUTER",
      "before",
      "$(cat <<INNER",
      "inside",
      "INNER",
      ")",
      "after",
      "OUTER",
    ),
  );
  const nestedFinal = writeSource(
    "nested-final",
    lines(
      "cat <<OUTER",
      "before",
      "$(cat <<INNER",
      "within-value",
      "INNER",
      ")",
      "after",
      "OUTER",
    ),
  );
  assertIncrementalEqualsFresh(
    nestedInitial,
    nestedFinal,
    "nested-here-document",
    "33 6 within-value",
  );

  const quotedInitial = writeSource(
    "quoted-initial",
    lines("cat <<EOF", "$value", "EOF"),
  );
  const quotedFinal = writeSource(
    "quoted-final",
    lines("cat <<'EOF'", "$value", "EOF"),
  );
  assertIncrementalEqualsFresh(
    quotedInitial,
    quotedFinal,
    "quoted-here-document",
    "6 3 'EOF'",
  );

  const longInitialDelimiter = "A".repeat(2_048);
  const longFinalDelimiter = "B".repeat(2_048);
  const longInitial = writeSource(
    "long-initial",
    `cat <<${longInitialDelimiter}\nbody\n${longInitialDelimiter}\n`,
  );
  const longFinal = writeSource(
    "long-final",
    `cat <<${longFinalDelimiter}\nbody\n${longFinalDelimiter}\nafter`,
  );
  for (const output of parseIncrementalAndFresh(
    longInitial,
    longFinal,
    "oversized-delimiter-recovery",
    `6 2048 ${longFinalDelimiter}`,
    `2060 2048 ${longFinalDelimiter}`,
    "4109 0 after",
  )) {
    assertCstRange(output, "3:0-3:5", "complete_command");
  }
});

test("word, parameter, and arithmetic categories survive edits", () => {
  const descriptorInitial = writeSource(
    "descriptor-initial",
    lines("<input 2x>output"),
  );
  const descriptorFinal = writeSource(
    "descriptor-final",
    lines("<input 2>output"),
  );
  assertIncrementalEqualsFresh(
    descriptorInitial,
    descriptorFinal,
    "descriptor-after-word-edit",
    "8 1",
  );

  const parameterTextInitial = writeSource(
    "parameter-text-initial",
    lines("printf x$%"),
  );
  const parameterTextFinal = writeSource(
    "parameter-text-final",
    lines("printf x$x"),
  );
  assertIncrementalEqualsFresh(
    parameterTextInitial,
    parameterTextFinal,
    "parameter-text-to-expansion",
    "9 1 x",
  );
  assertIncrementalEqualsFresh(
    parameterTextFinal,
    parameterTextInitial,
    "parameter-expansion-to-text",
    "9 1 %",
  );

  const parameterUnquoted = writeSource(
    "parameter-context-unquoted",
    lines("printf $" + "{x:-*.js}"),
  );
  const parameterQuoted = writeSource(
    "parameter-context-quoted",
    lines('printf "$' + '{x:-*.js}"'),
  );
  const parameterPattern = writeSource(
    "parameter-context-pattern",
    lines('printf "$' + '{x##*.js}"'),
  );
  assertIncrementalEqualsFresh(
    parameterUnquoted,
    parameterQuoted,
    "insert-parameter-outer-quotes",
    '7 0 "',
    '18 0 "',
  );
  assertIncrementalEqualsFresh(
    parameterQuoted,
    parameterUnquoted,
    "delete-parameter-outer-quotes",
    "7 1",
    "17 1",
  );
  assertIncrementalEqualsFresh(
    parameterQuoted,
    parameterPattern,
    "parameter-value-to-pattern-context",
    "11 2 ##",
  );
  assertIncrementalEqualsFresh(
    parameterPattern,
    parameterQuoted,
    "parameter-pattern-to-value-context",
    "11 2 :-",
  );

  const numericInitial = writeSource(
    "numeric-category-initial",
    lines('printf "$' + '{00}"'),
  );
  const numericFinal = writeSource(
    "numeric-category-final",
    lines('printf "$' + '{01}"'),
  );
  assertIncrementalEqualsFresh(
    numericInitial,
    numericFinal,
    "numeric-source-to-positional-parameter",
    "11 1 1",
  );
  assertIncrementalEqualsFresh(
    numericFinal,
    numericInitial,
    "positional-parameter-to-numeric-source",
    "11 1 0",
  );

  const arithmeticCategoryInitial = writeSource(
    "arithmetic-category-initial",
    lines(': "$((a | b && c))"'),
  );
  const arithmeticCategoryFinal = writeSource(
    "arithmetic-category-final",
    lines(': "$((a || b && c))"'),
  );
  assertIncrementalEqualsFresh(
    arithmeticCategoryInitial,
    arithmeticCategoryFinal,
    "insert-arithmetic-operator-category-character",
    "9 0 |",
  );
  assertIncrementalEqualsFresh(
    arithmeticCategoryFinal,
    arithmeticCategoryInitial,
    "delete-arithmetic-operator-category-character",
    "9 1",
  );

  const operandBoundaryInitial = writeSource(
    "arithmetic-operand-operator-boundary-initial",
    lines(': "$((a==b))"'),
  );
  const operandBoundaryFinal = writeSource(
    "arithmetic-operand-operator-boundary-final",
    lines(': "$((a\\', '==b))"'),
  );
  const operandBoundaryOutput = parseValidCst(operandBoundaryFinal);
  assertCstRange(operandBoundaryOutput, "0:3-1:5", "arithmetic_expansion");
  assertCstRange(operandBoundaryOutput, "0:7-1:0", "line_continuation");
  assertCstRange(
    operandBoundaryOutput,
    "1:0-1:2",
    "operator: arithmetic_operator",
  );
  assertNotContains(operandBoundaryOutput, "command_substitution");
  assertNotContains(operandBoundaryOutput, "ERROR");
  assertIncrementalEqualsFresh(
    operandBoundaryInitial,
    operandBoundaryFinal,
    "insert-arithmetic-operand-operator-boundary-continuation",
    "7 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    operandBoundaryFinal,
    operandBoundaryInitial,
    "delete-arithmetic-operand-operator-boundary-continuation",
    "7 2",
  );

  const missingOperand = writeSource(
    "arithmetic-missing-operand",
    lines(': "$((1 +\\', '))"'),
  );
  const missingOperandOutput = runParse({
    description: "arithmetic missing operand after continuation",
    source: missingOperand,
  }).output;
  assertContains(missingOperandOutput, "command_substitution");
  assertContains(missingOperandOutput, "subshell");
  assertNotContains(missingOperandOutput, "arithmetic_expansion");
  assertCstRange(missingOperandOutput, "0:9-1:0", "line_continuation");
  assertOccurrenceCount(missingOperandOutput, "line_continuation", 1);

  function assertRepeatedSignFallback(name, logicalText, physicalLines) {
    const logical = writeSource(`${name}-logical`, lines(logicalText));
    const physical = writeSource(`${name}-physical`, lines(...physicalLines));
    const logicalOutput = runParse({
      description: `${name} falls back to command substitution`,
      source: logical,
    }).output;
    assertContains(logicalOutput, "command_substitution");
    assertContains(logicalOutput, "subshell");
    assertNotContains(logicalOutput, "arithmetic_unary_expression");
    const physicalOutput = runParse({
      description: `split ${name} spelling`,
      mode: "recovery",
      source: physical,
    }).output;
    assertNotContains(physicalOutput, "arithmetic_unary_expression");
  }

  assertRepeatedSignFallback("arithmetic-prefix-increment", ': "$((++a))"', [
    ': "$((+\\',
    '+a))"',
  ]);
  assertRepeatedSignFallback("arithmetic-prefix-decrement", ': "$((--a))"', [
    ': "$((-\\',
    '-a))"',
  ]);
  assertRepeatedSignFallback("arithmetic-infix-increment", ': "$((a++b))"', [
    ': "$((a+\\',
    '+b))"',
  ]);
  assertRepeatedSignFallback("arithmetic-infix-decrement", ': "$((a--b))"', [
    ': "$((a-\\',
    '-b))"',
  ]);
});

test("arithmetic grouping, lvalues, and unary operators remain stable", () => {
  const backquoteBroken = writeSource(
    "arithmetic-backquote-broken",
    lines(": $((+x `+ y))"),
  );
  const backquoteRestored = writeSource(
    "arithmetic-backquote-restored",
    lines(": $((+x + y))"),
  );
  assertIncrementalEqualsFresh(
    backquoteBroken,
    backquoteRestored,
    "structured-reading-survives-backquote-undo",
    "8 1",
  );

  const parenthesizedInitial = writeSource(
    "arithmetic-parenthesized-initial",
    lines(': "$((a + b))"'),
  );
  const parenthesizedFinal = writeSource(
    "arithmetic-parenthesized-final",
    lines(': "$(((a + b)))"'),
  );
  assertIncrementalEqualsFresh(
    parenthesizedInitial,
    parenthesizedFinal,
    "parenthesize-arithmetic-expression",
    "6 5 (a + b)",
  );
  assertIncrementalEqualsFresh(
    parenthesizedFinal,
    parenthesizedInitial,
    "unparenthesize-arithmetic-expression",
    "6 7 a + b",
  );

  const lvalueInitial = writeSource(
    "arithmetic-lvalue-initial",
    lines(': "$((name = 1))"'),
  );
  const lvalueParenthesized = writeSource(
    "arithmetic-lvalue-parenthesized",
    lines(': "$(((name) = 1))"'),
  );
  const [, lvalueOutput] = assertIncrementalEqualsFresh(
    lvalueInitial,
    lvalueParenthesized,
    "parenthesize-arithmetic-assignment-lvalue",
    "6 0 (",
    "11 0 )",
  );
  assertIncrementalEqualsFresh(
    lvalueParenthesized,
    lvalueInitial,
    "unparenthesize-arithmetic-assignment-lvalue",
    "6 1",
    "10 1",
  );
  for (const [range, item] of [
    ["0:3-0:18", "arithmetic_expansion"],
    ["0:6-0:16", "expression: arithmetic_assignment_expression"],
    ["0:6-0:12", "left: parenthesized_arithmetic"],
    ["0:7-0:11", "expression: arithmetic_variable"],
    ["0:13-0:14", "operator: arithmetic_operator"],
    ["0:15-0:16", "right: arithmetic_number"],
  ]) {
    assertCstRange(lvalueOutput, range, item);
  }

  const nonLvalueInitial = writeSource(
    "arithmetic-non-lvalue-initial",
    lines(': "$(((name) = 2))"'),
  );
  const nonLvalueFinal = writeSource(
    "arithmetic-non-lvalue-final",
    lines(': "$(((name + 1) = 2))"'),
  );
  const nonLvalueOutputs = parseIncrementalAndFresh(
    nonLvalueInitial,
    nonLvalueFinal,
    "make-parenthesized-arithmetic-non-lvalue",
    "11 0  + 1",
  );
  assertIncrementalEqualsFresh(
    nonLvalueFinal,
    nonLvalueInitial,
    "restore-parenthesized-arithmetic-lvalue",
    "11 4",
  );
  for (const output of nonLvalueOutputs) {
    assertCstRange(output, "0:3-0:22", "command_substitution");
    assertCstRange(output, "0:5-0:21", "subshell");
    assertNotContains(output, "arithmetic_expansion");
    assertNotContains(output, "arithmetic_assignment_expression");
  }

  const openingLayoutInitial = writeSource(
    "arithmetic-opening-layout-initial",
    lines(': "$((\\', 'a+1))"'),
  );
  const openingLayoutFinal = writeSource(
    "arithmetic-opening-layout-final",
    lines(': "$(( \\', 'a+1))"'),
  );
  assertIncrementalEqualsFresh(
    openingLayoutInitial,
    openingLayoutFinal,
    "insert-arithmetic-opening-layout",
    "6 0  ",
  );
  assertIncrementalEqualsFresh(
    openingLayoutFinal,
    openingLayoutInitial,
    "delete-arithmetic-opening-layout",
    "6 1",
  );

  const negationInitial = writeSource(
    "arithmetic-negation-initial",
    lines(': "$((!a))"'),
  );
  const negationFinal = writeSource(
    "arithmetic-negation-final",
    lines(': "$((! a))"'),
  );
  assertIncrementalEqualsFresh(
    negationInitial,
    negationFinal,
    "insert-arithmetic-negation-layout",
    "7 0  ",
  );
  assertIncrementalEqualsFresh(
    negationFinal,
    negationInitial,
    "delete-arithmetic-negation-layout",
    "7 1",
  );

  const unaryBangInitial = writeSource(
    "arithmetic-unary-bang-initial",
    lines("x=$((!-a))"),
  );
  const unaryBangFinal = writeSource(
    "arithmetic-unary-bang-final",
    lines("x=$((!\\", "-a))"),
  );
  const unaryBangOutput = parseValidCst(unaryBangFinal);
  assertContains(unaryBangOutput, "assignment: assignment_word");
  assertCstRange(
    unaryBangOutput,
    "0:5-1:2",
    "expression: arithmetic_unary_expression",
  );
  assertCstRange(unaryBangOutput, "0:6-1:0", "line_continuation");
  assertCstRange(
    unaryBangOutput,
    "1:0-1:2",
    "operand: arithmetic_unary_expression",
  );
  assertNotContains(unaryBangOutput, "command_substitution");
  assertNotContains(unaryBangOutput, "ERROR");
  assertIncrementalEqualsFresh(
    unaryBangInitial,
    unaryBangFinal,
    "insert-arithmetic-unary-bang-continuation",
    "6 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    unaryBangFinal,
    unaryBangInitial,
    "delete-arithmetic-unary-bang-continuation",
    "6 2",
  );

  const unaryPlusInitial = writeSource(
    "arithmetic-unary-plus-initial",
    lines(": $((+-a))"),
  );
  const unaryPlusFinal = writeSource(
    "arithmetic-unary-plus-final",
    lines(": $((+\\", "\\", "-a))"),
  );
  const unaryPlusOutput = parseValidCst(unaryPlusFinal);
  assertCstRange(
    unaryPlusOutput,
    "0:5-2:2",
    "expression: arithmetic_unary_expression",
  );
  assertCstRange(unaryPlusOutput, "0:6-1:0", "line_continuation");
  assertCstRange(unaryPlusOutput, "1:0-2:0", "line_continuation");
  assertCstRange(
    unaryPlusOutput,
    "2:0-2:2",
    "operand: arithmetic_unary_expression",
  );
  assertNotContains(unaryPlusOutput, "command_substitution");
  assertNotContains(unaryPlusOutput, "ERROR");
  assertIncrementalEqualsFresh(
    unaryPlusInitial,
    unaryPlusFinal,
    "insert-arithmetic-unary-plus-continuations",
    "6 0 \\\n\\\n",
  );
  assertIncrementalEqualsFresh(
    unaryPlusFinal,
    unaryPlusInitial,
    "delete-arithmetic-unary-plus-continuations",
    "6 4",
  );

  const unaryMinusInitial = writeSource(
    "arithmetic-unary-minus-initial",
    lines(": $((-+a))"),
  );
  const unaryMinusFinal = writeSource(
    "arithmetic-unary-minus-final",
    lines(": $((-\\", "+a))"),
  );
  assertIncrementalEqualsFresh(
    unaryMinusInitial,
    unaryMinusFinal,
    "insert-arithmetic-unary-minus-continuation",
    "6 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    unaryMinusFinal,
    unaryMinusInitial,
    "delete-arithmetic-unary-minus-continuation",
    "6 2",
  );

  const separatedSignInitial = writeSource(
    "arithmetic-separated-sign-initial",
    lines(': "$((+ +a))"'),
  );
  const separatedSignFinal = writeSource(
    "arithmetic-separated-sign-final",
    lines(': "$((+\\', ' +a))"'),
  );
  assertIncrementalEqualsFresh(
    separatedSignInitial,
    separatedSignFinal,
    "insert-arithmetic-separated-sign-continuation",
    "7 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    separatedSignFinal,
    separatedSignInitial,
    "delete-arithmetic-separated-sign-continuation",
    "7 2",
  );

  const backquoteInitial = writeSource(
    "backquote-continuation-initial",
    lines("echo `printf body`"),
  );
  const backquoteFinal = writeSource(
    "backquote-continuation-final",
    lines("echo `\\", "printf body`"),
  );
  const backquoteOutput = parseValidCst(backquoteFinal);
  const backquoteToken = '"\\`"';
  assertCstRange(backquoteOutput, "0:5-0:6", backquoteToken);
  assertCstRange(backquoteOutput, "0:6-1:0", "line_continuation");
  assertCstRange(backquoteOutput, "1:11-1:12", backquoteToken);
  assertNotContains(backquoteOutput, "ERROR");
  assertIncrementalEqualsFresh(
    backquoteInitial,
    backquoteFinal,
    "insert-backquote-body-continuation",
    "6 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    backquoteFinal,
    backquoteInitial,
    "delete-backquote-body-continuation",
    "6 2",
  );
});

test("arithmetic raw newlines stay layout in every reading", () => {
  const structuredNewline = writeSource(
    "arithmetic-structured-newline",
    lines(': "$((1 +', '2))" "$((1', '))"'),
  );
  const structuredOutput = parseValidTree(structuredNewline);
  assertContains(
    structuredOutput,
    "expression: (arithmetic_binary_expression [0, 6] - [1, 1]",
  );
  assertContains(
    structuredOutput,
    "expression: (arithmetic_number [1, 9] - [1, 10])",
  );

  const dynamicSpace = writeSource(
    "arithmetic-dynamic-space",
    lines(': "$(($x y))"'),
  );
  const dynamicNewline = writeSource(
    "arithmetic-dynamic-newline",
    lines(': "$(($x', 'y))"'),
  );
  const [, dynamicOutput] = assertIncrementalEqualsFresh(
    dynamicSpace,
    dynamicNewline,
    "arithmetic-dynamic-space-to-newline",
    "8 1 \n",
  );
  assertCstRange(dynamicOutput, "0:6-1:1", "arithmetic_dynamic_expression");

  const incompleteAtEof = writeSource(
    "arithmetic-incomplete-newline-eof",
    lines("echo $((1 +"),
  );
  const incompleteOutput = parseValidTree(incompleteAtEof);
  assertContains(incompleteOutput, "(arithmetic_expansion [0, 5] - [1, 0]");
  assertContains(incompleteOutput, "(arithmetic_operator [0, 10] - [0, 11])");

  const incompleteInBackquote = writeSource(
    "arithmetic-incomplete-newline-backquote",
    lines("echo `echo $((1 +", "`"),
  );
  const backquoteArithmeticOutput = parseValidTree(incompleteInBackquote);
  assertContains(
    backquoteArithmeticOutput,
    "(backquote_substitution [0, 5] - [1, 1]",
  );
  assertContains(
    backquoteArithmeticOutput,
    "(arithmetic_expansion [0, 11] - [1, 0]",
  );
});

test("tilde, assignment, and compound-tail classifications remain stable", () => {
  const tildePercentInitial = writeSource(
    "tilde-percent-initial",
    lines(": ~alice/x"),
  );
  const tildePercentFinal = writeSource(
    "tilde-percent-final",
    lines(": ~alice%/x"),
  );
  const [, tildePercentOutput] = assertIncrementalEqualsFresh(
    tildePercentInitial,
    tildePercentFinal,
    "insert-percent-in-literal-tilde-user",
    "8 0 %",
  );
  assertIncrementalEqualsFresh(
    tildePercentFinal,
    tildePercentInitial,
    "delete-percent-from-literal-tilde-user",
    "8 1",
  );
  for (const [range, item] of [
    ["0:2-0:9", "tilde_expansion"],
    ["0:3-0:9", "user: tilde_user"],
    ["0:3-0:9", "literal"],
    ["0:9-0:10", '"/"'],
  ]) {
    assertCstRange(tildePercentOutput, range, item);
  }

  const assignmentPercentInitial = writeSource(
    "tilde-assignment-percent-initial",
    lines("A=~alice:x :"),
  );
  const assignmentPercentFinal = writeSource(
    "tilde-assignment-percent-final",
    lines("A=~alice%:x :"),
  );
  const [, assignmentPercentOutput] = assertIncrementalEqualsFresh(
    assignmentPercentInitial,
    assignmentPercentFinal,
    "insert-percent-before-assignment-tilde-colon",
    "8 0 %",
  );
  assertIncrementalEqualsFresh(
    assignmentPercentFinal,
    assignmentPercentInitial,
    "delete-percent-before-assignment-tilde-colon",
    "8 1",
  );
  for (const [range, item] of [
    ["0:2-0:11", "value: assignment_value"],
    ["0:2-0:9", "tilde_expansion"],
    ["0:3-0:9", "user: tilde_user"],
    ["0:9-0:10", '":"'],
  ]) {
    assertCstRange(assignmentPercentOutput, range, item);
  }

  const parameterPercentInitial = writeSource(
    "tilde-parameter-percent-initial",
    lines(": $" + "{v:-~alice/x}"),
  );
  const parameterPercentFinal = writeSource(
    "tilde-parameter-percent-final",
    lines(": $" + "{v:-~alice%/x}"),
  );
  const [, parameterPercentOutput] = assertIncrementalEqualsFresh(
    parameterPercentInitial,
    parameterPercentFinal,
    "insert-percent-in-parameter-word-tilde-user",
    "13 0 %",
  );
  assertIncrementalEqualsFresh(
    parameterPercentFinal,
    parameterPercentInitial,
    "delete-percent-from-parameter-word-tilde-user",
    "13 1",
  );
  for (const [range, item] of [
    ["0:2-0:17", "parameter_expansion"],
    ["0:7-0:16", "word: parameter_word"],
    ["0:7-0:14", "tilde_expansion"],
    ["0:8-0:14", "user: tilde_user"],
    ["0:14-0:15", '"/"'],
    ["0:16-0:17", '"}"'],
  ]) {
    assertCstRange(parameterPercentOutput, range, item);
  }

  const nestedUserInitial = writeSource(
    "tilde-nested-user-initial",
    lines(': ~"$(echo ab)"/x'),
  );
  const nestedUserFinal = writeSource(
    "tilde-nested-user-final",
    lines(': ~"$(echo a/b)"/x'),
  );
  const [, nestedUserOutput] = assertIncrementalEqualsFresh(
    nestedUserInitial,
    nestedUserFinal,
    "insert-slash-in-nested-tilde-user-substitution",
    "12 0 /",
  );
  assertIncrementalEqualsFresh(
    nestedUserFinal,
    nestedUserInitial,
    "delete-slash-from-nested-tilde-user-substitution",
    "12 1",
  );
  for (const [range, item] of [
    ["0:2-0:16", "tilde_expansion"],
    ["0:3-0:16", "user: tilde_user"],
    ["0:3-0:16", "double_quoted"],
    ["0:4-0:15", "command_substitution"],
    ["0:12-0:13", '"/"'],
    ["0:16-0:17", '"/"'],
  ]) {
    assertCstRange(nestedUserOutput, range, item);
  }

  const assignmentBoundaryInitial = writeSource(
    "assignment-boundary-initial",
    lines("name=value command"),
  );
  const assignmentBoundaryFinal = writeSource(
    "assignment-boundary-final",
    lines("name=value\\", " command"),
  );
  const assignmentBoundaryOutput = parseValidCst(assignmentBoundaryFinal);
  assertOccurrenceCount(
    assignmentBoundaryOutput,
    "assignment: assignment_word",
    1,
  );
  assertCstRange(
    assignmentBoundaryOutput,
    "0:5-0:10",
    "value: assignment_value",
  );
  assertCstRange(assignmentBoundaryOutput, "0:10-1:0", "line_continuation");
  assertNotContains(assignmentBoundaryOutput, "ERROR");
  assertIncrementalEqualsFresh(
    assignmentBoundaryInitial,
    assignmentBoundaryFinal,
    "insert-assignment-boundary-continuation",
    "10 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    assignmentBoundaryFinal,
    assignmentBoundaryInitial,
    "delete-assignment-boundary-continuation",
    "10 2",
  );

  const assignmentNewlineInitial = writeSource(
    "assignment-newline-initial",
    lines("x=a"),
  );
  const assignmentNewlineFinal = writeSource(
    "assignment-newline-final",
    lines("x=a\\", ""),
  );
  const assignmentNewlineOutput = parseValidCst(assignmentNewlineFinal);
  assertOccurrenceCount(
    assignmentNewlineOutput,
    "assignment: assignment_word",
    1,
  );
  assertCstRange(assignmentNewlineOutput, "0:2-0:3", "value: assignment_value");
  assertCstRange(assignmentNewlineOutput, "0:3-1:0", "line_continuation");
  assertCstRange(assignmentNewlineOutput, "1:0-2:0", "trailing: linebreak");
  assertNotContains(assignmentNewlineOutput, "name: cmd_name");
  assertNotContains(assignmentNewlineOutput, "ERROR");
  assertNotContains(assignmentNewlineOutput, "MISSING");
  assertIncrementalEqualsFresh(
    assignmentNewlineInitial,
    assignmentNewlineFinal,
    "insert-assignment-newline-continuation",
    "3 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    assignmentNewlineFinal,
    assignmentNewlineInitial,
    "delete-assignment-newline-continuation",
    "3 2",
  );

  const compoundTailInitial = writeSource(
    "compound-tail-initial",
    lines("(:)&", "child_pid=$!"),
  );
  const compoundTailFinal = writeSource(
    "compound-tail-final",
    lines("(:) &", "child_pid=$!"),
  );
  for (const output of assertIncrementalEqualsFresh(
    compoundTailInitial,
    compoundTailFinal,
    "insert-layout-before-asynchronous-separator",
    "3 0  ",
  )) {
    assertCstRange(output, "0:0-0:3", "subshell");
    assertCstRange(output, "0:4-0:5", "separator_op");
    assertCstRange(output, "1:0-1:12", "command: complete_command");
    assertNotContains(output, "ERROR");
    assertNotContains(output, "command_recovery");
  }
});

test("command separators and continuation boundaries remain stable", () => {
  const operatorBoundaries = writeSource(
    "operator-boundaries",
    lines(
      "echo word\\",
      "  |next",
      "echo word\\",
      "  ||next",
      "echo word\\",
      "  &&next",
      "case x in pattern\\",
      "  |other) : ;; esac",
    ),
  );
  const operatorOutput = parseValidCst(operatorBoundaries);
  for (const [range, item] of [
    ["0:9-1:0", "line_continuation"],
    ["2:9-3:0", "line_continuation"],
    ["3:2-3:4", "operator: or_if"],
    ["4:9-5:0", "line_continuation"],
    ["5:2-5:4", "operator: and_if"],
    ["6:17-7:0", "line_continuation"],
  ]) {
    assertCstRange(operatorOutput, range, item);
  }
  assertNotContains(operatorOutput, "ERROR");

  const repeatedPipeInitial = writeSource(
    "repeated-pipe-boundary-initial",
    lines("first|next"),
  );
  const repeatedPipeFinal = writeSource(
    "repeated-pipe-boundary-final",
    lines("first\\", "\\", "  |next"),
  );
  const repeatedPipeOutput = parseValidCst(repeatedPipeFinal);
  assertCstRange(repeatedPipeOutput, "0:5-1:0", "line_continuation");
  assertCstRange(repeatedPipeOutput, "1:0-2:0", "line_continuation");
  assertNotContains(repeatedPipeOutput, "ERROR");
  assertIncrementalEqualsFresh(
    repeatedPipeInitial,
    repeatedPipeFinal,
    "insert-repeated-pipe-boundary-continuations",
    "5 0 \\\n\\\n  ",
  );
  assertIncrementalEqualsFresh(
    repeatedPipeFinal,
    repeatedPipeInitial,
    "delete-repeated-pipe-boundary-continuations",
    "5 6",
  );

  const closedAndOrContinuationInitial = writeSource(
    "closed-and-or-continuation-initial",
    lines("{ a && b; }"),
  );
  const closedAndOrContinuationFinal = writeSource(
    "closed-and-or-continuation-final",
    lines("{ a && \\", "b; }"),
  );
  const closedAndOrLogicalOutput = parseValidCst(
    closedAndOrContinuationInitial,
  );
  const closedAndOrPhysicalOutput = parseValidCst(closedAndOrContinuationFinal);
  assertSameLogicalProjection(
    "closed AND-OR continuation",
    closedAndOrLogicalOutput,
    closedAndOrPhysicalOutput,
  );
  assertCstRange(closedAndOrPhysicalOutput, "0:0-1:4", "brace_group");
  assertCstRange(closedAndOrPhysicalOutput, "0:4-0:6", "operator: and_if");
  assertCstRange(closedAndOrPhysicalOutput, "0:7-1:0", "line_continuation");
  assertOccurrenceCount(closedAndOrPhysicalOutput, "line_continuation", 1);
  for (const recovery of ["ERROR", "MISSING", "_recovery"]) {
    assertNotContains(closedAndOrPhysicalOutput, recovery);
  }
  assertIncrementalEqualsFresh(
    closedAndOrContinuationInitial,
    closedAndOrContinuationFinal,
    "insert-closed-and-or-continuation",
    "7 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    closedAndOrContinuationFinal,
    closedAndOrContinuationInitial,
    "delete-closed-and-or-continuation",
    "7 2",
  );

  const compoundLayoutInitial = writeSource(
    "compound-layout-initial",
    lines("if { :; } \\", "then :; fi"),
  );
  const compoundLayoutFinal = writeSource(
    "compound-layout-final",
    lines("if { :; } \\", "\\", "then :; fi"),
  );
  assertIncrementalEqualsFresh(
    compoundLayoutInitial,
    compoundLayoutFinal,
    "insert-compound-layout-continuation",
    "12 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    compoundLayoutFinal,
    compoundLayoutInitial,
    "delete-compound-layout-continuation",
    "12 2",
  );

  const compoundSeparatorInitial = writeSource(
    "compound-separator-initial",
    lines("{ :; }"),
  );
  const compoundSeparatorFinal = writeSource(
    "compound-separator-final",
    lines("{ :\\", "; }"),
  );
  assertIncrementalEqualsFresh(
    compoundSeparatorInitial,
    compoundSeparatorFinal,
    "insert-compound-separator-continuation",
    "3 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    compoundSeparatorFinal,
    compoundSeparatorInitial,
    "delete-compound-separator-continuation",
    "3 2",
  );

  const forWordlistInitial = writeSource(
    "for-wordlist-initial",
    lines("for i in \\", "word; do :; done"),
  );
  const forWordlistFinal = writeSource(
    "for-wordlist-final",
    lines("for i in \\", "\\", "word; do :; done"),
  );
  assertIncrementalEqualsFresh(
    forWordlistInitial,
    forWordlistFinal,
    "insert-second-for-wordlist-continuation",
    "11 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    forWordlistFinal,
    forWordlistInitial,
    "delete-second-for-wordlist-continuation",
    "11 2",
  );

  const forFormalInitial = writeSource(
    "for-formal-initial",
    lines("for i in a; do :; done"),
  );
  const forFormalFinal = writeSource(
    "for-formal-final",
    lines("for i \\", "in a; do :; done"),
  );
  const forFormalOutput = parseValidCst(forFormalFinal);
  assertCstRange(forFormalOutput, "0:4-0:5", "name: name");
  assertCstRange(forFormalOutput, "0:6-1:0", "line_continuation");
  assertCstRange(forFormalOutput, "1:0-1:2", "in: in");
  assertNotContains(forFormalOutput, "ERROR");
  assertNotContains(forFormalOutput, "MISSING");
  assertIncrementalEqualsFresh(
    forFormalInitial,
    forFormalFinal,
    "insert-for-formal-continuation",
    "6 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    forFormalFinal,
    forFormalInitial,
    "delete-for-formal-continuation",
    "6 2",
  );

  const caseSubjectInitial = writeSource(
    "case-subject-initial",
    lines("case x \\", "in x) :;; esac"),
  );
  const caseSubjectFinal = writeSource(
    "case-subject-final",
    lines("case x \\", "\\", "in x) :;; esac"),
  );
  const caseSubjectOutput = parseValidCst(caseSubjectFinal);
  assertCstRange(caseSubjectOutput, "0:5-0:6", "word: word");
  assertCstRange(caseSubjectOutput, "0:7-1:0", "line_continuation");
  assertCstRange(caseSubjectOutput, "1:0-2:0", "line_continuation");
  assertCstRange(caseSubjectOutput, "2:0-2:2", "in: in");
  assertNotContains(caseSubjectOutput, "ERROR");
  assertNotContains(caseSubjectOutput, "MISSING");
  assertIncrementalEqualsFresh(
    caseSubjectInitial,
    caseSubjectFinal,
    "insert-second-case-subject-continuation",
    "9 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    caseSubjectFinal,
    caseSubjectInitial,
    "delete-second-case-subject-continuation",
    "9 2",
  );

  const boundaryPairs = [
    {
      edit: "4 0 \\\n",
      final: lines("case\\", " x in esac"),
      initial: lines("case x in esac"),
      name: "case-keyword-continuation",
      removal: "4 2",
    },
    {
      edit: "9 0 \\\n  ",
      final: lines("echo word\\", "  ||next"),
      initial: lines("echo word||next"),
      name: "or-boundary-continuation",
      removal: "9 4",
    },
    {
      edit: "9 0 \\\n  ",
      final: lines("echo word\\", "  &&next"),
      initial: lines("echo word&&next"),
      name: "and-boundary-continuation",
      removal: "9 4",
    },
    {
      edit: "17 0 \\\n  ",
      final: lines("case x in pattern\\", "  |other) : ;; esac"),
      initial: lines("case x in pattern|other) : ;; esac"),
      name: "case-pattern-boundary-continuation",
      removal: "17 4",
    },
  ];
  for (const contract of boundaryPairs) {
    const initial = writeSource(`${contract.name}-initial`, contract.initial);
    const final = writeSource(`${contract.name}-final`, contract.final);
    assertIncrementalEqualsFresh(
      initial,
      final,
      `insert-${contract.name}`,
      contract.edit,
    );
    assertIncrementalEqualsFresh(
      final,
      initial,
      `delete-${contract.name}`,
      contract.removal,
    );
  }

  const backquoteEndInitial = writeSource(
    "backquote-end-initial",
    lines("echo `printf x`"),
  );
  const backquoteEndFinal = writeSource(
    "backquote-end-final",
    lines("echo `printf x\\", "`"),
  );
  const backquoteEndOutput = parseValidCst(backquoteEndFinal);
  assertCstRange(backquoteEndOutput, "0:5-1:1", "backquote_substitution");
  assertCstRange(backquoteEndOutput, "0:14-1:0", "line_continuation");
  assertCstRange(backquoteEndOutput, "1:0-1:1", '"\\`"');
  assertNotContains(backquoteEndOutput, "backquote_end_recovery");
  assertNotContains(backquoteEndOutput, "ERROR");
  assertIncrementalEqualsFresh(
    backquoteEndInitial,
    backquoteEndFinal,
    "insert-backquote-end-continuation",
    "14 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    backquoteEndFinal,
    backquoteEndInitial,
    "delete-backquote-end-continuation",
    "14 2",
  );

  const substitutionEndInitial = writeSource(
    "command-substitution-end-initial",
    lines("echo $(printf x)"),
  );
  const substitutionEndFinal = writeSource(
    "command-substitution-end-final",
    lines("echo $(printf x\\", ")"),
  );
  const substitutionEndOutput = parseValidCst(substitutionEndFinal);
  assertCstRange(substitutionEndOutput, "0:5-1:1", "command_substitution");
  assertCstRange(substitutionEndOutput, "0:15-1:0", "line_continuation");
  assertCstRange(substitutionEndOutput, "1:0-1:1", '")"');
  assertNotContains(substitutionEndOutput, "ERROR");
  assertIncrementalEqualsFresh(
    substitutionEndInitial,
    substitutionEndFinal,
    "insert-command-substitution-end-continuation",
    "15 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    substitutionEndFinal,
    substitutionEndInitial,
    "delete-command-substitution-end-continuation",
    "15 2",
  );

  const functionLayoutInitial = writeSource(
    "function-layout-initial",
    lines(
      "f() {",
      " before",
      " while :; do :; done",
      " value=1",
      " after",
      "}",
    ),
  );
  const functionLayoutFinal = writeSource(
    "function-layout-final",
    lines(
      "f() {",
      " before",
      " while :; do",
      "  :",
      " done",
      " value=1",
      " after",
      "}",
    ),
  );
  for (const output of assertIncrementalEqualsFresh(
    functionLayoutInitial,
    functionLayoutFinal,
    "expand-function-loop-layout",
    "25 5 o\n  :\n ",
  )) {
    assertCstRange(output, "0:0-7:1", "function_definition");
    assertCstRange(output, "2:1-4:5", "while_clause");
    assertCstRange(output, "5:1-5:8", "assignment_word");
    assertNotContains(output, "ERROR");
    assertNotContains(output, "command_recovery");
  }
});

test("compound-list and case branches retain their public structure", () => {
  const branchInitial = writeSource(
    "compound-list-branch-initial",
    lines(
      "f() {",
      " a=",
      " if :; then",
      "  :",
      " else",
      "  :",
      " fi",
      " b=",
      "}",
    ),
  );
  const branchFinal = writeSource(
    "compound-list-branch-final",
    lines(
      "f() {",
      " a=",
      " if :; then",
      "  :",
      " else",
      "  :",
      " fi",
      " while :; do :; done",
      " b=",
      "}",
    ),
  );
  for (const output of assertIncrementalEqualsFresh(
    branchInitial,
    branchFinal,
    "insert-compound-list-loop",
    "40 0  while :; do :; done\n",
  )) {
    assertCstRange(output, "0:0-9:1", "function_definition");
    assertCstRange(output, "0:4-9:1", "brace_group");
    assertCstDirectChildRange(
      output,
      "0:4-9:1",
      "brace_group",
      "0:5-9:0",
      "body: compound_list",
    );
    assertCstDirectChildRange(
      output,
      "0:5-9:0",
      "body: compound_list",
      "1:1-8:3",
      "body: term",
    );
    assertCstRange(output, "2:1-6:3", "if_clause");
    assertCstRange(output, "7:1-7:20", "while_clause");
    assertOccurrenceCount(output, "assignment: assignment_word", 2);
    assertCstRange(output, "1:1-1:3", "assignment: assignment_word");
    assertCstRange(output, "8:1-8:3", "assignment: assignment_word");
    assertCstRange(output, "0:0-10:0", "program");
    assertNotContains(output, "ERROR");
    assertNotContains(output, "command_recovery");
    assertNotContains(output, "compound_command_recovery");
  }

  const caseBranchInitial = writeSource(
    "case-branch-initial",
    lines(
      "{",
      "  if :; then",
      "    :",
      "  else",
      "    :",
      "  fi",
      "  case x in",
      "    a) ;;",
      "    b) a= ;;",
      "  esac",
      "}",
    ),
  );
  const caseBranchFinal = writeSource(
    "case-branch-final",
    lines(
      "{",
      "  if :; then",
      "    :",
      "  else",
      "    :",
      "  fi",
      "  :",
      "  case x in",
      "    a) ;;",
      "    b) a= ;;",
      "  esac",
      "}",
    ),
  );
  for (const output of assertIncrementalEqualsFresh(
    caseBranchInitial,
    caseBranchFinal,
    "insert-command-before-case-branch",
    "39 0   :\n",
  )) {
    assertCstDirectChildRange(
      output,
      "0:1-11:0",
      "body: compound_list",
      "1:2-10:6",
      "body: term",
    );
    assertCstRange(output, "7:2-10:6", "case_clause");
    assertCstDirectChildRange(
      output,
      "7:2-10:6",
      "case_clause",
      "8:4-10:2",
      "items: case_list",
    );
    assertCstDirectChildRange(
      output,
      "8:4-10:2",
      "items: case_list",
      "9:4-10:2",
      "item: case_item",
    );
    assertCstDirectChildRange(
      output,
      "9:4-10:2",
      "item: case_item",
      "9:6-9:9",
      "body: compound_list",
    );
    assertCstDirectChildRange(
      output,
      "9:4-10:2",
      "item: case_item",
      "9:10-9:12",
      "terminator: dsemi",
    );
    assertCstDirectChildRange(
      output,
      "7:2-10:6",
      "case_clause",
      "10:2-10:6",
      "esac_keyword",
    );
    assertOccurrenceCount(output, "terminator: dsemi", 2);
    assertNotContains(output, "ERROR");
    assertNotContains(output, "command_recovery");
    assertNotContains(output, "compound_command_recovery");
  }

  const compoundRegression = writeSource(
    "compound-list-regression",
    lines(
      "regular_file_identity() {",
      "  CURRENT_FILE_ID=",
      '  [ -f "$1" ] && [ ! -L "$1" ] ||',
      "    return 1",
      "  file_owner=$(stat -f '%u' \"$1\" 2>/dev/null) || return 1",
      "  file_mode=$(stat -f '%Lp' \"$1\" 2>/dev/null) || return 1",
      "}",
    ),
  );
  assertValid(compoundRegression);

  const compoundListInitial = writeSource(
    "compound-list-initial",
    lines(
      "f() {",
      "  before=",
      "  first ||",
      "    second",
      "  target=$(one) || recover",
      "}",
    ),
  );
  const compoundListFinal = writeSource(
    "compound-list-final",
    lines(
      "f() {",
      "  before=",
      "  first ||",
      "    second",
      "  targets=$(one) || recover",
      "}",
    ),
  );
  assertValid(compoundListFinal);
  assertIncrementalEqualsFresh(
    compoundListInitial,
    compoundListFinal,
    "compound-list-assignment-edit",
    "46 0 s",
  );

  const caseItemLayout = writeSource(
    "case-item-layout",
    "case value in\n  tab) left\t|| right ;;\n  tight) left||right ;;\nesac\n",
  );
  assertValid(caseItemLayout);

  const caseItemInitial = writeSource(
    "case-item-initial",
    lines(
      "summarize() {",
      "  case value in",
      "    first) before ;;",
      "    second) condition ;;",
      "  esac",
      "}",
    ),
  );
  const caseItemFinal = writeSource(
    "case-item-final",
    lines(
      "summarize() {",
      "  case value in",
      "    first) before ;;",
      "    second) condition || invalid_input ;;",
      "  esac",
      "}",
    ),
  );
  assertValid(caseItemFinal);
  assertIncrementalEqualsFresh(
    caseItemInitial,
    caseItemFinal,
    "case-item-and-or-insertion",
    "73 0 || invalid_input ",
  );
  assertIncrementalEqualsFresh(
    caseItemFinal,
    caseItemInitial,
    "case-item-and-or-deletion",
    "73 17",
  );

  const caseItemBoundaryInitial = writeSource(
    "case-item-boundary-initial",
    lines("case x in x) :\\", ";; esac"),
  );
  const caseItemBoundaryFinal = writeSource(
    "case-item-boundary-final",
    lines("case x in x) :\\", "\\", ";; esac"),
  );
  assertIncrementalEqualsFresh(
    caseItemBoundaryInitial,
    caseItemBoundaryFinal,
    "insert-second-case-item-boundary-continuation",
    "16 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    caseItemBoundaryFinal,
    caseItemBoundaryInitial,
    "delete-second-case-item-boundary-continuation",
    "16 2",
  );

  const caseBodyInitial = writeSource(
    "case-body-separator-initial",
    lines("case x in x)echo z;;esac"),
  );
  const caseBodyFinal = writeSource(
    "case-body-separator-final",
    lines("case x in x)echo \\", "z;;esac"),
  );
  assertIncrementalEqualsFresh(
    caseBodyInitial,
    caseBodyFinal,
    "insert-case-body-separator-continuation",
    "17 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    caseBodyFinal,
    caseBodyInitial,
    "delete-case-body-separator-continuation",
    "17 2",
  );

  const followingInitial = writeSource(
    "case-item-following-initial",
    lines("case x in x):;; y):;;esac"),
  );
  const followingFinal = writeSource(
    "case-item-following-final",
    lines("case x in x):;;\\", " y):;;esac"),
  );
  assertIncrementalEqualsFresh(
    followingInitial,
    followingFinal,
    "insert-case-item-following-continuation",
    "15 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    followingFinal,
    followingInitial,
    "delete-case-item-following-continuation",
    "15 2",
  );

  const emptyItemInitial = writeSource(
    "empty-case-item-initial",
    lines("case x in x):;;esac"),
  );
  const emptyItemFinal = writeSource(
    "empty-case-item-final",
    lines("case x in x);;esac"),
  );
  assertIncrementalEqualsFresh(
    emptyItemInitial,
    emptyItemFinal,
    "delete-empty-case-item-body",
    "12 1",
  );

  const reservedForInitial = writeSource(
    "reserved-for-word-initial",
    lines("for x in ordinary; do :; done"),
  );
  const reservedForFinal = writeSource(
    "reserved-for-word-final",
    lines("for x in fi; do :; done"),
  );
  assertIncrementalEqualsFresh(
    reservedForInitial,
    reservedForFinal,
    "replace-for-word-with-reserved-closer-spelling",
    "9 8 fi",
  );
});

test("realistic function structure remains queryable", () => {
  const functionStructure = [
    "sample_log() {",
    "  cat <<'SAMPLE'",
    "run: 2026-08-01T15:24:00Z",
    "suite: checkout-api",
    "check: creates order | 142ms | PASS",
    "check: rejects expired card | 87ms | FAIL",
    "failure: rejects expired card: expected 422, got 500",
    "SAMPLE",
    "}",
    "",
    "summarize() {",
    "  run_value=",
    "  suite_value=",
    "  pass_count=0",
    "  fail_count=0",
    "  total_ms=0",
    "",
    "  printf '%s\\n' 'CI test summary'",
    "",
    '  while IFS= read -r line || [ -n "$line" ]; do',
    "    case $line in",
    "    'run: '*)",
    "      [ -z \"$run_value\" ] || invalid_input 'duplicate run'",
    "      run_value=$(printf '%s\\n' \"$line\" | sed 's/^run: //')",
    "      [ -n \"$run_value\" ] || invalid_input 'empty run'",
    "      printf 'run: %s\\n' \"$run_value\"",
    "      ;;",
    "    'suite: '*)",
    "      [ -n \"$run_value\" ] || invalid_input 'suite before run'",
    "      [ -z \"$suite_value\" ] || invalid_input 'duplicate suite'",
    "      suite_value=$(printf '%s\\n' \"$line\" | sed 's/^suite: //')",
    "      [ -n \"$suite_value\" ] || invalid_input 'empty suite'",
    "      printf 'suite: %s\\n' \"$suite_value\"",
    "      ;;",
    "    'check: '*)",
    "      [ -n \"$suite_value\" ] || invalid_input 'check before suite'",
    "      check_data=$(printf '%s\\n' \"$line\" | sed 's/^check: //')",
    "      IFS='|' read -r check_name check_duration check_status <<EOF",
    "$check_data",
    "EOF",
    "      check_name=$(printf '%s\\n' \"$check_name\" | trim)",
    "      check_duration=$(printf '%s\\n' \"$check_duration\" | trim)",
    "      check_status=$(printf '%s\\n' \"$check_status\" | trim)",
    '      [ "$check_data" = "$check_name | $check_duration | $check_status" ] || invalid_input \'malformed check\'',
    "      case $check_duration in",
    "      *ms) duration_value=$(printf '%s\\n' \"$check_duration\" | sed 's/ms$//') ;;",
    "      *) invalid_input 'check duration must end in ms' ;;",
    "      esac",
    "      case $duration_value in",
    "      '' | *[!0-9]*) invalid_input 'check duration must be an integer' ;;",
    "      esac",
    "      case $check_status in",
    "      PASS)",
    "        pass_count=$((pass_count + 1))",
    "        ;;",
    "      FAIL)",
    "        fail_count=$((fail_count + 1))",
    "        ;;",
    "      *)",
    "        invalid_input 'check status must be PASS or FAIL'",
    "        ;;",
    "      esac",
    "      total_ms=$((total_ms + duration_value))",
    '      printf \'%s %s (%sms)\\n\' "$check_status" "$check_name" "$duration_value"',
    "      ;;",
    "    'failure: '*)",
    "      [ \"$fail_count\" -gt 0 ] || invalid_input 'failure without a failed check'",
    "      failure_data=$(printf '%s\\n' \"$line\" | sed 's/^failure: //')",
    "      IFS=: read -r failure_name failure_message <<EOF",
    "$failure_data",
    "EOF",
    "      failure_name=$(printf '%s\\n' \"$failure_name\" | trim)",
    "      failure_message=$(printf '%s\\n' \"$failure_message\" | trim)",
    '      [ "$failure_data" = "$failure_name: $failure_message" ] || invalid_input \'malformed failure\'',
    '      [ -n "$failure_name" ] && [ -n "$failure_message" ] || invalid_input \'empty failure\'',
    '      printf \'  failure: %s: %s\\n\' "$failure_name" "$failure_message"',
    "      ;;",
    "    '')",
    "      ;;",
    "    *)",
    "      invalid_input 'unknown record'",
    "      ;;",
    "    esac",
    "  done",
    "",
    "  [ -n \"$run_value\" ] || invalid_input 'missing run'",
    "  [ -n \"$suite_value\" ] || invalid_input 'missing suite'",
    '  [ "$pass_count" -gt 0 ] || [ "$fail_count" -gt 0 ] || invalid_input \'missing check\'',
    '  printf \'result: %s passed, %s failed, %sms total\\n\' "$pass_count" "$fail_count" "$total_ms"',
    "",
    '  [ "$fail_count" -eq 0 ]',
    "}",
    "",
  ].join("\n");
  const source = writeSource("function-structure", functionStructure);
  parseValidTree(source);
  const queryOutput = runQuery(source);
  assertContains(queryOutput, "sample_log");
  assertContains(queryOutput, "summarize");
});

test("bracket fallback remains stable across complete and incomplete edits", () => {
  const bracketUnclosed = writeSource("bracket-unclosed", lines("printf [abc"));
  const bracketClosed = writeSource("bracket-closed", lines("printf [abc]"));
  const bracketRange = writeSource("bracket-range", lines("printf [a-z]"));
  const bracketUnclosedOutput = parseValidCst(bracketUnclosed);
  assertCstRange(bracketUnclosedOutput, "0:7-0:8", "literal");
  assertCstRange(bracketUnclosedOutput, "0:8-0:11", "literal");
  assertIncrementalEqualsFresh(
    bracketUnclosed,
    bracketClosed,
    "complete-bracket-expression",
    "11 0 ]",
  );
  assertIncrementalEqualsFresh(
    bracketClosed,
    bracketUnclosed,
    "unclose-bracket-expression",
    "11 1",
  );
  assertIncrementalEqualsFresh(
    bracketClosed,
    bracketRange,
    "bracket-list-to-range",
    "9 2 -z",
  );

  const specialUnclosed = writeSource(
    "special-bracket-unclosed",
    lines("printf [[."),
  );
  const specialClosed = writeSource(
    "special-bracket-closed",
    lines("printf [[.x.]]"),
  );
  assertIncrementalEqualsFresh(
    specialUnclosed,
    specialClosed,
    "complete-collating-symbol-bracket-expression",
    "10 0 x.]]",
  );
  assertIncrementalEqualsFresh(
    specialClosed,
    specialUnclosed,
    "unclose-collating-symbol-bracket-expression",
    "10 4",
  );

  const specialSuffixInitial = writeSource(
    "special-suffix-initial",
    lines("printf [[:alpha:]"),
  );
  const specialSuffixFinal = writeSource(
    "special-suffix-final",
    lines("printf [[:alpha:]]"),
  );
  assertIncrementalEqualsFresh(
    specialSuffixInitial,
    specialSuffixFinal,
    "complete-special-suffix-outer-bracket",
    "17 0 ]",
  );
  assertIncrementalEqualsFresh(
    specialSuffixFinal,
    specialSuffixInitial,
    "restore-special-suffix-literal-prefix",
    "17 1",
  );

  const operatorSuffixInitial = writeSource(
    "operator-suffix-initial",
    lines('printf [a"x"[.]'),
  );
  const operatorSuffixFinal = writeSource(
    "operator-suffix-final",
    lines('printf [a"x"*[.]'),
  );
  assertIncrementalEqualsFresh(
    operatorSuffixInitial,
    operatorSuffixFinal,
    "insert-operator-before-completed-bracket-suffix",
    "12 0 *",
  );
  assertIncrementalEqualsFresh(
    operatorSuffixFinal,
    operatorSuffixInitial,
    "delete-operator-before-completed-bracket-suffix",
    "12 1",
  );

  const terminalBracketInitial = writeSource(
    "terminal-bracket-initial",
    lines("echo [!]", "next"),
  );
  const terminalBracketFinal = writeSource(
    "terminal-bracket-final",
    lines("echo [!]\\", "", "next"),
  );
  assertIncrementalEqualsFresh(
    terminalBracketInitial,
    terminalBracketFinal,
    "insert-terminal-bracket-continuation",
    "8 0 \\\n",
  );
  assertIncrementalEqualsFresh(
    terminalBracketFinal,
    terminalBracketInitial,
    "delete-terminal-bracket-continuation",
    "8 2",
  );
});

test("parser resource bounds preserve complete roots and deterministic recovery", () => {
  const largeValidSources = [
    ["unmatched-brackets", `printf ${"[".repeat(80_000)}\n`],
    ["long-bracket-list", `printf [${"a".repeat(16_000)}]\n`],
    ["repeated-brackets", `printf ${"[abc]".repeat(4_000)}\n`],
    [
      "repeated-incomplete-special-brackets",
      `printf [${"[:alpha:]".repeat(4_000)}\n`,
    ],
    ["continued-blank-lines", `printf value ${"\\\n".repeat(20_000)}after\n`],
    ["continued-pipe-linebreak", `first|${"\\\n".repeat(20_000)}next\n`],
    [
      "here-document-dollars",
      `cat <<EOF\n${"$x".repeat(16_000)}\nEOF\nafter\n`,
    ],
  ];
  for (const [name, contents] of largeValidSources) {
    assertValid(writeSource(name, contents), name);
  }

  const manyDocuments = writeSource(
    "many-documents",
    `cat${" <<X".repeat(600)}\n${"body\nX\n".repeat(600)}after\n`,
  );
  assertRepeatedColdParse("resource", manyDocuments, "many-documents");

  function nestedDocuments(depth) {
    let contents = "cat <<X\n";
    contents += "$(cat <<X\n".repeat(depth - 1);
    contents += "leaf\n";
    for (let counter = depth - 1; counter >= 0; counter -= 1) {
      contents += "X\n";
      if (counter > 0) {
        contents += ")\n";
      }
    }
    return `${contents}after\n`;
  }

  const deepDocuments = writeSource("deep-documents", nestedDocuments(150));
  assertValid(deepDocuments);
  const boundedDocuments = writeSource(
    "deep-documents-bounded",
    nestedDocuments(300),
  );
  assertRepeatedColdParse(
    "resource",
    boundedDocuments,
    "deep-documents-bounded",
    30_000_000,
  );

  const nestedDelimiter = writeSource(
    "nested-delimiter-document",
    lines("cat <<$(cat <<\\eof", "a here-doc with )", "eof", ")", "after"),
  );
  const nestedDelimiterOutput = parseValidTree(nestedDelimiter);
  assertContains(nestedDelimiterOutput, "end: (here_end [0, 6] - [3, 1]");
  assertContains(
    nestedDelimiterOutput,
    "end: (here_document_end_recovery [5, 0] - [5, 0])",
  );

  const quoteRecoveryInitial = writeSource(
    "quote-recovery-initial",
    lines("cat <<EOF", '$("open', "EOF", "after"),
  );
  const quoteRecoveryFinal = writeSource(
    "quote-recovery-final",
    lines("cat <<EOF", '$("open")', "EOF", "after"),
  );
  assertContains(
    parseRecovery(quoteRecoveryInitial),
    "(complete_command [3, 0] - [3, 5]",
  );
  assertIncrementalEqualsFresh(
    quoteRecoveryInitial,
    quoteRecoveryFinal,
    "quote-recovery-at-here-document-end",
    '17 0 ")',
  );

  let nestedBackquotes = "echo `level1 ";
  for (let level = 2; level <= 6; level += 1) {
    nestedBackquotes += `${"\\".repeat(2 ** (level - 1) - 1)}\`level${level} `;
  }
  nestedBackquotes += "leaf";
  for (let level = 6; level >= 2; level -= 1) {
    nestedBackquotes += `${"\\".repeat(2 ** (level - 1) - 1)}\` end${level}`;
  }
  nestedBackquotes += "`\n";
  assertValid(writeSource("nested-backquotes", nestedBackquotes));
});

test("parser scaling remains linear within the existing guard", () => {
  function measure(source, name) {
    const started = process.hrtime.bigint();
    runParse({ description: `${name} performance`, format: "summary", source });
    return Number((process.hrtime.bigint() - started) / 1_000_000n);
  }

  const scalingSources = [
    {
      large: `printf value${" \\\n".repeat(12_000)}after\n`,
      name: "spaced line-continuation parsing",
      small: `printf value${" \\\n".repeat(3_000)}after\n`,
    },
    {
      large: `printf value ${"\\\n".repeat(12_000)}after\n`,
      name: "direct line-continuation parsing",
      small: `printf value ${"\\\n".repeat(3_000)}after\n`,
    },
    {
      large: `: "$((a${" \\\n".repeat(8_000)}+ b))"\n`,
      name: "arithmetic layout parsing",
      small: `: "$((a${" \\\n".repeat(2_000)}+ b))"\n`,
    },
    {
      large: `: "$((1${" \\\n".repeat(8_000)}$operator 2))"\n`,
      name: "dynamic arithmetic layout parsing",
      small: `: "$((1${" \\\n".repeat(2_000)}$operator 2))"\n`,
    },
    {
      large: `printf ${'[a"x"*[.]'.repeat(12_000)}\n`,
      name: "bracket suffix parsing",
      small: `printf ${'[a"x"*[.]'.repeat(3_000)}\n`,
    },
  ];

  for (const contract of scalingSources) {
    const smallSource = writeSource(`${contract.name}-small`, contract.small);
    const largeSource = writeSource(`${contract.name}-large`, contract.large);
    const smallMilliseconds = measure(smallSource, contract.name);
    const largeMilliseconds = measure(largeSource, contract.name);
    assert.ok(
      largeMilliseconds <= smallMilliseconds * 8 + 100,
      `${contract.name} scaled nonlinearly: ${smallMilliseconds}ms to ${largeMilliseconds}ms`,
    );
  }
});
