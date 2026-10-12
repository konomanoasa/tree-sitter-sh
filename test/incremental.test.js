import assert from "node:assert/strict";
import { test } from "node:test";
import {
  assertFreshOutputsEqual,
  cstFingerprint,
  hasRecovery,
  nodeIdentity,
  parseCst,
  sourceByteOffset,
} from "./support/cst.js";
import {
  applyEdits,
  assertIncrementalEqualsFresh,
  lines,
  parseRecoveryAfterEdits,
  runParse,
  writeSource,
} from "./support/parser.js";

test("sh: lexical source keeps its token kind through errors and repairs", () => {
  for (const [name, initial, removed, stray] of [
    [
      "greater-than-before-parenthesis",
      ": > file\necho after\n",
      " file",
      ") file",
    ],
    ["less-than-before-parenthesis", ": <file\necho after\n", "file", ")file"],
    ["missing-then-and-body", "if :; then :; fi\necho after\n", "then :; ", ""],
    ["missing-do-and-body", "while :; do :; done\necho after\n", "do :; ", ""],
    ["empty-if-condition", "if :; then :; fi\necho after\n", ":; ", ""],
    [
      "empty-if-body",
      "if :; then echo body; fi\necho after\n",
      "echo body; ",
      "",
    ],
    ["empty-while-condition", "while :; do :; done\necho after\n", ":; ", ""],
    ["empty-for-body", "for x in a; do :; done\necho after\n", ":; ", ""],
    ["redirect-before-ampersand", "cat > file & echo after\n", "file ", ""],
    [
      "continued-redirect-before-semicolon",
      "cat >\\\nfile; echo after\n",
      "file",
      "",
    ],
    [
      "continued-redirect-before-ampersand",
      "cat > \\\nfile & echo after\n",
      "file ",
      "",
    ],
    [
      "continued-fi-after-missing-body",
      "if :; then :; f\\\ni\necho after\n",
      "then :; ",
      "",
    ],
    [
      "quoted-substitution-missing-body",
      'echo "$(if :; then :; fi)"\necho after\n',
      "then :; ",
      "",
    ],
    [
      "quoted-substitution-stray-semicolon",
      'echo "$(echo inner)"\necho after\n',
      "echo inner",
      "; echo inner",
    ],
    [
      "heredoc-substitution-missing-body",
      "cat <<END\n$(while :; do :; done)\nEND\necho after\n",
      "do :; ",
      "",
    ],
  ]) {
    const offset = initial.indexOf(removed);
    const initialSource = writeSource(`${name}-initial`, initial);
    const brokenText =
      initial.slice(0, offset) + stray + initial.slice(offset + removed.length);
    const broken = writeSource(`${name}-broken`, brokenText);
    const insertion = {
      byte: offset,
      deleteBytes: removed.length,
      insert: stray,
    };
    for (const output of parseRecoveryAfterEdits(
      initialSource,
      broken,
      name,
      insertion,
    )) {
      assert.equal(hasRecovery(output), true, name);
      for (const entry of parseCst(output)) {
        const { type } = nodeIdentity(entry);
        if (type !== '">"' && type !== '"<"') continue;
        const [start, end] = entry.range
          .split("-")
          .map((point) => sourceByteOffset(brokenText, point));
        if (start === end) continue;
        assert.equal(
          brokenText.slice(start, end),
          JSON.parse(type),
          `${name}: ${entry.line}`,
        );
      }
    }
    const final = `${initial.slice(0, offset)} ${initial.slice(offset)}`;
    assertIncrementalEqualsFresh(
      initialSource,
      writeSource(`${name}-repaired`, final),
      `repair-${name}`,
      insertion,
      { byte: offset, deleteBytes: stray.length, insert: ` ${removed}` },
    );
  }
});

const fuzzSeed = 1n;
const fuzzHistoryCount = 100;
const fuzzEditsPerHistory = 5;
const fuzzMaximumFragments = 3;
const fuzzMaximumDeletionBytes = 2;
const randomStateBits = 64;
const randomMultiplier = 6_364_136_223_846_793_005n;
const randomOutputShift = 32n;

function createEditHistoryGenerator() {
  let seed = fuzzSeed;
  function next(maximum) {
    seed = BigInt.asUintN(randomStateBits, seed * randomMultiplier + 1n);
    return Number(seed >> randomOutputShift) % maximum;
  }
  return function* (fragments, insertions, joinSource) {
    for (let iteration = 0; iteration < fuzzHistoryCount; iteration++) {
      const parts = [];
      const count = 1 + next(fuzzMaximumFragments);
      for (let part = 0; part < count; part++) {
        parts.push(fragments[next(fragments.length)]);
      }
      const initial = joinSource(parts);
      let source = Buffer.from(initial);
      const edits = [];
      for (let step = 0; step < fuzzEditsPerHistory; step++) {
        const position = next(source.length + 1);
        const insert = next(2) === 0 || position === source.length;
        const edit = insert
          ? {
              byte: position,
              deleteBytes: 0,
              insert: insertions[next(insertions.length)],
            }
          : {
              byte: position,
              deleteBytes: Math.min(
                next(fuzzMaximumDeletionBytes) + 1,
                source.length - position,
              ),
              insert: "",
            };
        edits.push(edit);
        source = applyEdits(source, [edit]);
        yield {
          initial,
          source,
          edits: [...edits],
          context: `seed ${fuzzSeed}, iteration ${iteration}, source ${JSON.stringify(initial)}, edits ${JSON.stringify(edits)}`,
        };
      }
    }
  };
}

const fuzzFragments = [
  ":",
  "echo value",
  "name=value",
  "echo $name",
  'echo "$name"',
  "echo 'a b'",
  `echo \${name:-value}`,
  "echo $((1 + 2))",
  "echo $(printf x)",
  "echo `printf x`",
  "printf [a-z]",
  "cat <input >output",
  "cat <<EOF\nbody\nEOF",
  "if :; then :; fi",
  "while :; do :; done",
  "for x in a b; do echo $x; done",
  "case x in x) :;; esac",
  "f() { :; }",
  "(echo x)",
  "{ echo x; }",
  "a | b",
  "a && b",
  "a || b",
  "! a",
  "#comment",
  "echo ~user/path",
  "}",
  "if",
  "echo '",
  "echo ${",
  "cat >",
  "echo $(",
  "echo \\\nvalue",
];

const fuzzInsertions = "abcxyz12!{}();,\n\\/*[]().^$|+?-:=# \t'\"<>`";

test("sh: fixed-seed generated histories converge at every valid final source", (context) => {
  const generateHistories = createEditHistoryGenerator();
  let checked = 0;
  let compared = 0;
  for (const history of generateHistories(
    fuzzFragments,
    fuzzInsertions,
    (parts) => lines(...parts),
  )) {
    const initial = writeSource("generated-initial", history.initial);
    const final = writeSource("generated-final", history.source);
    const fresh = runParse({
      description: history.context,
      mode: "recovery",
      source: final,
    });
    assertFreshOutputsEqual(
      history.context,
      fresh,
      runParse({
        description: `${history.context}, repeated fresh parse`,
        mode: "recovery",
        source: final,
      }),
    );
    const incremental = runParse({
      description: history.context,
      mode: "recovery",
      source: initial,
      expectedSource: final,
      edits: history.edits,
    });
    if (fresh.status === 0 && !hasRecovery(fresh.output)) {
      assert.equal(incremental.status, 0, history.context);
      assert.equal(hasRecovery(incremental.output), false, history.context);
      assert.equal(
        cstFingerprint(incremental.output),
        cstFingerprint(fresh.output),
        history.context,
      );
      compared += 1;
    }
    checked += 1;
  }
  assert.equal(checked, fuzzHistoryCount * fuzzEditsPerHistory);
  assert.ok(compared > 0);
  context.diagnostic(
    `sh: checked ${checked} generated edit states, compared ${compared} valid CSTs`,
  );
});
