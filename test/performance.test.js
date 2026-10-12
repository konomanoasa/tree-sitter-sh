import assert from "node:assert/strict";
import { test } from "node:test";
import { grammars } from "../scripts/tree-sitter.js";
import { assertOccurrenceCount } from "./support/cst.js";
import {
  applyEdits,
  assertIncrementalEqualsFresh,
  assertRepeatedColdParse,
  assertValid,
  compareIncrementalAndFresh,
  incrementalParseArguments,
  parseRecoveryAfterEdits,
  parserLibrary,
  runParse,
  runParserCommand,
  writeSource,
} from "./support/parser.js";

const measurementSamples = 9;
const warmupSamples = 1;
const scalingInputMultiplier = 4;
const maximumDurationMultiplier = 8;
const scalingSlackMs = 100;
const localEditCases = [
  { prefix: "", lineCount: 1_200, referenceRatio: 0.75 },
  { prefix: "large-", lineCount: 4_800, referenceRatio: 0.5 },
];
const structureEditLineCount = 1_200;
const commentLineCount = 4_800;
const declarationArgumentCount = 1_200;
const arithmeticDepths = [16, 32];
const runtimeFragmentCounts = [64, 128];
const recoveryReferenceRatio = 1.5;

function editPositions(lineCount) {
  return Object.entries({
    head: 0,
    middle: Math.floor(lineCount / 2),
    tail: lineCount - 1,
  });
}

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
  for (let run = 0; run < measurementSamples + warmupSamples; run += 1) {
    const result = runParserCommand(
      arguments_,
      mode === "recovery" ? [0, 1] : [0],
    );
    samples.push(readDuration(result.stdout, label, description));
  }
  return medianDuration(samples.slice(warmupSamples));
}

function medianFreshParseDuration(source, description, mode = "valid") {
  return medianOperationDuration(
    [
      "parse",
      "--lib-path",
      parserLibrary,
      "--lang-name",
      grammars[0].name,
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

test("sh: large-input scaling stays within the growth budget", () => {
  const continuedComment = (count) => {
    const slash = "\\";
    const prefix = `: "\`: ${slash}"${slash}\`#x`;
    const pair = `${slash.repeat(3)}\n${slash.repeat(4)}\n`;
    return `${prefix}${pair.repeat(count)}world\n${slash}\`${slash}"\`"\n`;
  };
  const encodedWordlist = (count) => {
    let source = `for x in value${"\\\n".repeat(count)} ; do :; done`;
    const backquoteDepth = 3;
    for (let depth = 0; depth < backquoteDepth; depth += 1) {
      source = `: \`${source.replaceAll("\\", "\\\\").replaceAll("`", "\\`")}\``;
    }
    return `${source}\n`;
  };
  const scalingSources = [
    {
      name: "nested comment continuation parsing",
      units: 1_500,
      source: (count) => continuedComment(count),
    },
    {
      name: "backquote wordlist continuation parsing",
      units: 3_000,
      source: (count) => encodedWordlist(count),
    },
    {
      name: "spaced line-continuation parsing",
      units: 3_000,
      source: (count) => `printf value${" \\\n".repeat(count)}after\n`,
    },
    {
      name: "direct line-continuation parsing",
      units: 3_000,
      source: (count) => `printf value ${"\\\n".repeat(count)}after\n`,
    },
    {
      name: "arithmetic layout parsing",
      units: 2_000,
      source: (count) => `: "$((a${" \\\n".repeat(count)}+ b))"\n`,
    },
    {
      name: "dynamic arithmetic layout parsing",
      units: 2_000,
      source: (count) => `: "$((1${" \\\n".repeat(count)}$operator 2))"\n`,
    },
    {
      name: "bracket suffix parsing",
      units: 3_000,
      source: (count) => `printf ${'[a"x"*[.]'.repeat(count)}\n`,
    },
    {
      name: "bracket prefix and suffix parsing",
      units: 3_000,
      source: (count) => `printf ${"[x[:y]z[a]".repeat(count)}\n`,
    },
    {
      name: "chained bracket range parsing",
      units: 8_192,
      source: (count) => `printf [${"a-".repeat(count)}a]\n`,
    },
  ];

  for (const contract of scalingSources) {
    const smallSource = writeSource(
      `${contract.name}-small`,
      contract.source(contract.units),
    );
    const largeSource = writeSource(
      `${contract.name}-large`,
      contract.source(contract.units * scalingInputMultiplier),
    );
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
    assert.ok(
      largeMilliseconds <=
        smallMilliseconds * maximumDurationMultiplier + scalingSlackMs,
      `${contract.name} scaled nonlinearly: ${smallMilliseconds}ms to ${largeMilliseconds}ms`,
    );
  }
});

test("sh: incremental parsing reuses unchanged source and records editing timings", (context) => {
  function assertSourceReuse(parse, name) {
    assert.match(
      parse.debugOutput,
      /^reuse_node symbol:complete_command/m,
      `${name}: no complete command reuse recorded`,
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
  for (const { prefix, lineCount, referenceRatio } of localEditCases) {
    const contents = commandLine.repeat(lineCount);
    const source = writeSource(`reuse-${prefix}commands`, contents);
    for (const [position, index] of editPositions(lineCount)) {
      const name = `${prefix}${position}-command-replace`;
      const edit = {
        byte: commandLine.length * index + commandLine.indexOf("aaaa"),
        deleteBytes: 1,
        insert: "b",
      };
      const edited = editedSource(name, contents, edit);
      const [parse] = compareIncrementalAndFresh(
        { debug: true },
        source,
        edited,
        name,
        edit,
      );
      assertSourceReuse(parse, name);
      const incremental = medianIncrementalParseDuration(source, edit, name);
      const fresh = medianFreshParseDuration(edited, name);
      reportTiming(name, incremental, fresh, referenceRatio);
    }
  }

  const functionLine = "alpha() { : beta; }\n";
  const functionContents = functionLine.repeat(structureEditLineCount);
  const functionSource = writeSource("reuse-functions", functionContents);
  for (const [position, index] of editPositions(structureEditLineCount)) {
    const name = `${position}-function-closer-delete`;
    const edit = {
      byte: functionLine.length * index + functionLine.indexOf("}"),
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
    reportTiming(name, incremental, fresh, recoveryReferenceRatio);
  }

  const commentLine = "# comment\n";
  const largeCommentContents = commentLine.repeat(commentLineCount);
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
    recoveryReferenceRatio,
  );
});

test("sh: declaration context edits record incremental and fresh timings", (context) => {
  const arguments_ = " x=~/bin:$PATH".repeat(declarationArgumentCount);
  for (const [name, prefix, edit] of [
    [
      "declaration-name",
      "export",
      { byte: 0, deleteBytes: 6, insert: "printf" },
    ],
    [
      "command-option",
      "command export",
      { byte: 8, deleteBytes: 0, insert: "-- " },
    ],
  ]) {
    const initial = `${prefix}${arguments_}\n`;
    const source = writeSource(`${name}-initial`, initial);
    const edited = writeSource(
      `${name}-edited`,
      applyEdits(Buffer.from(initial), [edit]),
    );
    compareIncrementalAndFresh({}, source, edited, name, edit);
    const incremental = medianIncrementalParseDuration(source, edit, name);
    const fresh = medianFreshParseDuration(edited, name);
    context.diagnostic(
      `${name}: incremental ${incremental}ms, fresh ${fresh}ms`,
    );
  }
});

test("sh: nested arithmetic measurements cover ordinary editing depth", (context) => {
  for (const [kind, initialLeaf, finalLeaf] of [
    ["number", "1", "2"],
    ["parameter", `\${x:-1}`, `\${x:-2}`],
  ]) {
    const measurements = [];
    for (const depth of arithmeticDepths) {
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
        `${operation} nested arithmetic with ${kind} leaf, ${arithmeticDepths[0]} to ${arithmeticDepths[1]} levels: ${small}ms to ${large}ms (${ratio})`,
      );
    }
  }
});

test("sh: dense runtime arithmetic fragments preserve editing performance", (context) => {
  const fragment = ` \${x} 1`;
  for (const fixture of [
    {
      name: "viable dynamic arithmetic",
      source: (count, finalNumber) =>
        `echo $((1${fragment.repeat(count - 1)} \${x} ${finalNumber}))\n`,
      arithmeticCount: 1,
      commandCount: 0,
      extraParameters: 0,
    },
    {
      name: "global assignment fallback",
      source: (count, finalNumber) =>
        `echo $((1${fragment.repeat(count)} \${x} a = ${finalNumber}))\n`,
      arithmeticCount: 0,
      commandCount: 1,
      extraParameters: 1,
    },
    {
      name: "spaced runtime fragment fallback",
      source: (count, finalNumber) =>
        `echo $((1${` \${x} `.repeat(count)}= ${finalNumber}))\n`,
      arithmeticCount: 0,
      commandCount: 1,
      extraParameters: 0,
    },
  ]) {
    const measurements = [];
    for (const count of runtimeFragmentCounts) {
      const name = `dense-${fixture.name}-${count}`;
      const initialContents = fixture.source(count, "1");
      const finalContents = fixture.source(count, "2");
      const initial = writeSource(`${name}-initial`, initialContents);
      const final = writeSource(`${name}-final`, finalContents);
      const edit = {
        byte: initialContents.lastIndexOf("1"),
        deleteBytes: 1,
        insert: "2",
      };
      for (const output of assertIncrementalEqualsFresh(
        initial,
        final,
        name,
        edit,
      )) {
        assertOccurrenceCount(
          output,
          "arithmetic_expansion",
          fixture.arithmeticCount,
        );
        assertOccurrenceCount(
          output,
          "arithmetic_dynamic_expression",
          fixture.arithmeticCount,
        );
        assertOccurrenceCount(
          output,
          "command_substitution\n",
          fixture.commandCount,
        );
        assertOccurrenceCount(
          output,
          "parameter_expansion",
          count + fixture.extraParameters,
        );
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
        `${operation} ${fixture.name}, ${runtimeFragmentCounts[0] + fixture.extraParameters} to ${runtimeFragmentCounts[1] + fixture.extraParameters} runtime fragments: ${small}ms to ${large}ms (${ratio})`,
      );
    }
  }
});

test("sh: parser resource bounds preserve complete roots and deterministic recovery", () => {
  const completionOnly = { completionOnly: true };
  const arithmeticDepth = 2_000;
  const unmatchedBracketCount = 80_000;
  const bracketMemberCount = 16_000;
  const bracketGroupCount = 4_000;
  const incompleteClassCount = 4_000;
  const continuationCount = 20_000;
  const bodyParameterCount = 16_000;
  const pendingDocumentCount = 600;
  const maximumNestedDocuments = 44;
  const excessDocumentDepth = 300;
  const backquoteDepth = 6;
  const largeValidSources = [
    [
      "deep-arithmetic-expansions",
      `echo ${"$((".repeat(arithmeticDepth)}1${"))".repeat(arithmeticDepth)}\n`,
    ],
    ["unmatched-brackets", `printf ${"[".repeat(unmatchedBracketCount)}\n`],
    ["long-bracket-list", `printf [${"a".repeat(bracketMemberCount)}]\n`],
    ["repeated-brackets", `printf ${"[abc]".repeat(bracketGroupCount)}\n`],
    [
      "repeated-incomplete-special-brackets",
      `printf [${"[:alpha:]".repeat(incompleteClassCount)}\n`,
    ],
    [
      "continued-blank-lines",
      `printf value ${"\\\n".repeat(continuationCount)}after\n`,
    ],
    [
      "continued-pipe-linebreak",
      `first|${"\\\n".repeat(continuationCount)}next\n`,
    ],
    [
      "here-document-dollars",
      `cat <<EOF\n${"$x".repeat(bodyParameterCount)}\nEOF\nafter\n`,
    ],
  ];
  for (const [name, contents] of largeValidSources) {
    assertValid(writeSource(name, contents), name, completionOnly);
  }

  const manyDocuments = writeSource(
    "many-documents",
    `cat${" <<X".repeat(pendingDocumentCount)}\n${"body\nX\n".repeat(pendingDocumentCount)}after\n`,
  );
  assertRepeatedColdParse(
    "resource",
    manyDocuments,
    "many-documents",
    completionOnly,
  );

  function nestedDocuments(depth) {
    let contents = "cat <<D0\n";
    for (let counter = 1; counter < depth; counter += 1) {
      contents += `$(cat <<D${counter}\n`;
    }
    contents += "leaf\n";
    for (let counter = depth - 1; counter >= 0; counter -= 1) {
      contents += `D${counter}\n`;
      if (counter > 0) {
        contents += ")\n";
      }
    }
    return `${contents}after\n`;
  }

  const deepDocuments = writeSource(
    "deep-documents",
    nestedDocuments(maximumNestedDocuments),
  );
  assertValid(deepDocuments, "deep-documents", completionOnly);
  const excessDepth = writeSource(
    "excess-document-depth",
    nestedDocuments(maximumNestedDocuments + 1),
  );
  assert.equal(
    runParse({
      ...completionOnly,
      source: excessDepth,
      mode: "resource",
      description: "nested document metadata exceeds 1024 bytes",
    }).status,
    1,
  );
  assertRepeatedColdParse(
    "resource",
    excessDepth,
    "excess-document-depth",
    completionOnly,
  );
  const boundedDocuments = writeSource(
    "deep-documents-bounded",
    nestedDocuments(excessDocumentDepth),
  );
  assertRepeatedColdParse(
    "resource",
    boundedDocuments,
    "deep-documents-bounded",
    completionOnly,
  );

  let nestedBackquotes = "echo `level1 ";
  for (let level = 2; level <= backquoteDepth; level += 1) {
    nestedBackquotes += `${"\\".repeat(2 ** (level - 1) - 1)}\`level${level} `;
  }
  nestedBackquotes += "leaf";
  for (let level = backquoteDepth; level >= 2; level -= 1) {
    nestedBackquotes += `${"\\".repeat(2 ** (level - 1) - 1)}\` end${level}`;
  }
  nestedBackquotes += "`\n";
  assertValid(
    writeSource("nested-backquotes", nestedBackquotes),
    "nested-backquotes",
    completionOnly,
  );
});
