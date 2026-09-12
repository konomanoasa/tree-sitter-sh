import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { test } from "node:test";
import { pathToFileURL } from "node:url";
import nodeTypes from "../src/node-types.json" with { type: "json" };
import {
  assertContains,
  assertCstDirectChildRange,
  assertCstRange,
  assertIncrementalEqualsFresh,
  assertNotContains,
  assertOccurrenceCount,
  assertRepeatedColdParse,
  assertValid,
  cstFingerprint,
  hasRecovery,
  lineContinuationManifest,
  lines,
  parseValidCst,
  parseValidTree,
  runParse,
  runQuery,
  writeSource,
} from "./support/parser.js";

test("sh: backquote closers delimit keywords and trailing word continuations", () => {
  const contexts = [
    ["nested", ": `: \\`", "\\``\n", 2],
    ["deeper", ": `: \\`: \\\\\\`", "\\\\\\`\\``\n", 3],
    ["quoted", ': "`: \\`', '\\``"\n', 2],
  ];
  const bodies = [
    ["conditional", "if :; then :; fi", "if_clause", "fi_keyword", 14, 2],
    ["loop", "while :; do :; done", "while_clause", "done_keyword", 15, 4],
    ["case", "case x in x) :;; esac", "case_clause", "esac_keyword", 17, 4],
    ["brace", "{ :; }", "brace_group", '"}"', 5, 1],
    ["word", "x", "simple_command", "literal `x`", 0, 1],
  ];
  const layouts = [
    ["adjacent", "", []],
    ["continued", "\\\n", [0]],
    ["folded", "\\\\\n", [1]],
    ["spaced-folded", " \\\\\n", [2]],
  ];
  for (const [context, prefix, suffix, depth] of contexts) {
    for (const [bodyName, body, node, terminal, offset, length] of bodies) {
      for (const [layoutName, layout, continuationOffsets] of layouts) {
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
          continuationOffsets.map((column) => `0:${boundary + column}-1:0`),
          name,
        );
      }
    }
  }
});

test("sh: folded continuations preserve keyword and function-name classification", () => {
  for (const [name, sourceText, node, range] of [
    [
      "opening-keyword",
      ": `if\\\\\n :; then :; fi`\n",
      "if_keyword",
      "0:3-0:5",
    ],
    ["function-name", ": `f\\\\\n() { :; }`\n", "fname", "0:3-0:4"],
    ["spaced-function-name", ": `f \\\\\n() { :; }`\n", "fname", "0:3-0:4"],
  ]) {
    const source = writeSource(name, sourceText);
    const output = parseValidCst(source);
    assertCstRange(output, range, node);
    assertOccurrenceCount(output, node, 1);
    const newline = sourceText.indexOf("\n");
    assert.deepEqual(lineContinuationManifest(runQuery(source)), [
      `0:${newline - 1}-1:0`,
    ]);
  }
  for (const [name, sourceText] of [
    ["nested-start", ": `if\\`:\\``\n"],
    ["literal-slash-three", ": `if\\\\\\\n`\n"],
    ["literal-slash-four", ": `if\\\\\\\\\n`\n"],
  ]) {
    const source = writeSource(name, sourceText);
    const output = parseValidCst(source);
    assertOccurrenceCount(output, "if_keyword", 0);
    assertContains(output, "cmd_name");
    assert.deepEqual(lineContinuationManifest(runQuery(source)), []);
  }
});

test("sh: enclosing backquote escapes leave an unquoted delimiter unquoted", () => {
  for (const [name, contents, endRange, parameterRange] of [
    [
      "delimiter-outer-raw-escaping",
      lines(": `cat <<\\`printf x\\`", "\\$name", "\\`printf x\\`", "`"),
      "0:9-0:21",
      "1:0-1:6",
    ],
    [
      "delimiter-outer-double-quoted-escaping",
      lines(': "`cat <<\\`printf x\\`', "\\$name", "\\`printf x\\`", '`"'),
      "0:10-0:22",
      "1:0-1:6",
    ],
    [
      "delimiter-outer-nested-escaping",
      lines(
        ': `: "\\`cat <<\\\\\\`printf x\\\\\\`',
        "\\\\\\$name",
        "\\\\\\`printf x\\\\\\`",
        '\\`"',
        "`",
      ),
      "0:14-0:30",
      "1:0-1:8",
    ],
  ]) {
    const source = writeSource(name, contents);
    const { output } = runParse({ source, description: name });
    assertCstRange(output, endRange, "end: here_end");
    assertOccurrenceCount(output, "quoted_here_document_body", 0);
    assertOccurrenceCount(output, "here_document_body", 1);
    assertCstRange(output, parameterRange, "parameter_expansion");
    assertCstRange(output, "2:0-3:0", "end: here_document_end");
  }
});

test("sh: here-document delimiter commands share command boundary transitions", () => {
  for (const [name, contents, range] of [
    [
      "delimiter-for-without-separator",
      lines(
        "cat <<$(for x do case y in y) : ;; esac; done)",
        "$(for x do case y in y) : ;; esac; done)",
        "echo after",
      ),
      "0:6-0:46",
    ],
    [
      "delimiter-closed-command-redirect",
      lines(
        "cat <<$(case x in x) case y in y) { :; } >file esac;; z) : ;; esac)",
        "$(case x in x) case y in y) { :; } >file esac;; z) : ;; esac)",
        "echo after",
      ),
      "0:6-0:67",
    ],
  ]) {
    const source = writeSource(name, contents);
    const { output } = runParse({ source, description: name });
    assertCstDirectChildRange(
      output,
      range,
      "end: here_end",
      range,
      "word: word",
    );
    assertCstRange(output, "1:0-2:0", "end: here_document_end");
    assertCstRange(output, "2:0-2:10", "command: complete_command");
  }
});

test("sh: case-closing keyword prefixes remain ordinary words", () => {
  for (const [name, contents, expectedRanges] of [
    [
      "literal suffix",
      "case x in (x) esac% ;; esac\n",
      [
        ["0:14-0:19", "name: cmd_name"],
        ["0:14-0:19", "literal `esac%`"],
        ["0:23-0:27", "esac_keyword"],
      ],
    ],
    [
      "escaped suffix",
      "case x in (x) esac\\x ;; esac\n",
      [
        ["0:14-0:20", "name: cmd_name"],
        ["0:14-0:18", "literal `esac`"],
        ["0:18-0:20", "escaped_character"],
        ["0:24-0:28", "esac_keyword"],
      ],
    ],
    [
      "parameter suffix",
      "case x in (x) esac$var ;; esac\n",
      [
        ["0:14-0:22", "name: cmd_name"],
        ["0:14-0:18", "literal `esac`"],
        ["0:18-0:22", "parameter_expansion"],
        ["0:19-0:22", "parameter: variable_name `var`"],
        ["0:26-0:30", "esac_keyword"],
      ],
    ],
  ]) {
    const source = writeSource(`case-closing-keyword-${name}`, contents);
    const output = parseValidCst(source);
    assertOccurrenceCount(output, "esac_keyword", 1);
    for (const [range, item] of expectedRanges) {
      assertCstRange(output, range, item);
    }
  }
});

test("sh: realistic function structure remains queryable", () => {
  const functionStructure = [
    "sample_log() {",
    "  cat <<'SAMPLE'",
    "run: 2026-08-01T15:24:00Z",
    "suite: checkout-api",
    "check: creates order | 142ms | PASS",
    "check: rejects expired card | 87ms | FAIL",
    "failure: rejects expired card: expected 422, got 500",
    "SAMPLE",
    "}",
    "",
    "summarize() {",
    "  run_value=",
    "  suite_value=",
    "  pass_count=0",
    "  fail_count=0",
    "  total_ms=0",
    "",
    "  printf '%s\\n' 'CI test summary'",
    "",
    '  while IFS= read -r line || [ -n "$line" ]; do',
    "    case $line in",
    "    'run: '*)",
    "      [ -z \"$run_value\" ] || invalid_input 'duplicate run'",
    "      run_value=$(printf '%s\\n' \"$line\" | sed 's/^run: //')",
    "      [ -n \"$run_value\" ] || invalid_input 'empty run'",
    "      printf 'run: %s\\n' \"$run_value\"",
    "      ;;",
    "    'suite: '*)",
    "      [ -n \"$run_value\" ] || invalid_input 'suite before run'",
    "      [ -z \"$suite_value\" ] || invalid_input 'duplicate suite'",
    "      suite_value=$(printf '%s\\n' \"$line\" | sed 's/^suite: //')",
    "      [ -n \"$suite_value\" ] || invalid_input 'empty suite'",
    "      printf 'suite: %s\\n' \"$suite_value\"",
    "      ;;",
    "    'check: '*)",
    "      [ -n \"$suite_value\" ] || invalid_input 'check before suite'",
    "      check_data=$(printf '%s\\n' \"$line\" | sed 's/^check: //')",
    "      IFS='|' read -r check_name check_duration check_status <<EOF",
    "$check_data",
    "EOF",
    "      check_name=$(printf '%s\\n' \"$check_name\" | trim)",
    "      check_duration=$(printf '%s\\n' \"$check_duration\" | trim)",
    "      check_status=$(printf '%s\\n' \"$check_status\" | trim)",
    '      [ "$check_data" = "$check_name | $check_duration | $check_status" ] || invalid_input \'malformed check\'',
    "      case $check_duration in",
    "      *ms) duration_value=$(printf '%s\\n' \"$check_duration\" | sed 's/ms$//') ;;",
    "      *) invalid_input 'check duration must end in ms' ;;",
    "      esac",
    "      case $duration_value in",
    "      '' | *[!0-9]*) invalid_input 'check duration must be an integer' ;;",
    "      esac",
    "      case $check_status in",
    "      PASS)",
    "        pass_count=$((pass_count + 1))",
    "        ;;",
    "      FAIL)",
    "        fail_count=$((fail_count + 1))",
    "        ;;",
    "      *)",
    "        invalid_input 'check status must be PASS or FAIL'",
    "        ;;",
    "      esac",
    "      total_ms=$((total_ms + duration_value))",
    '      printf \'%s %s (%sms)\\n\' "$check_status" "$check_name" "$duration_value"',
    "      ;;",
    "    'failure: '*)",
    "      [ \"$fail_count\" -gt 0 ] || invalid_input 'failure without a failed check'",
    "      failure_data=$(printf '%s\\n' \"$line\" | sed 's/^failure: //')",
    "      IFS=: read -r failure_name failure_message <<EOF",
    "$failure_data",
    "EOF",
    "      failure_name=$(printf '%s\\n' \"$failure_name\" | trim)",
    "      failure_message=$(printf '%s\\n' \"$failure_message\" | trim)",
    '      [ "$failure_data" = "$failure_name: $failure_message" ] || invalid_input \'malformed failure\'',
    '      [ -n "$failure_name" ] && [ -n "$failure_message" ] || invalid_input \'empty failure\'',
    '      printf \'  failure: %s: %s\\n\' "$failure_name" "$failure_message"',
    "      ;;",
    "    '')",
    "      ;;",
    "    *)",
    "      invalid_input 'unknown record'",
    "      ;;",
    "    esac",
    "  done",
    "",
    "  [ -n \"$run_value\" ] || invalid_input 'missing run'",
    "  [ -n \"$suite_value\" ] || invalid_input 'missing suite'",
    '  [ "$pass_count" -gt 0 ] || [ "$fail_count" -gt 0 ] || invalid_input \'missing check\'',
    '  printf \'result: %s passed, %s failed, %sms total\\n\' "$pass_count" "$fail_count" "$total_ms"',
    "",
    '  [ "$fail_count" -eq 0 ]',
    "}",
    "",
  ].join("\n");
  const source = writeSource("function-structure", functionStructure);
  parseValidTree(source);
  const queryOutput = runQuery(source);
  assertContains(queryOutput, "sample_log");
  assertContains(queryOutput, "summarize");
});

test("sh: lexical leaves retain complete source without internal token children", () => {
  const source = writeSource(
    "lexical-source-leaves",
    lines(
      ": abc:def=a~b",
      ': "a\\q$"',
      ": [!:] [[:alpha:]] [[.a.b.]] [[=a=b=]]",
      "if :; then :; fi",
      ": && : || : >>out",
      `: \${#-} \${x:-y} $((1+2))`,
      "cat <<$",
      "\\q$",
      "$",
    ),
  );
  const output = parseValidCst(source);
  for (const [range, leaf] of [
    ["0:2-0:13", "literal `abc:def=a~b`"],
    ["1:3-1:6", "double_quote_text `a\\\\q`"],
    ["1:6-1:7", "double_quote_text `$`"],
    ["2:3-2:4", "pattern_bracket_negation_source `!`"],
    ["2:4-2:5", "pattern_bracket_character_source `:`"],
    ["2:10-2:15", "pattern_character_class_content_source `alpha`"],
    ["2:23-2:24", "pattern_collating_symbol_character_source `.`"],
    ["2:33-2:34", "pattern_equivalence_class_character_source `=`"],
    ["3:0-3:2", "if_keyword `if`"],
    ["3:6-3:10", "then_keyword `then`"],
    ["3:14-3:16", "fi_keyword `fi`"],
    ["4:2-4:4", "and_if `&&`"],
    ["4:7-4:9", "or_if `||`"],
    ["4:12-4:14", "dgreat `>>`"],
    ["5:4-5:5", "parameter_length_operator `#`"],
    ["5:5-5:6", "special_parameter `-`"],
    ["5:11-5:13", "parameter_value_operator `:-`"],
    ["5:20-5:21", "arithmetic_operator `+`"],
    ["6:4-6:6", "dless `<<`"],
    ["7:0-7:1", "here_document_text `\\\\`"],
    ["7:2-7:3", "here_document_text `$`"],
    ["8:0-8:1", "here_document_end_text `$`"],
  ]) {
    assertCstRange(output, range, leaf);
  }
});

test("sh: parameter bracket elements retain literal spaces and dollars", () => {
  for (const [name, contents, range, leaf] of [
    [
      "collating-space",
      `: \${x#[[. .]]}\n`,
      "0:9-0:10",
      "pattern_collating_symbol_character_source ` `",
    ],
    [
      "equivalence-space",
      `: \${x#[[= =]]}\n`,
      "0:9-0:10",
      "pattern_equivalence_class_character_source ` `",
    ],
    [
      "collating-semicolon",
      `: \${x#[[.;.]]}\n`,
      "0:9-0:10",
      "pattern_collating_symbol_character_source `;`",
    ],
    [
      "collating-literal-dollar",
      `: \${x#[[.$ .]]}\n`,
      "0:9-0:10",
      "pattern_collating_symbol_character_source `$`",
    ],
    [
      "deferred-literal-dollar",
      `: \${x#[[:$ ]}\n`,
      "0:9-0:10",
      "pattern_bracket_character_source `$`",
    ],
  ]) {
    const source = writeSource(name, contents);
    const output = parseValidCst(source);
    assertCstRange(output, range, leaf);
    assertOccurrenceCount(output, "parameter_expansion", 1);
    assertOccurrenceCount(output, "parameter_pattern_operator", 1);
    assertOccurrenceCount(output, "pattern_bracket_source", 1);
  }
});

test("sh: missing closing keywords remain public recovery tokens", () => {
  const directory = mkdtempSync(join(tmpdir(), "sh-missing-keywords-"));
  try {
    const query = join(directory, "missing.scm");
    writeFileSync(query, "(MISSING) @missing\n");
    for (const [name, contents, keyword] of [
      ["conditional", "if :; then :; f\n", "fi_keyword"],
      ["loop", "while :; do :;\n", "done_keyword"],
      ["case", "case x in x) :;;\n", "esac_keyword"],
    ]) {
      const source = writeSource(`missing-${name}-closer`, contents);
      const parsed = runParse({ source, description: name, mode: "recovery" });
      assert.equal(parsed.status, 1);
      assertCstRange(parsed.output, "1:0-1:0", keyword);
      const captures = runQuery(source, query);
      assertOccurrenceCount(captures, "capture: 0 - missing", 1);
      assertContains(captures, "start: (1, 0), end: (1, 0)");
    }
  } finally {
    rmSync(directory, { recursive: true, force: true });
  }
});

test("sh: pattern bracket ranges require one start and one end", () => {
  const range = nodeTypes.find(
    ({ type, named }) => named && type === "pattern_bracket_range_source",
  );
  assert.ok(range);
  for (const field of ["start", "end"]) {
    assert.equal(range.fields[field].required, true, field);
    assert.equal(range.fields[field].multiple, false, field);
  }
});

test("sh: public fields retain optional roots and singular source owners", () => {
  for (const [type, field, required, types] of [
    ["program", "commands", false, ["complete_commands"]],
    [
      "command",
      "body",
      true,
      ["compound_command", "function_definition", "simple_command"],
    ],
    ["command", "redirects", false, ["redirect_list"]],
    ["function_body", "body", true, ["compound_command"]],
    ["function_body", "redirects", false, ["redirect_list"]],
    ["here_end", "word", true, ["word"]],
    [
      "here_document",
      "body",
      false,
      ["here_document_body", "quoted_here_document_body"],
    ],
    ["here_document", "end", true, ["here_document_end"]],
    [
      "parameter_expansion",
      "parameter",
      false,
      ["positional_parameter", "special_parameter", "variable_name"],
    ],
  ]) {
    const node = nodeTypes.find((node) => node.named && node.type === type);
    assert.ok(node, type);
    assert.deepEqual(
      node.fields[field],
      {
        multiple: false,
        required,
        types: types.map((type) => ({ type, named: true })),
      },
      `${type}.${field}`,
    );
  }
});

test("sh: malformed commands parse through EOF with native errors", () => {
  for (const [name, invalidCommand] of [
    ["stray-right-parenthesis", ")"],
    ["missing-redirection-target", "broken >;"],
    ["closed-empty-parameter", `broken \${}`],
    ["empty-subshell", "()"],
    ["empty-subshell-with-blank", "( )"],
    ["empty-subshell-with-terminator", "(;)"],
    ["empty-subshell-with-newline", "(\n)"],
    ["empty-subshell-with-redirection", "() >x"],
    ["missing-target-in-closed-subshell", "( : > )"],
    ["reserved-closer-command", "fi"],
    ["right-brace-command", "}"],
    ["case-break-after-top-level-command", "broken;;"],
    ["case-fallthrough-after-top-level-command", "broken ;&"],
    ["bang-past-pipeline-head", "! ! alpha"],
    ["missing-pipe-command-after-comment", "broken | # pending\n;"],
    ["missing-and-command-after-linebreak", "broken &&\n;"],
    ["missing-command-after-operator-run", "broken && ! | ;"],
    ["missing-negated-command-before-newline", "! # pending"],
  ]) {
    const source = writeSource(
      `malformed-${name}`,
      lines("before alpha", invalidCommand, "after omega"),
    );
    const { status } = runParse({
      description: name,
      mode: "recovery",
      source,
    });
    assert.equal(status, 1, `${name}: invalid command parsed as valid`);
  }
});

test("sh: unterminated structures parse through EOF", () => {
  for (const [name, contents] of [
    ["double-quote", lines('echo "open', "after")],
    ["parameter-expansion", lines("echo $" + "{value", "after")],
    ["command-substitution", lines("echo $(inside", "after")],
    ["arithmetic-expansion", lines("echo $((1 +", "after")],
    ["backquote-substitution", lines("echo `inside", "after")],
    ["here-document", lines("cat <<EOF", "body", "after")],
    ["compound-command", lines("if condition; then", "inside", "after")],
  ]) {
    const recoverySource = writeSource(`unterminated-${name}`, contents);
    runParse({
      description: `unterminated ${name}`,
      mode: "recovery",
      source: recoverySource,
    });
  }
});

test("sh: I/O location extension is rejected after compound commands", () => {
  for (const [name, command] of [
    ["brace-group", "{ :; } {fd}>out"],
    ["function-body", "f() { :; } {fd}>out"],
  ]) {
    const source = writeSource(`io-location-extension-${name}`, lines(command));
    const { output, status } = runParse({
      description: `${name} I/O location extension`,
      mode: "recovery",
      source,
    });
    assert.equal(status, 1, `${name}: optional I/O location parsed as valid`);
    assertNotContains(output, "io_location");
  }
});

test("sh: nested here-documents activate at the scanner state capacity", () => {
  function source(outerLength) {
    const outer = "A".repeat(outerLength);
    const first = "B".repeat(251);
    const second = "C".repeat(251);
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

  const below = writeSource("nested-documents-below-capacity", source(501));
  const exact = writeSource("nested-documents-at-capacity", source(503));
  const above = writeSource("nested-documents-above-capacity", source(504));
  const outputs = [parseValidCst(exact)];
  outputs.push(
    ...assertIncrementalEqualsFresh(
      below,
      exact,
      "grow-nested-document-metadata-to-capacity",
      {
        byte: source(501).lastIndexOf("\nA") + 1,
        deleteBytes: 0,
        insert: "AA",
      },
      { byte: 6, deleteBytes: 0, insert: "AA" },
    ),
    ...assertIncrementalEqualsFresh(
      above,
      exact,
      "restore-nested-document-metadata-within-capacity",
      { byte: source(504).lastIndexOf("\nA") + 1, deleteBytes: 1, insert: "" },
      { byte: 6, deleteBytes: 1, insert: "" },
    ),
  );
  for (const output of outputs) {
    assertOccurrenceCount(output, "document: here_document", 3);
    assertCstRange(output, "0:6-0:509", "end: here_end");
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
    { byte: source(503).lastIndexOf("\nA") + 1, deleteBytes: 2, insert: "" },
    { byte: 6, deleteBytes: 2, insert: "" },
  );
  assertRepeatedColdParse("resource", above, "nested-documents-above-capacity");
});

test("sh: parser resource bounds preserve complete roots and deterministic recovery", () => {
  const largeValidSources = [
    [
      "deep-arithmetic-expansions",
      `echo ${"$((".repeat(2000)}1${"))".repeat(2000)}\n`,
    ],
    ["unmatched-brackets", `printf ${"[".repeat(80_000)}\n`],
    ["long-bracket-list", `printf [${"a".repeat(16_000)}]\n`],
    ["repeated-brackets", `printf ${"[abc]".repeat(4_000)}\n`],
    [
      "repeated-incomplete-special-brackets",
      `printf [${"[:alpha:]".repeat(4_000)}\n`,
    ],
    ["continued-blank-lines", `printf value ${"\\\n".repeat(20_000)}after\n`],
    ["continued-pipe-linebreak", `first|${"\\\n".repeat(20_000)}next\n`],
    [
      "here-document-dollars",
      `cat <<EOF\n${"$x".repeat(16_000)}\nEOF\nafter\n`,
    ],
  ];
  for (const [name, contents] of largeValidSources) {
    assertValid(writeSource(name, contents), name);
  }

  const manyDocuments = writeSource(
    "many-documents",
    `cat${" <<X".repeat(600)}\n${"body\nX\n".repeat(600)}after\n`,
  );
  assertRepeatedColdParse("resource", manyDocuments, "many-documents");

  function nestedDocuments(depth) {
    let contents = "cat <<X\n";
    contents += "$(cat <<X\n".repeat(depth - 1);
    contents += "leaf\n";
    for (let counter = depth - 1; counter >= 0; counter -= 1) {
      contents += "X\n";
      if (counter > 0) {
        contents += ")\n";
      }
    }
    return `${contents}after\n`;
  }

  const deepDocuments = writeSource("deep-documents", nestedDocuments(150));
  assertValid(deepDocuments);
  const boundedDocuments = writeSource(
    "deep-documents-bounded",
    nestedDocuments(300),
  );
  assertRepeatedColdParse(
    "resource",
    boundedDocuments,
    "deep-documents-bounded",
    30_000_000,
  );

  let nestedBackquotes = "echo `level1 ";
  for (let level = 2; level <= 6; level += 1) {
    nestedBackquotes += `${"\\".repeat(2 ** (level - 1) - 1)}\`level${level} `;
  }
  nestedBackquotes += "leaf";
  for (let level = 6; level >= 2; level -= 1) {
    nestedBackquotes += `${"\\".repeat(2 ** (level - 1) - 1)}\` end${level}`;
  }
  nestedBackquotes += "`\n";
  assertValid(writeSource("nested-backquotes", nestedBackquotes));
});

test("sh: parser rejects a timeout as structural recovery", () => {
  const source = writeSource("timeout-guard", `${":\n".repeat(10_000)}`);
  assert.throws(() =>
    runParse({
      description: "timeout guard",
      mode: "recovery",
      source,
      timeout: 1,
    }),
  );
});

test("sh: CST fingerprints distinguish anonymous tokens", () => {
  const semicolon = writeSource("semicolon-fingerprint", "a;b\n");
  const ampersand = writeSource("ampersand-fingerprint", "a&b\n");
  assert.notEqual(
    cstFingerprint(parseValidCst(semicolon)),
    cstFingerprint(parseValidCst(ampersand)),
  );
});

test("sh: CST fingerprints preserve continuation ranges and order without ownership", () => {
  const root = "0:0 - 2:0   program";
  const first = "0:0 - 1:0     line_continuation";
  const firstSource = "0:0 - 0:2       `\\\\\\n`";
  const second = "1:0 - 2:0     line_continuation";
  const secondSource = "1:0 - 1:2       `\\\\\\n`";
  const source = lines(root, first, firstSource, second, secondSource);
  const expected = cstFingerprint(source);
  assert.equal(
    cstFingerprint(
      lines(
        root,
        "0:0 - 1:0     leading: line_continuation",
        firstSource,
        "1:0 - 2:0     trailing: line_continuation",
        secondSource,
      ),
    ),
    expected,
  );
  assert.equal(cstFingerprint(lines(root, first, second)), expected);
  for (const [name, changed] of [
    ["missing continuation", lines(root, first, firstSource)],
    ["duplicate continuation", lines(root, first, first, second)],
    ["reversed continuations", lines(root, second, first)],
    [
      "changed continuation range",
      lines(root, first, "1:1 - 2:0     line_continuation"),
    ],
    ["changed root range", lines("0:0 - 3:0   program", first, second)],
  ]) {
    assert.notEqual(cstFingerprint(changed), expected, name);
  }
});

test("sh: Unicode source retains byte ranges without normalization", () => {
  const source = writeSource(
    "unicode-source-ranges",
    "printf é😀\nprintf e\u0301\n",
  );
  const output = parseValidCst(source);
  assertCstRange(output, "0:7-0:13", "literal `é😀`");
  assertCstRange(output, "1:7-1:10", "literal `é`");
});

test("sh: long unterminated quotes parse through EOF with native recovery", () => {
  const source = writeSource(
    "long-unterminated-quote",
    `printf "${"x".repeat(80_000)}`,
  );
  const result = runParse({
    source,
    mode: "recovery",
    description: "long unterminated quote",
  });
  assert.equal(hasRecovery(result.output), true);
});

test("sh: corpus fuzz propagates CLI failures even when its exit status is zero", () => {
  const directory = mkdtempSync(join(tmpdir(), "tree-sitter-fuzz-exit-#-"));
  const preload = join(directory, "cli.mjs");
  const script = join(import.meta.dirname, "..", "scripts", "tree-sitter.js");
  const fixtures = [
    {
      name: "successful CLI output",
      status: 0,
      stdout: "0 test_language corpus tests failed fuzzing\n",
      stderr: "",
      expectedStatus: 0,
    },
    {
      name: "failed fuzz case with successful CLI exit status",
      status: 0,
      stdout: "1 test_language corpus tests failed fuzzing\n",
      stderr: "",
      expectedStatus: 1,
    },
    {
      name: "failed CLI exit status",
      status: 1,
      stdout: "",
      stderr: "fuzz command failed\n",
      expectedStatus: 1,
    },
  ];
  try {
    for (const fixture of fixtures) {
      writeFileSync(
        preload,
        `
import childProcess from "node:child_process";
import { syncBuiltinESMExports } from "node:module";
const fixture = ${JSON.stringify(fixture)};
childProcess.spawnSync = (_command, arguments_) => {
  if (arguments_.includes("build")) return { status: 0, stdout: "", stderr: "" };
  if (arguments_.includes("fuzz")) return fixture;
  throw new Error("unexpected CLI invocation");
};
syncBuiltinESMExports();
`,
      );
      const result = spawnSync(
        process.execPath,
        ["--import", pathToFileURL(preload).href, script, "fuzz-all"],
        {
          encoding: "utf8",
          timeout: 60_000,
          killSignal: "SIGKILL",
        },
      );
      assert.ifError(result.error);
      assert.equal(
        result.status,
        fixture.expectedStatus,
        `${fixture.name}\n${result.stdout}${result.stderr}`,
      );
      assert.ok(
        result.stdout.includes(fixture.stdout),
        `${fixture.name}: CLI stdout is missing`,
      );
      assert.ok(
        result.stderr.includes(fixture.stderr),
        `${fixture.name}: CLI stderr is missing`,
      );
    }
  } finally {
    rmSync(directory, { recursive: true, force: true });
  }
});
