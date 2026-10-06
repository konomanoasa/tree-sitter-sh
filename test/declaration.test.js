import assert from "node:assert/strict";
import { test } from "node:test";
import { nodeIdentity, sourceByteOffset } from "./support/cst.js";
import {
  assertCstDirectChildRange,
  assertIncrementalEqualsFresh,
  assertNodeCount,
  assertSameLogicalProjection,
  parseCst,
  parseRecoveryAfterEdits,
  parseValidCst,
  writeSource,
} from "./support/parser.js";

function assignmentNames(output, source) {
  const bytes = Buffer.from(source);
  return parseCst(output)
    .filter((entry) => {
      const { field, type } = nodeIdentity(entry);
      return field === "name" && type === "variable_name";
    })
    .map((entry) => {
      const [start, end] = entry.range
        .split("-")
        .map((point) => sourceByteOffset(source, point));
      return bytes.subarray(start, end).toString();
    });
}

test("sh: declaration arguments retain WORD ownership and exact assignment ranges", () => {
  const output = parseValidCst(
    writeSource("declaration-ranges", "readonly x=sh y=\n"),
  );
  assertCstDirectChildRange(
    output,
    "0:8-0:16",
    "suffix: cmd_suffix",
    "0:9-0:13",
    "word: word",
  );
  for (const [range, child] of [
    ["0:9-0:10", "name: variable_name `x`"],
    ["0:10-0:11", '"="'],
    ["0:11-0:13", "value: assignment_value"],
  ]) {
    assertCstDirectChildRange(output, "0:9-0:13", "word: word", range, child);
  }
  assertCstDirectChildRange(
    output,
    "0:14-0:16",
    "word: word",
    "0:14-0:15",
    "name: variable_name `y`",
  );
  assertCstDirectChildRange(
    output,
    "0:14-0:16",
    "word: word",
    "0:15-0:16",
    '"="',
  );
  assertNodeCount(output, "assignment_value", 1);
  assertNodeCount(output, "assignment_word", 0);
});

test("sh: lexical declaration contexts survive ordinary arguments and redirects without escaping their command", () => {
  for (const { name, source, expected } of [
    {
      name: "prefix-and-forwarding",
      source: "p=v command >out command export -p x=sh name y=~\n",
      expected: ["p", "x", "y"],
    },
    {
      name: "redirect-targets",
      source: "export x=1 >file=value y=2 2>other=value z=3\n",
      expected: ["x", "y", "z"],
    },
    {
      name: "redirect-without-spaces",
      source: "command>out export x=1>next y=2\n",
      expected: ["x", "y"],
    },
    {
      name: "here-document-before-utility",
      source: "command <<END export x=1\nbody\nEND\n",
      expected: ["x"],
    },
    {
      name: "here-document-delimiter",
      source: "readonly <<x=y x=1 y=2\nbody\nx=y\n",
      expected: ["x", "y"],
    },
    {
      name: "nested-substitutions",
      source:
        'export outer="$(echo no=value; readonly inner=ok)" after=ok\necho plain=value\n',
      expected: ["outer", "inner", "after"],
    },
    {
      name: "backquotes",
      source: ": `command readonly x=one y=two`\n",
      expected: ["x", "y"],
    },
    {
      name: "nested-backquotes",
      source: ": `echo \\`command export x=one\\``\n",
      expected: ["x"],
    },
    {
      name: "here-document-body-substitution",
      source: "cat <<END\n$(command export x=one)\nEND\n",
      expected: ["x"],
    },
    {
      name: "command-boundaries",
      source: "export x=1 | echo a=2; command echo b=3 && readonly y=4\n",
      expected: ["x", "y"],
    },
    {
      name: "quoted-and-dynamic-names",
      source:
        '"export" a=1; \\readonly b=2; ex"port" c=3; $cmd d=4; command "$cmd" e=5; command "export" f=6\n',
      expected: [],
    },
    {
      name: "forwarding-options-and-ordinary-names",
      source:
        "command -- export a=1; command -p readonly b=2; command -v export c=3; command echo export d=4; readonlyx e=5\n",
      expected: [],
    },
    {
      name: "extension-commands",
      source: "local a=1; declare b=2; typeset c=3; builtin export d=4\n",
      expected: [],
    },
    {
      name: "non-assignment-operands",
      source: 'export \\x=1 "y=2" $name=3 x\\=4 1x=5 x+=6 z=7\n',
      expected: ["z"],
    },
    {
      name: "function-name",
      source: "command() { export x=1; }; readonly() { :; }\n",
      expected: ["x"],
    },
  ]) {
    const output = parseValidCst(writeSource(name, source));
    assert.deepEqual(assignmentNames(output, source), expected, name);
  }
});

test("sh: declaration assignments share value syntax and logical continuations", () => {
  const source = `export x=~/bin:~user y="\${v:-ok}" z=$((1+2)) g=[a-z]*\n`;
  const output = parseValidCst(writeSource("declaration-values", source));
  assert.deepEqual(assignmentNames(output, source), ["x", "y", "z", "g"]);
  assertNodeCount(output, "tilde_expansion", 2);
  assertNodeCount(output, "double_quoted", 1);
  assertNodeCount(output, "parameter_expansion", 1);
  assertNodeCount(output, "arithmetic_expansion", 1);
  assertNodeCount(output, "pattern_bracket_source", 1);
  for (const compact of [
    "command command readonly x=sh y=~/bin\n",
    ": `command export x=sh`\n",
    "cat <<END\n$(command export x=sh)\nEND\n",
  ]) {
    const continued = compact
      .replaceAll("command", "com\\\nmand")
      .replaceAll("export", "ex\\\nport")
      .replaceAll("readonly", "read\\\nonly")
      .replaceAll("x=sh", "x\\\n=sh");
    assertSameLogicalProjection(
      "declaration-continuations",
      parseValidCst(writeSource("declaration-compact", compact)),
      parseValidCst(writeSource("declaration-continued", continued)),
    );
  }
});

test("sh: declaration edits reclassify reused suffixes in both directions", () => {
  for (const {
    name,
    initial,
    byte,
    removed,
    inserted,
    beforeNames,
    afterNames,
  } of [
    {
      name: "command-name",
      initial: "export x=sh >out y=~\n",
      byte: 0,
      removed: "export",
      inserted: "printf",
      beforeNames: ["x", "y"],
      afterNames: [],
    },
    {
      name: "command-option",
      initial: "command command export x=sh\n",
      byte: 8,
      removed: "",
      inserted: "-- ",
      beforeNames: ["x"],
      afterNames: [],
    },
    {
      name: "quoted-utility",
      initial: "command export x=sh\n",
      byte: 8,
      removed: "export",
      inserted: '"export"',
      beforeNames: ["x"],
      afterNames: [],
    },
    {
      name: "assignment-name",
      initial: "readonly x=sh y=~\n",
      byte: 9,
      removed: "x",
      inserted: "1x",
      beforeNames: ["x", "y"],
      afterNames: ["y"],
    },
    {
      name: "assignment-equals",
      initial: "readonly x=sh y=~\n",
      byte: 10,
      removed: "=",
      inserted: "",
      beforeNames: ["x", "y"],
      afterNames: ["y"],
    },
    {
      name: "nested-command",
      initial: 'readonly x="$(echo a=b)" y=~\n',
      byte: 14,
      removed: "echo",
      inserted: "export",
      beforeNames: ["x", "y"],
      afterNames: ["x", "a", "y"],
    },
  ]) {
    assert.equal(initial.slice(byte, byte + removed.length), removed, name);
    const final =
      initial.slice(0, byte) + inserted + initial.slice(byte + removed.length);
    const initialSource = writeSource(`${name}-before`, initial);
    const finalSource = writeSource(`${name}-after`, final);
    for (const [before, after, text, deleteBytes, insert, expected] of [
      [initialSource, finalSource, final, removed.length, inserted, afterNames],
      [
        finalSource,
        initialSource,
        initial,
        inserted.length,
        removed,
        beforeNames,
      ],
    ]) {
      for (const output of assertIncrementalEqualsFresh(before, after, name, {
        byte,
        deleteBytes,
        insert,
      })) {
        assert.deepEqual(assignmentNames(output, text), expected, name);
      }
    }
  }
});

test("sh: declaration quotes and redirects recover to the fresh CST after repair", () => {
  for (const { source, removed } of [
    { source: 'readonly x="value" y=~\n', removed: '" y=~' },
    { source: "command >out export x=1\n", removed: "out" },
    { source: "export x=$(readonly y=1) z=2\n", removed: ") z=2" },
  ]) {
    const byte = source.indexOf(removed);
    const broken = source.slice(0, byte) + source.slice(byte + removed.length);
    const initial = writeSource("declaration-repair-initial", source);
    const deletion = { byte, deleteBytes: removed.length, insert: "" };
    parseRecoveryAfterEdits(
      initial,
      writeSource("declaration-broken", broken),
      removed,
      deletion,
    );
    assertIncrementalEqualsFresh(initial, initial, removed, deletion, {
      byte,
      deleteBytes: 0,
      insert: removed,
    });
  }
});
