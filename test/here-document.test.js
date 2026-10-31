import assert from "node:assert/strict";
import { test } from "node:test";
import {
  assertCstDirectChildRange,
  assertCstRange,
  assertNodeCount,
  assertNotContains,
  assertOccurrenceCount,
  continuationManifest,
  hasRecovery,
  lineContinuationManifest,
} from "./support/cst.js";
import {
  assertIncrementalEqualsFresh,
  assertRepeatedColdParse,
  lines,
  parseRecoveryAfterEdits,
  parseValidCst,
  runParse,
  runQuery,
  writeSource,
} from "./support/parser.js";

for (const [name, word, delimiter, quoted] of [
  ["empty parameter", `\${}`, `\${}`, false],
  ["parameter suffix", `\${a.b}`, `\${a.b}`, false],
  ["non-ASCII parameter suffix", `\${aé}`, `\${aé}`, false],
  ["length suffix", `\${#x:}`, `\${#x:}`, false],
  ["invalid initial byte", `\${.}`, `\${}`, false],
  ["invalid length byte", `\${#.}`, `\${#}`, false],
  ["parameter blank", `\${a b}`, `\${a b}`, false],
  ["unfinished command", "$(if)", "$(if)", false],
  ["command separator", "$(;)", "$(;)", false],
  ["unfinished pipeline", "$(a|)", "$(a|)", false],
  ["unfinished backquote command", "`if`", "`if`", false],
  ["empty arithmetic", "$(())", "$(())", false],
  ["unfinished arithmetic", "$((1+))", "$((1+))", false],
  ["command-like arithmetic", "$((: $x))", "$((: $x))", false],
  ["arithmetic parentheses", "$(((1+)))", "$(((1+)))", false],
  ["quoted invalid suffix", `\${a.b:-"x"}`, `\${a.b:-x}`, true],
]) {
  for (const [context, prefix, suffix, tabs, outerQuote] of [
    ["plain", "cat <<", "", "", ""],
    ["quoted", 'cat <<"', '"', "", '"'],
    ["tab-stripped", "cat <<-", "", "\t", ""],
    ["substitution", "echo $(cat <<", "", "", ""],
  ]) {
    test(`sh: unexpanded delimiter ${name} retains source and bounds the ${context} body`, () => {
      const isQuoted = quoted || outerQuote !== "";
      const body = isQuoted ? `\${x:bad}` : "$probe";
      const source = `${prefix}${word}${suffix}\n${tabs}${body}\n${tabs}${delimiter}\n${context === "substitution" ? ")\n" : ""}echo AFTER\n`;
      const output = parseValidCst(
        writeSource(`unexpanded-${name}-${context}`, source),
      );
      const start = Buffer.byteLength(prefix) - outerQuote.length;
      const end = Buffer.byteLength(prefix + word + suffix);
      assertCstRange(output, `0:${start}-0:${end}`, "here_end_text");
      assertCstRange(output, "2:0-3:0", "here_document_end");
      assertNodeCount(output, "parameter_expansion", isQuoted ? 0 : 1);
      assertNodeCount(output, "arithmetic_expansion", 0);
      assertNodeCount(output, "backquote_substitution", 0);
      assertNodeCount(
        output,
        "command_substitution",
        context === "substitution" ? 1 : 0,
      );
      assertNodeCount(output, "here_end_text", 1);
    });
  }
}

test("sh: editing an unexpanded parameter delimiter preserves matching and body expansion", () => {
  for (const quote of ["", '"']) {
    const initialText = `cat <<${quote}\${x}${quote}\n$probe\n\${x}\necho AFTER\n`;
    const finalText = initialText.replaceAll(`\${x}`, `\${}`);
    const initial = writeSource("named-parameter-delimiter", initialText);
    const final = writeSource("empty-parameter-delimiter", finalText);
    const edits = [
      initialText.lastIndexOf("x}"),
      initialText.indexOf("x}"),
    ].map((byte) => ({ byte, deleteBytes: 1, insert: "" }));
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      "empty-delimiter-parameter",
      ...edits,
    )) {
      assertCstRange(output, "2:0-3:0", "here_document_end");
      assertNodeCount(output, "parameter_expansion", quote === "" ? 1 : 0);
      assertNodeCount(output, "here_end_text", 1);
    }
  }
});

test("sh: C delimiter bytes preserve an unmatched quoted body through EOF", () => {
  const source = `cat <<"\${é}"\n\${x:bad}\n\${é}\necho AFTER\n`;
  const output = parseValidCst(writeSource("c-delimiter-bytes", source));
  assertCstRange(output, "0:6-0:13", "here_end_text");
  assertCstRange(output, "1:0-4:0", "quoted_here_document_body");
  assertNodeCount(output, "here_document_end", 0);
  assertNodeCount(output, "parameter_expansion", 0);
});

test("sh: an empty parameter delimiter is retained inside a backquote command", () => {
  const source = `: \`cat <<\${}\n$probe\n\${}\n\`\necho AFTER\n`;
  const output = parseValidCst(
    writeSource("backquote-empty-parameter-delimiter", source),
  );
  assertCstRange(output, "0:9-0:12", "here_end_text");
  assertCstRange(output, "2:0-3:0", "here_document_end");
  assertNodeCount(output, "parameter_expansion", 1);
});

test("sh: arithmetic delimiter quote edits preserve matching and body expansion", () => {
  for (const outer of ["", '"']) {
    const initialText = `cat <<${outer}$((1+2))${outer}\n$x\n$((1+2))\necho after\n`;
    const initial = writeSource(
      "arithmetic-delimiter-without-inner-quotes",
      initialText,
    );
    for (const quote of ["'", '"']) {
      const replacement = `${quote}2${quote}`;
      const finalText = initialText.replaceAll("2", replacement);
      const final = writeSource(
        "arithmetic-delimiter-with-inner-quotes",
        finalText,
      );
      const positions = [
        initialText.lastIndexOf("2"),
        initialText.indexOf("2"),
      ];
      for (const output of assertIncrementalEqualsFresh(
        initial,
        final,
        "insert-arithmetic-delimiter-quotes",
        ...positions.map((byte) => ({
          byte,
          deleteBytes: 1,
          insert: replacement,
        })),
      )) {
        assertNodeCount(output, "here_end_text", 1);
        assertNodeCount(output, "parameter_expansion", outer === "" ? 1 : 0);
        assertCstRange(output, "2:0-3:0", "end: here_document_end");
        assertCstRange(output, "3:0-3:10", "command: complete_command");
      }
      assertIncrementalEqualsFresh(
        final,
        initial,
        "remove-arithmetic-delimiter-quotes",
        ...[
          finalText.lastIndexOf(replacement),
          finalText.indexOf(replacement),
        ].map((byte) => ({
          byte,
          deleteBytes: replacement.length,
          insert: "2",
        })),
      );
    }
  }
});

for (const { name, source, suffix, endingCount, initialEndings = 0 } of [
  {
    name: "named",
    source: "cat <<E\nx\nE",
    suffix: "\n",
    endingCount: 1,
    initialEndings: 1,
  },
  {
    name: "quoted",
    source: "cat <<'E'\nx\nE",
    suffix: "\n",
    endingCount: 1,
    initialEndings: 1,
  },
  { name: "empty", source: "cat <<''\n", suffix: "\n", endingCount: 1 },
  { name: "empty tabs", source: "cat <<-''\n\t", suffix: "\n", endingCount: 1 },
  {
    name: "named tabs",
    source: "cat <<-E\nx\n\tE",
    suffix: "\n",
    endingCount: 1,
    initialEndings: 1,
  },
  {
    name: "empty after body",
    source: "cat <<''\nx\n",
    suffix: "\n",
    endingCount: 1,
  },
  {
    name: "continued delimiter",
    source: "cat <<E\nx\nE\\\n",
    suffix: "\n",
    endingCount: 1,
    initialEndings: 1,
  },
  {
    name: "body without newline",
    source: "cat <<E\nx",
    suffix: "\nE\necho after\n",
    endingCount: 1,
  },
  {
    name: "queued named bodies",
    source: "cat <<A <<B\nx",
    suffix: "\nA\ny\nB\n",
    endingCount: 2,
  },
  {
    name: "queued empty bodies",
    source: "cat <<'' <<''\n",
    suffix: "\n\n",
    endingCount: 2,
  },
  {
    name: "unstarted empty delimiter",
    source: "cat <<''",
    suffix: "\nbody\n\n",
    endingCount: 1,
  },
  {
    name: "unstarted continued header",
    source: "cat <<E \\\n",
    suffix: "\nbody\nE\n",
    endingCount: 1,
  },
  {
    name: "unstarted following command",
    source: "cat <<E; echo after",
    suffix: "\nbody\nE\n",
    endingCount: 1,
  },
  {
    name: "unstarted declaration",
    source: "cat <<E",
    suffix: "\nx\nE\n",
    endingCount: 1,
  },
  {
    name: "unstarted declaration with trailing backslash",
    source: "cat <<E\\",
    suffix: "\n\nx\nE\n",
    endingCount: 1,
  },
  {
    name: "unstarted quoted queue with comment",
    source: "cat <<'A' <<-B # pending",
    suffix: "\nx\nA\ny\n\tB\n",
    endingCount: 2,
  },
]) {
  test(`sh: ${name} switches between EOF and real here-document endings after edits`, () => {
    const initial = writeSource(`${name}-eof-body`, source);
    const final = writeSource(`${name}-terminated-body`, source + suffix);
    const append = { byte: source.length, deleteBytes: 0, insert: suffix };
    const remove = {
      byte: source.length,
      deleteBytes: suffix.length,
      insert: "",
    };
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      `${name}: append terminators`,
      append,
    )) {
      assertNodeCount(output, "here_document_end", endingCount);
    }
    for (const output of assertIncrementalEqualsFresh(
      final,
      initial,
      `${name}: remove terminators`,
      remove,
    )) {
      assertNodeCount(output, "here_document_end", initialEndings);
    }
    assertIncrementalEqualsFresh(
      initial,
      initial,
      `${name}: round trip`,
      append,
      remove,
    );
  });
}

for (const { name, prefix, suffix = "`\necho after\n", ending, count = 1 } of [
  { name: "unquoted body", prefix: "x=`cat <<E\nBODY", ending: "\nE\n" },
  { name: "quoted body", prefix: "x=`cat <<'E'\n${x", ending: "\nE\n" },
  { name: "empty delimiter", prefix: "x=`cat <<''\nBODY", ending: "\n\n" },
  { name: "empty body", prefix: "x=`cat <<''\n", ending: "\n" },
  { name: "empty tabs", prefix: "x=`cat <<-''\n\t", ending: "\n" },
  { name: "empty after body", prefix: "x=`cat <<''\nx\n", ending: "\n" },
  {
    name: "queued bodies",
    prefix: "x=`cat <<A <<B\nFIRST",
    ending: "\nA\nSECOND\nB\n",
    count: 2,
  },
  { name: "delimiter without newline", prefix: "x=`cat <<E\nE", ending: "\n" },
  {
    name: "quoted delimiter without newline",
    prefix: "x=`cat <<'E'\nx\nE",
    ending: "\n",
  },
  {
    name: "bracket",
    prefix: "echo [`cat <<E\nBODY",
    suffix: "`]\necho after\n",
    ending: "\nE\n",
  },
  {
    name: "arithmetic",
    prefix: "echo $((`cat <<E\nBODY",
    suffix: "`+1))\necho after\n",
    ending: "\nE\n",
  },
  {
    name: "nested backquote",
    prefix: ": `echo \\`cat <<E\nBODY",
    suffix: "\\``\necho after\n",
    ending: "\nE\n",
  },
]) {
  test(`sh: ${name} switches between backquote and real here-document endings`, () => {
    const initial = writeSource(`${name}-backquote-body`, prefix + suffix);
    const final = writeSource(
      `${name}-backquote-ending`,
      prefix + ending + suffix,
    );
    const insert = { byte: prefix.length, deleteBytes: 0, insert: ending };
    const remove = {
      byte: prefix.length,
      deleteBytes: ending.length,
      insert: "",
    };
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      name,
      insert,
    )) {
      assertNodeCount(output, "here_document_end", count);
    }
    for (const output of assertIncrementalEqualsFresh(
      final,
      initial,
      `${name}-undo`,
      remove,
    )) {
      assertNodeCount(output, "here_document_end", 0);
      const row = (prefix + suffix).split("\n").length - 2;
      assertCstRange(output, `${row}:0-${row}:10`, "command: complete_command");
      if (name === "delimiter without newline") {
        assertCstRange(output, "1:0-1:1", "here_document_text `E`");
      }
    }
    assertIncrementalEqualsFresh(
      initial,
      initial,
      `${name}-roundtrip`,
      insert,
      remove,
    );
  });
}

test("sh: a backquote ending does not complete a parameter inside an expanded here-document", () => {
  const quoted = writeSource(
    "backquote-quoted-unclosed-parameter",
    "x=`cat <<'E'\n${x`\necho after\n",
  );
  const expanded = writeSource(
    "backquote-expanded-unclosed-parameter",
    "x=`cat <<E\n${x`\necho after\n",
  );
  const unquote = { byte: 9, deleteBytes: 3, insert: "E" };
  for (const output of parseRecoveryAfterEdits(
    quoted,
    expanded,
    "unquote-backquote-body",
    unquote,
  )) {
    assert.ok(hasRecovery(output));
  }
  assertIncrementalEqualsFresh(expanded, quoted, "quote-backquote-body", {
    byte: 9,
    deleteBytes: 1,
    insert: "'E'",
  });
});

test("sh: nested here-documents activate at the scanner state capacity", () => {
  const nestedDelimiterBytes = 251;
  const capacityDelimiterBytes = 466;
  const belowCapacityBytes = capacityDelimiterBytes - 2;
  const aboveCapacityBytes = capacityDelimiterBytes + 1;
  function source(outerLength) {
    const outer = "A".repeat(outerLength);
    const first = "B".repeat(nestedDelimiterBytes);
    const second = "C".repeat(nestedDelimiterBytes);
    return lines(
      `cat <<${outer}`,
      `$(cat <<${first} <<${second}`,
      "b",
      first,
      "c",
      second,
      ")",
      outer,
      "after",
    );
  }

  const below = writeSource(
    "nested-documents-below-capacity",
    source(belowCapacityBytes),
  );
  const exact = writeSource(
    "nested-documents-at-capacity",
    source(capacityDelimiterBytes),
  );
  const above = writeSource(
    "nested-documents-above-capacity",
    source(aboveCapacityBytes),
  );
  const outputs = [parseValidCst(exact)];
  outputs.push(
    ...assertIncrementalEqualsFresh(
      below,
      exact,
      "grow-nested-document-metadata-to-capacity",
      {
        byte: source(belowCapacityBytes).lastIndexOf("\nA") + 1,
        deleteBytes: 0,
        insert: "AA",
      },
      { byte: 6, deleteBytes: 0, insert: "AA" },
    ),
    ...assertIncrementalEqualsFresh(
      above,
      exact,
      "restore-nested-document-metadata-within-capacity",
      {
        byte: source(aboveCapacityBytes).lastIndexOf("\nA") + 1,
        deleteBytes: 1,
        insert: "",
      },
      { byte: 6, deleteBytes: 1, insert: "" },
    ),
  );
  for (const output of outputs) {
    assertOccurrenceCount(output, "document: here_document", 3);
    assertCstRange(output, "0:6-0:472", "end: here_end");
    assertCstRange(output, "1:8-1:259", "end: here_end");
    assertCstRange(output, "1:262-1:513", "end: here_end");
    assertCstRange(output, "1:0-7:0", "body: here_document_body");
    assertCstRange(output, "2:0-3:0", "body: here_document_body");
    assertCstRange(output, "4:0-5:0", "body: here_document_body");
    assertCstRange(output, "3:0-4:0", "end: here_document_end");
    assertCstRange(output, "5:0-6:0", "end: here_document_end");
    assertCstRange(output, "7:0-8:0", "end: here_document_end");
    assertCstRange(output, "8:0-8:5", "literal `after`");
  }
  assertIncrementalEqualsFresh(
    exact,
    below,
    "shrink-nested-document-metadata-below-capacity",
    {
      byte: source(capacityDelimiterBytes).lastIndexOf("\nA") + 1,
      deleteBytes: 2,
      insert: "",
    },
    { byte: 6, deleteBytes: 2, insert: "" },
  );
  assert.equal(
    runParse({
      source: above,
      mode: "resource",
      description: "1025-byte state",
    }).status,
    1,
  );
  assertRepeatedColdParse("resource", above, "nested-documents-above-capacity");
});

test("changing a file redirect to a here-document updates tilde classification", () => {
  const file = writeSource("tilde filename", "cat <~root\n~root\n");
  const document = writeSource("tilde delimiter", "cat <<~root\n~root\n");
  assertNodeCount(parseValidCst(file), "tilde_expansion", 2);
  for (const output of assertIncrementalEqualsFresh(
    file,
    document,
    "replace filename with delimiter",
    { byte: 5, deleteBytes: 0, insert: "<" },
  )) {
    assertNodeCount(output, "tilde_expansion", 0);
    assertNodeCount(output, "io_here", 1);
    assertCstRange(output, "0:6-0:11", "here_end_text");
  }
  for (const output of assertIncrementalEqualsFresh(
    document,
    file,
    "replace delimiter with filename",
    { byte: 5, deleteBytes: 1, insert: "" },
  )) {
    assertNodeCount(output, "tilde_expansion", 2);
    assertNodeCount(output, "filename", 1);
  }
});

test("sh: heredoc read-time continuations retain comment structure after edits", () => {
  for (const tail of ["world", ") ; <<IN"]) {
    const prefix = "cat <<OUT\n$( : # hello";
    const suffix = `${tail}\n)\nOUT\n`;
    const joined = writeSource("heredoc-joined-comment", prefix + suffix);
    const continued = writeSource(
      "heredoc-continued-comment",
      `${prefix}\\\n${suffix}`,
    );
    const output = parseValidCst(continued);
    assertOccurrenceCount(output, "name: cmd_name", 2);
    assertOccurrenceCount(output, "comment\n", 1);
    assertNodeCount(output, "comment_text", 1);
    assertCstRange(output, `1:5-2:${tail.length}`, "comment");
    assertCstRange(output, `1:5-2:${tail.length}`, "comment_text");
    assertCstRange(output, "1:12-1:13", "line_continuation");
    assertCstRange(output, "4:0-5:0", "here_document_end");
    assertIncrementalEqualsFresh(
      joined,
      continued,
      "continue-heredoc-comment",
      {
        byte: prefix.length,
        deleteBytes: 0,
        insert: "\\\n",
      },
    );
    assertIncrementalEqualsFresh(continued, joined, "join-heredoc-comment", {
      byte: prefix.length,
      deleteBytes: 2,
      insert: "",
    });
  }
});

test("sh: heredoc expansions resume command lists through encoded continuations", () => {
  const slash = "\\";
  const prefix = `cat <<OUT\n$(: \`: ${slash}\``;
  for (const count of [2, 4]) {
    const suffix = `${slash.repeat(count)}\n world\n${slash}\`\`)\nOUT\n`;
    const initial = writeSource(
      `heredoc-layout-initial-${count}`,
      prefix + suffix,
    );
    const final = writeSource(
      `heredoc-layout-after-command-${count}`,
      `${prefix}a\n${suffix}`,
    );
    const output = parseValidCst(final);
    assertOccurrenceCount(output, "name: cmd_name", 5);
    assert.equal(continuationManifest(output).length, 1);
    assertCstRange(output, "1:9-1:10", "name: cmd_name");
    assertCstRange(output, `2:${count - 1}-2:${count}`, "line_continuation");
    assertCstRange(output, "3:1-3:6", "name: cmd_name");
    assertCstRange(output, "5:0-6:0", "here_document_end");
    assertIncrementalEqualsFresh(initial, final, `insert-command-${count}`, {
      byte: prefix.length,
      deleteBytes: 0,
      insert: "a\n",
    });
    assertIncrementalEqualsFresh(final, initial, `remove-command-${count}`, {
      byte: prefix.length,
      deleteBytes: 2,
      insert: "",
    });
  }
});

test("sh: heredoc closing lines join before remaining backquote removal after edits", () => {
  const prefix = ": \"`: \\`cat <<'$IN'\n";
  const suffix = '$IN\n\\``"\n';
  const joined = writeSource(
    "quoted-ancestor-joined-end",
    `${prefix}\\\\${suffix}`,
  );
  const continued = writeSource(
    "quoted-ancestor-continued-end",
    `${prefix}\\\\\\\n${suffix}`,
  );
  const joinedOutput = parseValidCst(joined);
  assertCstRange(joinedOutput, "1:0-2:0", "here_document_end");
  assertNotContains(joinedOutput, "quoted_here_document_body");
  const output = parseValidCst(continued);
  assertCstRange(output, "1:0-3:0", "here_document_end");
  assertCstRange(output, "1:0-2:3", "here_document_end_text");
  assertCstRange(output, "1:2-1:3", "line_continuation");
  assertNotContains(output, "quoted_here_document_body");
  assertIncrementalEqualsFresh(joined, continued, "split-quoted-ancestor-end", {
    byte: prefix.length + 2,
    deleteBytes: 0,
    insert: "\\\n",
  });
  assertIncrementalEqualsFresh(continued, joined, "join-quoted-ancestor-end", {
    byte: prefix.length + 2,
    deleteBytes: 2,
    insert: "",
  });
});

test("sh: heredoc quote edits restore ancestor continuation and escape ownership", () => {
  const body = `\`cat <<IN\n${"\\".repeat(3)}\n \nIN\n\`\nOUT\n`;
  const quoted = writeSource(
    "ancestor-quoted-escape-run",
    `cat <<'OUT'\n${body}`,
  );
  const unquoted = writeSource(
    "ancestor-unquoted-escape-run",
    `cat <<OUT\n${body}`,
  );
  const output = parseValidCst(unquoted);
  assertCstRange(output, "2:0-3:1", "here_document_text");
  assertCstRange(output, "2:2-2:3", "line_continuation");
  assertNotContains(output, "here_document_escape");
  assertIncrementalEqualsFresh(
    quoted,
    unquoted,
    "enable-ancestor-read-time-continuation",
    { byte: 10, deleteBytes: 1, insert: "" },
    { byte: 6, deleteBytes: 1, insert: "" },
  );
  assertIncrementalEqualsFresh(
    unquoted,
    quoted,
    "disable-ancestor-read-time-continuation",
    { byte: 6, deleteBytes: 0, insert: "'" },
    { byte: 10, deleteBytes: 0, insert: "'" },
  );
  const shorter = writeSource(
    "ancestor-continuation-without-retained-prefix",
    `cat <<OUT\n${body.replace("\\".repeat(3), "\\".repeat(2))}`,
  );
  const shorterOutput = parseValidCst(shorter);
  assertCstRange(shorterOutput, "2:1-2:2", "line_continuation");
  assertNotContains(shorterOutput, "here_document_escape");
  assertIncrementalEqualsFresh(
    unquoted,
    shorter,
    "remove-ancestor-retained-prefix",
    { byte: 22, deleteBytes: 1, insert: "" },
  );
  assertIncrementalEqualsFresh(
    shorter,
    unquoted,
    "restore-ancestor-retained-prefix",
    { byte: 22, deleteBytes: 0, insert: "\\" },
  );
});

test("sh: here-document terminator continuations and quote edits preserve ranges", () => {
  const fixtures = [
    {
      name: "continued",
      source: "cat <<EOF\nbody\n\\\nEOF\nafter\n",
      end: "2:0-4:0",
      text: ["3:0-3:3"],
      continuations: ["2:0-2:1"],
    },
    {
      name: "continued-after-text",
      source: "cat <<EOF\nbody\nEOF\\\n\nafter\n",
      end: "2:0-4:0",
      text: ["2:0-2:3"],
      continuations: ["2:3-2:4"],
    },
    {
      name: "continued-stripped-tabs",
      source: "cat <<-EOF\nbody\n\t\\\n\tEOF\nafter\n",
      end: "2:0-4:0",
      text: ["3:1-3:4"],
      continuations: ["2:1-2:2"],
    },
    {
      name: "repeated-continuations-and-stripped-tabs",
      source: "cat <<-EOF\nbody\n\t\\\n\t\t\\\n\tEOF\nafter\n",
      end: "2:0-5:0",
      text: ["4:1-4:4"],
      continuations: ["2:1-2:2", "3:2-3:3"],
    },
  ];
  for (const fixture of fixtures) {
    const source = writeSource(`terminator-${fixture.name}`, fixture.source);
    const output = parseValidCst(source);
    assertCstRange(output, fixture.end, "end: here_document_end");
    assertOccurrenceCount(
      output,
      "here_document_end_text",
      fixture.text.length,
    );
    for (const range of fixture.text) {
      assertCstRange(output, range, "here_document_end_text");
    }
    assert.deepEqual(
      lineContinuationManifest(runQuery(source)),
      fixture.continuations,
    );
  }

  const initial = writeSource(
    "terminator-edit-initial",
    lines("cat <<EOF", "body", "EOF", "after"),
  );
  const continued = writeSource(
    "terminator-edit-continued",
    fixtures[0].source,
  );
  assertIncrementalEqualsFresh(initial, continued, "continue-terminator", {
    byte: 15,
    deleteBytes: 0,
    insert: "\\\n",
  });
  assertIncrementalEqualsFresh(continued, initial, "restore-terminator", {
    byte: 15,
    deleteBytes: 2,
    insert: "",
  });

  const quoted = writeSource(
    "terminator-edit-quoted",
    lines("cat <<'EOF'", "body", "EOF", "after"),
  );
  assertIncrementalEqualsFresh(
    initial,
    quoted,
    "quote-terminator-declaration",
    { byte: 9, deleteBytes: 0, insert: "'" },
    { byte: 6, deleteBytes: 0, insert: "'" },
  );
  assertIncrementalEqualsFresh(
    quoted,
    initial,
    "unquote-terminator-declaration",
    { byte: 10, deleteBytes: 1, insert: "" },
    { byte: 6, deleteBytes: 1, insert: "" },
  );
});

test("sh: editing a quoted terminator to empty preserves the following document", () => {
  const namedSource = lines(
    "cat <<-'A' <<-'B'",
    "body-a",
    "\tA",
    "body-b",
    "\tB",
    "after",
  );
  const emptySource = lines(
    "cat <<-'' <<-'B'",
    "body-a",
    "\t",
    "body-b",
    "\tB",
    "after",
  );
  const named = writeSource("queued-named-terminator", namedSource);
  const empty = writeSource("queued-empty-terminator", emptySource);
  const fixtures = [
    {
      name: "remove-first-terminator-text",
      initial: named,
      final: empty,
      edits: [
        { byte: 26, deleteBytes: 1, insert: "" },
        { byte: 8, deleteBytes: 1, insert: "" },
      ],
      text: ["4:1-4:2"],
    },
    {
      name: "restore-first-terminator-text",
      initial: empty,
      final: named,
      edits: [
        { byte: 25, deleteBytes: 0, insert: "A" },
        { byte: 8, deleteBytes: 0, insert: "A" },
      ],
      text: ["2:1-2:2", "4:1-4:2"],
    },
  ];
  for (const fixture of fixtures) {
    for (const output of assertIncrementalEqualsFresh(
      fixture.initial,
      fixture.final,
      fixture.name,
      ...fixture.edits,
    )) {
      assertOccurrenceCount(output, "end: here_document_end", 2);
      assertCstRange(output, "2:0-3:0", "end: here_document_end");
      assertCstRange(output, "3:0-4:0", "body: quoted_here_document_body");
      assertCstRange(output, "4:0-5:0", "end: here_document_end");
      assertOccurrenceCount(
        output,
        "here_document_end_text",
        fixture.text.length,
      );
      for (const range of fixture.text) {
        assertCstRange(output, range, "here_document_end_text");
      }
      assertCstDirectChildRange(
        output,
        "4:0-5:0",
        "end: here_document_end",
        "4:1-4:2",
        "here_document_end_text `B`",
      );
    }
  }
});

test("sh: here-document parameter brackets keep raw newline members across edits", () => {
  const closedSource = lines("cat <<EOF", "${x#[a", "b]}", "EOF");
  const openSource = lines("cat <<EOF", "${x#[a", "b}", "EOF");
  const closed = writeSource("here-document-multiline-bracket", closedSource);
  const open = writeSource(
    "here-document-unclosed-multiline-bracket",
    openSource,
  );
  const closerOffset = closedSource.indexOf("]");

  for (const output of assertIncrementalEqualsFresh(
    open,
    closed,
    "close-multiline-here-document-bracket",
    { byte: closerOffset, deleteBytes: 0, insert: "]" },
  )) {
    assertOccurrenceCount(output, "pattern_bracket_source", 1);
    assertCstRange(output, "1:4-2:2", "pattern_bracket_source");
    assertCstRange(
      output,
      "1:6-2:0",
      "member: pattern_bracket_character_source",
    );
    assertCstRange(
      output,
      "2:0-2:1",
      "member: pattern_bracket_character_source",
    );
    assertCstRange(output, "3:0-4:0", "end: here_document_end");
  }
  for (const output of assertIncrementalEqualsFresh(
    closed,
    open,
    "open-multiline-here-document-bracket",
    { byte: closerOffset, deleteBytes: 1, insert: "" },
  )) {
    assertNotContains(output, "pattern_bracket_source");
    assertCstRange(output, "3:0-4:0", "end: here_document_end");
  }
});

test("sh: here-document body escape runs fold all enclosing backquotes", () => {
  for (const [
    name,
    initialText,
    finalText,
    offset,
    parameterRange,
    escapeRange,
    bodyRange,
  ] of [
    [
      "plain",
      ": `cat <<EOF\n\\$x\nEOF\n`\n",
      ": `cat <<EOF\n\\\\$x\nEOF\n`\n",
      13,
      "1:0-1:3",
      "1:0-1:3",
      "1:0-2:0",
    ],
    [
      "nested",
      ": `: \\`cat <<EOF\n\\\\\\$x\nEOF\n\\`\n`\n",
      ": `: \\`cat <<EOF\n\\\\\\\\$x\nEOF\n\\`\n`\n",
      17,
      "1:0-1:5",
      "1:0-1:5",
      "1:0-2:0",
    ],
    [
      "strip-tabs",
      ": `cat <<-EOF\n\t\\$x\n\tEOF\n`\n",
      ": `cat <<-EOF\n\t\\\\$x\n\tEOF\n`\n",
      15,
      "1:1-1:4",
      "1:1-1:4",
      "1:1-2:0",
    ],
  ]) {
    const initial = writeSource(`${name}-body-expansion`, initialText);
    const final = writeSource(`${name}-body-literal-dollar`, finalText);
    const initialOutput = parseValidCst(initial);
    assertCstRange(initialOutput, parameterRange, "parameter_expansion");
    assertOccurrenceCount(initialOutput, "parameter_expansion\n", 1);
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      `${name}-escape-body-dollar`,
      { byte: offset, deleteBytes: 0, insert: "\\" },
    )) {
      assertNotContains(output, "parameter_expansion");
      assertCstRange(output, escapeRange, "here_document_escape");
      assertCstRange(output, bodyRange, "body: here_document_body");
      assertCstRange(output, "1:0-3:0", "document: here_document");
      assertCstRange(output, "2:0-3:0", "end: here_document_end");
    }
    assertIncrementalEqualsFresh(
      final,
      initial,
      `${name}-restore-body-expansion`,
      { byte: offset, deleteBytes: 1, insert: "" },
    );
  }
});

test("sh: paired here-document backslashes retain the newline before the delimiter", () => {
  for (const { name, initialText, finalText, offset, width } of [
    {
      name: "plain",
      initialText: ": `cat <<EOF\nbody\nEOF\n`\n",
      finalText: ": `cat <<EOF\n\\\\\\\\\nEOF\n`\n",
      offset: 13,
      width: 4,
    },
    {
      name: "quoted",
      initialText: ': "`cat <<EOF\nbody\nEOF\n`"\n',
      finalText: ': "`cat <<EOF\n\\\\\\\\\nEOF\n`"\n',
      offset: 14,
      width: 4,
    },
    {
      name: "nested",
      initialText: ": `: \\`cat <<EOF\nbody\nEOF\n\\`\n`\n",
      finalText: ": `: \\`cat <<EOF\n\\\\\\\\\\\\\\\\\nEOF\n\\`\n`\n",
      offset: 17,
      width: 8,
    },
  ]) {
    const initial = writeSource(`${name}-body-before-paired-run`, initialText);
    const final = writeSource(`${name}-body-paired-newline`, finalText);
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      `${name}-replace-body-with-paired-run`,
      { byte: offset, deleteBytes: 4, insert: "\\".repeat(width) },
    )) {
      assertCstRange(output, "1:0-2:0", "body: here_document_body");
      assertCstRange(output, "2:0-3:0", "end: here_document_end");
      assert.deepEqual(continuationManifest(output), []);
    }
    assertIncrementalEqualsFresh(
      final,
      initial,
      `${name}-restore-body-before-paired-run`,
      { byte: offset, deleteBytes: width, insert: "body" },
    );
  }
});

test("sh: editing escaped delimiter bytes restores UTF-8 terminator matching", () => {
  for (const [name, unmatchedWord, matchedWord] of [
    ["hex-byte", "$'\\xFF'", "$'\\xC3\\xBF'"],
    ["octal-byte", "$'\\377'", "$'\\303\\277'"],
    ["joined-quotes", "$'\\xC3'", "$'\\xC3'$'\\277'"],
  ]) {
    const unmatched = writeSource(
      `${name}-unmatched`,
      lines(`cat <<${unmatchedWord}`, "body", "ÿ", "echo AFTER"),
    );
    const matched = writeSource(
      `${name}-matched`,
      lines(`cat <<${matchedWord}`, "body", "ÿ", "echo AFTER"),
    );
    const remove = {
      byte: 6,
      deleteBytes: matchedWord.length,
      insert: unmatchedWord,
    };
    const repair = {
      byte: 6,
      deleteBytes: unmatchedWord.length,
      insert: matchedWord,
    };
    for (const output of assertIncrementalEqualsFresh(
      matched,
      unmatched,
      `${name}-remove-matching-bytes`,
      remove,
    )) {
      assertCstRange(output, "1:0-4:0", "body: quoted_here_document_body");
      assertNodeCount(output, "here_document_end", 0);
      assertNodeCount(output, "complete_command", 1);
    }
    for (const output of assertIncrementalEqualsFresh(
      unmatched,
      matched,
      `${name}-repair-matching-bytes`,
      repair,
    )) {
      assertCstRange(output, "1:0-2:0", "body: quoted_here_document_body");
      assertCstRange(output, "2:0-2:2", "here_document_end_text");
      assertCstRange(output, "2:0-3:0", "end: here_document_end");
      assertCstRange(output, "3:0-3:10", "command: complete_command");
    }
    assertIncrementalEqualsFresh(
      matched,
      matched,
      `${name}-roundtrip`,
      remove,
      repair,
    );
  }
});

test("sh: editing substitution delimiter quotes updates matching and restores the CST", () => {
  for (const {
    name,
    initialWord,
    initialDelimiter,
    finalWord,
    finalDelimiter,
    finalQuoted,
  } of [
    {
      name: "parameter-inner-quotes",
      initialWord: `"\${x:-"ab"}"`,
      initialDelimiter: `\${x:-ab}`,
      finalWord: `"\${x:-'ab'}"`,
      finalDelimiter: `\${x:-'ab'}`,
      finalQuoted: true,
    },
    {
      name: "pattern-outer-quotes",
      initialWord: `"\${x#'ab'}"`,
      initialDelimiter: `\${x#ab}`,
      finalWord: `\${x#'ab'}`,
      finalDelimiter: `\${x#ab}`,
      finalQuoted: true,
    },
    {
      name: "command-outer-quotes",
      initialWord: "\"$(printf 'ab')\"",
      initialDelimiter: "$(printf ab)",
      finalWord: "$(printf 'ab')",
      finalDelimiter: "$(printf ab)",
      finalQuoted: true,
    },
    {
      name: "command-inner-quotes",
      initialWord: "\"$(printf 'ab')\"",
      initialDelimiter: "$(printf ab)",
      finalWord: '"$(printf "ab")"',
      finalDelimiter: "$(printf ab)",
      finalQuoted: true,
    },
    {
      name: "trailing-outer-quote",
      initialWord: "\"$(printf 'ab')\"",
      initialDelimiter: "$(printf ab)",
      finalWord: "$(printf 'ab')\"tail\"",
      finalDelimiter: "$(printf ab)tail",
      finalQuoted: true,
    },
  ]) {
    const initialText = lines(
      `cat <<${initialWord}`,
      "$literal",
      initialDelimiter,
      "echo after",
    );
    const finalText = lines(
      `cat <<${finalWord}`,
      "$literal",
      finalDelimiter,
      "echo after",
    );
    const initial = writeSource(`${name}-initial`, initialText);
    const final = writeSource(`${name}-final`, finalText);
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      name,
      {
        byte: initialText.indexOf(initialDelimiter, initialText.indexOf("\n")),
        deleteBytes: initialDelimiter.length,
        insert: finalDelimiter,
      },
      { byte: 6, deleteBytes: initialWord.length, insert: finalWord },
    )) {
      assertCstRange(
        output,
        "1:0-2:0",
        finalQuoted
          ? "body: quoted_here_document_body"
          : "body: here_document_body",
      );
      assertCstRange(
        output,
        "1:0-1:8",
        finalQuoted ? "quoted_here_document_text" : "parameter_expansion",
      );
      assertCstRange(output, "2:0-3:0", "end: here_document_end");
      assertCstRange(output, "3:0-3:10", "command: complete_command");
    }
    assertIncrementalEqualsFresh(
      final,
      initial,
      `${name}-undo`,
      {
        byte: finalText.indexOf(finalDelimiter, finalText.indexOf("\n")),
        deleteBytes: finalDelimiter.length,
        insert: initialDelimiter,
      },
      { byte: 6, deleteBytes: finalWord.length, insert: initialWord },
    );

    const incomplete = writeSource(
      `${name}-missing-outer-quote`,
      initialText.slice(0, 6) + initialText.slice(7),
    );
    const outputs = parseRecoveryAfterEdits(
      initial,
      incomplete,
      `${name}-remove-outer-quote`,
      { byte: 6, deleteBytes: 1, insert: "" },
    );
    for (const output of outputs) {
      assert.ok(
        hasRecovery(output),
        `${name}: missing quote must remain incomplete`,
      );
    }
    assertIncrementalEqualsFresh(
      incomplete,
      initial,
      `${name}-repair-outer-quote`,
      { byte: 6, deleteBytes: 0, insert: '"' },
    );
  }
});

test("sh: escape edits update delimiter bytes without discarding quoted source", () => {
  for (const {
    name,
    initialWord,
    initialDelimiter,
    finalWord,
    finalDelimiter,
  } of [
    {
      name: "unknown escape",
      initialWord: "$'\\q'",
      initialDelimiter: "\\q",
      finalWord: "$'\\%'",
      finalDelimiter: "\\%",
    },
    {
      name: "hex digit limit",
      initialWord: "$'\\x41'",
      initialDelimiter: "A",
      finalWord: "$'\\x4142'",
      finalDelimiter: "A42",
    },
    {
      name: "NUL quote scope",
      initialWord: "$'A\\0B'C",
      initialDelimiter: "AC",
      finalWord: "$'A\\qB'C",
      finalDelimiter: "A\\qBC",
    },
    {
      name: "empty hex",
      initialWord: "$'A\\xgB'C",
      initialDelimiter: "AC",
      finalWord: "$'A\\x41B'C",
      finalDelimiter: "AABC",
    },
    {
      name: "escaped quote after NUL",
      initialWord: "$'A\\0B'C",
      initialDelimiter: "AC",
      finalWord: "$'A\\0B\\'C'D",
      finalDelimiter: "AD",
    },
    {
      name: "unicode escape",
      initialWord: "$'\\u0041'",
      initialDelimiter: "A",
      finalWord: "$'\\u00e9'",
      finalDelimiter: "é",
    },
    {
      name: "wide unicode escape",
      initialWord: "$'\\u00e9'",
      initialDelimiter: "é",
      finalWord: "$'\\U00000041'",
      finalDelimiter: "A",
    },
  ]) {
    const source = (word, delimiter) =>
      lines(`cat <<${word}`, "$literal", delimiter, "echo after");
    const initialText = source(initialWord, initialDelimiter);
    const finalText = source(finalWord, finalDelimiter);
    const initial = writeSource(`${name}-initial`, initialText);
    const final = writeSource(`${name}-final`, finalText);
    const replace = [
      {
        byte: Buffer.byteLength(`cat <<${initialWord}\n$literal\n`),
        deleteBytes: Buffer.byteLength(initialDelimiter),
        insert: finalDelimiter,
      },
      { byte: 6, deleteBytes: initialWord.length, insert: finalWord },
    ];
    const undo = [
      {
        byte: Buffer.byteLength(`cat <<${finalWord}\n$literal\n`),
        deleteBytes: Buffer.byteLength(finalDelimiter),
        insert: initialDelimiter,
      },
      { byte: 6, deleteBytes: finalWord.length, insert: initialWord },
    ];
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      name,
      ...replace,
    )) {
      assertCstRange(output, `0:6-0:${6 + finalWord.length}`, "word: word");
      assertCstRange(output, "1:0-2:0", "body: quoted_here_document_body");
      assertCstRange(
        output,
        `2:0-2:${Buffer.byteLength(finalDelimiter)}`,
        "here_document_end_text",
      );
      assertCstRange(output, "3:0-3:10", "command: complete_command");
    }
    assertIncrementalEqualsFresh(final, initial, `${name}-undo`, ...undo);
    assertIncrementalEqualsFresh(
      initial,
      initial,
      `${name}-roundtrip`,
      ...replace,
      ...undo,
    );
  }
});

test("sh: here-document state, delimiters, and bodies remain deterministic", () => {
  for (const fixture of [
    {
      name: "dollar-apostrophe",
      initial: lines(": `cat <<$'x'", "body", "x", "`", "after"),
      final: lines(": `cat <<$'\\\\''", "body", "'", "`", "after"),
      edits: [
        { byte: 19, deleteBytes: 1, insert: "'" },
        { byte: 11, deleteBytes: 1, insert: "\\\\'" },
      ],
      restore: [
        { byte: 21, deleteBytes: 1, insert: "x" },
        { byte: 11, deleteBytes: 3, insert: "x" },
      ],
      delimiterRange: "0:9-0:15",
      afterRange: "4:0-4:5",
    },
    {
      name: "escaped-backtick",
      initial: lines(': `cat <<"x"', "body", "x", "`", "after"),
      final: lines(': `cat <<"\\\\\\`"', "body", "\\`", "`", "after"),
      edits: [
        { byte: 18, deleteBytes: 1, insert: "\\`" },
        { byte: 10, deleteBytes: 1, insert: "\\\\\\`" },
      ],
      restore: [
        { byte: 21, deleteBytes: 2, insert: "x" },
        { byte: 10, deleteBytes: 4, insert: "x" },
      ],
      delimiterRange: "0:9-0:15",
      afterRange: "4:0-4:5",
    },
    {
      name: "quoted-escaped-backtick",
      initial: lines(': "`cat <<\\"x\\"', "body", "x", '`"', "after"),
      final: lines(': "`cat <<\\"\\\\\\`\\"', "body", "\\`", '`"', "after"),
      edits: [
        { byte: 21, deleteBytes: 1, insert: "\\`" },
        { byte: 12, deleteBytes: 1, insert: "\\\\\\`" },
      ],
      restore: [
        { byte: 24, deleteBytes: 2, insert: "x" },
        { byte: 12, deleteBytes: 4, insert: "x" },
      ],
      delimiterRange: "0:10-0:18",
      afterRange: "4:0-4:5",
    },
    {
      name: "nested-escaped-backtick",
      initial: lines(': `echo \\`cat <<"x"', "body", "x", "\\`", "`", "after"),
      final: lines(
        ': `echo \\`cat <<"\\\\\\\\\\\\\\`"',
        "body",
        "\\\\\\`",
        "\\`",
        "`",
        "after",
      ),
      edits: [
        { byte: 25, deleteBytes: 1, insert: "\\\\\\`" },
        { byte: 17, deleteBytes: 1, insert: "\\\\\\\\\\\\\\`" },
      ],
      restore: [
        { byte: 32, deleteBytes: 4, insert: "x" },
        { byte: 17, deleteBytes: 8, insert: "x" },
      ],
      delimiterRange: "0:16-0:26",
      afterRange: "5:0-5:5",
    },
    {
      name: "escaped-backslash-backtick",
      initial: lines(': `cat <<"x"', "body", "x", "`", "after"),
      final: lines(
        ': `cat <<"\\\\\\\\\\\\\\`"',
        "body",
        "\\\\\\`",
        "`",
        "after",
      ),
      edits: [
        { byte: 18, deleteBytes: 1, insert: "\\\\\\`" },
        { byte: 10, deleteBytes: 1, insert: "\\\\\\\\\\\\\\`" },
      ],
      restore: [
        { byte: 25, deleteBytes: 4, insert: "x" },
        { byte: 10, deleteBytes: 8, insert: "x" },
      ],
      delimiterRange: "0:9-0:19",
      afterRange: "4:0-4:5",
    },
  ]) {
    const initial = writeSource(
      `${fixture.name}-delimiter-initial`,
      fixture.initial,
    );
    const final = writeSource(`${fixture.name}-delimiter-final`, fixture.final);
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      `replace-backquote-${fixture.name}-delimiter`,
      ...fixture.edits,
    )) {
      assertCstRange(output, fixture.delimiterRange, "end: here_end");
      assertCstRange(output, fixture.delimiterRange, "here_end_text");
      assertCstRange(output, "1:0-2:0", "quoted_here_document_body");
      assertCstRange(output, "2:0-3:0", "here_document_end");
      assertCstRange(output, fixture.afterRange, "command: complete_command");
    }
    assertIncrementalEqualsFresh(
      final,
      initial,
      `restore-backquote-${fixture.name}-delimiter`,
      ...fixture.restore,
    );
  }

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
  assertIncrementalEqualsFresh(
    backquoteHashInitial,
    backquoteHashFinal,
    "delete-backquote-delimiter-word-markers",
    { byte: 48, deleteBytes: 1, insert: "" },
    { byte: 19, deleteBytes: 1, insert: "" },
  );
  assertIncrementalEqualsFresh(
    backquoteHashFinal,
    backquoteHashInitial,
    "insert-backquote-delimiter-word-markers",
    { byte: 47, deleteBytes: 0, insert: "X" },
    { byte: 19, deleteBytes: 0, insert: "X" },
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
    ["0:6-0:7", "line_continuation"],
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
    { byte: 6, deleteBytes: 0, insert: "\\\n" },
  );
  assertIncrementalEqualsFresh(
    delimiterBoundaryFinal,
    delimiterBoundaryInitial,
    "delete-here-document-delimiter-boundary-continuation",
    { byte: 6, deleteBytes: 2, insert: "" },
  );

  const continuedTabInitial = writeSource(
    "continued-tab-initial",
    "cat <<-AB\nA\\\nB\nAB\nafter\n",
  );
  const continuedTabFinal = writeSource(
    "continued-tab-final",
    "cat <<-AB\nA\\\n\tB\nAB\nafter\n",
  );
  const continuedTabOutput = parseValidCst(continuedTabFinal);
  assertCstRange(continuedTabOutput, "1:1-1:2", "line_continuation");
  assertCstRange(continuedTabOutput, "3:0-4:0", "end: here_document_end");
  assertIncrementalEqualsFresh(
    continuedTabInitial,
    continuedTabFinal,
    "insert-tab-after-here-document-continuation",
    { byte: 13, deleteBytes: 0, insert: "\t" },
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
  assertCstRange(continuedDollarOutput, "1:1-1:2", "line_continuation");
  assertNotContains(continuedDollarOutput, "dollar_single_quoted");
  assertIncrementalEqualsFresh(
    continuedDollarInitial,
    continuedDollarFinal,
    "insert-continuation-in-here-document-body",
    { byte: 11, deleteBytes: 0, insert: "\\\n" },
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
    { byte: 33, deleteBytes: 6, insert: "within-value" },
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
    { byte: 6, deleteBytes: 3, insert: "'EOF'" },
  );

  const oversizedDelimiterBytes = 2_048;
  const longInitialDelimiter = "A".repeat(oversizedDelimiterBytes);
  const longFinalDelimiter = "B".repeat(oversizedDelimiterBytes);
  const documentPrefix = "cat <<";
  const documentBody = "\nbody\n";
  const longInitialContents = `${documentPrefix}${longInitialDelimiter}${documentBody}${longInitialDelimiter}\n`;
  const longInitial = writeSource("long-initial", longInitialContents);
  const longFinal = writeSource(
    "long-final",
    `${documentPrefix}${longFinalDelimiter}${documentBody}${longFinalDelimiter}\nafter`,
  );
  parseRecoveryAfterEdits(
    longInitial,
    longFinal,
    "oversized-delimiter-recovery",
    {
      byte: documentPrefix.length,
      deleteBytes: oversizedDelimiterBytes,
      insert: longFinalDelimiter,
    },
    {
      byte:
        documentPrefix.length + oversizedDelimiterBytes + documentBody.length,
      deleteBytes: oversizedDelimiterBytes,
      insert: longFinalDelimiter,
    },
    { byte: longInitialContents.length, deleteBytes: 0, insert: "after" },
  );

  const bodyCloserInitial = writeSource(
    "body-substitution-closer-initial",
    lines("cat <<E", "$(a", "b", "E"),
  );
  const bodyCloserFinal = writeSource(
    "body-substitution-closer-final",
    lines("cat <<E", "$(a", "b)", "E"),
  );
  for (const output of assertIncrementalEqualsFresh(
    bodyCloserInitial,
    bodyCloserFinal,
    "close-body-substitution-after-separator",
    { byte: 13, deleteBytes: 0, insert: ")" },
  )) {
    assertCstRange(output, "1:3-2:0", "separator: newline_list");
    assertCstRange(output, "3:0-4:0", "end: here_document_end");
  }

  const bracketDelimiterInitial = writeSource(
    "bracket-special-delimiter-initial",
    lines("cat", " <<[[.", "[[.", "after"),
  );
  const bracketDelimiterFinal = writeSource(
    "bracket-special-delimiter-final",
    lines("cat <<[[.", "[[.", "after"),
  );
  const bracketDelimiterOutput = parseValidCst(bracketDelimiterFinal);
  assertCstRange(bracketDelimiterOutput, "0:6-0:9", "end: here_end");
  assertCstRange(bracketDelimiterOutput, "1:0-2:0", "end: here_document_end");
  assertIncrementalEqualsFresh(
    bracketDelimiterInitial,
    bracketDelimiterFinal,
    "join-bracket-special-delimiter-declaration",
    { byte: 3, deleteBytes: 1, insert: "" },
  );
  assertIncrementalEqualsFresh(
    bracketDelimiterFinal,
    bracketDelimiterInitial,
    "split-bracket-special-delimiter-declaration",
    { byte: 3, deleteBytes: 0, insert: "\n" },
  );
  const arithmeticDelimiter = writeSource(
    "arithmetic-here-document-delimiter",
    lines("cat <<$((case)) >out arg", "body", "$((case))", "after"),
  );
  const commandLikeDelimiter = writeSource(
    "command-like-arithmetic-delimiter",
    lines(
      "cat <<$((case x in x)esac)) >out arg",
      "body",
      "$((case x in x)esac))",
      "after",
    ),
  );
  for (const {
    initial,
    final,
    name,
    edits,
    member,
    end,
    redirect,
    argument,
  } of [
    {
      initial: arithmeticDelimiter,
      final: commandLikeDelimiter,
      name: "insert-command-source-in-arithmetic-delimiter",
      member: "here_end_text `$((case x in x)esac))`",
      edits: [
        { byte: 37, deleteBytes: 0, insert: " x in x)esac" },
        { byte: 13, deleteBytes: 0, insert: " x in x)esac" },
      ],
      end: 27,
      redirect: "0:28-0:32",
      argument: "0:33-0:36",
    },
    {
      initial: commandLikeDelimiter,
      final: arithmeticDelimiter,
      name: "remove-command-source-from-arithmetic-delimiter",
      member: "here_end_text `$((case))`",
      edits: [
        { byte: 49, deleteBytes: 12, insert: "" },
        { byte: 13, deleteBytes: 12, insert: "" },
      ],
      end: 15,
      redirect: "0:16-0:20",
      argument: "0:21-0:24",
    },
  ]) {
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      name,
      ...edits,
    )) {
      assertCstRange(output, `0:4-0:${end}`, "redirect: io_redirect");
      assertCstDirectChildRange(
        output,
        `0:6-0:${end}`,
        "end: here_end",
        `0:6-0:${end}`,
        "word: word",
      );
      assertCstDirectChildRange(
        output,
        `0:6-0:${end}`,
        "word: word",
        `0:6-0:${end}`,
        member,
      );
      assertCstRange(output, redirect, "redirect: io_redirect");
      assertCstRange(output, argument, "word: word");
      assertCstRange(output, "1:0-2:0", "body: here_document_body");
      assertCstRange(output, "2:0-3:0", "end: here_document_end");
      assertCstRange(output, "3:0-3:5", "command: complete_command");
    }
  }
});

test("sh: here-document redirect lines keep a continuation before a following command", () => {
  const withoutContinuation = writeSource(
    "heredoc-continuation-initial",
    lines("cat <<EOF", "EOF", "echo after"),
  );
  const withContinuation = writeSource(
    "heredoc-continuation-final",
    "cat <<EOF \\\n\nEOF\necho after\n",
  );
  assertIncrementalEqualsFresh(
    withoutContinuation,
    withContinuation,
    "heredoc-continuation",
    { byte: 9, deleteBytes: 0, insert: " \\\n" },
  );
});

test("sh: nested here-documents inherit enclosing input removal across quote edits", () => {
  const fixtures = [
    {
      name: "plain-inner-inherits-tabs",
      source: "cat <<-OUT\n$(cat <<IN\n\tbody\n\tIN\n)\nOUT\n",
      ends: [["3:0-4:0", "3:1-3:3"]],
      continuations: [],
      quotedBodies: 0,
    },
    {
      name: "quoted-inner-inherits-tabs-without-expansion",
      source: "cat <<-OUT\n$(cat <<'IN'\n\t$x\n\tIN\n)\nOUT\n",
      ends: [["3:0-4:0", "3:1-3:3"]],
      continuations: [],
      quotedBodies: 1,
    },
    {
      name: "grandchild-inherits-tabs-through-plain-parent",
      source:
        "cat <<-OUT\n$(cat <<MID\n$(cat <<'IN'\n\t$x\n\tIN\n)\n\tMID\n)\nOUT\n",
      ends: [
        ["4:0-5:0", "4:1-4:3"],
        ["6:0-7:0", "6:1-6:4"],
      ],
      continuations: [],
      quotedBodies: 1,
    },
    {
      name: "own-tab-removal-does-not-leak-to-pending-sibling",
      source: "cat <<OUT\n$(cat <<-A <<'B'\n\tA\n\tB\nB\n)\nOUT\n",
      ends: [
        ["2:0-3:0", "2:1-2:2"],
        ["4:0-5:0", "4:0-4:1"],
      ],
      continuations: [],
      quotedBodies: 1,
    },
    {
      name: "tab-removal-ends-with-enclosing-body",
      source: "cat <<-OUT\n$(cat <<'IN'\n\tIN\n)\nOUT\ncat <<'IN'\n\tIN\nIN\n",
      ends: [
        ["2:0-3:0", "2:1-2:3"],
        ["7:0-8:0", "7:0-7:2"],
      ],
      continuations: [],
      quotedBodies: 1,
    },
    {
      name: "quoted-inner-inherits-continuation-before-delimiter",
      source: "cat <<OUT\n$(cat <<'IN'\n\\\nIN\n)\nOUT\n",
      ends: [["2:0-4:0", "3:0-3:2"]],
      continuations: ["2:0-2:1"],
      quotedBodies: 0,
    },
    {
      name: "quoted-inner-body-remains-opaque-before-continuation",
      source: "cat <<OUT\n$(cat <<'IN'\n$x\n\\\nIN\n)\nOUT\n",
      ends: [["3:0-5:0", "4:0-4:2"]],
      continuations: ["3:0-3:1"],
      quotedBodies: 1,
    },
    {
      name: "quoted-empty-delimiter-inherits-continuation",
      source: "cat <<OUT\n$(cat <<''\n\\\n\n)\nOUT\n",
      ends: [["2:0-4:0", null]],
      continuations: ["2:0-2:1"],
      quotedBodies: 0,
    },
    {
      name: "quoted-inner-preserves-text-before-boundary-continuation",
      source: "cat <<OUT\n$(cat <<'IN'\n\\\\\\\n \nIN\n)\nOUT\n",
      ends: [["4:0-5:0", "4:0-4:2"]],
      continuations: ["2:2-2:3"],
      prefixes: ["2:0-3:1"],
      quotedBodies: 1,
    },
    {
      name: "continuation-removal-precedes-inherited-tab-removal",
      source: "cat <<-OUT\n$(cat <<'IN'\n\t\\\n\tIN\n)\nOUT\n",
      ends: [["2:0-4:0", "3:1-3:3"]],
      continuations: ["2:1-2:2"],
      quotedBodies: 0,
    },
    {
      name: "continuation-removal-ends-with-enclosing-body",
      source: "cat <<OUT\n$(cat <<'IN'\n\\\nIN\n)\nOUT\ncat <<'IN'\n\\\nIN\n",
      ends: [
        ["2:0-4:0", "3:0-3:2"],
        ["8:0-9:0", "8:0-8:2"],
      ],
      continuations: ["2:0-2:1"],
      quotedBodies: 1,
    },
  ];
  for (const fixture of fixtures) {
    const delimiterByte = fixture.source.indexOf("OUT");
    const initial = writeSource(
      `${fixture.name}-quoted-outer`,
      fixture.source.replace("OUT", "'OUT'"),
    );
    const source = writeSource(fixture.name, fixture.source);
    for (const output of assertIncrementalEqualsFresh(
      initial,
      source,
      fixture.name,
      { byte: delimiterByte + 4, deleteBytes: 1, insert: "" },
      { byte: delimiterByte, deleteBytes: 1, insert: "" },
    )) {
      for (const [end, text] of fixture.ends) {
        assertCstRange(output, end, "end: here_document_end");
        if (text !== null) {
          assertCstRange(output, text, "here_document_end_text");
        }
      }
      assertOccurrenceCount(
        output,
        "quoted_here_document_body",
        fixture.quotedBodies,
      );
      for (const range of fixture.prefixes ?? []) {
        assertCstRange(output, range, "quoted_here_document_text");
      }
      assertNotContains(output, "parameter_expansion");
    }
    assert.deepEqual(
      lineContinuationManifest(runQuery(source)),
      fixture.continuations,
      fixture.name,
    );
    assertIncrementalEqualsFresh(
      source,
      initial,
      `${fixture.name}-restore-quoted-outer`,
      { byte: delimiterByte, deleteBytes: 0, insert: "'" },
      { byte: delimiterByte + 4, deleteBytes: 0, insert: "'" },
    );
  }
});

test("sh: editing retained quoted-body backslash prefixes matches a fresh parse", () => {
  const prefix = "cat <<OUT\n$(cat <<'IN'\n";
  const suffix = "\n \nIN\n)\nOUT\n";
  const initial = writeSource(
    "quoted-body-prefix-initial",
    `${prefix}${"\\".repeat(3)}${suffix}`,
  );
  for (const { name, run, edit, continuations, textRange } of [
    {
      name: "extend-prefix",
      run: "\\".repeat(5),
      edit: { byte: prefix.length + 1, deleteBytes: 0, insert: "\\\\" },
      continuations: ["2:4-2:5"],
      textRange: "2:0-3:1",
    },
    {
      name: "remove-continuation-by-changing-parity",
      run: "\\\\",
      edit: { byte: prefix.length + 1, deleteBytes: 1, insert: "" },
      continuations: [],
      textRange: "2:0-2:2",
    },
    {
      name: "replace-counted-backslash-with-text",
      run: "\\x\\",
      edit: { byte: prefix.length + 1, deleteBytes: 1, insert: "x" },
      continuations: ["2:2-2:3"],
      textRange: "2:0-3:1",
    },
  ]) {
    const source = writeSource(name, `${prefix}${run}${suffix}`);
    for (const output of assertIncrementalEqualsFresh(
      initial,
      source,
      name,
      edit,
    )) {
      assertCstRange(output, textRange, "quoted_here_document_text");
      assertCstRange(output, "4:0-5:0", "end: here_document_end");
    }
    assert.deepEqual(
      lineContinuationManifest(runQuery(source)),
      continuations,
      name,
    );
  }
});
