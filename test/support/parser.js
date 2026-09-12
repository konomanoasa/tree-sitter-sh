import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { after, before } from "node:test";
import {
  createEnvironmentDirectory,
  grammarDirectory,
  grammarName,
  runTreeSitter,
} from "../../scripts/tree-sitter.js";
import { applyEdits, formatEdit, sourceEndPoint } from "./source.js";

const contractsQuerySource = `(line_continuation) @line.continuation

(function_definition
  name: (fname) @function)
`;

const defaultParseTimeout = 10_000_000;

const parserProcessTimeout = 60_000;

let contractsQuery;

let runtimeDirectory;

let parserLibrary;

let sourceSequence = 0;

before(() => {
  runtimeDirectory = createEnvironmentDirectory("tree-sitter-sh-parser");
  contractsQuery = path.join(runtimeDirectory, "contracts.scm");
  fs.writeFileSync(contractsQuery, contractsQuerySource);
  parserLibrary = path.join(
    runtimeDirectory,
    process.platform === "win32" ? "parser.dll" : "parser",
  );
  runTreeSitter(["build", "--output", parserLibrary, grammarDirectory], {
    environmentDirectory: runtimeDirectory,
    timeout: parserProcessTimeout,
  });
});

after(() => {
  fs.rmSync(runtimeDirectory, { force: true, recursive: true });
});

function runParserCommand(arguments_, allowedStatuses = [0]) {
  return runTreeSitter(arguments_, {
    allowedStatuses,
    environmentDirectory: runtimeDirectory,
    timeout: parserProcessTimeout,
  });
}

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

function incrementalParseArguments(source, edit, ...extra) {
  return [
    "parse",
    "--lib-path",
    parserLibrary,
    "--lang-name",
    grammarName,
    "--edits",
    formatEdit(edit),
    ...extra,
    "--",
    source,
  ];
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

function runParse({
  debug = false,
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
  if (debug) {
    arguments_.push("-d");
  }
  if (format === "cst") {
    arguments_.push("--cst");
  } else if (format === "summary") {
    arguments_.push("--quiet", "--json-summary");
  } else {
    assert.equal(format, "tree", `${description}: unknown parse output format`);
  }
  arguments_.push("--timeout", String(timeout));
  if (edits.length > 0) {
    arguments_.push("--edits", ...edits.map(formatEdit));
  }
  arguments_.push("--", source);

  const result = runParserCommand(arguments_, [0, 1]);
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
  if (format === "cst" && mode === "valid") {
    assert.equal(
      parseCst(output)[0]?.content,
      "program",
      `${description}: parser produced no complete program root\n${output}`,
    );
  }

  if (mode === "valid") {
    if (format === "cst") assert.equal(hasRecovery(output), false, output);
    else if (format === "tree")
      assert.doesNotMatch(output, /\((ERROR|MISSING)[ \t]/);
    else
      assert.equal(
        JSON.parse(output).parse_summaries[0].successful,
        true,
        output,
      );
  }

  const expectedEnd = sourceEndPoint(fs.readFileSync(expectedSource));
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
  return {
    debugOutput: result.stderr,
    output,
    status: result.status,
    recovery: format === "cst" && hasRecovery(output),
  };
}

function runQuery(source, queryPath = contractsQuery) {
  return runParserCommand([
    "query",
    "--lib-path",
    parserLibrary,
    "--lang-name",
    grammarName,
    "--captures",
    queryPath,
    "--",
    source,
  ]).stdout;
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

function cstFingerprint(output) {
  const structure = [];
  const continuations = [];
  let continuationDepth;
  for (const entry of parseCst(output)) {
    if (continuationDepth !== undefined) {
      if (entry.depth > continuationDepth) continue;
      continuationDepth = undefined;
    }
    if (/^([a-z_]+: )?line_continuation([ ]|$)/.test(entry.content)) {
      continuations.push(entry.range);
      continuationDepth = entry.depth;
    } else {
      structure.push(entry.line);
    }
  }
  assert.notEqual(structure.length, 0, "CST fingerprint is empty");
  return JSON.stringify([structure, continuations]);
}

function assertCstOutputsEqual(name, left, right) {
  assert.equal(left.status, right.status, `${name}: parse statuses differ`);
  assert.equal(
    cstFingerprint(left.output),
    cstFingerprint(right.output),
    `${name}: public CST contracts differ`,
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
  if (mode === "resource") {
    assert.equal(first.status, second.status, `${name}: parse statuses differ`);
    assert.deepEqual(
      parseCst(first.output),
      parseCst(second.output),
      `${name}: resource recovery is not deterministic`,
    );
  } else {
    assertCstOutputsEqual(`${name} repeated cold parses`, first, second);
  }
}

function compareIncrementalAndFresh(
  { debug = false, mode = "valid" },
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
    debug,
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
  if (mode === "valid") {
    assertCstOutputsEqual(
      `${name} incremental and fresh parses`,
      incremental,
      fresh,
    );
  }
  return [incremental, fresh];
}

function assertIncrementalEqualsFresh(
  initialSource,
  finalSource,
  name,
  ...edits
) {
  return compareIncrementalAndFresh(
    {},
    initialSource,
    finalSource,
    name,
    ...edits,
  ).map(({ output }) => output);
}

function parseRecoveryAfterEdits(initialSource, finalSource, name, ...edits) {
  return compareIncrementalAndFresh(
    { mode: "recovery" },
    initialSource,
    finalSource,
    name,
    ...edits,
  ).map(({ output }) => output);
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

function hasRecovery(cst) {
  return /^[0-9: \t-]+•/m.test(cst);
}

export {
  applyEdits,
  assertContains,
  assertCstDirectChildRange,
  assertCstRange,
  assertIncrementalEqualsFresh,
  assertLineContinuationManifest,
  assertNoLineContinuations,
  assertNotContains,
  assertOccurrenceCount,
  assertRepeatedColdParse,
  assertSameLogicalProjection,
  assertValid,
  compareIncrementalAndFresh,
  cstFingerprint,
  hasRecovery,
  incrementalParseArguments,
  lineContinuationManifest,
  lines,
  parseCst,
  parseRecovery,
  parseRecoveryAfterEdits,
  parserLibrary,
  parseValidCst,
  parseValidTree,
  runParse,
  runParserCommand,
  runQuery,
  writeSource,
};
