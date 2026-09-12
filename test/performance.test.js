import assert from "node:assert/strict";
import { test } from "node:test";

import { grammarName } from "../scripts/tree-sitter.js";
import {
  applyEdits,
  assertIncrementalEqualsFresh,
  assertOccurrenceCount,
  assertValid,
  compareIncrementalAndFresh,
  incrementalParseArguments,
  parseRecoveryAfterEdits,
  parserLibrary,
  runParserCommand,
  writeSource,
} from "./support/parser.js";

const measurementSamples = 9;

function medianDuration(samples) {
  const sorted = [...samples].sort((left, right) => left - right);
  return sorted[(sorted.length - 1) >> 1];
}

function readDuration(stdout, label, description) {
  const value = Number(
    stdout.match(new RegExp(`${label}:[ ]+([0-9.]+) ms`))?.[1],
  );
  assert.ok(
    Number.isFinite(value),
    `${description}: ${label} timing is unreadable\n${stdout}`,
  );
  return value;
}

function medianOperationDuration(arguments_, label, description, mode) {
  const samples = [];
  for (let run = 0; run < measurementSamples + 1; run += 1) {
    const result = runParserCommand(
      arguments_,
      mode === "recovery" ? [0, 1] : [0],
    );
    samples.push(readDuration(result.stdout, label, description));
  }
  return medianDuration(samples.slice(1));
}

function medianFreshParseDuration(source, description, mode = "valid") {
  return medianOperationDuration(
    [
      "parse",
      "--lib-path",
      parserLibrary,
      "--lang-name",
      grammarName,
      "--time",
      "--quiet",
      "--",
      source,
    ],
    "Parse",
    description,
    mode,
  );
}

function medianIncrementalParseDuration(
  source,
  edit,
  description,
  mode = "valid",
) {
  return medianOperationDuration(
    incrementalParseArguments(source, edit, "--time", "--quiet"),
    "Edit",
    description,
    mode,
  );
}

test("sh: parser scaling remains linear within the existing guard", () => {
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
    assertValid(smallSource, `${contract.name} small`);
    assertValid(largeSource, `${contract.name} large`);
    const smallMilliseconds = medianFreshParseDuration(
      smallSource,
      contract.name,
    );
    const largeMilliseconds = medianFreshParseDuration(
      largeSource,
      contract.name,
    );
    const maximumScale = 8;
    assert.ok(
      largeMilliseconds <= smallMilliseconds * maximumScale + 100,
      `${contract.name} scaled nonlinearly: ${smallMilliseconds}ms to ${largeMilliseconds}ms`,
    );
  }
});

test("sh: incremental parsing reuses unchanged source and records editing timings", (context) => {
  function assertSourceReuse(parse, name) {
    assert.match(
      parse.debugOutput,
      /^reuse_node /m,
      `${name}: no source reuse recorded`,
    );
  }

  function reportTiming(name, incremental, fresh, reference) {
    const ratio =
      fresh === 0
        ? "below timing resolution"
        : `${(incremental / fresh).toFixed(2)}x`;
    const assessment =
      incremental > fresh * reference ? "investigate" : "within reference";
    context.diagnostic(
      `${name}: incremental ${incremental}ms, fresh ${fresh}ms (${ratio}; reference ${reference}x, ${assessment})`,
    );
  }

  function editedSource(name, initialContents, edit) {
    return writeSource(
      `${name}-edited`,
      applyEdits(Buffer.from(initialContents), [edit]),
    );
  }

  const commandLine = "echo aaaa\n";
  const commandEditAt = (index) => ({
    byte: commandLine.length * index + 5,
    deleteBytes: 1,
    insert: "b",
  });
  const commandCount = 1200;
  const commandContents = commandLine.repeat(commandCount);
  const commandSource = writeSource("reuse-commands", commandContents);
  for (const [position, index] of [
    ["head", 0],
    ["middle", 600],
    ["tail", 1199],
  ]) {
    const name = `${position}-command-replace`;
    const edit = commandEditAt(index);
    const edited = editedSource(name, commandContents, edit);
    const [parse] = compareIncrementalAndFresh(
      { debug: true },
      commandSource,
      edited,
      name,
      edit,
    );
    assertSourceReuse(parse, name);
    const incremental = medianIncrementalParseDuration(
      commandSource,
      edit,
      name,
    );
    const fresh = medianFreshParseDuration(edited, name);
    reportTiming(name, incremental, fresh, 0.75);
  }

  const largeCommandCount = 4800;
  const largeCommandContents = commandLine.repeat(largeCommandCount);
  const largeCommandSource = writeSource(
    "reuse-commands-large",
    largeCommandContents,
  );
  for (const [position, index] of [
    ["head", 0],
    ["middle", 2400],
    ["tail", 4799],
  ]) {
    const name = `large-${position}-command-replace`;
    const edit = commandEditAt(index);
    const edited = editedSource(name, largeCommandContents, edit);
    const [parse] = compareIncrementalAndFresh(
      { debug: true },
      largeCommandSource,
      edited,
      name,
      edit,
    );
    assertSourceReuse(parse, name);
    const incremental = medianIncrementalParseDuration(
      largeCommandSource,
      edit,
      name,
    );
    const fresh = medianFreshParseDuration(edited, name);
    reportTiming(name, incremental, fresh, 0.5);
  }

  const functionLine = "alpha() { : beta; }\n";
  const functionContents = functionLine.repeat(commandCount);
  const functionSource = writeSource("reuse-functions", functionContents);
  for (const [position, index] of [
    ["head", 0],
    ["middle", 600],
    ["tail", 1199],
  ]) {
    const name = `${position}-function-closer-delete`;
    const edit = {
      byte: functionLine.length * index + 18,
      deleteBytes: 1,
      insert: "",
    };
    const edited = editedSource(name, functionContents, edit);
    parseRecoveryAfterEdits(functionSource, edited, name, edit);
    const incremental = medianIncrementalParseDuration(
      functionSource,
      edit,
      name,
      "recovery",
    );
    const fresh = medianFreshParseDuration(edited, name, "recovery");
    reportTiming(name, incremental, fresh, 1.5);
  }

  const commentLine = "# comment\n";
  const largeCommentContents = commentLine.repeat(4800);
  const largeCommentSource = writeSource(
    "reuse-comments-large",
    largeCommentContents,
  );
  const commentName = "final-comment-byte-replace";
  const largeCommentEdit = {
    byte: largeCommentContents.length - 2,
    deleteBytes: 1,
    insert: "x",
  };
  const editedComments = editedSource(
    commentName,
    largeCommentContents,
    largeCommentEdit,
  );
  assertIncrementalEqualsFresh(
    largeCommentSource,
    editedComments,
    commentName,
    largeCommentEdit,
  );
  const largeCommentIncremental = medianIncrementalParseDuration(
    largeCommentSource,
    largeCommentEdit,
    commentName,
  );
  const largeCommentFreshEdited = medianFreshParseDuration(
    editedComments,
    commentName,
  );
  reportTiming(
    commentName,
    largeCommentIncremental,
    largeCommentFreshEdited,
    1.5,
  );
});

test("sh: nested arithmetic measurements cover ordinary editing depth", (context) => {
  for (const [kind, initialLeaf, finalLeaf] of [
    ["number", "1", "2"],
    ["parameter", `\${x:-1}`, `\${x:-2}`],
  ]) {
    const measurements = [];
    for (const depth of [16, 32]) {
      const name = `nested-arithmetic-${kind}-${depth}`;
      const prefix = `echo ${"$((".repeat(depth)}`;
      const suffix = `${"))".repeat(depth)}\n`;
      const edit = {
        byte: prefix.length + initialLeaf.indexOf("1"),
        deleteBytes: 1,
        insert: "2",
      };
      const initial = writeSource(
        `${name}-initial`,
        prefix + initialLeaf + suffix,
      );
      const final = writeSource(`${name}-final`, prefix + finalLeaf + suffix);
      for (const output of assertIncrementalEqualsFresh(
        initial,
        final,
        name,
        edit,
      )) {
        assertOccurrenceCount(output, "arithmetic_expansion", depth);
      }
      measurements.push({
        fresh: medianFreshParseDuration(final, name),
        incremental: medianIncrementalParseDuration(initial, edit, name),
      });
    }
    for (const operation of ["fresh", "incremental"]) {
      const small = measurements[0][operation];
      const large = measurements[1][operation];
      const ratio =
        small === 0
          ? "below timing resolution"
          : `${(large / small).toFixed(2)}x`;
      context.diagnostic(
        `${operation} nested arithmetic with ${kind} leaf, 16 to 32 levels: ${small}ms to ${large}ms (${ratio})`,
      );
    }
  }
});
