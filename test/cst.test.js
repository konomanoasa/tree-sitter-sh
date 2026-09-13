import assert from "node:assert/strict";
import { test } from "node:test";
import {
  assertCstSourceContract,
  continuationManifest,
  cstFingerprint,
  logicalProjection,
  parseCst,
} from "./support/cst.js";

const slash = JSON.stringify("\\");

test("CST projection preserves punctuation, fields, hierarchy and cardinality", () => {
  const source = [
    "0:0 - 0:3   program",
    "0:0 - 0:3     body: list",
    "0:0 - 0:1       word `a`",
    '0:1 - 0:2       ";"',
    "0:2 - 0:3       word `b`",
  ].join("\n");
  for (const changed of [
    source.replace('";"', '"&"'),
    source.replace("body: list", "commands: list"),
    source.replace("       word `b`", "     word `b`"),
    `${source}\n0:2 - 0:3       word`,
  ]) {
    assert.notEqual(logicalProjection(changed), logicalProjection(source));
    assert.notEqual(cstFingerprint(changed), cstFingerprint(source));
  }
  assert.equal(
    logicalProjection(source.replace("word `a`", "word")),
    logicalProjection(source),
  );
});

test("CST fingerprints preserve individual slash ranges and order without ownership", () => {
  const root = "0:0 - 2:3   program";
  const word = "0:0 - 2:3     word";
  const first = `0:3 - 0:4       ${slash}`;
  const second = `1:0 - 1:1       ${slash}`;
  const source = [root, word, first, second].join("\n");
  const expected = cstFingerprint(source);
  assert.equal(
    cstFingerprint(
      [root, word, first.replace("       ", "     leading: "), second].join(
        "\n",
      ),
    ),
    expected,
  );
  assert.equal(
    logicalProjection(source),
    logicalProjection([root, word].join("\n")),
  );
  assert.deepEqual(continuationManifest(source), ["0:3-0:4", "1:0-1:1"]);
  for (const changed of [
    [root, word, first],
    [root, word, first, first, second],
    [root, word, second, first],
    [root, word, first, second.replace("1:1", "1:2")],
    [root.replace("2:3", "3:0"), word, first, second],
  ])
    assert.notEqual(cstFingerprint(changed.join("\n")), expected);
});

test("CST parsing ignores multiline source display fragments", () => {
  const output = [
    "0:0 - 2:0   program",
    "0:0 - 1:1     single_quote_content `a",
    "1:0 - 1:1       `b`",
  ].join("\n");
  assert.deepEqual(
    parseCst(output).map(({ content }) => content),
    ["program", "single_quote_content `a"],
  );
});

test("CST projection retains text containing Unicode line and paragraph separators", () => {
  const output = [
    "0:0 - 0:9   program",
    "0:0 - 0:9     word: literal `a\u2028b\u2029c`",
  ].join("\n");
  assert.deepEqual(
    parseCst(output).map(({ content, range }) => [content, range]),
    [
      ["program", "0:0-0:9"],
      ["word: literal `a\u2028b\u2029c`", "0:0-0:9"],
    ],
  );
  assert.deepEqual(JSON.parse(logicalProjection(output)), [
    [0, null, "program", false],
    [2, "word", "literal", false],
  ]);
});

test("CST projection distinguishes recovery markers from literal bullets", () => {
  const output = ["0:0 - 0:3   program", "0:0 - 0:3     literal `•`"].join(
    "\n",
  );
  assert.deepEqual(JSON.parse(logicalProjection(output)), [
    [0, null, "program", false],
    [2, null, "literal", false],
  ]);
  assert.deepEqual(
    JSON.parse(
      logicalProjection(output.replace("     literal `•`", "    •ERROR")),
    ),
    [
      [0, null, "program", false],
      [2, null, "ERROR", true],
    ],
  );
});

test("lexical nodes expose only real one-byte continuation children", () => {
  const source = "foo\\\nbar";
  const output = [
    "0:0 - 1:3   program",
    "0:0 - 1:3     literal",
    `0:3 - 0:4       ${slash}`,
  ].join("\n");
  assertCstSourceContract(output, source);
  for (const changed of [
    output.replace("0:3 - 0:4", "0:3 - 1:0"),
    `0:2 - 0:3     "'"`,
    `${output}\n0:3 - 0:4       ${slash}`,
    `${output}\n0:0 - 0:3       literal`,
    `${output}\n0:4 - 1:0       "\\n"`,
    `${output}\n0:3 - 0:4         literal`,
  ])
    assert.throws(() => assertCstSourceContract(changed, source));
  assert.throws(() => assertCstSourceContract(output, "foo\\xbar"));
});

test("source contracts use UTF-8 byte coordinates", () => {
  assertCstSourceContract(
    [
      "0:0 - 1:2   program",
      "0:0 - 1:2     literal",
      `0:6 - 0:7       ${slash}`,
    ].join("\n"),
    "é🙂\\\né",
  );
});

test("anonymous punctuation and unclassified numeric digits retain exact source", () => {
  assertCstSourceContract(
    ["0:0 - 0:1   program", '0:0 - 0:1     "\\`"'].join("\n"),
    "`",
  );
  const output = [
    "0:0 - 0:8   program",
    '0:0 - 0:1     "$"',
    `0:1 - 0:2     "'"`,
    `0:2 - 0:3     "'"`,
    '0:3 - 0:4     "$"',
    '0:4 - 0:5     "{"',
    '0:5 - 0:6     "numeric_parameter_source"',
    '0:6 - 0:7     "numeric_parameter_source"',
    '0:7 - 0:8     "}"',
  ].join("\n");
  const source = `$''\${00}`;
  assertCstSourceContract(output, source);
  for (const changed of [
    output.replace("0:0 - 0:1", "0:0 - 0:2"),
    output.replace("0:5 - 0:6", "0:5 - 0:7"),
    output.replace('"{"', '"("'),
  ])
    assert.throws(() => assertCstSourceContract(changed, source));
});
