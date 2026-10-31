import assert from "node:assert/strict";
import { test } from "node:test";
import {
  assertContains,
  assertCstDirectChildRange,
  assertCstRange,
  assertNodeCount,
  assertNotContains,
  assertOccurrenceCount,
  assertSameLogicalProjection,
  continuationManifest,
  hasRecovery,
  lexicalTexts,
  lineContinuationManifest,
  parseCst,
} from "./support/cst.js";
import {
  applyEdits,
  assertIncrementalEqualsFresh,
  lines,
  parseRecovery,
  parseRecoveryAfterEdits,
  parseValidCst,
  runParse,
  runQuery,
  writeSource,
} from "./support/parser.js";

test("sh: embedded backquote case closers preserve enclosing arithmetic and patterns", () => {
  for (const [name, inner, depth] of [
    ["direct", "case y in esac", 1],
    ["nested", "case y in y) : \\`case z in esac\\`;; z) :;; esac", 2],
  ]) {
    for (const [context, prefix, suffix, node] of [
      ["arithmetic", ": $((1+$(", ")))\n", "arithmetic_expansion"],
      ["pattern", ": [$(", ")]\n", "pattern_bracket_source"],
    ]) {
      const command = `case x in x) : \`${inner}`;
      const tail = `\`;; z) :;; esac${suffix}`;
      const compactText = prefix + command + tail;
      const spacedText = `${prefix}${command} ${tail}`;
      const compact = writeSource(`${name}-${context}-compact`, compactText);
      const spaced = writeSource(`${name}-${context}-spaced`, spacedText);
      const offset = prefix.length + command.length;
      assertSameLogicalProjection(
        `${name}-${context}`,
        parseValidCst(spaced),
        parseValidCst(compact),
      );
      for (const [before, after, deleteBytes, insert] of [
        [spaced, compact, 1, ""],
        [compact, spaced, 0, " "],
      ]) {
        for (const output of assertIncrementalEqualsFresh(
          before,
          after,
          `${name}-${context}`,
          { byte: offset, deleteBytes, insert },
        )) {
          assertOccurrenceCount(output, node, 1);
          assertOccurrenceCount(output, "command_substitution_body", 1);
          assertOccurrenceCount(output, "backquote_substitution_body", depth);
          assertNotContains(output, "subshell");
        }
      }
    }
  }
});

test("sh: nested backquotes preserve arithmetic classification across edits", () => {
  for (const [name, initialBody, finalBody, initialKind, finalKind] of [
    [
      "operator-to-subshell",
      ": $((1 `printf +` 2))",
      ": $((1 `printf +` 2; :))",
      "arithmetic_dynamic_expression",
      "subshell",
    ],
    [
      "assignment-target-to-operand",
      ": $((`printf a` = 1))",
      ": $((`printf a` + 1))",
      "arithmetic_dynamic_expression",
      "arithmetic_binary_expression",
    ],
  ]) {
    for (const depth of [1, 2]) {
      for (const quoted of [false, true]) {
        const wrap = (body) => {
          for (let level = 0; level < depth; level += 1) {
            body = `: \`${body.replaceAll("\\", "\\\\").replaceAll("`", "\\`")}\``;
          }
          return `${quoted ? `: "${body.slice(2)}"` : body}\n`;
        };
        const initialText = wrap(initialBody);
        const finalText = wrap(finalBody);
        const initial = writeSource(`${name}-initial`, initialText);
        const final = writeSource(`${name}-final`, finalText);
        let start = 0;
        while (initialText[start] === finalText[start]) start += 1;
        let suffix = 0;
        while (initialText.at(-suffix - 1) === finalText.at(-suffix - 1)) {
          suffix += 1;
        }
        for (const [before, after, beforeText, afterText, kind] of [
          [initial, final, initialText, finalText, finalKind],
          [final, initial, finalText, initialText, initialKind],
        ]) {
          for (const output of assertIncrementalEqualsFresh(
            before,
            after,
            `${name}-${depth}-${quoted}`,
            {
              byte: start,
              deleteBytes: beforeText.length - suffix - start,
              insert: afterText.slice(start, afterText.length - suffix),
            },
          )) {
            assertOccurrenceCount(output, `${kind}\n`, 1);
            assertOccurrenceCount(
              output,
              "arithmetic_expansion\n",
              kind === "subshell" ? 0 : 1,
            );
            assertOccurrenceCount(
              output,
              "command_substitution\n",
              kind === "subshell" ? 1 : 0,
            );
            assertOccurrenceCount(
              output,
              "backquote_substitution_body",
              depth + 1,
            );
          }
        }
      }
    }
  }
});

test("sh: numeric quote escapes keep their digit limits across edits", () => {
  for (const {
    name,
    initialText,
    finalText,
    edit,
    initialRange,
    finalRange,
    initialTail,
    finalTail,
  } of [
    {
      name: "third-hexadecimal-digit",
      initialText: ": $'\\x12g'\n",
      finalText: ": $'\\x123g'\n",
      edit: { byte: 8, deleteBytes: 0, insert: "3" },
      initialRange: "0:4-0:8",
      finalRange: "0:4-0:8",
      initialTail: "g",
      finalTail: "3g",
    },
    {
      name: "fourth-hexadecimal-digit",
      initialText: ": $'\\x123g'\n",
      finalText: ": $'\\x1234g'\n",
      edit: { byte: 9, deleteBytes: 0, insert: "4" },
      initialRange: "0:4-0:8",
      finalRange: "0:4-0:8",
      initialTail: "3g",
      finalTail: "34g",
    },
    {
      name: "backquote-third-hexadecimal-digit",
      initialText: ": `: $'\\\\x12g'`\n",
      finalText: ": `: $'\\\\x123g'`\n",
      edit: { byte: 12, deleteBytes: 0, insert: "3" },
      initialRange: "0:7-0:12",
      finalRange: "0:7-0:12",
      initialTail: "g",
      finalTail: "3g",
    },
    {
      name: "fifth-unicode-digit",
      initialText: ": $'\\u0041g'\n",
      finalText: ": $'\\u00412g'\n",
      edit: { byte: 10, deleteBytes: 0, insert: "2" },
      initialRange: "0:4-0:10",
      finalRange: "0:4-0:10",
      initialTail: "g",
      finalTail: "2g",
    },
    {
      name: "ninth-unicode-digit",
      initialText: ": $'\\U0001f600g'\n",
      finalText: ": $'\\U0001f6001g'\n",
      edit: { byte: 14, deleteBytes: 0, insert: "1" },
      initialRange: "0:4-0:14",
      finalRange: "0:4-0:14",
      initialTail: "g",
      finalTail: "1g",
    },
  ]) {
    const initial = writeSource(`${name}-initial`, initialText);
    const final = writeSource(`${name}-final`, finalText);
    for (const { before, after, change, range, tail } of [
      {
        before: initial,
        after: final,
        change: edit,
        range: finalRange,
        tail: finalTail,
      },
      {
        before: final,
        after: initial,
        change: { byte: edit.byte, deleteBytes: 1, insert: "" },
        range: initialRange,
        tail: initialTail,
      },
    ]) {
      for (const output of assertIncrementalEqualsFresh(
        before,
        after,
        name,
        change,
      )) {
        assertOccurrenceCount(output, "dollar_single_quote_escape", 1);
        assertCstRange(output, range, "dollar_single_quote_escape");
        const end = Number(range.split("-")[1].split(":")[1]);
        assertCstRange(
          output,
          `0:${end}-0:${end + tail.length}`,
          `dollar_single_quote_text \`${tail}\``,
        );
        assertCstRange(
          output,
          `0:${end + tail.length}-0:${end + tail.length + 1}`,
          JSON.stringify("'"),
        );
      }
    }
  }
});

test("sh: backquote closers delimit keywords and trailing word continuations", () => {
  const contexts = [
    { context: "nested", prefix: ": `: \\`", suffix: "\\``\n", depth: 2 },
    {
      context: "deeper",
      prefix: ": `: \\`: \\\\\\`",
      suffix: "\\\\\\`\\``\n",
      depth: 3,
    },
    { context: "quoted", prefix: ': "`: \\`', suffix: '\\``"\n', depth: 2 },
  ];
  const bodies = [
    {
      bodyName: "conditional",
      body: "if :; then :; fi",
      node: "if_clause",
      terminal: "fi_keyword",
      offset: 14,
      length: 2,
    },
    {
      bodyName: "loop",
      body: "while :; do :; done",
      node: "while_clause",
      terminal: "done_keyword",
      offset: 15,
      length: 4,
    },
    {
      bodyName: "case",
      body: "case x in x) :;; esac",
      node: "case_clause",
      terminal: "esac_keyword",
      offset: 17,
      length: 4,
    },
    {
      bodyName: "brace",
      body: "{ :; }",
      node: "brace_group",
      terminal: '"}"',
      offset: 5,
      length: 1,
    },
    {
      bodyName: "word",
      body: "x",
      node: "simple_command",
      terminal: "literal `x`",
      offset: 0,
      length: 1,
    },
  ];
  const layouts = [
    { layoutName: "adjacent", layout: "", continuationOffsets: [] },
    { layoutName: "continued", layout: "\\\n", continuationOffsets: [0] },
    { layoutName: "folded", layout: "\\\\\n", continuationOffsets: [1] },
    {
      layoutName: "spaced-folded",
      layout: " \\\\\n",
      continuationOffsets: [2],
    },
  ];
  for (const { context, prefix, suffix, depth } of contexts) {
    for (const { bodyName, body, node, terminal, offset, length } of bodies) {
      for (const { layoutName, layout, continuationOffsets } of layouts) {
        const name = `${context}-${bodyName}-${layoutName}`;
        const sourceText = prefix + body + layout + suffix;
        const source = writeSource(name, sourceText);
        const boundary = prefix.length + body.length;
        const spacedText = `${sourceText.slice(0, boundary)} ${sourceText.slice(boundary)}`;
        const spaced = writeSource(`${name}-spaced`, spacedText);
        for (const output of assertIncrementalEqualsFresh(
          spaced,
          source,
          name,
          { byte: boundary, deleteBytes: 1, insert: "" },
        )) {
          assertContains(output, node);
          assertOccurrenceCount(output, "backquote_substitution_body", depth);
          assertCstRange(
            output,
            `0:${prefix.length + offset}-0:${prefix.length + offset + length}`,
            terminal,
          );
        }
        assert.deepEqual(
          lineContinuationManifest(runQuery(source)),
          continuationOffsets.map(
            (column) => `0:${boundary + column}-0:${boundary + column + 1}`,
          ),
          name,
        );
      }
    }
  }
});

test("sh: tilde prefixes retain ownership across bracket and backquote edits", () => {
  for (const { name, contents, prefixes } of [
    { name: "complete bracket", contents: "A=[a:~/x]\n", prefixes: ["~"] },
    { name: "incomplete bracket", contents: "A=[a:~/x\n", prefixes: ["~"] },
    { name: "nested bracket", contents: "A=[a[b:~/x]]\n", prefixes: ["~"] },
    {
      name: "closing bracket in login name",
      contents: "A=[a:~root]/x\n",
      prefixes: ["~root]"],
    },
    {
      name: "character class",
      contents: "A=[[:alpha:]:~/x]\n",
      prefixes: ["~"],
    },
    { name: "class marker colon", contents: "A=[[:~/x]]\n", prefixes: ["~"] },
    { name: "collating symbol", contents: "A=[[.a:~/x.]]\n", prefixes: ["~"] },
    { name: "equivalence class", contents: "A=[[=a:~/x=]]\n", prefixes: ["~"] },
    { name: "two prefixes", contents: "A=[a:~/x]:~/y\n", prefixes: ["~", "~"] },
    {
      name: "parameter after colon",
      contents: `A=[a:\${x}:~/x]\n`,
      prefixes: ["~"],
    },
    {
      name: "command after colon",
      contents: "A=[a:$(printf x):~/x]\n",
      prefixes: ["~"],
    },
    {
      name: "backquote after colon",
      contents: "A=[a:`printf x`:~/x]\n",
      prefixes: ["~"],
    },
    {
      name: "nested backquote",
      contents: ": `: \\`A=[a:~x\\``\n",
      prefixes: ["~x"],
    },
    {
      name: "nested backquote assignment",
      contents: ": `: \\`A=~x\\``\n",
      prefixes: ["~x"],
    },
    {
      name: "nested backquote argument",
      contents: ": `: \\`: ~x\\``\n",
      prefixes: ["~x"],
    },
    { name: "escaped colon", contents: "A=[a\\:~/x]\n", prefixes: [] },
    { name: "quoted colon", contents: 'A=[a":"~/x]\n', prefixes: [] },
    { name: "nested colon", contents: `A=[a\${x:-:}~/x]\n`, prefixes: [] },
    { name: "quoted tilde", contents: 'A=[a:"~"/x]\n', prefixes: [] },
    { name: "ordinary word", contents: ": [a:~/x]\n", prefixes: [] },
    { name: "parameter word", contents: `: \${x:-[a:~/x]}\n`, prefixes: [] },
  ]) {
    const initial = writeSource(name, contents.replaceAll("~", "Q"));
    const final = writeSource(name, contents);
    const edits = Array.from(contents.matchAll(/~/g), (match) => ({
      byte: match.index,
      deleteBytes: 1,
      insert: "~",
    }));
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      name,
      ...edits,
    )) {
      assertOccurrenceCount(output, "tilde_expansion", prefixes.length);
      let offset = 0;
      for (const prefix of prefixes) {
        const start = contents.indexOf(prefix, offset);
        const end = start + prefix.length;
        assertCstRange(output, `0:${start}-0:${end}`, "tilde_expansion");
        offset = end;
      }
    }
  }
});

test("sh: runtime fragments can complete arithmetic lexemes and productions", () => {
  for (const { name, expression, completion, completed, lexemes = [] } of [
    {
      name: "hexadecimal-prefix",
      expression: `0x\${x}`,
      completion: "F",
      completed: "0xF",
      lexemes: [["0x", "arithmetic_source_fragment"]],
    },
    {
      name: "uppercase-hexadecimal-prefix",
      expression: `0X\${x}`,
      completion: "F",
      completed: "0XF",
      lexemes: [["0X", "arithmetic_source_fragment"]],
    },
    {
      name: "octal-suffix-completed-as-decimal",
      expression: `\${x}08`,
      completion: "1",
      completed: "108",
      lexemes: [["08", "arithmetic_source_fragment"]],
    },
    {
      name: "numeric-suffix-completed-as-name",
      expression: `\${x}123abc`,
      completion: "value",
      completed: "value123abc",
      lexemes: [["123abc", "arithmetic_source_fragment"]],
    },
    {
      name: "nonhexadecimal-suffix-completed-as-name",
      expression: `\${x}0xg`,
      completion: "value",
      completed: "value0xg",
      lexemes: [["0xg", "arithmetic_source_fragment"]],
    },
    {
      name: "parenthesized-hexadecimal-prefix",
      expression: `(0x\${x}) * 2`,
      completion: "F",
      completed: "(0xF) * 2",
      lexemes: [["0x", "arithmetic_source_fragment"]],
    },
    {
      name: "name-prefix",
      expression: `foo\${x}`,
      completion: "bar",
      completed: "foobar",
      lexemes: [["foo", "arithmetic_variable"]],
    },
    {
      name: "octal-prefix",
      expression: `0\${x}`,
      completion: "7",
      completed: "07",
    },
    {
      name: "empty-numeric-fragment",
      expression: `1\${x}2`,
      completion: "",
      completed: "12",
    },
    {
      name: "numeric-suffix-and-operator",
      expression: `1\${x} 2`,
      completion: "+",
      completed: "1+ 2",
    },
    {
      name: "equality-operator",
      expression: `1 =\${x} 2`,
      completion: "=",
      completed: "1 == 2",
    },
    {
      name: "conditional-prefix",
      expression: `\${x} : 2`,
      completion: "1 ? 3",
      completed: "1 ? 3 : 2",
    },
    {
      name: "conditional-suffix",
      expression: `1 ? \${x}`,
      completion: "2 : 3",
      completed: "1 ? 2 : 3",
    },
    {
      name: "assignment-target",
      expression: `\${x} = 1`,
      completion: "a",
      completed: "a = 1",
    },
    {
      name: "parenthesized-assignment-target",
      expression: `(\${x}) = 1`,
      completion: "a",
      completed: "(a) = 1",
    },
    {
      name: "conditional-assignment-consequence",
      expression: `a + \${x} = 2 : 3`,
      completion: "b ? c",
      completed: "a + b ? c = 2 : 3",
    },
    {
      name: "parenthesized-operand",
      expression: `(1 \${x}) * 2`,
      completion: "+ 3",
      completed: "(1 + 3) * 2",
    },
    {
      name: "operator-before-group",
      expression: `\${x}(1 + 2)`,
      completion: "2 * ",
      completed: "2 * (1 + 2)",
    },
  ]) {
    assert.equal(expression.replace(`\${x}`, completion), completed, name);
    for (const [variant, value] of [
      ["runtime", expression],
      ["completion", completed],
    ]) {
      const contents = `echo $((${value}))\n`;
      const output = parseValidCst(writeSource(`${name}-${variant}`, contents));
      assertOccurrenceCount(output, "arithmetic_expansion\n", 1);
      assertOccurrenceCount(output, "command_substitution\n", 0);
      assertCstRange(
        output,
        `0:5-0:${contents.length - 1}`,
        "arithmetic_expansion",
      );
      assertOccurrenceCount(
        output,
        "arithmetic_source_fragment",
        variant === "runtime"
          ? lexemes.filter(([, type]) => type === "arithmetic_source_fragment")
              .length
          : 0,
      );
      if (variant === "runtime") {
        for (const [spelling, type] of lexemes) {
          const start = contents.indexOf(spelling, 8);
          assertCstRange(
            output,
            `0:${start}-0:${start + spelling.length}`,
            type,
          );
        }
      }
    }
  }
});

test("sh: dynamic arithmetic retains later runtime fragments across edits", () => {
  for (const {
    name,
    prefix,
    fragment,
    suffix,
    parameters,
    arithmetic,
    commands,
    backquotes,
  } of [
    {
      name: "adjacent-parameters",
      prefix: "a$x",
      fragment: "$y",
      suffix: "",
      parameters: 2,
      arithmetic: 1,
      commands: 0,
      backquotes: 0,
    },
    {
      name: "braced-parameters",
      prefix: `a\${x}`,
      fragment: `\${y}`,
      suffix: "",
      parameters: 2,
      arithmetic: 1,
      commands: 0,
      backquotes: 0,
    },
    {
      name: "later-variable-suffix",
      prefix: "a$x + a",
      fragment: "$y",
      suffix: "",
      parameters: 2,
      arithmetic: 1,
      commands: 0,
      backquotes: 0,
    },
    {
      name: "parenthesized-parameters",
      prefix: "(a$x",
      fragment: "$y",
      suffix: ")",
      parameters: 2,
      arithmetic: 1,
      commands: 0,
      backquotes: 0,
    },
    {
      name: "later-command-substitution",
      prefix: "a$x",
      fragment: "$(echo y)",
      suffix: "",
      parameters: 1,
      arithmetic: 1,
      commands: 1,
      backquotes: 0,
    },
    {
      name: "later-arithmetic-expansion",
      prefix: "a$x",
      fragment: "$((1))",
      suffix: "",
      parameters: 1,
      arithmetic: 2,
      commands: 0,
      backquotes: 0,
    },
    {
      name: "later-backquote-substitution",
      prefix: "a$x",
      fragment: "`echo y`",
      suffix: "",
      parameters: 1,
      arithmetic: 1,
      commands: 0,
      backquotes: 1,
    },
  ]) {
    const initial = writeSource(
      `${name}-initial`,
      `echo $((${prefix}${suffix}))\n`,
    );
    const final = writeSource(
      `${name}-final`,
      `echo $((${prefix}${fragment}${suffix}))\n`,
    );
    const byte = 8 + prefix.length;
    for (const output of assertIncrementalEqualsFresh(initial, final, name, {
      byte,
      deleteBytes: 0,
      insert: fragment,
    })) {
      assertContains(output, "arithmetic_dynamic_expression\n");
      assertOccurrenceCount(output, "parameter_expansion\n", parameters);
      assertOccurrenceCount(output, "arithmetic_expansion\n", arithmetic);
      assertOccurrenceCount(output, "command_substitution\n", commands);
      assertOccurrenceCount(output, "backquote_substitution\n", backquotes);
      assertNotContains(output, "subshell");
    }
    assertIncrementalEqualsFresh(final, initial, `${name}-remove`, {
      byte,
      deleteBytes: fragment.length,
      insert: "",
    });
  }
});

test("sh: arithmetic viability follows edits through nested scanner callers", () => {
  for (const { name, initial, final, range, outerArithmetic } of [
    {
      name: "plain-substitution",
      initial: "echo $((1 $x))\n",
      final: "echo $((: $x))\n",
      range: "0:5-0:14",
      outerArithmetic: 0,
    },
    {
      name: "nested-arithmetic",
      initial: "echo $((1 + $((1 $x))))\n",
      final: "echo $((1 + $((: $x))))\n",
      range: "0:12-0:21",
      outerArithmetic: 1,
    },
    {
      name: "quoted-parameter-word",
      initial: `echo "\${value:-$((1 $x))}"\n`,
      final: `echo "\${value:-$((: $x))}"\n`,
      range: "0:15-0:24",
      outerArithmetic: 0,
    },
    {
      name: "here-document-body",
      initial: "cat <<END\n$((1 $x))\nEND\n",
      final: "cat <<END\n$((: $x))\nEND\n",
      range: "1:0-1:9",
      outerArithmetic: 0,
    },
    {
      name: "backquote-command",
      initial: "echo `echo $((1 $x))`\n",
      final: "echo `echo $((: $x))`\n",
      range: "0:11-0:20",
      outerArithmetic: 0,
    },
  ]) {
    const initialSource = writeSource(`${name}-viable`, initial);
    const finalSource = writeSource(`${name}-impossible`, final);
    const edits = [];
    for (
      let byte = initial.indexOf("1 $x");
      byte !== -1;
      byte = initial.indexOf("1 $x", byte + 1)
    ) {
      edits.push({ byte, deleteBytes: 1, insert: ":" });
    }
    for (const output of assertIncrementalEqualsFresh(
      initialSource,
      finalSource,
      `${name}-select-command-substitution`,
      ...edits,
    )) {
      assertCstRange(output, range, "command_substitution");
      assertOccurrenceCount(output, "arithmetic_expansion\n", outerArithmetic);
      assertOccurrenceCount(output, "command_substitution\n", 1);
    }
    for (const output of assertIncrementalEqualsFresh(
      finalSource,
      initialSource,
      `${name}-restore-arithmetic-expansion`,
      ...edits.map((edit) => ({ ...edit, insert: "1" })),
    )) {
      assertCstRange(output, range, "arithmetic_expansion");
      assertOccurrenceCount(
        output,
        "arithmetic_expansion\n",
        outerArithmetic + 1,
      );
      assertOccurrenceCount(output, "command_substitution\n", 0);
    }
  }
});

test("sh: character-class source colons close only before a right bracket", () => {
  for (const [
    name,
    initial,
    final,
    byte,
    classRange,
    contentRange,
    content,
    closerRange,
  ] of [
    [
      "assignment-adjacent-colons",
      "x=[[:a:]]\n",
      "x=[[:a::]]\n",
      6,
      "0:3-0:9",
      "0:5-0:7",
      "a:",
      "0:7-0:8",
    ],
    [
      "word-interior-colon",
      "echo [[:ab:]]\n",
      "echo [[:a:b:]]\n",
      9,
      "0:6-0:13",
      "0:8-0:11",
      "a:b",
      "0:11-0:12",
    ],
    [
      "parameter-pattern-colon",
      `echo \${x#[[:a:]]}\n`,
      `echo \${x#[[:a::]]}\n`,
      13,
      "0:10-0:16",
      "0:12-0:14",
      "a:",
      "0:14-0:15",
    ],
    [
      "tilde-user-colon",
      "echo ~[[:a:]]\n",
      "echo ~[[:a::]]\n",
      10,
      "0:7-0:13",
      "0:9-0:11",
      "a:",
      "0:11-0:12",
    ],
    [
      "colon-before-expansion",
      "echo [[:a$x:]]\n",
      "echo [[:a:$x:]]\n",
      9,
      "0:6-0:14",
      "0:8-0:10",
      "a:",
      "0:12-0:13",
    ],
  ]) {
    for (const output of assertIncrementalEqualsFresh(
      writeSource(`${name}-initial`, initial),
      writeSource(`${name}-final`, final),
      name,
      { byte, deleteBytes: 0, insert: ":" },
    )) {
      assertCstDirectChildRange(
        output,
        classRange,
        "member: pattern_character_class_source",
        contentRange,
        `content: pattern_character_class_content_source \`${content}\``,
      );
      assertCstDirectChildRange(
        output,
        classRange,
        "member: pattern_character_class_source",
        closerRange,
        '":"',
      );
    }
  }
});

test("sh: bracket ranges consume source endpoints from left to right", () => {
  for (const { name, pattern, ranges } of [
    { name: "shared-endpoint", pattern: "[a-c-e]", ranges: [[1, 4]] },
    {
      name: "separate-ranges",
      pattern: "[a-c-e-g]",
      ranges: [
        [1, 4],
        [5, 8],
      ],
    },
    { name: "trailing-hyphens", pattern: "[a-b--]", ranges: [[1, 4]] },
    {
      name: "hyphen-start-after-range",
      pattern: "[a-b--c]",
      ranges: [
        [1, 4],
        [4, 7],
      ],
    },
    { name: "repeated-hyphens", pattern: "[-----]", ranges: [[1, 4]] },
    { name: "repeated-endpoints", pattern: "[a-a-a]", ranges: [[1, 4]] },
    { name: "trailing-hyphen", pattern: "[a-]", ranges: [] },
    { name: "class-after-hyphen", pattern: "[a-[:alpha:]]", ranges: [[1, 4]] },
    { name: "class-before-hyphen", pattern: "[[:alpha:]-z]", ranges: [] },
    { name: "equivalence-after-hyphen", pattern: "[a-[=x=]]", ranges: [] },
    { name: "collating-endpoint", pattern: "[a-[.ch.]-z]", ranges: [[1, 9]] },
    { name: "quoted-endpoints", pattern: "['a'-'c'-e]", ranges: [[1, 8]] },
    { name: "expanded-endpoint", pattern: `[a-\${x}-e]`, ranges: [[1, 7]] },
    { name: "initial-right-bracket", pattern: "[]-a-c]", ranges: [[1, 4]] },
    { name: "left-bracket-endpoint", pattern: "[a-[]", ranges: [[1, 4]] },
    { name: "escaped-right-bracket", pattern: "[a-\\]-e]", ranges: [[1, 5]] },
  ]) {
    for (const [context, prefix, suffix] of [
      ["word", "printf ", "\n"],
      ["assignment", "value=", "\n"],
      ["parameter", ': "${v#', '}"\n'],
      ["case", "case v in ", ") :;; esac\n"],
    ]) {
      const source = writeSource(
        `range-${name}-${context}`,
        prefix + pattern + suffix,
      );
      const output = parseValidCst(source);
      assertOccurrenceCount(output, "pattern_bracket_source", 1);
      assertOccurrenceCount(
        output,
        "pattern_bracket_range_source",
        ranges.length,
      );
      for (const [start, end] of ranges) {
        assertCstRange(
          output,
          `0:${prefix.length + start}-0:${prefix.length + end}`,
          "pattern_bracket_range_source",
        );
      }
    }
  }
});

test("sh: right-hand character class shapes and collating symbols own different bracket boundaries", () => {
  for (const [context, prefix, suffix] of [
    ["word", "printf ", "\n"],
    ["assignment", "value=", "\n"],
    ["parameter", ': "${v#', '}"\n'],
    ["case", "case v in ", ") :;; esac\n"],
  ]) {
    const initialText = `${prefix}[Xa-[:digit:]]${suffix}`;
    const finalText = `${prefix}[Xa-[.c.]]${suffix}`;
    const initial = writeSource(`${context}-class-range-end`, initialText);
    const final = writeSource(`${context}-collating-range-end`, finalText);
    const edit = {
      byte: prefix.length + 4,
      deleteBytes: 9,
      insert: "[.c.]",
    };
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      `${context}-class-to-collating`,
      edit,
    )) {
      assertOccurrenceCount(output, "pattern_collating_symbol_source", 1);
      assertCstRange(
        output,
        `0:${prefix.length}-0:${prefix.length + 10}`,
        "pattern_bracket_source",
      );
    }
    for (const output of assertIncrementalEqualsFresh(
      final,
      initial,
      `${context}-collating-to-class`,
      { byte: prefix.length + 4, deleteBytes: 5, insert: "[:digit:]" },
    )) {
      assertOccurrenceCount(output, "pattern_character_class_source", 0);
      assertCstRange(
        output,
        `0:${prefix.length}-0:${prefix.length + 13}`,
        "pattern_bracket_source",
      );
      assertCstDirectChildRange(
        output,
        `0:${prefix.length + 2}-0:${prefix.length + 5}`,
        "member: pattern_bracket_range_source",
        `0:${prefix.length + 4}-0:${prefix.length + 5}`,
        "end: pattern_bracket_character_source `[`",
      );
      assertCstRange(
        output,
        `0:${prefix.length + 13}-0:${prefix.length + 14}`,
        "literal `]`",
      );
    }
  }
});

test("sh: long unterminated quotes parse through EOF with native recovery", () => {
  const quoteContentBytes = 80_000;
  const source = writeSource(
    "long-unterminated-quote",
    `printf "${"x".repeat(quoteContentBytes)}`,
  );
  const result = runParse({
    source,
    mode: "recovery",
    description: "long unterminated quote",
  });
  assert.equal(hasRecovery(result.output), true);
});

const fixtures = [
  {
    name: "ordinary punctuation stays in one literal run",
    source: ": foo/§bar:baz#x=~$§.\n",
    type: "literal",
    texts: [":", "foo/bar:baz#x=~$."],
    brackets: 0,
  },
  {
    name: "unclosed brackets stay in one literal run",
    source: ": [§[§prefix\n",
    type: "literal",
    texts: [":", "[[prefix"],
    brackets: 0,
  },
  {
    name: "failed outer class opener preserves its complete inner bracket",
    source: ": [§[:§invalid]§suffix\n",
    type: "literal",
    texts: [":", "[", "suffix"],
    brackets: 1,
  },
  {
    name: "nested failed class prefixes retain the final complete bracket",
    source: ": [[:§[§[:§]\n",
    type: "literal",
    texts: [":", "[[:["],
    brackets: 1,
  },
  {
    name: "deferred bracket literals resume after a wildcard",
    source: ": [§*§[§ab§c\n",
    type: "literal",
    texts: [":", "[", "[abc"],
    brackets: 0,
  },
  {
    name: "quoted incomplete prefixes retain repeated complete suffixes",
    source: ': [§a"x"§*§[.§][§a"x"§*§[.§]\n',
    type: "literal",
    texts: [":", "[a", "[a"],
    brackets: 2,
  },
  {
    name: "fallback scope resumes after a nested command source",
    source: ": [§a$( : [§b)§tail\n",
    type: "literal",
    texts: [":", "[a", ":", "[b", "tail"],
    brackets: 0,
  },
  {
    name: "parameter incomplete classes preserve the inner complete bracket",
    source: `: \${x#[§[:§$ §]}\n`,
    type: "literal",
    texts: [":", "["],
    brackets: 1,
  },
  {
    name: "tilde user boundaries precede apparent bracket closers",
    source: ": ~[§a§/b]\n",
    type: "literal",
    texts: [":", "[a", "/b]"],
    brackets: 0,
    tildes: 1,
  },
  {
    name: "assignment fallback keeps the tilde colon in one literal",
    source: "A=[§a§:§~/x]\n",
    type: "literal",
    texts: ["[a:", "/x]"],
    brackets: 0,
    tildes: 1,
  },
  {
    name: "assignment class prefixes yield to a following tilde",
    source: "A=[§[:§~/x:]]\n",
    type: "literal",
    texts: ["[[:", "/x:]]"],
    brackets: 0,
    tildes: 1,
  },
  {
    name: "bracket punctuation has individual character owners",
    source: ": [a§!§:§.§=§$§]\n",
    type: "pattern_bracket_character_source",
    texts: ["a", "!", ":", ".", "=", "$"],
    brackets: 1,
  },
  {
    name: "collating symbols keep every logical character",
    source: ": [§[§.c§h.§]§]\n",
    type: "pattern_collating_symbol_character_source",
    texts: ["c", "h"],
    brackets: 1,
  },
  {
    name: "collating marker inside content is a character",
    source: ": [[.a§.§b.]]\n",
    type: "pattern_collating_symbol_character_source",
    texts: ["a", ".", "b"],
    brackets: 1,
  },
  {
    name: "equivalence marker inside content is a character",
    source: ": [[=a§=§b=]]\n",
    type: "pattern_equivalence_class_character_source",
    texts: ["a", "=", "b"],
    brackets: 1,
  },
  {
    name: "equivalence classes retain delimiter ownership",
    source: ": [§[§=x§=§]§]\n",
    type: "pattern_equivalence_class_character_source",
    texts: ["x"],
    brackets: 1,
  },
  {
    name: "parameter brackets use the same character owners",
    source: `: \${v#[a§!§:§.§=§$§]}\n`,
    type: "pattern_bracket_character_source",
    texts: ["a", "!", ":", ".", "=", "$"],
    brackets: 1,
  },
  {
    name: "ordinary tilde after a quote remains literal",
    source: ': "x"§~roo§t\n',
    type: "literal",
    texts: [":", "~root"],
    brackets: 0,
    tildes: 0,
  },
  {
    name: "parameter tilde after a quote remains literal",
    source: `: \${v:-"x"§~roo§t}\n`,
    type: "literal",
    texts: [":", "~root"],
    brackets: 0,
    tildes: 0,
  },
];

for (const fixture of fixtures) {
  test(`pattern source: ${fixture.name}`, () => {
    let logicalOutput;
    for (const continuation of ["", "\\\n"]) {
      const source = fixture.source.replaceAll("§", continuation);
      const output = parseValidCst(writeSource(fixture.name, source));
      assert.deepEqual(
        lexicalTexts(output, source, fixture.type),
        fixture.texts,
      );
      assertNodeCount(output, "pattern_bracket_source", fixture.brackets);
      if (fixture.tildes !== undefined)
        assertNodeCount(output, "tilde_expansion", fixture.tildes);
      if (logicalOutput === undefined) logicalOutput = output;
      else assertSameLogicalProjection(fixture.name, logicalOutput, output);
    }
  });
}

test("backquote escape decoding resumes at the next bracket character", () => {
  for (const count of [1, 2, 3, 4]) {
    for (const suffix of ["]", "a]", "-a]"]) {
      const source = `: \`: [${"\\".repeat(count)}${suffix}\`\n`;
      const output = parseValidCst(
        writeSource(`bracket-escape-${count}-${suffix}`, source),
      );
      const escapedEnd = 6 + count + (count < 3 ? 1 : 0);
      assertCstRange(output, `0:6-0:${escapedEnd}`, "escaped_character");
      assertNodeCount(output, "escaped_character", 1);
      assertNodeCount(
        output,
        "pattern_bracket_source",
        suffix === "]" && count < 3 ? 0 : 1,
      );
      assertNodeCount(
        output,
        "pattern_bracket_range_source",
        suffix === "-a]" && count >= 3 ? 1 : 0,
      );
    }
  }
});

test("parameter word escapes cannot become continuation extras", () => {
  for (const fixture of [
    {
      name: "unquoted default escaped closer",
      initial: `echo \${#-}\n`,
      source: `echo \${#-\\}}\n`,
      insert: "\\}",
      byte: 9,
      escape: "escaped_character",
      range: "0:9-0:11",
    },
    {
      name: "quoted default escaped backslash",
      initial: `echo "\${#-}"\n`,
      source: `echo "\${#-\\\\}"\n`,
      insert: "\\\\",
      byte: 10,
      escape: "double_quote_escape",
      range: "0:10-0:12",
    },
    {
      name: "quoted error escaped closer",
      initial: `echo "\${#?}"\n`,
      source: `echo "\${#?\\}}"\n`,
      insert: "\\}",
      byte: 10,
      escape: "double_quote_escape",
      range: "0:10-0:12",
    },
    {
      name: "unquoted error escaped backslash",
      initial: `echo \${#?}\n`,
      source: `echo \${#?\\\\}\n`,
      insert: "\\\\",
      byte: 9,
      escape: "escaped_character",
      range: "0:9-0:11",
    },
  ]) {
    const initial = writeSource(`${fixture.name}-initial`, fixture.initial);
    const source = writeSource(fixture.name, fixture.source);
    const output = parseValidCst(source);
    assertNodeCount(output, "parameter_length_operator", 0);
    assertNodeCount(output, "parameter_value_operator", 1);
    assertNodeCount(output, "parameter_word", 1);
    assertNodeCount(output, fixture.escape, 1);
    assertCstRange(output, fixture.range, fixture.escape);
    assert.deepEqual(continuationManifest(output), [], fixture.name);
    assertIncrementalEqualsFresh(initial, source, fixture.name, {
      byte: fixture.byte,
      deleteBytes: 0,
      insert: fixture.insert,
    });
  }
});

test("sh: quoting a backquote changes comment endings before inner tokenization", () => {
  const body = "`a #x\\\nb\n`";
  const unquotedText = `echo ${body}\n`;
  const unquoted = writeSource("unquoted-backquote-comment", unquotedText);
  const quoted = writeSource("quoted-backquote-comment", `echo "${body}"\n`);
  const initialOutput = parseValidCst(unquoted);
  assertOccurrenceCount(initialOutput, "name: cmd_name", 3);
  assertOccurrenceCount(initialOutput, "comment\n", 1);
  assertNodeCount(initialOutput, "comment_text", 1);
  assertCstRange(initialOutput, "0:8-0:11", "comment");
  assertCstRange(initialOutput, "0:8-0:11", "comment_text");
  assert.deepEqual(continuationManifest(initialOutput), []);
  const output = parseValidCst(quoted);
  assertOccurrenceCount(output, "name: cmd_name", 2);
  assertOccurrenceCount(output, "comment\n", 1);
  assertNodeCount(output, "comment_text", 1);
  assertCstRange(output, "0:9-1:1", "comment");
  assertCstRange(output, "0:9-1:1", "comment_text");
  assertCstRange(output, "0:11-0:12", "line_continuation");
  assertIncrementalEqualsFresh(
    unquoted,
    quoted,
    "quote-backquote-comment",
    { byte: 5, deleteBytes: 0, insert: '"' },
    { byte: unquotedText.length, deleteBytes: 0, insert: '"' },
  );
  assertIncrementalEqualsFresh(
    quoted,
    unquoted,
    "unquote-backquote-comment",
    { byte: unquotedText.length, deleteBytes: 1, insert: "" },
    { byte: 5, deleteBytes: 1, insert: "" },
  );
});

test("sh: quoted backquote ancestors remove continuations before inner tokenization", () => {
  for (const { count, ranges } of [
    { count: 7, ranges: ["0:10-0:16"] },
    { count: 15, ranges: ["0:10-0:18", "0:18-0:24"] },
  ]) {
    const body = `\`: \\\`: ${"\\".repeat(count)}\n :\\\`\``;
    const unquotedText = `: ${body}\n`;
    const unquoted = writeSource(`unquoted-ancestor-${count}`, unquotedText);
    const quoted = writeSource(`quoted-ancestor-${count}`, `: "${body}"\n`);
    const initialOutput = parseValidCst(unquoted);
    assertOccurrenceCount(initialOutput, "name: cmd_name", 4);
    assert.deepEqual(continuationManifest(initialOutput), []);
    const output = parseValidCst(quoted);
    assertOccurrenceCount(output, "name: cmd_name", 3);
    assert.deepEqual(
      parseCst(output)
        .filter((entry) => entry.content.startsWith("escaped_character "))
        .map((entry) => entry.range),
      ranges,
    );
    assertCstRange(
      output,
      `0:${9 + count}-0:${10 + count}`,
      "line_continuation",
    );
    assertIncrementalEqualsFresh(
      unquoted,
      quoted,
      `quote-ancestor-${count}`,
      { byte: 2, deleteBytes: 0, insert: '"' },
      { byte: unquotedText.length, deleteBytes: 0, insert: '"' },
    );
    assertIncrementalEqualsFresh(
      quoted,
      unquoted,
      `unquote-ancestor-${count}`,
      { byte: unquotedText.length, deleteBytes: 1, insert: "" },
      { byte: 2, deleteBytes: 1, insert: "" },
    );
  }
});

test("sh: backquote escape leaves retain their full source across run edits", () => {
  const prefix = "before\n: `: ";
  const suffix = "* tail`\nafter\n";
  const cases = [
    { count: 1, ranges: ["1:5-1:7"] },
    { count: 2, ranges: ["1:5-1:8"] },
    { count: 3, ranges: ["1:5-1:8"] },
    { count: 4, ranges: ["1:5-1:9"] },
    { count: 5, ranges: ["1:5-1:9", "1:9-1:11"] },
    { count: 6, ranges: ["1:5-1:9", "1:9-1:12"] },
    { count: 7, ranges: ["1:5-1:9", "1:9-1:12"] },
    { count: 8, ranges: ["1:5-1:9", "1:9-1:13"] },
  ].map(({ count, ranges }) => {
    const source = writeSource(
      `backquote-escape-${count}`,
      prefix + "\\".repeat(count) + suffix,
    );
    const output = parseValidCst(source);
    assert.deepEqual(
      parseCst(output)
        .filter((entry) => entry.content.startsWith("escaped_character "))
        .map((entry) => entry.range),
      ranges,
      `${count} raw backslashes before a pattern character`,
    );
    return { count, source };
  });

  for (let index = 1; index < cases.length; index += 1) {
    const previous = cases[index - 1];
    const current = cases[index];
    const byte = prefix.length + previous.count;
    assertIncrementalEqualsFresh(
      previous.source,
      current.source,
      `extend-backquote-escape-${previous.count}`,
      { byte, deleteBytes: 0, insert: "\\" },
    );
    assertIncrementalEqualsFresh(
      current.source,
      previous.source,
      `shorten-backquote-escape-${current.count}`,
      { byte, deleteBytes: 1, insert: "" },
    );
  }
});

test("sh: backquote dollar prefixes change from expansions to text after edits", () => {
  for (const { name, prefix, suffix, leaf, width = 2 } of [
    { name: "argument", prefix: ": `: ", suffix: " `\n", leaf: "literal" },
    {
      name: "double-quoted",
      prefix: ': `: "',
      suffix: ' "`\n',
      leaf: "double_quote_text",
      width: 3,
    },
    {
      name: "parameter-word",
      prefix: ": `: ${x:-",
      suffix: " }`\n",
      leaf: "literal",
      width: 3,
    },
    {
      name: "quoted-parameter-word",
      prefix: ': `: "${x:-',
      suffix: ' }"`\n',
      leaf: "double_quote_text",
      width: 3,
    },
  ]) {
    const initial = writeSource(
      `backquote-dollar-${name}-expansion`,
      `${prefix}\\$x${suffix}`,
    );
    const final = writeSource(
      `backquote-dollar-${name}-text`,
      `${prefix}\\$${suffix}`,
    );
    const output = parseValidCst(final);
    assertCstRange(
      output,
      `0:${prefix.length}-0:${prefix.length + width}`,
      leaf,
    );
    assertNotContains(output, "escaped_character");
    assertNotContains(output, "double_quote_escape");
    assertIncrementalEqualsFresh(initial, final, `dollar-${name}-to-text`, {
      byte: prefix.length + 2,
      deleteBytes: 1,
      insert: "",
    });
    assertIncrementalEqualsFresh(
      final,
      initial,
      `dollar-${name}-to-expansion`,
      {
        byte: prefix.length + 2,
        deleteBytes: 0,
        insert: "x",
      },
    );
  }
});

test("sh: named tilde prefixes end before backquote continuations after edits", () => {
  for (const [name, prefix, suffix] of [
    ["argument", ": `: ", " :`\n"],
    ["assignment", ": `A=", " :`\n"],
    ["filename", ": `: >", " :`\n"],
    ["case-pattern", ": `case x in ", " ) :;; esac`\n"],
    ["for-wordlist", ": `for x in ", " ; do :; done`\n"],
  ]) {
    const initial = writeSource(
      `tilde-${name}-adjacent`,
      `${prefix}~user${suffix}`,
    );
    const final = writeSource(
      `tilde-${name}-continued`,
      `${prefix}~user${"\\".repeat(2)}\n${suffix}`,
    );
    const initialOutput = parseValidCst(initial);
    const output = parseValidCst(final);
    assertCstRange(
      initialOutput,
      `0:${prefix.length}-0:${prefix.length + 5}`,
      "tilde_expansion",
    );
    assertSameLogicalProjection(
      `continued tilde ${name}`,
      initialOutput,
      output,
    );
    assertCstRange(
      output,
      `0:${prefix.length + 1}-0:${prefix.length + 5}`,
      "tilde_user",
    );
    assertCstRange(
      output,
      `0:${prefix.length + 6}-0:${prefix.length + 7}`,
      "line_continuation",
    );
    assertIncrementalEqualsFresh(initial, final, `continue-tilde-${name}`, {
      byte: prefix.length + 5,
      deleteBytes: 0,
      insert: `${"\\".repeat(2)}\n`,
    });
    assertIncrementalEqualsFresh(final, initial, `join-tilde-${name}`, {
      byte: prefix.length + 5,
      deleteBytes: 3,
      insert: "",
    });
  }
});

test("sh: arithmetic grouping, lvalues, and unary operators remain stable", () => {
  const backquoteBroken = writeSource(
    "arithmetic-backquote-broken",
    lines(": $((+x `+ y))"),
  );
  const backquoteRestored = writeSource(
    "arithmetic-backquote-restored",
    lines(": $((+x + y))"),
  );
  assertIncrementalEqualsFresh(
    backquoteBroken,
    backquoteRestored,
    "structured-reading-survives-backquote-undo",
    { byte: 8, deleteBytes: 1, insert: "" },
  );

  const parenthesizedInitial = writeSource(
    "arithmetic-parenthesized-initial",
    lines(': "$((a + b))"'),
  );
  const parenthesizedFinal = writeSource(
    "arithmetic-parenthesized-final",
    lines(': "$(((a + b)))"'),
  );
  assertIncrementalEqualsFresh(
    parenthesizedInitial,
    parenthesizedFinal,
    "parenthesize-arithmetic-expression",
    { byte: 6, deleteBytes: 5, insert: "(a + b)" },
  );
  assertIncrementalEqualsFresh(
    parenthesizedFinal,
    parenthesizedInitial,
    "unparenthesize-arithmetic-expression",
    { byte: 6, deleteBytes: 7, insert: "a + b" },
  );

  const lvalueInitial = writeSource(
    "arithmetic-lvalue-initial",
    lines(': "$((name = 1))"'),
  );
  const lvalueParenthesized = writeSource(
    "arithmetic-lvalue-parenthesized",
    lines(': "$(((name) = 1))"'),
  );
  const [, lvalueOutput] = assertIncrementalEqualsFresh(
    lvalueInitial,
    lvalueParenthesized,
    "parenthesize-arithmetic-assignment-lvalue",
    { byte: 6, deleteBytes: 0, insert: "(" },
    { byte: 11, deleteBytes: 0, insert: ")" },
  );
  assertIncrementalEqualsFresh(
    lvalueParenthesized,
    lvalueInitial,
    "unparenthesize-arithmetic-assignment-lvalue",
    { byte: 6, deleteBytes: 1, insert: "" },
    { byte: 10, deleteBytes: 1, insert: "" },
  );
  for (const [range, item] of [
    ["0:3-0:18", "arithmetic_expansion"],
    ["0:6-0:16", "expression: arithmetic_assignment_expression"],
    ["0:6-0:12", "left: parenthesized_arithmetic"],
    ["0:7-0:11", "expression: arithmetic_variable"],
    ["0:13-0:14", "operator: arithmetic_operator"],
    ["0:15-0:16", "right: arithmetic_number"],
  ]) {
    assertCstRange(lvalueOutput, range, item);
  }

  const nonLvalueInitial = writeSource(
    "arithmetic-non-lvalue-initial",
    lines(': "$(((name) = 2))"'),
  );
  const nonLvalueFinal = writeSource(
    "arithmetic-non-lvalue-final",
    lines(': "$(((name + 1) = 2))"'),
  );
  parseRecoveryAfterEdits(
    nonLvalueInitial,
    nonLvalueFinal,
    "make-parenthesized-arithmetic-non-lvalue",
    { byte: 11, deleteBytes: 0, insert: " + 1" },
  );
  assertIncrementalEqualsFresh(
    nonLvalueFinal,
    nonLvalueInitial,
    "restore-parenthesized-arithmetic-lvalue",
    { byte: 11, deleteBytes: 4, insert: "" },
  );

  const openingLayoutInitial = writeSource(
    "arithmetic-opening-layout-initial",
    lines(': "$((a+1))"'),
  );
  const openingLayoutFinal = writeSource(
    "arithmetic-opening-layout-final",
    lines(': "$(( a+1))"'),
  );
  assertIncrementalEqualsFresh(
    openingLayoutInitial,
    openingLayoutFinal,
    "insert-arithmetic-opening-layout",
    { byte: 6, deleteBytes: 0, insert: " " },
  );
  assertIncrementalEqualsFresh(
    openingLayoutFinal,
    openingLayoutInitial,
    "delete-arithmetic-opening-layout",
    { byte: 6, deleteBytes: 1, insert: "" },
  );

  const negationInitial = writeSource(
    "arithmetic-negation-initial",
    lines(': "$((!a))"'),
  );
  const negationFinal = writeSource(
    "arithmetic-negation-final",
    lines(': "$((! a))"'),
  );
  assertIncrementalEqualsFresh(
    negationInitial,
    negationFinal,
    "insert-arithmetic-negation-layout",
    { byte: 7, deleteBytes: 0, insert: " " },
  );
  assertIncrementalEqualsFresh(
    negationFinal,
    negationInitial,
    "delete-arithmetic-negation-layout",
    { byte: 7, deleteBytes: 1, insert: "" },
  );

  for (const [
    name,
    initialText,
    finalText,
    offset,
    expressionRange,
    operandRange,
  ] of [
    ["bang", "x=$((!-a))", "x=$((! -a))", 6, "0:5-0:9", "0:7-0:9"],
    ["plus", ": $((+-a))", ": $((+ -a))", 6, "0:5-0:9", "0:7-0:9"],
    ["minus", ": $((-+a))", ": $((- +a))", 6, "0:5-0:9", "0:7-0:9"],
    [
      "separated-sign",
      ': "$((+ +a))"',
      ': "$((+  +a))"',
      7,
      "0:6-0:11",
      "0:9-0:11",
    ],
  ]) {
    const initial = writeSource(
      `arithmetic-unary-${name}-initial`,
      lines(initialText),
    );
    const final = writeSource(
      `arithmetic-unary-${name}-final`,
      lines(finalText),
    );
    const [, output] = assertIncrementalEqualsFresh(
      initial,
      final,
      `insert-arithmetic-unary-${name}-blank`,
      { byte: offset, deleteBytes: 0, insert: " " },
    );
    assertCstRange(
      output,
      expressionRange,
      "expression: arithmetic_unary_expression",
    );
    assertCstRange(
      output,
      operandRange,
      "operand: arithmetic_unary_expression",
    );
    assertOccurrenceCount(output, "arithmetic_unary_expression", 2);
    assertNotContains(output, "command_substitution");
    assertIncrementalEqualsFresh(
      final,
      initial,
      `delete-arithmetic-unary-${name}-blank`,
      { byte: offset, deleteBytes: 1, insert: "" },
    );
  }

  const backquoteInitial = writeSource(
    "backquote-continuation-initial",
    lines("echo `printf body`"),
  );
  const backquoteFinal = writeSource(
    "backquote-continuation-final",
    lines("echo `\\", "printf body`"),
  );
  const backquoteOutput = parseValidCst(backquoteFinal);
  const backquoteToken = '"\\`"';
  assertCstRange(backquoteOutput, "0:5-0:6", backquoteToken);
  assertCstRange(backquoteOutput, "0:6-0:7", "line_continuation");
  assertCstRange(backquoteOutput, "1:11-1:12", backquoteToken);
  assertNotContains(backquoteOutput, "ERROR");
  assertIncrementalEqualsFresh(
    backquoteInitial,
    backquoteFinal,
    "insert-backquote-body-continuation",
    { byte: 6, deleteBytes: 0, insert: "\\\n" },
  );
  assertIncrementalEqualsFresh(
    backquoteFinal,
    backquoteInitial,
    "delete-backquote-body-continuation",
    { byte: 6, deleteBytes: 2, insert: "" },
  );

  for (const [name, initial, final, edit] of [
    [
      "operand",
      "x=$((1 +",
      "x=$((1 + 2))\n",
      { byte: 8, deleteBytes: 0, insert: " 2))\n" },
    ],
    [
      "parameter",
      "x=$((1 + $",
      "x=$((1 + $x))\n",
      { byte: 10, deleteBytes: 0, insert: "x))\n" },
    ],
  ]) {
    const incomplete = writeSource(`arithmetic-incomplete-${name}`, initial);
    assertNotContains(
      parseRecovery(incomplete, `input ending before an arithmetic ${name}`),
      "command_substitution",
    );
    const completed = writeSource(`arithmetic-completed-${name}`, final);
    for (const output of assertIncrementalEqualsFresh(
      incomplete,
      completed,
      `complete-arithmetic-${name}-after-recovery`,
      edit,
    )) {
      assertContains(output, "arithmetic_expansion");
    }
  }
});

test("sh: replacing arithmetic blanks with vertical tabs and form feeds preserves the CST", () => {
  for (const [name, whitespace] of [
    ["vertical-tab", "\v"],
    ["form-feed", "\f"],
  ]) {
    const initial = writeSource(
      `arithmetic-${name}-initial`,
      "echo $((1 + 2))\n",
    );
    const final = writeSource(
      `arithmetic-${name}-final`,
      `echo $((1${whitespace}+ 2))\n`,
    );
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      `replace-arithmetic-space-with-${name}`,
      { byte: 9, deleteBytes: 1, insert: whitespace },
    )) {
      assertCstRange(output, "0:8-0:13", "arithmetic_binary_expression");
    }
    assertIncrementalEqualsFresh(
      final,
      initial,
      `replace-arithmetic-${name}-with-space`,
      { byte: 9, deleteBytes: 1, insert: " " },
    );
  }
});

test("sh: arithmetic source fragments retain classification after completion and repair", () => {
  for (const {
    name,
    expression,
    byte,
    replaced,
    replacement,
    completed,
    type,
  } of [
    {
      name: "hexadecimal-prefix",
      expression: `0x\${digits}`,
      byte: 10,
      replaced: `\${digits}`,
      replacement: "F",
      completed: "0xF",
      type: "arithmetic_number",
    },
    {
      name: "octal-suffix",
      expression: `\${prefix}08`,
      byte: 8,
      replaced: `\${prefix}`,
      replacement: "1",
      completed: "108",
      type: "arithmetic_number",
    },
    {
      name: "numeric-name-suffix",
      expression: `\${prefix}123abc`,
      byte: 8,
      replaced: `\${prefix}`,
      replacement: "a",
      completed: "a123abc",
      type: "arithmetic_variable",
    },
  ]) {
    const initialText = `echo $((${expression}))\n`;
    const initial = writeSource(`${name}-fragment`, initialText);
    const final = writeSource(`${name}-completed`, `echo $((${completed}))\n`);
    for (const output of assertIncrementalEqualsFresh(initial, final, name, {
      byte,
      deleteBytes: replaced.length,
      insert: replacement,
    })) {
      assertOccurrenceCount(output, "arithmetic_source_fragment", 0);
      assertCstRange(output, `0:8-0:${8 + completed.length}`, type);
    }
    for (const output of assertIncrementalEqualsFresh(final, initial, name, {
      byte,
      deleteBytes: replacement.length,
      insert: replaced,
    })) {
      assertOccurrenceCount(output, "arithmetic_source_fragment", 1);
      assertOccurrenceCount(output, "arithmetic_dynamic_expression", 1);
    }
    const closer = initialText.indexOf("}");
    const deletion = { byte: closer, deleteBytes: 1, insert: "" };
    const broken = writeSource(
      `${name}-missing-parameter-closer`,
      initialText.slice(0, closer) + initialText.slice(closer + 1),
    );
    parseRecoveryAfterEdits(initial, broken, name, deletion);
    for (const output of assertIncrementalEqualsFresh(
      initial,
      initial,
      `${name} repair`,
      deletion,
      { byte: closer, deleteBytes: 0, insert: "}" },
    )) {
      assertOccurrenceCount(output, "arithmetic_source_fragment", 1);
    }
  }
});

test("sh: arithmetic quote edits preserve classification across source contexts", () => {
  for (const [name, prefix, suffix] of [
    ["plain", "echo $((", "))\n"],
    ["double-quoted", 'echo "$((1+', '))"\n'],
    ["dynamic", "echo $((", "$x))\n"],
    ["parenthesized", "echo $(((", "+$x)))\n"],
    ["nested-arithmetic", "echo $((1+$((", "))))\n"],
    ["parameter-word", 'echo "${x:-$((', '))}"\n'],
    ["command", "echo $(echo $((", ")))\n"],
    ["backquote", "echo `echo $((", "))`\n"],
    ["here-document-body", "cat <<E\n$((", "))\nE\n"],
  ]) {
    const initial = writeSource(
      `${name}-arithmetic-expression`,
      `${prefix}1${suffix}`,
    );
    for (const quote of ["'", '"']) {
      const quoted = `${quote}1${quote}`;
      const final = writeSource(
        `${name}-arithmetic-quote-source`,
        prefix + quoted + suffix,
      );
      for (const output of assertIncrementalEqualsFresh(
        initial,
        final,
        `${name}-insert-arithmetic-quotes`,
        { byte: prefix.length, deleteBytes: 1, insert: quoted },
      )) {
        assertNodeCount(output, "arithmetic_source", 1);
        assertNodeCount(output, "arithmetic_source_fragment", 2);
      }
      for (const output of assertIncrementalEqualsFresh(
        final,
        initial,
        `${name}-remove-arithmetic-quotes`,
        { byte: prefix.length, deleteBytes: quoted.length, insert: "1" },
      )) {
        assertNodeCount(output, "arithmetic_source", 0);
      }
    }
  }

  const complete = 'echo BEFORE\necho $(("1"+2))\necho AFTER\n';
  const closed = writeSource("arithmetic-quote-source-closed", complete);
  const byte = complete.indexOf("))");
  const open = writeSource(
    "arithmetic-quote-source-open",
    complete.slice(0, byte) + complete.slice(byte + 2),
  );
  parseRecoveryAfterEdits(closed, open, "remove-arithmetic-source-closer", {
    byte,
    deleteBytes: 2,
    insert: "",
  });
  assertIncrementalEqualsFresh(
    open,
    closed,
    "restore-arithmetic-source-closer",
    {
      byte,
      deleteBytes: 0,
      insert: "))",
    },
  );
});

test("sh: a newline edit preserves dynamic arithmetic layout", () => {
  const dynamicSpace = writeSource(
    "arithmetic-dynamic-space",
    lines(': "$(($x y))"'),
  );
  const dynamicNewline = writeSource(
    "arithmetic-dynamic-newline",
    lines(': "$(($x', 'y))"'),
  );
  const [, dynamicOutput] = assertIncrementalEqualsFresh(
    dynamicSpace,
    dynamicNewline,
    "arithmetic-dynamic-space-to-newline",
    { byte: 8, deleteBytes: 1, insert: "\n" },
  );
  assertCstRange(dynamicOutput, "0:6-1:1", "arithmetic_dynamic_expression");
});

test("sh: dynamic parameter words keep tilde literal across separator edits", () => {
  const initial = writeSource("parameter-tilde-slash", `echo \${x:-~$u/b}\n`);
  for (const [name, separator, end] of [
    ["space", " ", "0:15"],
    ["newline", "\n", "1:1"],
  ]) {
    const final = writeSource(
      `parameter-tilde-${name}`,
      `echo \${x:-~$u${separator}b}\n`,
    );
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      `replace-tilde-slash-with-${name}`,
      { byte: 13, deleteBytes: 1, insert: separator },
    )) {
      assertNodeCount(output, "tilde_expansion", 0);
      assertCstRange(output, `0:10-${end}`, "word: parameter_word");
      assertCstRange(output, "0:11-0:13", "parameter_expansion");
    }
    for (const output of assertIncrementalEqualsFresh(
      final,
      initial,
      `replace-tilde-${name}-with-slash`,
      { byte: 13, deleteBytes: 1, insert: "/" },
    )) {
      assertNodeCount(output, "tilde_expansion", 0);
      assertCstRange(output, "0:10-0:15", "word: parameter_word");
      assertCstRange(output, "0:11-0:13", "parameter_expansion");
    }
  }
});

test("sh: expansion dollars stay expansions after recovery edits", () => {
  const dollarInitial = writeSource(
    "expansion-dollar-initial",
    lines('echo "$' + "{x:-$'text'}\""),
  );
  const dollarFinal = writeSource(
    "expansion-dollar-final",
    lines('ech*o "$' + "{a=x$" + "{x:-y}:-$'text'}\""),
  );
  assertIncrementalEqualsFresh(
    dollarInitial,
    dollarFinal,
    "expansion-dollar",
    { byte: 3, deleteBytes: 0, insert: "*" },
    { byte: 10, deleteBytes: 0, insert: `\${x:-y}` },
    { byte: 9, deleteBytes: 0, insert: "a=" },
  );
});

test("sh: bracket range ownership follows endpoint and hyphen edits", () => {
  for (const { name, before, after, edits, undo } of [
    {
      name: "extend-chain",
      before: "[a-c-e]",
      after: "[a-c-e-g]",
      edits: [{ byte: 6, deleteBytes: 0, insert: "-g" }],
      undo: [{ byte: 6, deleteBytes: 2, insert: "" }],
    },
    {
      name: "complete-range",
      before: "[a-]",
      after: "[a-z]",
      edits: [{ byte: 3, deleteBytes: 0, insert: "z" }],
      undo: [{ byte: 3, deleteBytes: 1, insert: "" }],
    },
    {
      name: "hyphen-endpoint",
      before: "[a-b--]",
      after: "[a-b--c]",
      edits: [{ byte: 6, deleteBytes: 0, insert: "c" }],
      undo: [{ byte: 6, deleteBytes: 1, insert: "" }],
    },
    {
      name: "shift-endpoint",
      before: "[a-c-e]",
      after: "[a--c-e]",
      edits: [{ byte: 3, deleteBytes: 0, insert: "-" }],
      undo: [{ byte: 3, deleteBytes: 1, insert: "" }],
    },
    {
      name: "class-to-collating",
      before: "[a-[:x:]]",
      after: "[a-[.x.]]",
      edits: [
        { byte: 4, deleteBytes: 1, insert: "." },
        { byte: 6, deleteBytes: 1, insert: "." },
      ],
      undo: [
        { byte: 4, deleteBytes: 1, insert: ":" },
        { byte: 6, deleteBytes: 1, insert: ":" },
      ],
    },
    {
      name: "equivalence-to-collating",
      before: "[a-[=x=]]",
      after: "[a-[.x.]]",
      edits: [
        { byte: 4, deleteBytes: 1, insert: "." },
        { byte: 6, deleteBytes: 1, insert: "." },
      ],
      undo: [
        { byte: 4, deleteBytes: 1, insert: "=" },
        { byte: 6, deleteBytes: 1, insert: "=" },
      ],
    },
  ]) {
    for (const [context, prefix, suffix] of [
      ["word", "printf ", "\n"],
      ["parameter", ': "${v#', '}"\n'],
    ]) {
      const initial = writeSource(
        `${name}-${context}-initial`,
        prefix + before + suffix,
      );
      const final = writeSource(
        `${name}-${context}-final`,
        prefix + after + suffix,
      );
      const withPrefix = ({ byte, deleteBytes, insert }) => ({
        byte: prefix.length + byte,
        deleteBytes,
        insert,
      });
      assertIncrementalEqualsFresh(
        initial,
        final,
        `${name}-${context}`,
        ...edits.map(withPrefix),
      );
      assertIncrementalEqualsFresh(
        final,
        initial,
        `undo-${name}-${context}`,
        ...undo.map(withPrefix),
      );
    }
  }

  const initial = writeSource("range-unclosed-quote", 'printf [a-"z]\n');
  const final = writeSource("range-repaired-quote", 'printf [a-"z"]\n');
  assertIncrementalEqualsFresh(initial, final, "repair-range-endpoint-quote", {
    byte: 12,
    deleteBytes: 0,
    insert: '"',
  });
});

test("sh: bracket fallback remains stable across complete and incomplete edits", () => {
  const bracketUnclosed = writeSource("bracket-unclosed", lines("printf [abc"));
  const bracketClosed = writeSource("bracket-closed", lines("printf [abc]"));
  const bracketRange = writeSource("bracket-range", lines("printf [a-z]"));
  const bracketUnclosedOutput = parseValidCst(bracketUnclosed);
  assertCstRange(bracketUnclosedOutput, "0:7-0:11", "literal `[abc`");
  assertIncrementalEqualsFresh(
    bracketUnclosed,
    bracketClosed,
    "complete-bracket-expression",
    { byte: 11, deleteBytes: 0, insert: "]" },
  );
  assertIncrementalEqualsFresh(
    bracketClosed,
    bracketUnclosed,
    "unclose-bracket-expression",
    { byte: 11, deleteBytes: 1, insert: "" },
  );
  assertIncrementalEqualsFresh(
    bracketClosed,
    bracketRange,
    "bracket-list-to-range",
    { byte: 9, deleteBytes: 2, insert: "-z" },
  );

  const specialUnclosed = writeSource(
    "special-bracket-unclosed",
    lines("printf [[."),
  );
  const specialClosed = writeSource(
    "special-bracket-closed",
    lines("printf [[.x.]]"),
  );
  assertIncrementalEqualsFresh(
    specialUnclosed,
    specialClosed,
    "complete-collating-symbol-bracket-expression",
    { byte: 10, deleteBytes: 0, insert: "x.]]" },
  );
  assertIncrementalEqualsFresh(
    specialClosed,
    specialUnclosed,
    "unclose-collating-symbol-bracket-expression",
    { byte: 10, deleteBytes: 4, insert: "" },
  );

  const specialSuffixInitial = writeSource(
    "special-suffix-initial",
    lines("printf [[:alpha:]"),
  );
  const specialSuffixFinal = writeSource(
    "special-suffix-final",
    lines("printf [[:alpha:]]"),
  );
  assertIncrementalEqualsFresh(
    specialSuffixInitial,
    specialSuffixFinal,
    "complete-special-suffix-outer-bracket",
    { byte: 17, deleteBytes: 0, insert: "]" },
  );
  assertIncrementalEqualsFresh(
    specialSuffixFinal,
    specialSuffixInitial,
    "restore-special-suffix-literal-prefix",
    { byte: 17, deleteBytes: 1, insert: "" },
  );

  const operatorSuffixInitial = writeSource(
    "operator-suffix-initial",
    lines('printf [a"x"[.]'),
  );
  const operatorSuffixFinal = writeSource(
    "operator-suffix-final",
    lines('printf [a"x"*[.]'),
  );
  assertIncrementalEqualsFresh(
    operatorSuffixInitial,
    operatorSuffixFinal,
    "insert-operator-before-completed-bracket-suffix",
    { byte: 12, deleteBytes: 0, insert: "*" },
  );
  assertIncrementalEqualsFresh(
    operatorSuffixFinal,
    operatorSuffixInitial,
    "delete-operator-before-completed-bracket-suffix",
    { byte: 12, deleteBytes: 1, insert: "" },
  );

  const terminalBracketInitial = writeSource(
    "terminal-bracket-initial",
    lines("echo [!]", "next"),
  );
  const terminalBracketFinal = writeSource(
    "terminal-bracket-final",
    lines("echo [!]\\", "", "next"),
  );
  assertIncrementalEqualsFresh(
    terminalBracketInitial,
    terminalBracketFinal,
    "insert-terminal-bracket-continuation",
    { byte: 8, deleteBytes: 0, insert: "\\\n" },
  );
  assertIncrementalEqualsFresh(
    terminalBracketFinal,
    terminalBracketInitial,
    "delete-terminal-bracket-continuation",
    { byte: 8, deleteBytes: 2, insert: "" },
  );

  const pipelineMergeInitial = writeSource(
    "bracket-name-pipeline-merge-initial",
    lines("[a] x |i"),
  );
  const pipelineMergeFinal = writeSource(
    "bracket-name-pipeline-merge-final",
    lines("[a] x i"),
  );
  assertIncrementalEqualsFresh(
    pipelineMergeInitial,
    pipelineMergeFinal,
    "delete-pipe-merging-bracket-command-suffix",
    { byte: 6, deleteBytes: 1, insert: "" },
  );

  const newlineMergeInitial = writeSource(
    "bracket-name-newline-merge-initial",
    lines("[a] x ", "i"),
  );
  const newlineMergeFinal = writeSource(
    "bracket-name-newline-merge-final",
    lines("[a] x i"),
  );
  assertIncrementalEqualsFresh(
    newlineMergeInitial,
    newlineMergeFinal,
    "delete-newline-merging-bracket-command-suffix",
    { byte: 6, deleteBytes: 1, insert: "" },
  );
  const quotedBracket = writeSource("bracket-dollar-quoted", lines(": [$']'"));
  const escapedQuoteBracket = writeSource(
    "bracket-dollar-escaped-quote",
    lines(": [$'\\']'"),
  );
  const closedEscapedQuoteBracket = writeSource(
    "bracket-dollar-escaped-quote-closed",
    lines(": [$'\\']']"),
  );
  const escapedQuoteOutput = parseValidCst(escapedQuoteBracket);
  assertNotContains(escapedQuoteOutput, "pattern_bracket_source");
  assertCstRange(escapedQuoteOutput, "0:2-0:3", "literal");
  assertCstRange(escapedQuoteOutput, "0:3-0:9", "dollar_single_quoted");
  assertCstRange(escapedQuoteOutput, "0:5-0:7", "dollar_single_quote_escape");
  assertCstRange(
    parseValidCst(closedEscapedQuoteBracket),
    "0:2-0:10",
    "pattern_bracket_source",
  );
  assertIncrementalEqualsFresh(
    quotedBracket,
    escapedQuoteBracket,
    "insert-escaped-apostrophe-before-quoted-bracket",
    { byte: 5, deleteBytes: 0, insert: "\\'" },
  );
  assertIncrementalEqualsFresh(
    escapedQuoteBracket,
    quotedBracket,
    "remove-escaped-apostrophe-before-quoted-bracket",
    { byte: 5, deleteBytes: 2, insert: "" },
  );
  assertIncrementalEqualsFresh(
    escapedQuoteBracket,
    closedEscapedQuoteBracket,
    "close-bracket-after-dollar-quoted-apostrophe",
    { byte: 9, deleteBytes: 0, insert: "]" },
  );
  assertIncrementalEqualsFresh(
    closedEscapedQuoteBracket,
    escapedQuoteBracket,
    "remove-bracket-after-dollar-quoted-apostrophe",
    { byte: 9, deleteBytes: 1, insert: "" },
  );
  const arithmeticBracket = writeSource(
    "bracket-arithmetic-member",
    lines(": [$((case))] arg >out"),
  );
  const fallbackBracket = writeSource(
    "bracket-command-fallback-member",
    lines(": [$((case x in x)esac))] arg >out"),
  );
  for (const {
    initial,
    final,
    name,
    edit,
    member,
    end,
    argument,
    redirect,
  } of [
    {
      initial: arithmeticBracket,
      final: fallbackBracket,
      name: "arithmetic-member-to-command-fallback",
      edit: { byte: 10, deleteBytes: 0, insert: " x in x)esac" },
      member: "command_substitution",
      end: 24,
      argument: "0:26-0:29",
      redirect: "0:30-0:34",
    },
    {
      initial: fallbackBracket,
      final: arithmeticBracket,
      name: "command-fallback-member-to-arithmetic",
      edit: { byte: 10, deleteBytes: 12, insert: "" },
      member: "arithmetic_expansion",
      end: 12,
      argument: "0:14-0:17",
      redirect: "0:18-0:22",
    },
  ]) {
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      name,
      edit,
    )) {
      assertCstRange(output, `0:2-0:${end + 1}`, "pattern_bracket_source");
      assertCstDirectChildRange(
        output,
        `0:3-0:${end}`,
        "members: pattern_bracket_members_source",
        `0:3-0:${end}`,
        `member: ${member}`,
      );
      assertCstDirectChildRange(
        output,
        `0:3-0:${end}`,
        `member: ${member}`,
        "0:3-0:4",
        '"$"',
      );
      assertCstDirectChildRange(
        output,
        `0:3-0:${end}`,
        `member: ${member}`,
        "0:4-0:5",
        '"("',
      );
      assertCstRange(output, argument, "word: word");
      assertCstRange(output, redirect, "redirect: io_redirect");
    }
  }

  const unclosedFallbackBracket = writeSource(
    "bracket-command-fallback-unclosed",
    lines(": [$((case x in x)esac)) arg >out"),
  );
  for (const output of assertIncrementalEqualsFresh(
    fallbackBracket,
    unclosedFallbackBracket,
    "unclose-bracket-after-command-fallback",
    { byte: 24, deleteBytes: 1, insert: "" },
  )) {
    assertNotContains(output, "pattern_bracket_source");
    assertCstRange(output, "0:2-0:3", "literal");
    assertCstRange(output, "0:3-0:24", "command_substitution");
    assertCstRange(output, "0:25-0:28", "word: word");
    assertCstRange(output, "0:29-0:33", "redirect: io_redirect");
  }
  for (const output of assertIncrementalEqualsFresh(
    unclosedFallbackBracket,
    fallbackBracket,
    "restore-bracket-after-command-fallback",
    { byte: 24, deleteBytes: 0, insert: "]" },
  )) {
    assertCstRange(output, "0:2-0:25", "pattern_bracket_source");
    assertCstRange(output, "0:3-0:24", "member: command_substitution");
  }
});

test("sh: nested arithmetic classification follows edits and recovery", () => {
  const fixtures = [
    {
      name: "structured-to-dynamic-inner-expression",
      initial: ": $(( $((1 + 2)) + $((3)) ))\n",
      removed: "+",
      inserted: "$op",
      expected: "arithmetic_dynamic_expression",
    },
    {
      name: "parameter-word-has-an-independent-arithmetic-context",
      initial: ": $(( $((1)) + $value + $((4)) ))\n",
      removed: "$value",
      inserted: `\${value:-$((2 $op 3))}`,
      expected: "parameter_word",
    },
    {
      name: "command-substitution-fallback-inside-arithmetic",
      initial: ": $(( $((1)) + $((2)) ))\n",
      removed: "1",
      inserted: "echo 1",
      expected: "command_substitution",
    },
    {
      name: "repair-an-incomplete-outer-expression",
      initial: ": $(( $((1)) + $((2)) \n",
      removed: "\n",
      inserted: "))\n",
      expected: "arithmetic_binary_expression",
    },
    {
      name: "repair-a-broken-inner-expression",
      initial: ": $(( $((1)) + $((#)) )) $((3 $op 4))\n",
      removed: "#",
      inserted: "2",
      expected: "arithmetic_dynamic_expression",
    },
    {
      name: "here-document-body-has-an-independent-arithmetic-context",
      initial: ": $(( $((1)) + $value + $((4)) ))\n",
      removed: "$value",
      inserted: "$(cat <<E\n$((2 $op 3))\nE\n)",
      expected: "here_document_body",
    },
  ];
  for (const fixture of fixtures) {
    const offset = fixture.initial.indexOf(fixture.removed);
    const edit = {
      byte: offset,
      deleteBytes: fixture.removed.length,
      insert: fixture.inserted,
    };
    const initial = writeSource(`${fixture.name}-initial`, fixture.initial);
    const final = writeSource(
      `${fixture.name}-final`,
      applyEdits(Buffer.from(fixture.initial), [edit]),
    );
    const [, output] = assertIncrementalEqualsFresh(
      initial,
      final,
      fixture.name,
      edit,
    );
    assertContains(output, fixture.expected);
  }
});

test("sh: hash parameter word repairs and operator edits match a fresh parse", () => {
  for (const { name, expression, offset, count } of [
    {
      name: "quoted-empty-alternative",
      expression: `\${#+''}`,
      offset: 4,
      count: 2,
    },
    {
      name: "quoted-empty-assignment",
      expression: `\${#=''}`,
      offset: 4,
      count: 2,
    },
    { name: "colon-alternative", expression: `\${#:+}`, offset: 3, count: 1 },
  ]) {
    const initialText = `echo ${expression}\nafter\n`;
    const byte = 5 + offset;
    const initial = writeSource(`${name}-complete`, initialText);
    const broken = writeSource(
      `${name}-missing-word`,
      initialText.slice(0, byte) + initialText.slice(byte + count),
    );
    for (const output of parseRecoveryAfterEdits(initial, broken, name, {
      byte,
      deleteBytes: count,
      insert: "",
    })) {
      assert.ok(hasRecovery(output), `${name}: missing word must be rejected`);
    }
    assertIncrementalEqualsFresh(broken, initial, `${name}-restore`, {
      byte,
      deleteBytes: 0,
      insert: initialText.slice(byte, byte + count),
    });
  }
  const initial = writeSource("hash-suffix-pattern", `echo \${#%}\n`);
  const final = writeSource("hash-length", `echo \${#?}\n`);
  for (const output of assertIncrementalEqualsFresh(
    initial,
    final,
    "pattern-to-length",
    { byte: 8, deleteBytes: 1, insert: "?" },
  )) {
    assertContains(output, "parameter_length_operator `#`");
    assertContains(output, "special_parameter `?`");
  }
  for (const output of assertIncrementalEqualsFresh(
    final,
    initial,
    "length-to-pattern",
    { byte: 8, deleteBytes: 1, insert: "%" },
  )) {
    assertContains(output, "parameter_pattern_operator `%`");
    assertNotContains(output, "parameter_length_operator");
  }
});

test("sh: backquote run edits expose complete escapes before ordinary text", () => {
  for (const [
    name,
    initialText,
    finalText,
    offset,
    escapeRange,
    escapeNode,
    literalRange,
    literalNode,
  ] of [
    [
      "word",
      ": `: \\\\q`\n",
      ": `: \\\\\\q`\n",
      5,
      "0:5-0:8",
      "escaped_character",
      "0:8-0:9",
      "literal",
    ],
    [
      "double-quote",
      ': `: "\\\\q"`\n',
      ': `: "\\\\\\q"`\n',
      6,
      "0:6-0:9",
      "double_quote_escape",
      "0:9-0:10",
      "double_quote_text",
    ],
    [
      "here-document",
      ": `cat <<EOF\n\\\\q\nEOF\n`\n",
      ": `cat <<EOF\n\\\\\\q\nEOF\n`\n",
      13,
      "1:0-1:3",
      "here_document_escape",
      "1:3-1:4",
      "here_document_text",
    ],
  ]) {
    const initial = writeSource(`${name}-paired-run`, initialText);
    const final = writeSource(`${name}-ordinary-tail`, finalText);
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      `${name}-grow-ordinary-run`,
      { byte: offset, deleteBytes: 0, insert: "\\" },
    )) {
      assertCstRange(output, escapeRange, escapeNode);
      assertCstRange(output, literalRange, literalNode);
    }
    assertIncrementalEqualsFresh(
      final,
      initial,
      `${name}-shrink-ordinary-run`,
      { byte: offset, deleteBytes: 1, insert: "" },
    );
  }
});

test("sh: backquote tails follow double-quote and parameter operand escape rules", () => {
  for (const [
    name,
    initialText,
    finalText,
    offset,
    range,
    finalNode,
    finalEscapeCount,
  ] of [
    [
      "ordinary",
      ': `: "\\a"`\n',
      ': `: "\\}"`\n',
      7,
      "0:6-0:8",
      "double_quote_text",
      0,
    ],
    [
      "parameter",
      ': `: "$' + '{x:-\\a}"`\n',
      ': `: "$' + '{x:-\\}}"`\n',
      12,
      "0:11-0:13",
      "double_quote_escape",
      1,
    ],
  ]) {
    const initial = writeSource(`${name}-ordinary-quoted-tail`, initialText);
    const final = writeSource(`${name}-closing-brace-tail`, finalText);
    const initialOutput = parseValidCst(initial);
    assertCstRange(initialOutput, range, "double_quote_text");
    assertNotContains(initialOutput, "double_quote_escape");
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      `${name}-replace-tail-with-brace`,
      { byte: offset, deleteBytes: 1, insert: "}" },
    )) {
      assertCstRange(output, range, finalNode);
      assertOccurrenceCount(output, "double_quote_escape", finalEscapeCount);
    }
    assertIncrementalEqualsFresh(
      final,
      initial,
      `${name}-restore-ordinary-tail`,
      { byte: offset, deleteBytes: 1, insert: "a" },
    );
  }
});

test("sh: an opening-blank edit preserves nested case and bracket structure", () => {
  const initial = writeSource(
    "nested-case-with-opening-blank",
    lines(": [$(: $( case x in x) :;; esac))]"),
  );
  const final = writeSource(
    "nested-case-without-opening-blank",
    lines(": [$(: $(case x in x) :;; esac))]"),
  );
  for (const output of assertIncrementalEqualsFresh(
    initial,
    final,
    "remove-nested-command-opening-blank",
    { byte: 9, deleteBytes: 1, insert: "" },
  )) {
    assertCstRange(output, "0:2-0:33", "pattern_bracket_source");
  }
  for (const output of assertIncrementalEqualsFresh(
    final,
    initial,
    "restore-nested-command-opening-blank",
    { byte: 9, deleteBytes: 0, insert: " " },
  )) {
    assertCstRange(output, "0:2-0:34", "pattern_bracket_source");
  }
});

test("sh: editing enclosing quotes updates backquote command word structure", () => {
  const initialText = 'echo `printf %s \\"a b\\"`\n';
  const finalText = 'echo "`printf %s \\"a b\\"`"\n';
  const initial = writeSource(
    "backquote-without-enclosing-quotes",
    initialText,
  );
  const final = writeSource("backquote-with-enclosing-quotes", finalText);
  const outputs = assertIncrementalEqualsFresh(
    initial,
    final,
    "insert enclosing quotes",
    { byte: initialText.length - 1, deleteBytes: 0, insert: '"' },
    { byte: 5, deleteBytes: 0, insert: '"' },
  );
  for (const output of outputs) assertOccurrenceCount(output, "word: word", 3);
  const restored = assertIncrementalEqualsFresh(
    final,
    initial,
    "remove enclosing quotes",
    { byte: finalText.length - 2, deleteBytes: 1, insert: "" },
    { byte: 5, deleteBytes: 1, insert: "" },
  );
  for (const output of restored) assertOccurrenceCount(output, "word: word", 4);

  for (const {
    name,
    initialText,
    finalText,
    offset,
    quoteRange,
    escapeRanges,
    quoteNode = "dollar_single_quoted",
    parameterRange,
  } of [
    {
      name: "plain",
      initialText: ": `: $'\\''`",
      finalText: ": `: $'\\\\''`",
      offset: 7,
      quoteRange: "0:5-0:11",
      escapeRanges: ["0:7-0:10"],
    },
    {
      name: "quoted",
      initialText: ": \"`: $'\\''`\"",
      finalText: ": \"`: $'\\\\''`\"",
      offset: 8,
      quoteRange: "0:6-0:12",
      escapeRanges: ["0:8-0:11"],
    },
    {
      name: "nested",
      initialText: ": \"`: \\`: $'\\\\\\''\\``\"",
      finalText: ": \"`: \\`: $'\\\\\\\\''\\``\"",
      offset: 12,
      quoteRange: "0:10-0:18",
      escapeRanges: ["0:12-0:17"],
    },
    {
      name: "paired",
      initialText: ": `: $'\\\\\\\\\\''`",
      finalText: ": `: $'\\\\\\\\\\\\''`",
      offset: 7,
      quoteRange: "0:5-0:15",
      escapeRanges: ["0:7-0:11", "0:11-0:14"],
    },
    {
      name: "control",
      initialText: ": `: $'\\c\\\\\\\\'`",
      finalText: ": `: $'\\\\c\\\\\\\\'`",
      offset: 7,
      quoteRange: "0:5-0:15",
      escapeRanges: ["0:7-0:14"],
    },
    {
      name: "nested-control",
      initialText: ": \"`: \\`: $'\\\\\\c\\\\\\\\\\\\\\\\'\\``\"",
      finalText: ": \"`: \\`: $'\\\\\\\\c\\\\\\\\\\\\\\\\'\\``\"",
      offset: 12,
      quoteRange: "0:10-0:26",
      escapeRanges: ["0:12-0:25"],
    },
    {
      name: "escaped-opener",
      initialText: ": `: $'\\\\''`",
      finalText: ": `: \\$'\\\\''`",
      offset: 5,
      quoteRange: "0:5-0:12",
      escapeRanges: ["0:8-0:11"],
    },
    {
      name: "double-quote-text",
      initialText: "echo `a \"$'x'\"`",
      finalText: "echo `a \"\\$'x'\"`",
      offset: 9,
      quoteRange: "0:8-0:15",
      escapeRanges: [],
      quoteNode: "double_quoted",
    },
    {
      name: "parameter-word-text",
      initialText: 'echo `a "$' + "{x:-$'x'}\"`",
      finalText: 'echo `a "$' + "{x:-\\$'x'}\"`",
      offset: 14,
      quoteRange: "0:8-0:21",
      escapeRanges: [],
      quoteNode: "double_quoted",
      parameterRange: "0:9-0:20",
    },
  ]) {
    const initial = writeSource(
      `backquote-dollar-quote-${name}-initial`,
      lines(initialText),
    );
    const final = writeSource(
      `backquote-dollar-quote-${name}-final`,
      lines(finalText),
    );
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      `insert-backquote-dollar-quote-${name}-escape`,
      { byte: offset, deleteBytes: 0, insert: "\\" },
    )) {
      assertCstRange(output, quoteRange, quoteNode);
      assertOccurrenceCount(
        output,
        "dollar_single_quoted",
        quoteNode === "dollar_single_quoted" ? 1 : 0,
      );
      if (quoteNode === "double_quoted") {
        assertContains(output, "double_quote_text");
        assertOccurrenceCount(
          output,
          "parameter_expansion",
          parameterRange === undefined ? 0 : 1,
        );
        if (parameterRange !== undefined) {
          assertCstRange(output, parameterRange, "parameter_expansion");
        }
      }
      assertOccurrenceCount(
        output,
        "dollar_single_quote_escape",
        escapeRanges.length,
      );
      for (const range of escapeRanges) {
        assertCstRange(output, range, "dollar_single_quote_escape");
      }
    }
    for (const output of assertIncrementalEqualsFresh(
      final,
      initial,
      `delete-backquote-dollar-quote-${name}-escape`,
      { byte: offset, deleteBytes: 1, insert: "" },
    )) {
      assertOccurrenceCount(
        output,
        "dollar_single_quoted",
        quoteNode === "dollar_single_quoted" ? 1 : 0,
      );
    }
  }
});

test("sh: a quoted-parameter apostrophe edit preserves bracket structure", () => {
  const initial = writeSource(
    "quoted-parameter-bracket-before-apostrophe-edit",
    lines(": $" + '{v#["$' + '{x-a}"]}'),
  );
  const final = writeSource(
    "quoted-parameter-bracket-after-apostrophe-edit",
    lines(": $" + '{v#["$' + "{x-'}\"]}"),
  );
  assertIncrementalEqualsFresh(
    initial,
    final,
    "quoted-parameter-apostrophe-retains-bracket",
    { byte: 12, deleteBytes: 1, insert: "'" },
  );
});

test("sh: arithmetic lookahead folds surrounding backquote escapes", () => {
  const cases = [
    {
      name: "parameter-prefix",
      initial: ": `: $((x))`",
      final: ": `: $((\\$x))`",
      edit: { byte: 8, deleteBytes: 0, insert: "\\$" },
      reverse: { byte: 8, deleteBytes: 2, insert: "" },
      arithmetic: 1,
      commands: 0,
      ranges: [
        ["0:5-0:13", "arithmetic_expansion"],
        ["0:8-0:11", "expression: parameter_expansion"],
      ],
    },
    {
      name: "nested-arithmetic-parameter-prefix",
      initial: ": `: $(( $((x)) ))`",
      final: ": `: $(( $((\\$x)) ))`",
      edit: { byte: 12, deleteBytes: 0, insert: "\\$" },
      reverse: { byte: 12, deleteBytes: 2, insert: "" },
      arithmetic: 2,
      commands: 0,
      ranges: [
        ["0:5-0:20", "arithmetic_expansion"],
        ["0:9-0:17", "expression: arithmetic_expansion"],
        ["0:12-0:15", "expression: parameter_expansion"],
      ],
    },
    {
      name: "command-quoted-parenthesis",
      initial: ': "`: $(( $(printf 1; : \\"a\\") ))`"',
      final: ': "`: $(( $(printf 1; : \\")\\") ))`"',
      edit: { byte: 26, deleteBytes: 1, insert: ")" },
      reverse: { byte: 26, deleteBytes: 1, insert: "a" },
      arithmetic: 1,
      commands: 1,
      ranges: [
        ["0:6-0:33", "arithmetic_expansion"],
        ["0:10-0:30", "expression: command_substitution"],
        ["0:24-0:29", "double_quoted"],
      ],
    },
    {
      name: "parameter-quoted-brace",
      initial: ': "`: $(( $' + '{x:-\\"a\\"} ))`"',
      final: ': "`: $(( $' + '{x:-\\"}\\"} ))`"',
      edit: { byte: 17, deleteBytes: 1, insert: "}" },
      reverse: { byte: 17, deleteBytes: 1, insert: "a" },
      arithmetic: 1,
      commands: 0,
      ranges: [
        ["0:6-0:24", "arithmetic_expansion"],
        ["0:10-0:21", "expression: parameter_expansion"],
        ["0:15-0:20", "double_quoted"],
      ],
    },
    {
      name: "surviving-parameter-escape",
      initial: ": `: $((\\$x))`",
      final: ": `: $((\\\\$x))`",
      edit: { byte: 8, deleteBytes: 0, insert: "\\" },
      reverse: { byte: 8, deleteBytes: 1, insert: "" },
      arithmetic: 0,
      commands: 1,
      ranges: [["0:5-0:14", "command_substitution"]],
    },
  ];
  for (const entry of cases) {
    const initial = writeSource(
      `arithmetic-backquote-${entry.name}-initial`,
      lines(entry.initial),
    );
    const final = writeSource(
      `arithmetic-backquote-${entry.name}-final`,
      lines(entry.final),
    );
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      `change-arithmetic-backquote-${entry.name}`,
      entry.edit,
    )) {
      assertOccurrenceCount(output, "arithmetic_expansion\n", entry.arithmetic);
      assertOccurrenceCount(output, "command_substitution\n", entry.commands);
      for (const [range, node] of entry.ranges) {
        assertCstRange(output, range, node);
      }
    }
    assertIncrementalEqualsFresh(
      final,
      initial,
      `restore-arithmetic-backquote-${entry.name}`,
      entry.reverse,
    );
  }
});

test("sh: backquote bracket escapes retain logical range endpoints and separate members", () => {
  const slash = "\\";
  const cases = [
    {
      name: "escaped-ordinary-endpoint",
      source: ": `: [0-\\\\z]`\n",
      range: "0:6-0:11",
      escapes: ["0:8-0:11"],
    },
    {
      name: "complete-raw-pair-endpoint",
      source: ": `: [0-\\\\\\\\z]`\n",
      range: "0:6-0:12",
      escapes: ["0:8-0:12"],
    },
    {
      name: "complete-trailing-member",
      source: ": `: [0-\\\\\\\\\\\\\\\\z]`\n",
      range: "0:6-0:12",
      escapes: ["0:8-0:12", "0:12-0:16"],
    },
    {
      name: "nested-backquote-endpoint",
      source: ": `: \\`: [0-\\\\\\\\\\\\\\\\z]\\``\n",
      range: "0:10-0:20",
      escapes: ["0:12-0:20"],
    },
    {
      name: "nested-backquote-separate-member",
      source: ": `: \\`: [0-\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\z]\\``\n",
      range: "0:10-0:20",
      escapes: ["0:12-0:20", "0:20-0:28"],
    },
  ];
  const paths = new Map();
  for (const entry of cases) {
    const source = writeSource(entry.name, entry.source);
    paths.set(entry.name, source);
    const output = parseValidCst(source);
    assertOccurrenceCount(output, "pattern_bracket_range_source", 1);
    assertCstRange(output, entry.range, "pattern_bracket_range_source");
    assertOccurrenceCount(output, "escaped_character", entry.escapes.length);
    for (const range of entry.escapes) {
      assertCstRange(output, range, "escaped_character");
    }
  }

  for (const { initialName, finalName, offset, inserted } of [
    {
      initialName: "escaped-ordinary-endpoint",
      finalName: "complete-raw-pair-endpoint",
      offset: 8,
      inserted: 2,
    },
    {
      initialName: "complete-raw-pair-endpoint",
      finalName: "complete-trailing-member",
      offset: 8,
      inserted: 4,
    },
    {
      initialName: "nested-backquote-endpoint",
      finalName: "nested-backquote-separate-member",
      offset: 12,
      inserted: 8,
    },
  ]) {
    const initial = paths.get(initialName);
    const final = paths.get(finalName);
    assertIncrementalEqualsFresh(initial, final, `grow-${initialName}`, {
      byte: offset,
      deleteBytes: 0,
      insert: slash.repeat(inserted),
    });
    assertIncrementalEqualsFresh(final, initial, `shrink-${finalName}`, {
      byte: offset,
      deleteBytes: inserted,
      insert: "",
    });
  }
});

test("sh: enclosing backquote escape runs keep bracket classification across edits", () => {
  for (const [
    name,
    initialSource,
    finalSource,
    edit,
    restore,
    quoteRange,
    escapeRange,
  ] of [
    [
      "apostrophe",
      ": `: [a-$'x']`\n",
      ": `: [a-$'\\\\'']`\n",
      { byte: 10, deleteBytes: 1, insert: "\\\\'" },
      { byte: 10, deleteBytes: 3, insert: "x" },
      "0:8-0:14",
      "0:10-0:13",
    ],
    [
      "backslash",
      ": `: [a-$'x']`\n",
      ": `: [a-$'\\\\\\\\']`\n",
      { byte: 10, deleteBytes: 1, insert: "\\\\\\\\" },
      { byte: 10, deleteBytes: 4, insert: "x" },
      "0:8-0:15",
      "0:10-0:14",
    ],
    [
      "nested-apostrophe",
      ": `: \\`: [a-$'x']\\``\n",
      ": `: \\`: [a-$'\\\\\\\\'']\\``\n",
      { byte: 14, deleteBytes: 1, insert: "\\\\\\\\'" },
      { byte: 14, deleteBytes: 5, insert: "x" },
      "0:12-0:20",
      "0:14-0:19",
    ],
  ]) {
    const initial = writeSource(
      `bracket-dollar-quote-${name}-initial`,
      initialSource,
    );
    const final = writeSource(
      `bracket-dollar-quote-${name}-final`,
      finalSource,
    );
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      `escape-bracket-dollar-quote-${name}`,
      edit,
    )) {
      assertCstRange(output, quoteRange, "end: dollar_single_quoted");
      assertCstRange(output, escapeRange, "dollar_single_quote_escape");
      assertOccurrenceCount(output, "pattern_bracket_range_source", 1);
    }
    assertIncrementalEqualsFresh(
      final,
      initial,
      `restore-bracket-dollar-quote-${name}`,
      restore,
    );
  }

  const escapedClose = writeSource(
    "backquote-bracket-escaped-close",
    "echo `echo [\\\\]`\n",
  );
  const closingBracket = writeSource(
    "backquote-bracket-closing",
    "echo `echo [\\\\\\]`\n",
  );
  assertIncrementalEqualsFresh(
    escapedClose,
    closingBracket,
    "backquote-bracket-grow",
    { byte: 14, deleteBytes: 0, insert: "\\" },
  );
  assertIncrementalEqualsFresh(
    closingBracket,
    escapedClose,
    "backquote-bracket-shrink",
    { byte: 14, deleteBytes: 1, insert: "" },
  );
  for (const { name, plainSource, escapedSource, offset, openerRange } of [
    {
      name: "unquoted",
      plainSource: lines(": `: [$(cat <<X", "]", "X", ")]`"),
      escapedSource: lines(": `: [$(cat <<\\\\X", "]", "X", ")]`"),
      offset: 14,
      openerRange: "0:5-0:6",
    },
    {
      name: "double-quoted",
      plainSource: lines(': "`: [$(cat <<X', "]", "X", ')]`"'),
      escapedSource: lines(': "`: [$(cat <<\\\\X', "]", "X", ')]`"'),
      offset: 15,
      openerRange: "0:6-0:7",
    },
  ]) {
    const plain = writeSource(`bracket-heredoc-${name}-plain`, plainSource);
    const escaped = writeSource(
      `bracket-heredoc-${name}-escaped`,
      escapedSource,
    );
    const escapedOutput = parseValidCst(escaped);
    assertCstRange(
      escapedOutput,
      `${openerRange.split("-", 1)[0]}-3:2`,
      "pattern_bracket_source",
    );
    assertCstRange(escapedOutput, "1:0-2:0", "quoted_here_document_body");
    assertCstRange(escapedOutput, "2:0-3:0", "here_document_end");
    assertIncrementalEqualsFresh(
      plain,
      escaped,
      `fold-backquote-bracket-heredoc-delimiter-${name}`,
      { byte: offset, deleteBytes: 0, insert: "\\\\" },
    );
    assertIncrementalEqualsFresh(
      escaped,
      plain,
      `unquote-backquote-bracket-heredoc-delimiter-${name}`,
      { byte: offset, deleteBytes: 2, insert: "" },
    );

    const closer = escapedSource.lastIndexOf("]");
    const unclosed = writeSource(
      `bracket-heredoc-${name}-unclosed`,
      escapedSource.slice(0, closer) + escapedSource.slice(closer + 1),
    );
    const unclosedOutput = parseValidCst(unclosed);
    assertNotContains(unclosedOutput, "pattern_bracket_source");
    assertCstRange(unclosedOutput, openerRange, "literal");
    assertIncrementalEqualsFresh(
      escaped,
      unclosed,
      `remove-bracket-after-backquote-heredoc-${name}`,
      { byte: closer, deleteBytes: 1, insert: "" },
    );
    assertIncrementalEqualsFresh(
      unclosed,
      escaped,
      `restore-bracket-after-backquote-heredoc-${name}`,
      { byte: closer, deleteBytes: 0, insert: "]" },
    );
  }
});

test("sh: adjacent comment continuations retain raw prefixes across edits", () => {
  const slash = "\\";
  const prefix = `: "\`: ${slash}"${slash}\`#x${slash.repeat(3)}\n`;
  const suffix = `\nworld\n${slash}\`${slash}"\`"\n`;
  const terminated = writeSource(
    "adjacent-comment-terminated",
    prefix + slash.repeat(2) + suffix,
  );
  const continued = writeSource(
    "adjacent-comment-continued",
    prefix + slash.repeat(4) + suffix,
  );
  const carried = writeSource(
    "comment-continuation-from-prefix",
    prefix + suffix,
  );
  const carriedOutput = parseValidCst(carried);
  assertOccurrenceCount(carriedOutput, "name: cmd_name", 2);
  assertOccurrenceCount(carriedOutput, "comment\n", 1);
  assertNodeCount(carriedOutput, "comment_text", 1);
  assert.equal(continuationManifest(carriedOutput).length, 1);
  assertCstRange(carriedOutput, "0:10-2:5", "comment");
  assertCstRange(carriedOutput, "0:10-2:5", "comment_text");
  assertCstRange(carriedOutput, "0:14-0:15", "line_continuation");
  const terminatedOutput = parseValidCst(terminated);
  assertOccurrenceCount(terminatedOutput, "name: cmd_name", 3);
  assertOccurrenceCount(terminatedOutput, "comment\n", 1);
  assertNodeCount(terminatedOutput, "comment_text", 1);
  assert.equal(continuationManifest(terminatedOutput).length, 1);
  assertCstRange(terminatedOutput, "0:10-1:2", "comment");
  assertCstRange(terminatedOutput, "0:10-1:2", "comment_text");
  assertCstRange(terminatedOutput, "0:14-0:15", "line_continuation");
  assertCstRange(terminatedOutput, "2:0-2:5", "name: cmd_name");
  const continuedOutput = parseValidCst(continued);
  assertOccurrenceCount(continuedOutput, "name: cmd_name", 2);
  assertOccurrenceCount(continuedOutput, "comment\n", 1);
  assertNodeCount(continuedOutput, "comment_text", 1);
  assert.equal(continuationManifest(continuedOutput).length, 2);
  assertCstRange(continuedOutput, "0:10-2:5", "comment");
  assertCstRange(continuedOutput, "0:10-2:5", "comment_text");
  assertCstRange(continuedOutput, "0:14-0:15", "line_continuation");
  assertCstRange(continuedOutput, "1:3-1:4", "line_continuation");
  assertIncrementalEqualsFresh(
    carried,
    terminated,
    "interrupt-carried-comment",
    {
      byte: prefix.length,
      deleteBytes: 0,
      insert: slash.repeat(2),
    },
  );
  assertIncrementalEqualsFresh(terminated, carried, "restore-carried-comment", {
    byte: prefix.length,
    deleteBytes: 2,
    insert: "",
  });
  assertIncrementalEqualsFresh(
    terminated,
    continued,
    "extend-adjacent-comment-run",
    { byte: prefix.length + 2, deleteBytes: 0, insert: slash.repeat(2) },
  );
  assertIncrementalEqualsFresh(
    continued,
    terminated,
    "shorten-adjacent-comment-run",
    { byte: prefix.length + 2, deleteBytes: 2, insert: "" },
  );
});

test("sh: word classification stays fresh-equivalent when static boundaries change", () => {
  for (const fixture of [
    {
      name: "quoted declaration name",
      source: '"exporx" X=~\n',
      edit: { byte: 6, deleteBytes: 1, insert: "t" },
      nodes: [
        ["assignment_value", 0, 1],
        ["tilde_expansion", 0, 1],
      ],
    },
    {
      name: "empty quote after tilde user",
      source: ": ~root/tail\n",
      edit: { byte: 7, deleteBytes: 0, insert: '""' },
      nodes: [["tilde_expansion", 1, 0]],
    },
    {
      name: "caret negation after empty quotes",
      source: ": [''^a]\n",
      edit: { byte: 3, deleteBytes: 2, insert: "" },
      nodes: [["pattern_bracket_negation_source", 1, 1]],
    },
    {
      name: "single quoted continuation is content rather than an empty quote",
      source: ": ['\\\n'^a]\n",
      edit: { byte: 4, deleteBytes: 2, insert: "" },
      nodes: [["pattern_bracket_negation_source", 0, 1]],
    },
    {
      name: "filename slash replaces a range operator",
      source: ": [a-b]\n",
      edit: { byte: 4, deleteBytes: 1, insert: "/" },
      nodes: [["pattern_bracket_source", 1, 0]],
    },
    {
      name: "quoted slash exposes filename wildcard",
      source: ': [a*"x"b[xy]\n',
      edit: { byte: 6, deleteBytes: 1, insert: "/" },
      nodes: [
        ["pattern_bracket_source", 1, 1],
        ["pattern_star_source", 0, 1],
      ],
    },
    {
      name: "case pattern and nested command use separate slash contexts",
      source: "case x in $(printf [a-b])[a/b]) :;; esac\n",
      edit: { byte: 20, deleteBytes: 1, insert: "/" },
      nodes: [["pattern_bracket_source", 2, 1]],
    },
  ]) {
    const initial = writeSource(`${fixture.name}-before`, fixture.source);
    const final = writeSource(
      `${fixture.name}-after`,
      applyEdits(fixture.source, [fixture.edit]).toString(),
    );
    const inverse = {
      byte: fixture.edit.byte,
      deleteBytes: Buffer.byteLength(fixture.edit.insert),
      insert: Buffer.from(fixture.source)
        .subarray(
          fixture.edit.byte,
          fixture.edit.byte + fixture.edit.deleteBytes,
        )
        .toString(),
    };
    for (const { before, after, edit, expectedIndex } of [
      { before: initial, after: final, edit: fixture.edit, expectedIndex: 2 },
      { before: final, after: initial, edit: inverse, expectedIndex: 1 },
    ]) {
      for (const output of assertIncrementalEqualsFresh(
        before,
        after,
        fixture.name,
        edit,
      )) {
        for (const nodes of fixture.nodes) {
          assertNodeCount(output, nodes[0], nodes[expectedIndex]);
        }
      }
    }
  }
});
