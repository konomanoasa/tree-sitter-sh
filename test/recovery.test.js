import assert from "node:assert/strict";
import { test } from "node:test";
import {
  assertContains,
  assertCstRange,
  assertNodeCount,
  assertOccurrenceCount,
  continuationManifest,
  cstFingerprint,
  hasRecovery,
  nodeIdentity,
  parseCst,
  sourceByteOffset,
  sourcePoint,
} from "./support/cst.js";
import {
  applyEdits,
  assertIncrementalEqualsFresh,
  assertRepeatedColdParse,
  lines,
  parseRecovery,
  parseRecoveryAfterEdits,
  parseValidCst,
  runParse,
  writeSource,
} from "./support/parser.js";

test("sh: incomplete compounds reach EOF and regain their closing keywords", () => {
  for (const [name, incomplete, closing, keyword] of [
    ["conditional", "if :; then :; f\n", "fi\n", "fi_keyword"],
    ["loop", "while :; do :;\n", "done\n", "done_keyword"],
    ["case", "case x in x) :;;\n", "esac\n", "esac_keyword"],
  ]) {
    const source = writeSource(`missing-${name}-closer`, incomplete);
    const repaired = writeSource(
      `repaired-${name}-closer`,
      incomplete + closing,
    );
    const parsed = runParse({ source, description: name, mode: "recovery" });
    assert.equal(parsed.status, 1);
    for (const output of assertIncrementalEqualsFresh(
      source,
      repaired,
      `restore-${name}-closing-keyword`,
      { byte: incomplete.length, deleteBytes: 0, insert: closing },
    )) {
      assertCstRange(output, `1:0-1:${closing.length - 1}`, keyword);
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

test("continued shell constructs complete and disappear after recovery edits", () => {
  const prefix = "#!/usr/bin/env sh\n\nbrew install btm\n\n";
  const initial = writeSource("before-incomplete-construct", prefix);
  for (const [name, incomplete, completion] of [
    ["if", "i\\\nf\n", "true; then :; fi\n"],
    ["while", "w\\\nhile\n", "true; do :; done\n"],
    ["for", "f\\\nor item in one\n", "do :; done\n"],
    ["case", "c\\\nase x in\nx)", " :;; esac\n"],
    ["function", "f\\\n() {\n", ":;\n}\n"],
    ["pipeline", "e\\\ncho inner |\n", "cat\n"],
    ["parameter", "e\\\ncho ${value:-", "inner}\n"],
    ["substitution", "e\\\ncho $(", "echo inner)\n"],
  ]) {
    const broken = writeSource(`${name}-incomplete`, prefix + incomplete);
    const complete = writeSource(
      `${name}-complete`,
      prefix + incomplete + completion,
    );
    const insertion = {
      byte: prefix.length,
      deleteBytes: 0,
      insert: incomplete,
    };
    parseRecoveryAfterEdits(initial, broken, `${name} insertion`, insertion);
    for (const output of assertIncrementalEqualsFresh(
      initial,
      complete,
      `${name} completion`,
      insertion,
      {
        byte: prefix.length + incomplete.length,
        deleteBytes: 0,
        insert: completion,
      },
    )) {
      assertNodeCount(output, "line_continuation", 1);
      assertCstRange(output, "4:1-4:2", "line_continuation");
    }
    assertIncrementalEqualsFresh(complete, initial, `${name} removal`, {
      byte: prefix.length,
      deleteBytes: incomplete.length + completion.length,
      insert: "",
    });
  }
});

test("continued closing keywords retain their leaves after body and header repairs", () => {
  for (const [context, opener, closer] of [
    ["top level", "", ""],
    ["quoted substitution", 'echo "$(\n', ')"\n'],
    ["here document substitution", "cat <<EOF\n$(\n", ")\nEOF\n"],
  ]) {
    for (const [name, header, removed, ending] of [
      ["if body", "if true; then\n", ":\n", "f\\\ni\n"],
      ["while body", "while true; do\n", ":\n", "do\\\nne\n"],
      ["until body", "until false; do\n", ":\n", "d\\\none\n"],
      ["for body", "for x; do\n", ":\n", "don\\\ne\n"],
      ["then header", "if true; ", "then :; ", "f\\\ni\n"],
      ["do header", "while true; ", "do :; ", "do\\\nne\n"],
    ]) {
      const description = `${context} ${name}`;
      const prefix = `echo BEFORE\n${opener}${header}`;
      const suffix = `${ending}echo AFTER_ONE\necho AFTER_TWO\n${closer}`;
      const source = prefix + removed + suffix;
      const initial = writeSource(`${description} complete`, source);
      const broken = writeSource(`${description} incomplete`, prefix + suffix);
      const deletion = {
        byte: prefix.length,
        deleteBytes: removed.length,
        insert: "",
      };
      const restoration = {
        byte: prefix.length,
        deleteBytes: 0,
        insert: removed,
      };
      const slash = source.indexOf("\\\n");
      const expected = [
        `${sourcePoint(source, slash)}-${sourcePoint(source, slash + 1)}`,
      ];
      assert.deepEqual(continuationManifest(parseValidCst(initial)), expected);
      parseRecoveryAfterEdits(initial, broken, description, deletion);
      for (const output of [
        ...assertIncrementalEqualsFresh(
          initial,
          initial,
          `${description} round trip`,
          deletion,
          restoration,
        ),
        ...assertIncrementalEqualsFresh(
          broken,
          initial,
          `${description} cold repair`,
          restoration,
        ),
      ]) {
        assert.deepEqual(continuationManifest(output), expected, description);
      }
    }
  }
});

test("nested commands recover through enclosing terminators after completion", () => {
  const prefix = "echo before\n\n";
  const body = "echo nested\n";
  const incomplete = "if\n";
  const completion = "true; then :; fi\n";
  for (const [name, opener, closer, node] of [
    ["brace", "{\n", "}\n", "brace_group"],
    ["subshell", "(\n", ")\n", "subshell"],
    ["function", "f() {\n", "}\n", "function_definition"],
    ["then", "if\n", "then\n:; fi\n", "if_clause"],
    ["elif", "if true; then\n", "elif true; then :; fi\n", "if_clause"],
    ["else", "if true; then\n", "else\n:; fi\n", "if_clause"],
    ["fi", "if true; then\n", "fi\n", "if_clause"],
    ["do", "while\n", "do\n:; done\n", "while_clause"],
    ["done", "while true; do\n", "done\n", "while_clause"],
    ["esac", "case x in\nx)\n", ";;\nesac\n", "case_clause"],
    ["substitution", "echo $(\n", ")\n", "command_substitution"],
    ["quoted substitution", 'echo "$(\n', ')"\n', "command_substitution"],
    ["nested subshell", 'echo "$( (\n', ') )"\n', "subshell"],
    [
      "arithmetic substitution",
      "echo $((1 + $(\n",
      ") ))\n",
      "arithmetic_expansion",
    ],
    ["backquote", "echo `", "`\n", "backquote_substitution"],
    ["here document", "cat <<END\n$(\n", ")\nEND\n", "here_document"],
  ]) {
    for (const continued of [false, true]) {
      const ending = continued
        ? `${closer.slice(0, 1)}\\\n${closer.slice(1)}`
        : closer;
      const start = prefix + opener + body;
      const description = `${name}${continued ? " with continued terminator" : ""}`;
      const initial = writeSource(`${description} initial`, start + ending);
      const broken = writeSource(
        `${description} incomplete`,
        start + incomplete + ending,
      );
      const complete = writeSource(
        `${description} complete`,
        start + incomplete + completion + ending,
      );
      const insertion = {
        byte: start.length,
        deleteBytes: 0,
        insert: incomplete,
      };
      parseRecoveryAfterEdits(initial, broken, description, insertion);
      for (const output of assertIncrementalEqualsFresh(
        initial,
        complete,
        description,
        insertion,
        {
          byte: start.length + incomplete.length,
          deleteBytes: 0,
          insert: completion,
        },
      )) {
        assertNodeCount(output, node, node === "if_clause" ? 2 : 1);
        assertNodeCount(output, "line_continuation", continued ? 1 : 0);
      }
      assertIncrementalEqualsFresh(
        complete,
        initial,
        `${description} removal`,
        {
          byte: start.length,
          deleteBytes: incomplete.length + completion.length,
          insert: "",
        },
      );
    }
  }
});

test("sh: missing redirect filenames preserve surrounding commands and recover after repair", () => {
  for (const [name, source] of [
    ["output-ampersand", "cat > file & echo after\n"],
    ["input-semicolon", "cat < file ; echo after\n"],
    ["continued-semicolon", "cat >\\\nfile; echo after\n"],
    ["continued-ampersand", "cat > \\\nfile & echo after\n"],
    ["append-pipeline", "cat >> file | echo after\n"],
    ["read-write-and", "cat <> file && echo after\n"],
    ["clobber-or", "cat >| file || echo after\n"],
    ["duplicate-input", "cat <& file ; echo after\n"],
    ["duplicate-output", "cat >& file & echo after\n"],
    ["continued-and", "cat > file &\\\n& echo after\n"],
    ["continued-or", "cat > file |\\\n| echo after\n"],
    ["command-substitution", 'echo "$(cat < file && echo after)"\n'],
    ["heredoc-substitution", "cat <<END\n$(cat >> file || echo after)\nEND\n"],
    ["backquote-substitution", "echo `cat > file | echo after`\n"],
  ]) {
    const initialText = `echo before\n${source}echo final\n`;
    const byte = initialText.indexOf("file");
    const edit = { byte, deleteBytes: 4, insert: "" };
    const brokenText = initialText.slice(0, byte) + initialText.slice(byte + 4);
    const initial = writeSource(`${name}-initial`, initialText);
    const broken = writeSource(`${name}-broken`, brokenText);
    const expected = ["before", "final"].map((word) => ({
      start: brokenText.indexOf(`echo ${word}`),
      length: 4,
      type: "cmd_name",
    }));
    for (const output of parseRecoveryAfterEdits(initial, broken, name, edit)) {
      assert.equal(hasRecovery(output), true, name);
      const entries = parseCst(output).map((entry) => {
        const [start, end] = entry.range
          .split("-")
          .map((point) => sourceByteOffset(brokenText, point));
        return { type: nodeIdentity(entry).type, start, end };
      });
      for (const { start, length, type } of expected) {
        assert.ok(start >= 0, `${name}: missing expected source`);
        const end = start + length;
        assert.ok(
          entries.some(
            (entry) =>
              entry.type === type && entry.start === start && entry.end === end,
          ),
          `${name}: expected ${type} at ${start}-${end}`,
        );
        assert.equal(
          entries.some(
            (entry) =>
              entry.type === "ERROR" &&
              entry.start <= start &&
              entry.end >= end,
          ),
          false,
          `${name}: ${type} remains inside ERROR`,
        );
      }
    }
    assertIncrementalEqualsFresh(initial, initial, `repair-${name}`, edit, {
      byte,
      deleteBytes: 0,
      insert: "file",
    });
  }
});

test("sh: compound operator errors preserve repairs across quoted source contexts", () => {
  for (const [name, prefix, suffix] of [
    ["command", 'echo "$(\n', ')"\necho after\n'],
    ["backquote", 'echo "`\n', '`"\necho after\n'],
    ["heredoc", 'cat <<END\n$(echo "$(\n', ')")\nEND\necho after\n'],
  ]) {
    for (const operator of ["&&", ";;", ";&", "&\\\n&", ";\\\n;", ";\\\n&"]) {
      const initialText = `${prefix}:\n${suffix}`;
      const initial = writeSource(`${name}-operator-initial`, initialText);
      const stray = `${operator}\n`;
      const broken = writeSource(
        `${name}-operator-broken`,
        `${prefix}${stray}:\n${suffix}`,
      );
      const insertion = { byte: prefix.length, deleteBytes: 0, insert: stray };
      for (const output of parseRecoveryAfterEdits(
        initial,
        broken,
        `${name}-${operator}`,
        insertion,
      )) {
        assert.equal(hasRecovery(output), true);
      }
      const repaired = writeSource(
        `${name}-operator-repaired`,
        `${prefix} :\n${suffix}`,
      );
      assertIncrementalEqualsFresh(
        initial,
        repaired,
        `repair-${name}-${operator}`,
        insertion,
        {
          byte: prefix.length,
          deleteBytes: stray.length,
          insert: " ",
        },
      );
    }
  }
});

test("sh: missing case in keywords preserve selector ownership after repair", () => {
  for (const [name, selector, prefix, suffix] of [
    ["literal", "word", "", ""],
    ["quoted", '"$value"', "", ""],
    ["continued", "wo\\\nrd", "", ""],
    ["substitution", "$(echo word)", "{\n", "}\n"],
    ["nested", `\${value:-word}`, 'echo "$(\n', ')"\n'],
  ]) {
    const header = `${prefix}case ${selector} `;
    const tail = `one) :;; esac\n${suffix}echo after\n`;
    const initial = writeSource(`case-${name}-initial`, `${header}in ${tail}`);
    const broken = writeSource(`case-${name}-missing-in`, `${header}${tail}`);
    const removal = { byte: header.length, deleteBytes: 3, insert: "" };
    for (const output of parseRecoveryAfterEdits(
      initial,
      broken,
      `case-${name}-remove-in`,
      removal,
    )) {
      assert.equal(hasRecovery(output), true);
    }
    const repaired = writeSource(
      `case-${name}-repaired`,
      `${header}in  ${tail}`,
    );
    for (const output of assertIncrementalEqualsFresh(
      initial,
      repaired,
      `case-${name}-repair-in`,
      removal,
      {
        byte: header.length,
        deleteBytes: 0,
        insert: "in  ",
      },
    )) {
      assertNodeCount(output, "case_clause", 1);
      assertNodeCount(output, "in_keyword", 1);
    }
  }
});

test("sh: repairing pipeline errors restores continued and nested command source", () => {
  for (const [name, brokenText, repairedText] of [
    [
      "continued-pipeline",
      "echo ready | | cat\\\nmore\necho after\n",
      "echo ready | cat\\\nmore\necho after\n",
    ],
    [
      "substitution-pipeline",
      'echo "$(echo ready | | cat\necho inside)"\necho after\n',
      'echo "$(echo ready | cat\necho inside)"\necho after\n',
    ],
    [
      "heredoc-pipeline",
      "cat <<END | | cat\nbody\nEND\necho after\n",
      "cat <<END | cat\nbody\nEND\necho after\n",
    ],
  ]) {
    const broken = writeSource(`${name}-broken`, brokenText);
    const repaired = writeSource(`${name}-repaired`, repairedText);
    const byte = brokenText.indexOf("| |") + 1;
    for (const output of parseRecoveryAfterEdits(
      repaired,
      broken,
      `${name}-insert-pipe`,
      { byte, deleteBytes: 0, insert: " |" },
    )) {
      assert.equal(hasRecovery(output), true);
    }
    assertIncrementalEqualsFresh(broken, repaired, `${name}-remove-pipe`, {
      byte,
      deleteBytes: 2,
      insert: "",
    });
  }
});

test("sh: repairing missing operands and compound bodies matches a fresh parse", () => {
  const prefix = "before alpha\n";
  const suffix = "after omega\n";
  for (const { name, broken, restored, wordOffset, removedLength = 0 } of [
    {
      name: "if-empty-body",
      broken: "if :;then |;fi",
      restored: "if :;then fixed;fi",
      wordOffset: 10,
      removedLength: 1,
    },
    {
      name: "if-body-pipes",
      broken: "if :;then | |;fi",
      restored: "if :;then fixed;fi",
      wordOffset: 10,
      removedLength: 3,
    },
    {
      name: "if-empty-condition",
      broken: "if |;then :;fi",
      restored: "if fixed;then :;fi",
      wordOffset: 3,
      removedLength: 1,
    },
    {
      name: "if-body-and",
      broken: "if :;then &&;fi",
      restored: "if :;then fixed;fi",
      wordOffset: 10,
      removedLength: 2,
    },
    {
      name: "if-body-semi-pipe",
      broken: "if :;then ;|;fi",
      restored: "if :;then fixed;fi",
      wordOffset: 10,
      removedLength: 2,
    },
    {
      name: "while-empty-body",
      broken: "while :;do |;done",
      restored: "while :;do fixed;done",
      wordOffset: 11,
      removedLength: 1,
    },
    {
      name: "while-empty-condition",
      broken: "while |;do :;done",
      restored: "while fixed;do :;done",
      wordOffset: 6,
      removedLength: 1,
    },
    {
      name: "until-empty-body",
      broken: "until :;do &&;done",
      restored: "until :;do fixed;done",
      wordOffset: 11,
      removedLength: 2,
    },
    {
      name: "for-empty-body",
      broken: "for x;do |;done",
      restored: "for x;do fixed;done",
      wordOffset: 9,
      removedLength: 1,
    },
    {
      name: "brace-empty-body",
      broken: "{ |; }",
      restored: "{ fixed; }",
      wordOffset: 2,
      removedLength: 1,
    },
    {
      name: "brace-body-pipes",
      broken: "{ | |; }",
      restored: "{ fixed; }",
      wordOffset: 2,
      removedLength: 3,
    },
    {
      name: "brace-body-and",
      broken: "{ &&; }",
      restored: "{ fixed; }",
      wordOffset: 2,
      removedLength: 2,
    },
    {
      name: "brace-body-semi-pipe",
      broken: "{ ;|; }",
      restored: "{ fixed; }",
      wordOffset: 2,
      removedLength: 2,
    },
    {
      name: "subshell-empty-body",
      broken: "(|;)",
      restored: "(fixed;)",
      wordOffset: 1,
      removedLength: 1,
    },
    {
      name: "subshell-body-semi-pipe",
      broken: "(;|;)",
      restored: "(fixed;)",
      wordOffset: 1,
      removedLength: 2,
    },
    {
      name: "pipe",
      broken: "broken | ;",
      restored: "broken | fixed;",
      wordOffset: 9,
    },
    {
      name: "and",
      broken: "broken && ;",
      restored: "broken && fixed;",
      wordOffset: 10,
    },
    {
      name: "or",
      broken: "broken || &",
      restored: "broken || fixed&",
      wordOffset: 10,
    },
    { name: "negation", broken: "! ;", restored: "! fixed;", wordOffset: 2 },
    {
      name: "if-pipe",
      broken: "if :;then :|;fi",
      restored: "if :;then :|fixed;fi",
      wordOffset: 12,
    },
    {
      name: "else-empty-command",
      broken: "if :;then :;else |;fi",
      restored: "if :;then :;else fixed;fi",
      wordOffset: 17,
      removedLength: 1,
    },
    {
      name: "elif-empty-condition",
      broken: "if :;then :;elif |;then :;fi",
      restored: "if :;then :;elif fixed;then :;fi",
      wordOffset: 17,
      removedLength: 1,
    },
    {
      name: "while-and",
      broken: "while :; do : && ; done",
      restored: "while :; do : && fixed; done",
      wordOffset: 17,
    },
    {
      name: "for-or",
      broken: "for x;do :||;done",
      restored: "for x;do :||fixed;done",
      wordOffset: 12,
    },
    {
      name: "brace-pipe",
      broken: "{ :|; }",
      restored: "{ :|fixed; }",
      wordOffset: 4,
    },
    {
      name: "case-first-pattern",
      broken: "case x in ) :;;esac",
      restored: "case x in fixed) :;;esac",
      wordOffset: 10,
    },
    {
      name: "case-next-pattern",
      broken: "case x in x|) :;;esac",
      restored: "case x in x|fixed) :;;esac",
      wordOffset: 12,
    },
    {
      name: "case-both-patterns",
      broken: "case x in |) :;;esac",
      restored: "case x in fixed) :;;esac",
      wordOffset: 10,
      removedLength: 1,
    },
    {
      name: "case-pipe",
      broken: "case x in x) : | ;; esac",
      restored: "case x in x) : | fixed;; esac",
      wordOffset: 17,
    },
  ]) {
    const initial = writeSource(
      `missing-${name}-operand`,
      `${prefix}${lines(broken)}${suffix}`,
    );
    const { recovery } = runParse({
      description: `${name}: malformed command`,
      mode: "recovery",
      source: initial,
    });
    assert.equal(recovery, true, `${name}: invalid command parsed as valid`);
    const final = writeSource(
      `restored-${name}-operand`,
      `${prefix}${lines(restored)}${suffix}`,
    );
    assertIncrementalEqualsFresh(initial, final, `restore-${name}`, {
      byte: prefix.length + wordOffset,
      deleteBytes: removedLength,
      insert: "fixed",
    });
  }
});

test("sh: complete commands keep their terminator across recovery edits", () => {
  const semiInitial = writeSource("terminator-semi-initial", "for \\\n");
  const semiFinal = writeSource("terminator-semi-final", "r ;\\\n");
  assertIncrementalEqualsFresh(
    semiInitial,
    semiFinal,
    "terminator-semi",
    { byte: 0, deleteBytes: 0, insert: "}" },
    { byte: 0, deleteBytes: 0, insert: "}" },
    { byte: 6, deleteBytes: 0, insert: ";;" },
    { byte: 0, deleteBytes: 1, insert: "" },
    { byte: 0, deleteBytes: 3, insert: "" },
    { byte: 6, deleteBytes: 0, insert: "" },
    { byte: 3, deleteBytes: 1, insert: "" },
  );

  const loneInitial = writeSource("terminator-lone-initial", ":bar ;");
  const loneFinal = writeSource("terminator-lone-final", "/ ;");
  assertIncrementalEqualsFresh(
    loneInitial,
    loneFinal,
    "terminator-lone",
    { byte: 0, deleteBytes: 1, insert: "" },
    { byte: 0, deleteBytes: 2, insert: "" },
    { byte: 3, deleteBytes: 0, insert: ";" },
    { byte: 0, deleteBytes: 0, insert: "&" },
    { byte: 0, deleteBytes: 2, insert: "" },
    { byte: 0, deleteBytes: 0, insert: "&" },
    { byte: 0, deleteBytes: 1, insert: "" },
    { byte: 0, deleteBytes: 0, insert: "/" },
    { byte: 4, deleteBytes: 0, insert: "" },
    { byte: 3, deleteBytes: 1, insert: "" },
  );
});

test("sh: function bodies become complete after recovery edits", () => {
  for (const [name, initialContents, finalContents, edit, bodyRange] of [
    [
      "missing-function-body",
      "f()\n",
      "f()\n{ :; }\n",
      { byte: 4, deleteBytes: 0, insert: "{ :; }\n" },
      "1:0-1:6",
    ],
    [
      "replace-invalid-function-body",
      "f() words\n",
      "f() { :; }\n",
      { byte: 4, deleteBytes: 6, insert: "{ :; }\n" },
      "0:4-0:10",
    ],
    [
      "complete-function-after-here-document",
      "cat <<E; f()\nbody\nE\n",
      "cat <<E; f()\nbody\nE\n{ :; }\n",
      { byte: 20, deleteBytes: 0, insert: "{ :; }\n" },
      "3:0-3:6",
    ],
  ]) {
    const initial = writeSource(`${name}-initial`, initialContents);
    const final = writeSource(`${name}-final`, finalContents);
    parseRecovery(initial, `${name} recovery`);
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      name,
      edit,
    )) {
      assertOccurrenceCount(output, "function_definition", 1);
      assertCstRange(output, bodyRange, "body: function_body");
      assertCstRange(output, bodyRange, "brace_group");
    }
  }
});

test("sh: commands regain their public structure after command boundary errors", () => {
  for (const { name, initial, final, offset, replacement, stray = ";" } of [
    {
      name: "simple-command",
      initial: "a; b\n",
      final: "a; c\n",
      offset: 3,
      replacement: "c",
    },
    {
      name: "redirection-target-before-semicolon",
      initial: "cat >x; echo after\n",
      final: "cat >y; echo after\n",
      offset: 5,
      replacement: "y",
      stray: "",
    },
    {
      name: "redirection-target-before-ampersand",
      initial: "cat >x & echo after\n",
      final: "cat >y & echo after\n",
      offset: 5,
      replacement: "y",
      stray: "",
    },
    {
      name: "arithmetic-operand-before-semicolon",
      initial: "echo $((1 + (2))); echo after\n",
      final: "echo $((1 + (3))); echo after\n",
      offset: 13,
      replacement: "3",
      stray: "",
    },
    {
      name: "for-header-before-subshell",
      initial: "for x in a; do :; done\n(echo after)\necho tail\n",
      final: "for x in a;  do :; done\n(echo after)\necho tail\n",
      offset: 12,
      replacement: " d",
      stray: "",
    },
    {
      name: "function",
      initial: "f() { a; b; }\n",
      final: "f() { a; c; }\n",
      offset: 9,
      replacement: "c",
    },
    {
      name: "if-clause",
      initial: "if a; then b; c; fi\n",
      final: "if a; then b; d; fi\n",
      offset: 14,
      replacement: "d",
    },
    {
      name: "case-item",
      initial: "case x in x) a; b;; esac\n",
      final: "case x in x) a; c;; esac\n",
      offset: 16,
      replacement: "c",
    },
    {
      name: "newline",
      initial: "a\nb\nc\n",
      final: "a\nd\nc\n",
      offset: 2,
      replacement: "d",
      stray: ")",
    },
    {
      name: "terminated-line",
      initial: "a;\nb\nc\n",
      final: "a;\nd\nc\n",
      offset: 3,
      replacement: "d",
      stray: ";;",
    },
    {
      name: "comment-and-blank-line",
      initial: "a \n# boundary\n\nb\nc\n",
      final: "a \n# boundary\n\nd\nc\n",
      offset: 15,
      replacement: "d",
      stray: "|",
    },
    {
      name: "continued-layout",
      initial: "a\n\\\nb\nc\n",
      final: "a\n\\\nd\nc\n",
      offset: 4,
      replacement: "d",
      stray: "&",
    },
    {
      name: "continued-word-after-stray",
      initial: "a; b; ec\\\nho tail\n",
      final: "a; d; ec\\\nho tail\n",
      offset: 3,
      replacement: "d",
      stray: ")",
    },
    {
      name: "single-quoted-word-after-stray",
      initial: "a; b; 'echo' tail\n",
      final: "a; d; 'echo' tail\n",
      offset: 3,
      replacement: "d",
      stray: ")",
    },
    {
      name: "double-quoted-word-after-stray",
      initial: 'a; b; "$command" tail\n',
      final: 'a; d; "$command" tail\n',
      offset: 3,
      replacement: "d",
      stray: ")",
    },
    {
      name: "parameter-word-after-stray",
      initial: `a; b; \${command} tail\n`,
      final: `a; d; \${command} tail\n`,
      offset: 3,
      replacement: "d",
      stray: ")",
    },
    {
      name: "heredoc-substitution",
      initial: "cat <<END\n$(a\nb\nc)\nEND\nafter\n",
      final: "cat <<END\n$(a\nd\nc)\nEND\nafter\n",
      offset: 14,
      replacement: "d",
      stray: ";;",
    },
  ]) {
    const before = writeSource(`${name}-before-stray-separator`, initial);
    const insertion = { byte: offset, deleteBytes: 1, insert: stray };
    const broken = writeSource(
      `${name}-with-stray-separator`,
      initial.slice(0, offset) + stray + initial.slice(offset + 1),
    );
    for (const output of parseRecoveryAfterEdits(
      before,
      broken,
      `insert-${name}-stray-separator`,
      insertion,
    )) {
      assert.equal(hasRecovery(output), true, `${name}\n${output}`);
    }
    assertIncrementalEqualsFresh(
      before,
      writeSource(`${name}-after-stray-separator`, final),
      `restore-${name}-after-stray-separator`,
      insertion,
      { byte: offset, deleteBytes: stray.length, insert: replacement },
    );
  }
});

test("sh: restoring missing compound keywords preserves command source", () => {
  for (const {
    name,
    initial,
    final,
    keyword,
    owner,
    commands = 3,
    keywords = 1,
  } of [
    {
      name: "if-closer",
      initial: "if true; then echo inside; fi\necho after\n",
      final: "if true; then echo inside; fi \necho after\n",
      keyword: "fi",
      owner: "if_clause",
    },
    {
      name: "loop-closer",
      initial: "while true; do echo inside; done\necho after\n",
      final: "while true; do echo inside; done \necho after\n",
      keyword: "done",
      owner: "while_clause",
    },
    {
      name: "for-header-do",
      initial: "for x in a; do\necho inside; done\necho after\n",
      final: "for x in a; do \necho inside; done\necho after\n",
      keyword: "do",
      owner: "for_clause",
      commands: 2,
    },
    {
      name: "if-header-then",
      initial: "if true; then\necho inside; fi\necho after\n",
      final: "if true; then \necho inside; fi\necho after\n",
      keyword: "then",
      owner: "if_clause",
    },
    {
      name: "elif-header-then",
      initial:
        "if false; then :; elif true; then\necho inside; fi\necho after\n",
      final:
        "if false; then :; elif true; then \necho inside; fi\necho after\n",
      keyword: "then",
      owner: "else_part",
      commands: 5,
      keywords: 2,
    },
  ]) {
    const offset = initial.indexOf(`${keyword}\n`);
    const removal = { byte: offset, deleteBytes: keyword.length, insert: "" };
    const complete = writeSource(`${name}-complete`, initial);
    const incomplete = writeSource(
      `${name}-incomplete`,
      initial.slice(0, offset) + initial.slice(offset + keyword.length),
    );
    for (const output of parseRecoveryAfterEdits(
      complete,
      incomplete,
      `remove-${name}`,
      removal,
    )) {
      assert.equal(hasRecovery(output), true);
    }
    for (const output of assertIncrementalEqualsFresh(
      complete,
      writeSource(`${name}-restored`, final),
      `restore-${name}`,
      removal,
      { byte: offset, deleteBytes: 0, insert: `${keyword} ` },
    )) {
      assertNodeCount(output, owner, 1);
      assertNodeCount(output, "simple_command", commands);
      assertNodeCount(output, `${keyword}_keyword`, keywords);
    }
  }
});

test("sh: repairing a function header preserves a following backquote assignment", () => {
  const source = writeSource(
    "repaired-function-before-backquote-assignment",
    "\nworker() {\n  :\n}\nresult=`printf nested`\n",
  );
  for (const output of assertIncrementalEqualsFresh(
    source,
    source,
    "repaired function header",
    { byte: 9, deleteBytes: 3, insert: "Wl35" },
    { byte: 9, deleteBytes: 4, insert: " {\n" },
  )) {
    assertOccurrenceCount(output, "assignment_word", 1);
  }
});

test("sh: parameter errors preserve closing quotes and following commands through repairs", () => {
  for (const [name, initialText, removed, replacement] of [
    ["quoted-value", `echo "\${v:-word}"\necho after\n`, ":-", " @"],
    ["quoted-brace", `echo "\${v:-"}"}"\necho after\n`, ":-", " @"],
    ["escaped-brace", `echo "\${v:-\\}}"\necho after\n`, ":-", " @"],
    [
      "nested-parameter",
      `echo "\${v:-"\${x:-word}"}"\necho after\n`,
      "x:-",
      "x @",
    ],
    [
      "pattern-operand",
      `echo "\${v#"\${x:-word}"}"\necho after\n`,
      "x:-",
      "x @",
    ],
    ["invalid-name", `echo "\${v:-"word"}"\necho after\n`, "v", "!v"],
    ["junk-before-operator", `echo "\${v:-word}"\necho after\n`, "v", "v @"],
    [
      "command-substitution",
      `echo "$(echo "\${v:-"}"}")"\necho after\n`,
      ":-",
      " @",
    ],
    [
      "heredoc-substitution",
      `cat <<END\n$(echo "\${v:-"}"}")\nEND\necho after\n`,
      ":-",
      " @",
    ],
    [
      "continued-value-operator",
      `echo "\${v:\\\n-word}"\necho after\n`,
      ":\\\n-",
      " @",
    ],
    [
      "continued-pattern-operator",
      `echo "\${v#\\\n#word}"\necho after\n`,
      "#\\\n#",
      " @",
    ],
  ]) {
    const byte = initialText.indexOf(removed);
    const edit = { byte, deleteBytes: removed.length, insert: replacement };
    const brokenText =
      initialText.slice(0, byte) +
      replacement +
      initialText.slice(byte + removed.length);
    const initial = writeSource(`${name}-initial`, initialText);
    const broken = writeSource(`${name}-broken`, brokenText);
    const row = brokenText.split("\n").length - 2;
    for (const output of parseRecoveryAfterEdits(initial, broken, name, edit)) {
      assert.equal(hasRecovery(output), true, name);
      const ancestors = [];
      let command = false;
      for (const entry of parseCst(output)) {
        while (ancestors.at(-1)?.depth >= entry.depth) ancestors.pop();
        if (
          entry.range === `${row}:0-${row}:4` &&
          nodeIdentity(entry).type === "cmd_name"
        ) {
          command = true;
          assert.equal(
            ancestors.some(
              (ancestor) => nodeIdentity(ancestor).type === "ERROR",
            ),
            false,
            `${name}: following command remains inside ERROR`,
          );
        }
        ancestors.push(entry);
      }
      assert.equal(command, true, `${name}: following command was swallowed`);
    }
    assertIncrementalEqualsFresh(initial, initial, `repair-${name}`, edit, {
      byte,
      deleteBytes: replacement.length,
      insert: removed,
    });
  }
});

test("sh: arithmetic parameter repairs restore quote context across edit histories", () => {
  for (const [name, source] of [
    ["structured", `echo $((\${v:-\\q}))\necho after\n`],
    ["dynamic", `echo $((0x\${v:-\\q}))\necho after\n`],
    ["nested-parameter", `echo $((\${w:-\${v:-\\q}}))\necho after\n`],
    ["nested-command", `echo $(($(echo \${v:-\\q})))\necho after\n`],
  ]) {
    const byte = source.indexOf("v:-") + 1;
    const damage = { byte, deleteBytes: 2, insert: " @" };
    const repair = { byte, deleteBytes: 2, insert: ":-" };
    const initial = writeSource(`arithmetic-parameter-${name}`, source);
    const broken = writeSource(
      `arithmetic-parameter-${name}-broken`,
      applyEdits(source, [damage]),
    );
    for (const output of parseRecoveryAfterEdits(
      initial,
      broken,
      name,
      damage,
    )) {
      assert.equal(hasRecovery(output), true, name);
    }
    assertIncrementalEqualsFresh(broken, initial, `repair-${name}`, repair);
    assertIncrementalEqualsFresh(
      initial,
      initial,
      `roundtrip-${name}`,
      damage,
      repair,
    );
  }
});

test("sh: repairing an encoded backquote closer restores two nested substitutions", () => {
  const broken = writeSource("backquote-closer-broken", "echo `\\`a`b`\n");
  const repaired = writeSource(
    "backquote-closer-repaired",
    "echo `\\`a\\`b`\n",
  );
  const initial = runParse({ source: broken, mode: "recovery" });
  assert.equal(initial.status, 1);
  assert.equal(hasRecovery(initial.output), true);
  for (const output of assertIncrementalEqualsFresh(
    broken,
    repaired,
    "repair-encoded-backquote-closer",
    { byte: 9, deleteBytes: 0, insert: "\\" },
  )) {
    assertNodeCount(output, "backquote_substitution", 2);
  }
  for (const output of parseRecoveryAfterEdits(
    repaired,
    broken,
    "remove-encoded-backquote-closer",
    { byte: 9, deleteBytes: 1, insert: "" },
  )) {
    assert.equal(hasRecovery(output), true);
  }
});

test("sh: repairing terminator newlines at source and substitution boundaries matches a fresh parse", () => {
  for (const [name, prefix, closing] of [
    ["source-named", "cat <<E\nx\nE", ""],
    ["source-quoted", "cat <<'E'\nx\nE", ""],
    ["source-empty", "cat <<''\n", ""],
    ["source-empty-tabs", "cat <<-''\n\t", ""],
    ["source-named-tabs", "cat <<-E\nx\n\tE", ""],
    ["source-empty-after-body", "cat <<''\nx\n", ""],
    ["source-continuation", "cat <<E\nx\nE\\\n", ""],
    ["command-named", ": $(cat <<E\nx\nE", ")\n"],
    ["command-empty", ": $(cat <<''\n", ")\n"],
    ["command-empty-tabs", ": $(cat <<-''\n\t", ")\n"],
    ["backquote-named", ": `cat <<E\nx\nE", "`\n"],
    ["backquote-quoted", ": `cat <<'E'\nx\nE", "`\n"],
    ["backquote-empty", ": `cat <<''\n", "`\n"],
    ["backquote-empty-tabs", ": `cat <<-''\n\t", "`\n"],
    ["backquote-empty-after-body", ": `cat <<''\nx\n", "`\n"],
  ]) {
    const broken = writeSource(
      `${name}-without-terminator-newline`,
      prefix + closing,
    );
    const repaired = writeSource(
      `${name}-with-terminator-newline`,
      `${prefix}\n${closing}`,
    );
    const byte = prefix.length;
    const remove = { byte, deleteBytes: 1, insert: "" };
    const repair = { byte, deleteBytes: 0, insert: "\n" };
    for (const output of parseRecoveryAfterEdits(
      repaired,
      broken,
      name,
      remove,
    )) {
      assert.equal(hasRecovery(output), true, name);
    }
    assertIncrementalEqualsFresh(broken, repaired, `repair-${name}`, repair);
    assertIncrementalEqualsFresh(
      repaired,
      repaired,
      `roundtrip-${name}`,
      remove,
      repair,
    );
  }
});

test("sh: heredoc lookahead terminates at source boundaries and preserves repairs", () => {
  const lookaheadTimeoutMs = 5_000;
  const options = { processTimeoutMs: lookaheadTimeoutMs };
  for (const { name, source, status } of [
    { name: "missing-tab-eof", source: ": [$(cat <<-X\n\t\t", status: 1 },
    {
      name: "missing-tab-backquote",
      source: ": `: [$(cat <<-X\n\t\t`\n",
      status: 1,
    },
    {
      name: "empty-tabs-eof",
      source: "cat <<-''\n\t\t",
      status: 1,
    },
    {
      name: "empty-backquote-bracket",
      source: "echo [`cat <<''\n`]\n",
      status: 1,
    },
    {
      name: "empty-backquote-arithmetic",
      source: "echo $((`cat <<''\n`+1))\n",
      status: 1,
    },
    {
      name: "empty-after-body-bracket",
      source: "echo [`cat <<''\nx\n`]\n",
      status: 1,
    },
    {
      name: "empty-after-body-arithmetic",
      source: "echo $((`cat <<''\nx\n`+1))\n",
      status: 1,
    },
    {
      name: "partial-end-backquote",
      source: "echo [`cat <<X\nX`]\n",
      status: 1,
    },
  ]) {
    const output = runParse({
      ...options,
      source: writeSource(name, source),
      mode: status === 0 ? "valid" : "recovery",
    });
    assert.equal(output.status, status, name);
  }
  const broken = writeSource(
    "bracket-heredoc-unfinished",
    ": `: [$(cat <<X\n)]`\n",
  );
  const repaired = writeSource(
    "bracket-heredoc-terminated",
    ": `: [$(cat <<X\nX\n)]`\n",
  );
  const initial = runParse({ ...options, source: broken, mode: "recovery" });
  assert.equal(initial.status, 1);
  assert.equal(hasRecovery(initial.output), true);
  const fresh = runParse({ ...options, source: repaired });
  const incremental = runParse({
    ...options,
    source: broken,
    expectedSource: repaired,
    edits: [{ byte: 16, deleteBytes: 0, insert: "X\n" }],
  });
  assert.equal(
    cstFingerprint(incremental.output),
    cstFingerprint(fresh.output),
  );
  assertNodeCount(incremental.output, "here_document", 1);
  const removed = runParse({
    ...options,
    source: repaired,
    expectedSource: broken,
    mode: "recovery",
    edits: [{ byte: 16, deleteBytes: 2, insert: "" }],
  });
  assert.equal(removed.status, 1);
  assert.equal(hasRecovery(removed.output), true);
});

test("sh: restoring a subshell before nested here-documents matches a fresh parse", () => {
  const suffix = lines(
    "",
    "# between",
    "case x in x) (cat <<EOF",
    "$(printf value)",
    "EOF",
    ");; esac",
  );
  for (const [broken, restored] of [
    ["()", "(: )"],
    ["(;)", "(: ;)"],
  ]) {
    const initial = writeSource("broken-subshell", `${lines(broken)}${suffix}`);
    const final = writeSource(
      "restored-subshell",
      `${lines(restored)}${suffix}`,
    );
    assertIncrementalEqualsFresh(
      initial,
      final,
      `${broken}: restore subshell body`,
      { byte: 1, deleteBytes: 0, insert: ": " },
    );
  }
});

test("sh: here-document expansions require their closers and recover after repair", () => {
  for (const [name, broken, restored, closer, expectedNode] of [
    ["parameter", "${value", `\${value}`, "}", "parameter_expansion"],
    ["command", "$(printf x", "$(printf x)", ")", "command_substitution"],
    ["arithmetic", "$((1 + 2", "$((1 + 2))", "))", "arithmetic_expansion"],
    ["backquote", "`printf x", "`printf x`", "`", "backquote_substitution"],
    [
      "double-quote",
      '${value:-"text',
      `\${value:-"text"}`,
      '"}',
      "double_quoted",
    ],
    [
      "pattern-single-quote",
      "${value#'text",
      `\${value#'text'}`,
      "'}",
      "single_quoted",
    ],
    [
      "pattern-dollar-single-quote",
      "${value#$'text",
      `\${value#$'text'}`,
      "'}",
      "dollar_single_quoted",
    ],
  ]) {
    const prefix = "cat <<EOF\n";
    const initial = writeSource(
      `here-document-unclosed-${name}`,
      `${prefix}${lines(broken, "EOF", "after")}`,
    );
    const { status } = runParse({
      description: `unclosed here-document ${name}`,
      mode: "recovery",
      source: initial,
    });
    assert.equal(
      status,
      1,
      `${name}: here-document delimiter closed an expansion`,
    );
    const final = writeSource(
      `here-document-closed-${name}`,
      `${prefix}${lines(restored, "EOF", "after")}`,
    );
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      `close-here-document-${name}`,
      {
        byte: prefix.length + broken.length,
        deleteBytes: 0,
        insert: closer,
      },
    )) {
      assertContains(output, expectedNode);
      assertCstRange(output, "2:0-3:0", "here_document_end");
    }
  }
});

test("sh: here-document delimiters bound expansions before their later closers", () => {
  for (const [name, prefix, delimiter, suffix] of [
    ["parameter", "cat <<EOF\n${value:-\n", "EOF\n", "}\nEOF\nafter\n"],
    ["arithmetic", "cat <<EOF\n$((1 +\n", "EOF\n", "))\nEOF\nafter\n"],
    ["command", "cat <<EOF\n$(printf x\n", "EOF\n", ")\nEOF\nafter\n"],
    ["backquote", "cat <<EOF\n`printf x\n", "EOF\n", "`\nEOF\nafter\n"],
    ["double-quote", 'cat <<EOF\n${value:-"\n', "EOF\n", '"}\nEOF\nafter\n'],
    ["single-quote", "cat <<EOF\n${value#'\n", "EOF\n", "'}\nEOF\nafter\n"],
    [
      "dollar-single-quote",
      "cat <<EOF\n${value#$'\n",
      "EOF\n",
      "'}\nEOF\nafter\n",
    ],
    ["nested", "cat <<EOF\n$(cat <<INNER\n", "EOF\n", "INNER\n)\nEOF\nafter\n"],
    [
      "nested-quoted",
      "cat <<EOF\n$(cat <<'INNER'\n",
      "EOF\n",
      "INNER\n)\nEOF\nafter\n",
    ],
    ["stripped-tabs", "cat <<-EOF\n${value:-\n", "\tEOF\n", "}\nEOF\nafter\n"],
    [
      "continued-delimiter",
      "cat <<EOF\n${value:-\n",
      "E\\\nOF\n",
      "}\nEOF\nafter\n",
    ],
  ]) {
    const initial = writeSource(
      `bounded-document-${name}`,
      `${prefix}x${delimiter}${suffix}`,
    );
    const broken = writeSource(
      `early-document-end-${name}`,
      `${prefix}${delimiter}${suffix}`,
    );
    const remove = { byte: prefix.length, deleteBytes: 1, insert: "" };
    for (const output of parseRecoveryAfterEdits(
      initial,
      broken,
      `expose-${name}-delimiter`,
      remove,
    )) {
      assert.ok(
        hasRecovery(output),
        `${name}: expansion crossed the first delimiter`,
      );
    }
    assertIncrementalEqualsFresh(
      initial,
      initial,
      `restore-${name}-body-line`,
      remove,
      { byte: prefix.length, deleteBytes: 0, insert: "x" },
    );
  }
});

test("sh: pending here-documents recover through EOF edits and completion", () => {
  for (const [name, header, ending] of [
    ["plain", "cat <<EOF", "\nbody\nEOF\n"],
    ["quoted", "cat <<'EOF'", "\n$literal\nEOF\n"],
    ["empty-delimiter", "cat <<''", "\nbody\n\n"],
    ["tabs", "cat <<-EOF", "\n\tbody\n\tEOF\n"],
    ["comment", "cat <<EOF # comment", "\nbody\nEOF\n"],
    ["continuation", "cat <<EOF \\\n", "\nbody\nEOF\n"],
    ["following-command", "cat <<EOF; echo after", "\nbody\nEOF\n"],
    ["multiple", "cat <<FIRST <<SECOND", "\nFIRST\nSECOND\n"],
  ]) {
    const complete = writeSource(`${name}-complete-document`, header + ending);
    const incomplete = writeSource(`${name}-pending-document`, header);
    const remove = {
      byte: header.length,
      deleteBytes: ending.length,
      insert: "",
    };
    const restore = { byte: header.length, deleteBytes: 0, insert: ending };
    for (const output of parseRecoveryAfterEdits(
      complete,
      incomplete,
      `${name}-remove-body`,
      remove,
    )) {
      assert.equal(hasRecovery(output), true, output);
    }
    assertIncrementalEqualsFresh(
      incomplete,
      complete,
      `${name}-complete-body`,
      restore,
    );
    assertIncrementalEqualsFresh(
      complete,
      complete,
      `${name}-restore-body`,
      remove,
      restore,
    );
  }
});

test("sh: empty quoted here-document delimiters preserve enclosing source after recovery", () => {
  const fixtures = [
    {
      name: "arithmetic-empty-single-quoted-delimiter",
      initial: lines("echo $(( $(cat <<'", "1", "", ") + 1 ))"),
      source: lines("echo $(( $(cat <<''", "1", "", ") + 1 ))"),
      edit: { byte: 18, deleteBytes: 0, insert: "'" },
      expectedRange: "0:5-3:8",
      expectedNode: "arithmetic_expansion",
      delimiterRange: "0:17-0:19",
      delimiterNode: "single_quoted",
    },
    {
      name: "bracket-empty-double-quoted-delimiter",
      initial: lines('echo [$(cat <<"', "1", "", ")]"),
      source: lines('echo [$(cat <<""', "1", "", ")]"),
      edit: { byte: 15, deleteBytes: 0, insert: '"' },
      expectedRange: "0:5-3:2",
      expectedNode: "pattern_bracket_source",
      delimiterRange: "0:14-0:16",
      delimiterNode: "double_quoted",
    },
    {
      name: "arithmetic-empty-dollar-single-quoted-delimiter",
      initial: lines("echo $(( $(cat <<$'", "1", "", ") + 1 ))"),
      source: lines("echo $(( $(cat <<$''", "1", "", ") + 1 ))"),
      edit: { byte: 19, deleteBytes: 0, insert: "'" },
      expectedRange: "0:5-3:8",
      expectedNode: "arithmetic_expansion",
      delimiterRange: "0:17-0:20",
      delimiterNode: "dollar_single_quoted",
    },
  ];
  for (const fixture of fixtures) {
    const initial = writeSource(`${fixture.name}-initial`, fixture.initial);
    const source = writeSource(fixture.name, fixture.source);
    parseRecovery(initial, `${fixture.name} unfinished quote`);
    for (const output of assertIncrementalEqualsFresh(
      initial,
      source,
      fixture.name,
      fixture.edit,
    )) {
      assertCstRange(output, fixture.expectedRange, fixture.expectedNode);
      assertCstRange(output, fixture.delimiterRange, "here_end");
      assertCstRange(output, fixture.delimiterRange, fixture.delimiterNode);
      assertCstRange(output, "1:0-2:0", "quoted_here_document_body");
      assertCstRange(output, "2:0-3:0", "here_document_end");
    }
  }
});

test("sh: errors preserve source token kinds and repairs restore structure", () => {
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
    [
      "empty-if-lines-before-eof",
      "echo BEFORE\nif\n:; then :;\nfi\n",
      ":; then :;\n",
      "",
    ],
    [
      "empty-while-lines-before-eof",
      "echo BEFORE\nwhile\n:; do :;\ndone\n",
      ":; do :;\n",
      "",
    ],
    [
      "empty-until-lines-before-eof",
      "echo BEFORE\nuntil\n:; do :;\ndone\n",
      ":; do :;\n",
      "",
    ],
    [
      "empty-if-inside-nested-functions",
      lines(
        "echo BEFORE",
        "outer() {",
        "middle() {",
        "inner() {",
        "if",
        ":; then :;",
        "fi",
        "}",
        "}",
        "}",
        "echo AFTER",
      ),
      ":; then :;\n",
      "",
    ],
    [
      "unfinished-parameter-after-function-header-layout",
      lines(
        "echo BEFORE",
        "outer()",
        "{",
        "middle() # header",
        "{",
        "inner() \\",
        "{",
        `echo \${x:-w}`,
        "}",
        "}",
        "}",
      ),
      "}\n}\n}\n}\n",
      "",
    ],
    [
      "missing-body-inside-nested-functions",
      lines(
        "echo BEFORE",
        "outer() {",
        "middle() {",
        "inner() {",
        "leaf() { :; }",
        "}",
        "}",
        "}",
        "echo AFTER",
      ),
      "{ :; }",
      "",
    ],
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

test("sh: repairs restore parameter contexts and nested structures", () => {
  const fixtures = [
    ["unquoted parameter", `echo \${x:-value}\n`, ":-", "!"],
    ["quoted parameter", `echo "\${x:-value}"\n`, ":-", "!"],
    ["nested parameter", `echo \${outer:-\${x:-value}}\n`, "x:-", "x!"],
    [
      "quoted pattern parameter",
      `echo "\${outer#\${x:-value}}"\n`,
      "x:-",
      "x!",
    ],
    ["quoted substitution", `echo "$(echo \${x:-value})"\n`, ":-", "!"],
    [
      "backquote parameter",
      `echo \`\${x:-value}\`\n`,
      ":-value",
      "!`printf value`",
    ],
    ["here parameter", `cat <<END\n\${x:-value}\nEND\n`, ":-", "!"],
    [
      "here pattern parameter",
      `cat <<END\n\${outer%\${x:-value}}\nEND\n`,
      "x:-",
      "x!",
    ],
    [
      "here substitution parameter",
      `cat <<END\n$(echo \${x:-value})\nEND\n`,
      ":-",
      "!",
    ],
    [
      "here parameter with unfinished quoted substitution",
      `cat <<END\n\${x:-$(echo "value")}\nEND\n`,
      ':-$(echo "value")}',
      '!$(echo "value)',
    ],
  ];
  const nestingDepth = 3;
  const hereDelimiters = Array.from(
    { length: nestingDepth },
    (_, index) => `END${index}`,
  );
  for (const [name, prefix, suffix] of [
    ...[
      ["while", "while :; do\n", "done\n"],
      ["until", "until :; do\n", "done\n"],
      ["case", "case x\nin\nx)\n", ";; esac\n"],
      ["assignment", 'x="$(\n', ')"\n'],
      ["declaration assignment", 'export x="$(\n', ')"\n'],
      ["word parts", 'echo prefix"quoted"$(\n', ")\n"],
      ["quoted parts", `echo "one\${x:-a}$(\n`, ')"\n'],
      ["initial quoted parts", `"one\${x:-a}$(\n`, ')"\n'],
    ].map(([name, opening, closing]) => [
      name,
      opening.repeat(nestingDepth),
      closing.repeat(nestingDepth),
    ]),
    [
      "here-document",
      hereDelimiters.map((end) => `cat <<${end} # body\n$(\n`).join(""),
      hereDelimiters
        .toReversed()
        .map((end) => `)\n${end}\n`)
        .join(""),
    ],
  ]) {
    fixtures.push([
      `nested ${name}`,
      `${prefix}echo \${x:-value}\n${suffix}`,
      `}\n${suffix}`,
      "\n",
    ]);
  }
  for (const [name, body, removed, inserted] of fixtures) {
    const contents = `echo BEFORE\n${body}echo AFTER\n`;
    const offset = contents.indexOf(removed);
    assert.notEqual(offset, -1, name);
    const broken =
      contents.slice(0, offset) +
      inserted +
      contents.slice(offset + removed.length);
    const initial = writeSource(`${name}-valid`, contents);
    const damaged = writeSource(`${name}-damaged`, broken);
    assert.equal(
      runParse({ source: damaged, mode: "recovery" }).recovery,
      true,
      name,
    );
    assertRepeatedColdParse("recovery", damaged, name);
    const edit = {
      byte: Buffer.byteLength(contents.slice(0, offset)),
      deleteBytes: Buffer.byteLength(removed),
      insert: inserted,
    };
    const repair = {
      byte: edit.byte,
      deleteBytes: Buffer.byteLength(inserted),
      insert: removed,
    };
    assertIncrementalEqualsFresh(damaged, initial, name, repair);
    assertIncrementalEqualsFresh(initial, initial, name, edit, repair);
  }
});
