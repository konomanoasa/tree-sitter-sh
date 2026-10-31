import assert from "node:assert/strict";
import { test } from "node:test";
import {
  assertCstDirectChildRange,
  assertOccurrenceCount,
  cstFingerprint,
  hasRecovery,
} from "./support/cst.js";
import {
  applyEdits,
  compareIncrementalAndFresh,
  parseRecoveryAfterEdits,
  runParse,
  writeSource,
} from "./support/parser.js";
import { editingWorkloads } from "./support/workloads.js";

for (const { name, source, edit } of [
  {
    name: "Unicode content changes rows and byte columns",
    source: "printf '終😀'\necho after\n",
    edit: { byte: 8, deleteBytes: 7, insert: "é\n行" },
  },
  {
    name: "zero parameter becomes positional",
    source: `echo \${00}\n`,
    edit: { byte: 8, deleteBytes: 1, insert: "1" },
  },
  {
    name: "zero parameter gains a continuation",
    source: `echo \${00}\n`,
    edit: { byte: 8, deleteBytes: 0, insert: "\\\n" },
  },
  {
    name: "EOF backslash gains an escaped character",
    source: "echo abc\\",
    edit: { byte: 9, deleteBytes: 0, insert: "x" },
  },
  {
    name: "EOF backslash becomes a line continuation",
    source: "echo abc\\",
    edit: { byte: 9, deleteBytes: 0, insert: "\nnext\n" },
  },
  {
    name: "EOF backslash after a continuation gains an escaped character",
    source: "echo \\\n\\",
    edit: { byte: 8, deleteBytes: 0, insert: "x" },
  },
]) {
  test(`sh: ${name} matches fresh parsing in both directions`, () => {
    const initial = writeSource(name, source);
    const final = writeSource(`${name}-edited`, applyEdits(source, [edit]));
    const reverse = {
      byte: edit.byte,
      deleteBytes: Buffer.byteLength(edit.insert),
      insert: Buffer.from(source)
        .subarray(edit.byte, edit.byte + edit.deleteBytes)
        .toString(),
    };
    compareIncrementalAndFresh({}, initial, final, name, edit);
    compareIncrementalAndFresh({}, final, initial, `${name}: reverse`, reverse);
    compareIncrementalAndFresh(
      {},
      initial,
      initial,
      `${name}: round trip`,
      edit,
      reverse,
    );
  });
}

const nestedDocuments =
  "<<ZZQ\n$(<<E\nE\nuntil(z);do>f;x&(<<'E'\nE\nf ) done)\nZZQ\n";

for (const { name, source: contents, byte } of [
  { name: "at EOF", source: nestedDocuments, byte: 5 },
  {
    name: "before trailing layout",
    source: `${nestedDocuments} \t# trailing\n\n`,
    byte: 5,
  },
  {
    name: "before another command",
    source: `${nestedDocuments}echo after\n`,
    byte: 5,
  },
  { name: "in a subshell", source: `(\n${nestedDocuments})\n`, byte: 7 },
  { name: "in a function", source: `f() {\n${nestedDocuments}}\n`, byte: 11 },
  {
    name: "in a command substitution",
    source: `echo "$(\n${nestedDocuments})"\n`,
    byte: 14,
  },
  {
    name: "in a backquote substitution",
    source: `echo \`\n${nestedDocuments}\`\n`,
    byte: 12,
  },
  {
    name: "in another here-document",
    source: `cat <<OUT\n$(\n${nestedDocuments})\nOUT\n`,
    byte: 18,
  },
  {
    name: "with a quoted body",
    source: "cat <<'OUT'\n$(if :; then printf x; fi)\nOUT\n",
    byte: 11,
  },
  {
    name: "with leading tabs removed",
    source: "cat <<-OUT\n\t$(printf x)\n\tOUT\n",
    byte: 10,
  },
  {
    name: "with queued quoted and unquoted bodies",
    source: "cat <<'A' <<B\n$(printf first)\nA\n$(printf second)\nB\n",
    byte: 13,
  },
  { name: "with an empty body", source: "cat <<OUT\nOUT\n", byte: 9 },
]) {
  test(`sh: here-document layout ${name} matches fresh parsing after terminator edits`, () => {
    const insertion = { byte, deleteBytes: 0, insert: ";" };
    const deletion = { byte, deleteBytes: 1, insert: "" };
    const initial = writeSource("here-document-layout-initial", contents);
    const edited = writeSource(
      "here-document-layout-edited",
      applyEdits(contents, [insertion]),
    );
    for (const { phase, from, to, edits } of [
      { phase: "insert", from: initial, to: edited, edits: [insertion] },
      { phase: "remove", from: edited, to: initial, edits: [deletion] },
      {
        phase: "round trip",
        from: initial,
        to: initial,
        edits: [insertion, deletion],
      },
    ]) {
      compareIncrementalAndFresh({}, from, to, `${name}: ${phase}`, ...edits);
    }
  });
}

test("sh: appending and removing a command preserves here-document layout ownership", () => {
  const insertion = {
    byte: nestedDocuments.length,
    deleteBytes: 0,
    insert: "echo after\n",
  };
  const deletion = {
    byte: nestedDocuments.length,
    deleteBytes: insertion.insert.length,
    insert: "",
  };
  const initial = writeSource("trailing-here-document", nestedDocuments);
  const edited = writeSource(
    "separator-here-document",
    applyEdits(nestedDocuments, [insertion]),
  );
  for (const { phase, from, to, edits } of [
    { phase: "append", from: initial, to: edited, edits: [insertion] },
    { phase: "remove", from: edited, to: initial, edits: [deletion] },
    {
      phase: "round trip",
      from: initial,
      to: initial,
      edits: [insertion, deletion],
    },
  ]) {
    compareIncrementalAndFresh({}, from, to, phase, ...edits);
  }
});

test("sh: fixed-seed edit histories restore fresh CSTs after repairs", () => {
  const seed = 1n;
  const historiesPerInput = 2;
  const forwardEditCount = 2;
  const maximumDeletionBytes = 2;
  const randomStateBits = 64;
  const randomMultiplier = 6_364_136_223_846_793_005n;
  const randomOutputShift = 32n;
  const inputs = [
    `echo x\\\nvalue "\${v:-$(printf x)}"\n`,
    `echo $((1 + \${v:-2}))\n`,
    "echo `echo \\`echo inner\\``\n",
    "cat <<OUT\n$(cat <<'IN'\nbody\nIN\n)\nOUT\n",
    "case x in [a-z]) printf '%s' \"$x\";; esac\n",
    "f() { if :; then echo ok; fi; } >output\n",
  ];
  const insertions = ["x", " ", "\n", "\\", "'", '"', "$", "(", ")", "}"];
  let random = seed;
  function next(maximum) {
    random = BigInt.asUintN(randomStateBits, random * randomMultiplier + 1n);
    return Number(random >> randomOutputShift) % maximum;
  }
  for (const [index, contents] of inputs.entries()) {
    const initial = writeSource("generated-initial", contents);
    for (let history = 0; history < historiesPerInput; history++) {
      let source = Buffer.from(contents);
      const edits = [];
      const reversals = [];
      for (let step = 0; step < forwardEditCount * 2; step++) {
        let edit;
        if (step < forwardEditCount) {
          const byte = next(source.length + 1);
          const deleteBytes = Math.min(
            next(maximumDeletionBytes + 1),
            source.length - byte,
          );
          const insert = insertions[next(insertions.length)];
          edit = { byte, deleteBytes, insert };
          reversals.push({
            byte,
            deleteBytes: Buffer.byteLength(insert),
            insert: source.subarray(byte, byte + deleteBytes).toString(),
          });
        } else {
          edit = reversals.pop();
        }
        edits.push(edit);
        source = applyEdits(source, [edit]);
        const final = writeSource("generated-final", source);
        const label = `seed ${seed}, input ${index}, history ${history}, step ${step}`;
        const results = compareIncrementalAndFresh(
          { mode: "recovery" },
          initial,
          final,
          label,
          ...edits,
        );
        if (step === forwardEditCount * 2 - 1) {
          for (const result of results) {
            assert.equal(result.status, 0, label);
            assert.equal(result.recovery, false, label);
          }
        }
      }
      assert.deepEqual(source, Buffer.from(contents));
    }
  }
});

for (const {
  name,
  source: contents,
  edit,
  recovery,
  reuse,
  counts = [],
} of editingWorkloads()) {
  test(`sh: ${name} preserves structure after editing`, () => {
    const source = writeSource(`${name}-initial`, contents);
    const final = writeSource(
      `${name}-edited`,
      applyEdits(Buffer.from(contents), [edit]),
    );
    if (recovery) {
      parseRecoveryAfterEdits(source, final, name, edit);
      return;
    }
    const results = compareIncrementalAndFresh(
      { debug: reuse },
      source,
      final,
      name,
      edit,
    );
    if (reuse)
      assert.match(
        results[0].debugOutput,
        /^reuse_node symbol:complete_command/m,
        name,
      );
    for (const result of results) {
      for (const [node, count] of counts)
        assertOccurrenceCount(result.output, node, count);
    }
  });
}

test("sh: every byte inside a Unicode payload can be deleted and repaired through an edit history", () => {
  const prefix = "printf '";
  const suffix = "' after\n";
  const payload = "é😀";
  const source = prefix + payload + suffix;
  const changed = `${prefix}x${suffix}`;
  const payloadByte = Buffer.byteLength(prefix);
  const payloadBytes = Buffer.byteLength(payload);
  const initial = writeSource("utf8-history-initial", source);
  const brokenPayloads = [
    [0xa9, 0xf0, 0x9f, 0x98, 0x80],
    [0xc3, 0xf0, 0x9f, 0x98, 0x80],
    [0xc3, 0xa9, 0x9f, 0x98, 0x80],
    [0xc3, 0xa9, 0xf0, 0x98, 0x80],
    [0xc3, 0xa9, 0xf0, 0x9f, 0x80],
    [0xc3, 0xa9, 0xf0, 0x9f, 0x98],
  ];
  for (const [removedByte, brokenPayload] of brokenPayloads.entries()) {
    const edits = [
      { byte: payloadByte + removedByte, deleteBytes: 1, insert: "" },
      { byte: payloadByte, deleteBytes: payloadBytes - 1, insert: payload },
      { byte: payloadByte, deleteBytes: payloadBytes, insert: "x" },
      { byte: payloadByte, deleteBytes: 1, insert: payload },
    ];
    const expected = [
      Buffer.concat([
        Buffer.from(prefix),
        Buffer.from(brokenPayload),
        Buffer.from(suffix),
      ]),
      Buffer.from(source),
      Buffer.from(changed),
      Buffer.from(source),
    ];
    for (const [step, expectedSource] of expected.entries()) {
      const history = edits.slice(0, step + 1);
      const label = `UTF-8 byte ${removedByte}, edit ${step + 1}`;
      assert.deepEqual(applyEdits(source, history), expectedSource, label);
      const final = writeSource("utf8-history-expected", expectedSource);
      const fresh = runParse({
        source: final,
        mode: "recovery",
        description: label,
      });
      const incremental = runParse({
        source: initial,
        expectedSource: final,
        edits: history,
        mode: "recovery",
        description: label,
      });
      if (step === 0) continue;
      for (const result of [fresh, incremental]) {
        assert.equal(result.status, 0, label);
        assert.equal(hasRecovery(result.output), false, label);
        const contents = step === 2 ? "x" : payload;
        const end = payloadByte + Buffer.byteLength(contents);
        assertCstDirectChildRange(
          result.output,
          `0:${payloadByte - 1}-0:${end + 1}`,
          "single_quoted",
          `0:${payloadByte}-0:${end}`,
          `single_quote_content \`${contents}\``,
        );
      }
      assert.equal(
        cstFingerprint(incremental.output),
        cstFingerprint(fresh.output),
        label,
      );
    }
  }
});
