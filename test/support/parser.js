import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { after, before } from "node:test";
import { createTreeSitter, grammars, root } from "../../scripts/tree-sitter.js";
import {
  assertCstOutputsEqual,
  assertCstSourceContract,
  assertFreshOutputsEqual,
  assertSameLogicalProjection,
  continuationManifest,
  hasRecovery,
  lineContinuationManifest,
  parseCst,
  rootEndPoint,
  sourcePoint,
} from "./cst.js";

const grammarName = grammars[0].name;

const contractsQuerySource = `(line_continuation) @line.continuation

(function_definition
  name: (fname) @function)
`;

const parseTimeoutMicros = 10_000_000;
const parserProcessTimeoutMs = 60_000;

let contractsQuery;
let runtimeDirectory;
let treeSitter;
let parserLibrary;
let sourceSequence = 0;

before(() => {
  treeSitter = createTreeSitter();
  runtimeDirectory = treeSitter.directory;
  contractsQuery = path.join(runtimeDirectory, "contracts.scm");
  fs.writeFileSync(contractsQuery, contractsQuerySource);
  parserLibrary = path.join(
    runtimeDirectory,
    process.platform === "win32" ? "parser.dll" : "parser",
  );
  runParserCommand([
    "build",
    "--output",
    parserLibrary,
    path.join(root, grammars[0].path),
  ]);
});

after(() => {
  treeSitter?.close();
});

function applyEdits(source, edits) {
  let bytes = Buffer.from(source);
  for (const edit of edits) {
    const { byte, deleteBytes, insert } = edit;
    const description = JSON.stringify(edit);
    assert.ok(
      Number.isSafeInteger(byte) && byte >= 0,
      `invalid byte offset: ${description}`,
    );
    assert.ok(
      Number.isSafeInteger(deleteBytes) && deleteBytes >= 0,
      `invalid deletion length: ${description}`,
    );
    assert.equal(typeof insert, "string", `invalid insertion: ${description}`);
    assert.ok(
      byte <= bytes.length && deleteBytes <= bytes.length - byte,
      `edit exceeds ${bytes.length} source bytes: ${description}`,
    );
    bytes = Buffer.concat([
      bytes.subarray(0, byte),
      Buffer.from(insert),
      bytes.subarray(byte + deleteBytes),
    ]);
  }
  return bytes;
}

function formatEdit({ byte, deleteBytes, insert }) {
  return `${byte} ${deleteBytes} ${insert}`;
}

function runParserCommand(
  arguments_,
  allowedStatuses = [0],
  processTimeoutMs = parserProcessTimeoutMs,
) {
  const result = treeSitter.run(arguments_, {
    timeout: processTimeoutMs,
  });
  assert.ifError(result.error);
  assert.ok(
    allowedStatuses.includes(result.status),
    result.stdout + result.stderr,
  );
  return result;
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

function runParse(options) {
  const {
    debug = false,
    description,
    edits = [],
    format = "cst",
    mode = "valid",
    source,
    expectedSource = source,
    timeoutMicros = parseTimeoutMicros,
    completionOnly = false,
    processTimeoutMs = parserProcessTimeoutMs,
  } = options;
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
  if (!completionOnly) arguments_.push("--timeout", String(timeoutMicros));
  if (edits.length > 0) {
    arguments_.push("--edits", ...edits.map(formatEdit));
  }
  arguments_.push("--", source);

  const result = runParserCommand(arguments_, [0, 1], processTimeoutMs);
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

  const expectedBytes = fs.readFileSync(expectedSource);
  const expectedEnd = sourcePoint(expectedBytes, expectedBytes.length);
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
  if (format === "cst" && mode === "valid") {
    assertCstSourceContract(output, expectedBytes);
  }
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

function assertValid(
  source,
  description = path.basename(source),
  options = {},
) {
  return runParse({ ...options, description, format: "summary", source });
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

function assertRepeatedColdParse(mode, source, name, options = {}) {
  const first = runParse({
    description: `${name} first cold parse`,
    mode,
    source,
    ...options,
  });
  const second = runParse({
    description: `${name} second cold parse`,
    mode,
    source,
    ...options,
  });
  assertFreshOutputsEqual(name, first, second);
}

function compareIncrementalAndFresh(
  { debug = false, mode = "valid" },
  initialSource,
  finalSource,
  name,
  ...edits
) {
  assert.ok(
    edits.length > 0,
    `${name}: incremental parse requires an edit history`,
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
  if (fresh.status === 0 && !fresh.recovery) {
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

function assertLineContinuationManifest(
  name,
  physicalSource,
  logicalSource,
  expected,
) {
  const physicalOutput = parseValidCst(physicalSource, `${name} physical`);
  const logicalOutput = parseValidCst(logicalSource, `${name} logical`);
  const actual = lineContinuationManifest(runQuery(physicalSource));
  assert.deepEqual(continuationManifest(physicalOutput), actual);
  assert.deepEqual(actual, expected, `${name}: continuation ranges differ`);
  assertSameLogicalProjection(name, logicalOutput, physicalOutput);
}

export {
  applyEdits,
  assertIncrementalEqualsFresh,
  assertLineContinuationManifest,
  assertRepeatedColdParse,
  assertValid,
  compareIncrementalAndFresh,
  incrementalParseArguments,
  lines,
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
