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
  lineContinuationManifest,
  nodeIdentity,
  parseCst,
  sourceByteOffset,
} from "./support/cst.js";
import {
  assertIncrementalEqualsFresh,
  assertRepeatedColdParse,
  assertValid,
  lines,
  parseRecoveryAfterEdits,
  parseValidCst,
  parseValidTree,
  runParse,
  runQuery,
  writeSource,
} from "./support/parser.js";
import { scalingWorkloads } from "./support/workloads.js";

const scalingInputMultipliers = [1, 2, 4];

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
      `0:${newline - 1}-0:${newline}`,
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

test("sh: function-name edits enforce special built-in restrictions on logical source", () => {
  for (const [name, initial] of [
    ["continued-export", "ex\\\nport_() { :; }\n"],
    ["continued-readonly", "rea\\\ndonly_ () { :; }\n"],
    ["command-substitution", "echo $(continue_() { :; })\n"],
    ["backquote-substitution", ": `un\\\\\nset_() { :; }`\n"],
    ["here-document-substitution", "cat <<EOF\n$(trap_() { :; })\nEOF\n"],
  ]) {
    const byte = initial.indexOf("_");
    const invalid = initial.slice(0, byte) + initial.slice(byte + 1);
    const validSource = writeSource(`${name}-valid`, initial);
    const invalidSource = writeSource(`${name}-invalid`, invalid);
    for (const output of parseRecoveryAfterEdits(
      validSource,
      invalidSource,
      name,
      { byte, deleteBytes: 1, insert: "" },
    )) {
      assert.equal(hasRecovery(output), true, `${name}\n${output}`);
    }
    for (const output of assertIncrementalEqualsFresh(
      invalidSource,
      validSource,
      `${name}-repair`,
      { byte, deleteBytes: 0, insert: "_" },
    )) {
      assertNodeCount(output, "fname", 1);
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

test("sh: continued declaration arguments preserve assignment structure", () => {
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
      afterNames: ["x"],
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

test("sh: encoded backquote continuations preserve I/O number classification", () => {
  for (const prefix of [": `", ": `: ", ": `A= ", ": `: arg "]) {
    const initial = writeSource(
      "backquote-io-number-adjacent",
      `${prefix}2>out\`\n`,
    );
    const final = writeSource(
      "backquote-io-number-continued",
      `${prefix}2${"\\".repeat(2)}\n>out\`\n`,
    );
    const output = parseValidCst(final);
    assertCstRange(
      output,
      `0:${prefix.length}-0:${prefix.length + 1}`,
      "io_number",
    );
    assertCstRange(
      output,
      `0:${prefix.length + 2}-0:${prefix.length + 3}`,
      "line_continuation",
    );
    assertIncrementalEqualsFresh(
      initial,
      final,
      "continue-backquote-io-number",
      {
        byte: prefix.length + 1,
        deleteBytes: 0,
        insert: `${"\\".repeat(2)}\n`,
      },
    );
    assertIncrementalEqualsFresh(final, initial, "join-backquote-io-number", {
      byte: prefix.length + 1,
      deleteBytes: 3,
      insert: "",
    });
  }
});

test("sh: I/O number classification survives quoted filename and continuation edits", () => {
  for (const { name, marked, ioNumbers } of [
    { name: "quoted-prefix", marked: "echo >''2§>3\n", ioNumbers: 0 },
    { name: "quoted-tail", marked: "echo >2§''>3\n", ioNumbers: 0 },
    { name: "assignment-value", marked: "x=2§>3\n", ioNumbers: 0 },
    { name: "separated-filename", marked: "echo >2§ >3\n", ioNumbers: 0 },
    { name: "descriptor", marked: "echo 2§>3\n", ioNumbers: 1 },
  ]) {
    const offset = marked.indexOf("§");
    const plain = writeSource(`io-word-${name}-plain`, marked.replace("§", ""));
    const continued = writeSource(
      `io-word-${name}-continued`,
      marked.replace("§", "\\\n"),
    );
    const plainOutput = parseValidCst(plain);
    const output = parseValidCst(continued);
    assertNodeCount(output, "io_number", ioNumbers);
    assert.deepEqual(continuationManifest(output), [
      `0:${offset}-0:${offset + 1}`,
    ]);
    assertSameLogicalProjection(name, plainOutput, output);
    assertIncrementalEqualsFresh(plain, continued, `continue-io-${name}`, {
      byte: offset,
      deleteBytes: 0,
      insert: "\\\n",
    });
    assertIncrementalEqualsFresh(continued, plain, `join-io-${name}`, {
      byte: offset,
      deleteBytes: 2,
      insert: "",
    });
  }
  for (const { name, invalid, offset } of [
    { name: "output-filename", invalid: "echo >2>3\n", offset: 6 },
    { name: "input-filename", invalid: "<2>x\n", offset: 1 },
    {
      name: "continued-output-filename",
      invalid: "echo >2\\\n>3\n",
      offset: 6,
    },
    { name: "continued-input-filename", invalid: "<2\\\n>x\n", offset: 1 },
  ]) {
    const source = writeSource(`io-word-${name}-invalid`, invalid);
    const { status } = runParse({
      source,
      description: name,
      mode: "recovery",
    });
    assert.equal(status, 1);
    const repaired = writeSource(
      `io-word-${name}-quoted`,
      `${invalid.slice(0, offset)}''${invalid.slice(offset)}`,
    );
    for (const output of assertIncrementalEqualsFresh(
      source,
      repaired,
      `quote-${name}`,
      { byte: offset, deleteBytes: 0, insert: "''" },
    )) {
      assertNodeCount(output, "io_number", 0);
    }
  }
});

test("sh: case closers and keyword headers survive edits", () => {
  const emptyNsItemBodyInitial = writeSource(
    "case-ns-item-body-initial",
    lines("case x in x) run", "esac"),
  );
  const emptyNsItemBodyFinal = writeSource(
    "case-ns-item-body-final",
    lines("case x in x)", "esac"),
  );
  parseRecoveryAfterEdits(
    emptyNsItemBodyInitial,
    emptyNsItemBodyFinal,
    "delete-ns-item-body",
    { byte: 12, deleteBytes: 4, insert: "" },
  );

  const headerSeparatorInitial = writeSource(
    "for-header-separator-initial",
    lines("for x; do :; done", "after"),
  );
  const headerSeparatorFinal = writeSource(
    "for-header-separator-final",
    lines("for; do :; done", "after"),
  );
  parseRecoveryAfterEdits(
    headerSeparatorInitial,
    headerSeparatorFinal,
    "delete-for-name-before-separator",
    { byte: 3, deleteBytes: 2, insert: "" },
  );

  const loneSeparatorInitial = writeSource(
    "lone-separator-initial",
    lines("a; b"),
  );
  const loneSeparatorFinal = writeSource("lone-separator-final", lines("a; ;"));
  parseRecoveryAfterEdits(
    loneSeparatorInitial,
    loneSeparatorFinal,
    "replace-command-with-lone-separator",
    { byte: 3, deleteBytes: 1, insert: ";" },
  );
});

test("sh: reserved-word closers keep their term stable across edits", () => {
  const blankInitial = writeSource(
    "closer-blank-initial",
    lines("if a", "then b ", "else c", "fi"),
  );
  const blankFinal = writeSource(
    "closer-blank-final",
    lines("if a", "then b ", "exse c", "fi"),
  );
  assertIncrementalEqualsFresh(blankInitial, blankFinal, "closer-blank", {
    byte: 14,
    deleteBytes: 1,
    insert: "x",
  });

  const elseInitial = writeSource(
    "closer-else-initial",
    lines("if", "d", "then \\", " c", "else", "fi"),
  );
  const elseFinal = writeSource(
    "closer-else-final",
    lines("if", "d", "then \\", " c", "el#se", "fi"),
  );
  assertIncrementalEqualsFresh(
    elseInitial,
    elseFinal,
    "closer-else",
    { byte: 0, deleteBytes: 0, insert: "" },
    { byte: 17, deleteBytes: 0, insert: "#" },
  );

  const elifInitial = writeSource(
    "closer-elif-initial",
    lines("_()if", "sac;then \\", " c", "elif", "fi"),
  );
  const elifFinal = writeSource(
    "closer-elif-final",
    lines("_()if", "c;then \\", " c", "e=lif", "fi"),
  );
  assertIncrementalEqualsFresh(
    elifInitial,
    elifFinal,
    "closer-elif",
    { byte: 6, deleteBytes: 2, insert: "" },
    { byte: 19, deleteBytes: 0, insert: "=" },
  );

  const blankLineInitial = writeSource(
    "closer-blank-line-initial",
    lines("if", "e", "then \\", " ", "else", "fi"),
  );
  const blankLineFinal = writeSource(
    "closer-blank-line-final",
    lines("if", "e", "then \\", " -", "els", "fi"),
  );
  assertIncrementalEqualsFresh(
    blankLineInitial,
    blankLineFinal,
    "closer-blank-line",
    { byte: 13, deleteBytes: 0, insert: "-" },
    { byte: 0, deleteBytes: 0, insert: "" },
    { byte: 18, deleteBytes: 1, insert: "" },
  );
});

test("sh: a compound-list closer stays stable when an edit turns it into a command", () => {
  const withClose = writeSource(
    "closer-stable-initial",
    "if x; then a\\\n\nelse b\nfi\n",
  );
  const withCommand = writeSource(
    "closer-stable-final",
    "if x; then a\\\n\nelsXe b\nfi\n",
  );
  assertIncrementalEqualsFresh(withClose, withCommand, "closer-stable-else", {
    byte: 18,
    deleteBytes: 0,
    insert: "X",
  });

  const chainClose = writeSource(
    "closer-chain-initial",
    "if a; then b\\\n\nelif c; then d\\\n\nelse e\\\n\nfi\n",
  );
  const chainCommand = writeSource(
    "closer-chain-final",
    "if a; then b\\\n\nelif c; then d\\\n\nelsXe e\\\n\nfi\n",
  );
  assertIncrementalEqualsFresh(
    chainClose,
    chainCommand,
    "closer-stable-chain",
    { byte: 35, deleteBytes: 0, insert: "X" },
  );
});

test("sh: compound-list and case branches retain their public structure", () => {
  const branchInitial = writeSource(
    "compound-list-branch-initial",
    lines(
      "f() {",
      " a=",
      " if :; then",
      "  :",
      " else",
      "  :",
      " fi",
      " b=",
      "}",
    ),
  );
  const branchFinal = writeSource(
    "compound-list-branch-final",
    lines(
      "f() {",
      " a=",
      " if :; then",
      "  :",
      " else",
      "  :",
      " fi",
      " while :; do :; done",
      " b=",
      "}",
    ),
  );
  for (const output of assertIncrementalEqualsFresh(
    branchInitial,
    branchFinal,
    "insert-compound-list-loop",
    { byte: 40, deleteBytes: 0, insert: " while :; do :; done\n" },
  )) {
    assertCstRange(output, "0:0-9:1", "function_definition");
    assertCstRange(output, "0:4-9:1", "brace_group");
    assertCstDirectChildRange(
      output,
      "0:4-9:1",
      "brace_group",
      "0:5-9:0",
      "body: compound_list",
    );
    assertCstDirectChildRange(
      output,
      "0:5-9:0",
      "body: compound_list",
      "1:1-8:3",
      "body: term",
    );
    assertCstRange(output, "2:1-6:3", "if_clause");
    assertCstRange(output, "7:1-7:20", "while_clause");
    assertOccurrenceCount(output, "assignment: assignment_word", 2);
    assertCstRange(output, "1:1-1:3", "assignment: assignment_word");
    assertCstRange(output, "8:1-8:3", "assignment: assignment_word");
    assertCstRange(output, "0:0-10:0", "program");
    assertNotContains(output, "ERROR");
  }

  const caseBranchInitial = writeSource(
    "case-branch-initial",
    lines(
      "{",
      "  if :; then",
      "    :",
      "  else",
      "    :",
      "  fi",
      "  case x in",
      "    a) ;;",
      "    b) a= ;;",
      "  esac",
      "}",
    ),
  );
  const caseBranchFinal = writeSource(
    "case-branch-final",
    lines(
      "{",
      "  if :; then",
      "    :",
      "  else",
      "    :",
      "  fi",
      "  :",
      "  case x in",
      "    a) ;;",
      "    b) a= ;;",
      "  esac",
      "}",
    ),
  );
  for (const output of assertIncrementalEqualsFresh(
    caseBranchInitial,
    caseBranchFinal,
    "insert-command-before-case-branch",
    { byte: 39, deleteBytes: 0, insert: "  :\n" },
  )) {
    assertCstDirectChildRange(
      output,
      "0:1-11:0",
      "body: compound_list",
      "1:2-10:6",
      "body: term",
    );
    assertCstRange(output, "7:2-10:6", "case_clause");
    assertCstDirectChildRange(
      output,
      "7:2-10:6",
      "case_clause",
      "8:4-10:2",
      "items: case_list",
    );
    assertCstDirectChildRange(
      output,
      "8:4-10:2",
      "items: case_list",
      "9:4-10:2",
      "item: case_item",
    );
    assertCstDirectChildRange(
      output,
      "9:4-10:2",
      "item: case_item",
      "9:6-9:9",
      "body: compound_list",
    );
    assertCstDirectChildRange(
      output,
      "9:4-10:2",
      "item: case_item",
      "9:10-9:12",
      "terminator: dsemi `;;`",
    );
    assertCstDirectChildRange(
      output,
      "7:2-10:6",
      "case_clause",
      "10:2-10:6",
      "esac_keyword `esac`",
    );
    assertOccurrenceCount(output, "terminator: dsemi", 2);
    assertNotContains(output, "ERROR");
  }

  const compoundListInitial = writeSource(
    "compound-list-initial",
    lines(
      "f() {",
      "  before=",
      "  first ||",
      "    second",
      "  target=$(one) || recover",
      "}",
    ),
  );
  const compoundListFinal = writeSource(
    "compound-list-final",
    lines(
      "f() {",
      "  before=",
      "  first ||",
      "    second",
      "  targets=$(one) || recover",
      "}",
    ),
  );
  assertValid(compoundListFinal);
  assertIncrementalEqualsFresh(
    compoundListInitial,
    compoundListFinal,
    "compound-list-assignment-edit",
    { byte: 46, deleteBytes: 0, insert: "s" },
  );

  const caseItemInitial = writeSource(
    "case-item-initial",
    lines(
      "summarize() {",
      "  case value in",
      "    first) before ;;",
      "    second) condition ;;",
      "  esac",
      "}",
    ),
  );
  const caseItemFinal = writeSource(
    "case-item-final",
    lines(
      "summarize() {",
      "  case value in",
      "    first) before ;;",
      "    second) condition || invalid_input ;;",
      "  esac",
      "}",
    ),
  );
  assertValid(caseItemFinal);
  assertIncrementalEqualsFresh(
    caseItemInitial,
    caseItemFinal,
    "case-item-and-or-insertion",
    { byte: 73, deleteBytes: 0, insert: "|| invalid_input " },
  );
  assertIncrementalEqualsFresh(
    caseItemFinal,
    caseItemInitial,
    "case-item-and-or-deletion",
    { byte: 73, deleteBytes: 17, insert: "" },
  );

  const caseItemBoundaryInitial = writeSource(
    "case-item-boundary-initial",
    lines("case x in x) :\\", ";; esac"),
  );
  const caseItemBoundaryFinal = writeSource(
    "case-item-boundary-final",
    lines("case x in x) :\\", "\\", ";; esac"),
  );
  assertIncrementalEqualsFresh(
    caseItemBoundaryInitial,
    caseItemBoundaryFinal,
    "insert-second-case-item-boundary-continuation",
    { byte: 16, deleteBytes: 0, insert: "\\\n" },
  );
  assertIncrementalEqualsFresh(
    caseItemBoundaryFinal,
    caseItemBoundaryInitial,
    "delete-second-case-item-boundary-continuation",
    { byte: 16, deleteBytes: 2, insert: "" },
  );

  const caseBodyInitial = writeSource(
    "case-body-separator-initial",
    lines("case x in x)echo z;;esac"),
  );
  const caseBodyFinal = writeSource(
    "case-body-separator-final",
    lines("case x in x)echo \\", "z;;esac"),
  );
  assertIncrementalEqualsFresh(
    caseBodyInitial,
    caseBodyFinal,
    "insert-case-body-separator-continuation",
    { byte: 17, deleteBytes: 0, insert: "\\\n" },
  );
  assertIncrementalEqualsFresh(
    caseBodyFinal,
    caseBodyInitial,
    "delete-case-body-separator-continuation",
    { byte: 17, deleteBytes: 2, insert: "" },
  );

  const followingInitial = writeSource(
    "case-item-following-initial",
    lines("case x in x):;; y):;;esac"),
  );
  const followingFinal = writeSource(
    "case-item-following-final",
    lines("case x in x):;;\\", " y):;;esac"),
  );
  assertIncrementalEqualsFresh(
    followingInitial,
    followingFinal,
    "insert-case-item-following-continuation",
    { byte: 15, deleteBytes: 0, insert: "\\\n" },
  );
  assertIncrementalEqualsFresh(
    followingFinal,
    followingInitial,
    "delete-case-item-following-continuation",
    { byte: 15, deleteBytes: 2, insert: "" },
  );

  const emptyItemInitial = writeSource(
    "empty-case-item-initial",
    lines("case x in x):;;esac"),
  );
  const emptyItemFinal = writeSource(
    "empty-case-item-final",
    lines("case x in x);;esac"),
  );
  assertIncrementalEqualsFresh(
    emptyItemInitial,
    emptyItemFinal,
    "delete-empty-case-item-body",
    { byte: 12, deleteBytes: 1, insert: "" },
  );

  const reservedForInitial = writeSource(
    "reserved-for-word-initial",
    lines("for x in ordinary; do :; done"),
  );
  const reservedForFinal = writeSource(
    "reserved-for-word-final",
    lines("for x in fi; do :; done"),
  );
  assertIncrementalEqualsFresh(
    reservedForInitial,
    reservedForFinal,
    "replace-for-word-with-reserved-closer-spelling",
    { byte: 9, deleteBytes: 8, insert: "fi" },
  );

  const blankClosingInitial = writeSource(
    "case-ns-blank-closing-initial",
    lines("case x in<<", "a)", " esac"),
  );
  const blankClosingFinal = writeSource(
    "case-ns-blank-closing-final",
    lines("case x in", "a)", " esac"),
  );
  const blankClosingOutput = parseValidCst(blankClosingFinal);
  assertCstRange(blankClosingOutput, "1:0-2:1", "item: case_item_ns");
  assertIncrementalEqualsFresh(
    blankClosingInitial,
    blankClosingFinal,
    "delete-operator-before-case-ns-blank-closing",
    { byte: 9, deleteBytes: 2, insert: "" },
  );
});

test("sh: formal right-parenthesis ownership survives boundary continuations", () => {
  const logical = writeSource(
    "formal-right-parenthesis-logical",
    lines("(printf )", "after"),
  );
  const physical = writeSource(
    "formal-right-parenthesis-physical",
    lines("(printf \\", ")", "after"),
  );
  const logicalOutput = parseValidCst(logical);
  const physicalOutput = parseValidCst(physical);
  assertSameLogicalProjection(
    "formal right parenthesis boundary continuation",
    logicalOutput,
    physicalOutput,
  );
  assertCstRange(physicalOutput, "0:0-1:1", "subshell");
  assertCstRange(physicalOutput, "0:8-0:9", "line_continuation");
  assert.equal(continuationManifest(physicalOutput).length, 1);
  assertCstDirectChildRange(
    physicalOutput,
    "0:0-1:1",
    "subshell",
    "1:0-1:1",
    '")"',
  );
  assertCstRange(physicalOutput, "2:0-2:5", "command: complete_command");
  assertNotContains(physicalOutput, "ERROR");
  assertIncrementalEqualsFresh(
    logical,
    physical,
    "insert-formal-right-parenthesis-boundary-continuation",
    { byte: 8, deleteBytes: 0, insert: "\\\n" },
  );

  const repeatedLogical = writeSource(
    "formal-right-parenthesis-repeated-logical",
    lines("(printf  )", "after"),
  );
  const repeatedPhysical = writeSource(
    "formal-right-parenthesis-repeated-physical",
    lines("(printf \\", " \\", ")", "after"),
  );
  const repeatedLogicalOutput = parseValidCst(repeatedLogical);
  const repeatedPhysicalOutput = parseValidCst(repeatedPhysical);
  assertSameLogicalProjection(
    "formal right parenthesis repeated boundary continuations",
    repeatedLogicalOutput,
    repeatedPhysicalOutput,
  );
  assertCstRange(repeatedPhysicalOutput, "0:8-0:9", "line_continuation");
  assertCstRange(repeatedPhysicalOutput, "1:1-1:2", "line_continuation");
  assert.equal(continuationManifest(repeatedPhysicalOutput).length, 2);
  assertCstDirectChildRange(
    repeatedPhysicalOutput,
    "0:0-2:1",
    "subshell",
    "2:0-2:1",
    '")"',
  );
  assertCstRange(
    repeatedPhysicalOutput,
    "3:0-3:5",
    "command: complete_command",
  );
  assertNotContains(repeatedPhysicalOutput, "ERROR");
});

test("sh: command separators and continuation boundaries remain stable", () => {
  const operatorBoundaries = writeSource(
    "operator-boundaries",
    lines(
      "echo word\\",
      "  |next",
      "echo word\\",
      "  ||next",
      "echo word\\",
      "  &&next",
      "case x in pattern\\",
      "  |other) : ;; esac",
    ),
  );
  const operatorOutput = parseValidCst(operatorBoundaries);
  for (const [range, item] of [
    ["0:9-0:10", "line_continuation"],
    ["2:9-2:10", "line_continuation"],
    ["3:2-3:4", "operator: or_if"],
    ["4:9-4:10", "line_continuation"],
    ["5:2-5:4", "operator: and_if"],
    ["6:17-6:18", "line_continuation"],
  ]) {
    assertCstRange(operatorOutput, range, item);
  }
  assertNotContains(operatorOutput, "ERROR");

  const repeatedPipeInitial = writeSource(
    "repeated-pipe-boundary-initial",
    lines("first|next"),
  );
  const repeatedPipeFinal = writeSource(
    "repeated-pipe-boundary-final",
    lines("first\\", "\\", "  |next"),
  );
  const repeatedPipeOutput = parseValidCst(repeatedPipeFinal);
  assertCstRange(repeatedPipeOutput, "0:5-0:6", "line_continuation");
  assertCstRange(repeatedPipeOutput, "1:0-1:1", "line_continuation");
  assertNotContains(repeatedPipeOutput, "ERROR");
  assertIncrementalEqualsFresh(
    repeatedPipeInitial,
    repeatedPipeFinal,
    "insert-repeated-pipe-boundary-continuations",
    { byte: 5, deleteBytes: 0, insert: "\\\n\\\n  " },
  );
  assertIncrementalEqualsFresh(
    repeatedPipeFinal,
    repeatedPipeInitial,
    "delete-repeated-pipe-boundary-continuations",
    { byte: 5, deleteBytes: 6, insert: "" },
  );

  const closedAndOrContinuationInitial = writeSource(
    "closed-and-or-continuation-initial",
    lines("{ a && b; }"),
  );
  const closedAndOrContinuationFinal = writeSource(
    "closed-and-or-continuation-final",
    lines("{ a && \\", "b; }"),
  );
  const closedAndOrLogicalOutput = parseValidCst(
    closedAndOrContinuationInitial,
  );
  const closedAndOrPhysicalOutput = parseValidCst(closedAndOrContinuationFinal);
  assertSameLogicalProjection(
    "closed AND-OR continuation",
    closedAndOrLogicalOutput,
    closedAndOrPhysicalOutput,
  );
  assertCstRange(closedAndOrPhysicalOutput, "0:0-1:4", "brace_group");
  assertCstRange(closedAndOrPhysicalOutput, "0:4-0:6", "operator: and_if");
  assertCstRange(closedAndOrPhysicalOutput, "0:7-0:8", "line_continuation");
  assert.equal(continuationManifest(closedAndOrPhysicalOutput).length, 1);
  for (const recovery of ["ERROR", "MISSING", "_recovery"]) {
    assertNotContains(closedAndOrPhysicalOutput, recovery);
  }
  assertIncrementalEqualsFresh(
    closedAndOrContinuationInitial,
    closedAndOrContinuationFinal,
    "insert-closed-and-or-continuation",
    { byte: 7, deleteBytes: 0, insert: "\\\n" },
  );
  assertIncrementalEqualsFresh(
    closedAndOrContinuationFinal,
    closedAndOrContinuationInitial,
    "delete-closed-and-or-continuation",
    { byte: 7, deleteBytes: 2, insert: "" },
  );

  const compoundLayoutInitial = writeSource(
    "compound-layout-initial",
    lines("if { :; } \\", "then :; fi"),
  );
  const compoundLayoutFinal = writeSource(
    "compound-layout-final",
    lines("if { :; } \\", "\\", "then :; fi"),
  );
  assertIncrementalEqualsFresh(
    compoundLayoutInitial,
    compoundLayoutFinal,
    "insert-compound-layout-continuation",
    { byte: 12, deleteBytes: 0, insert: "\\\n" },
  );
  assertIncrementalEqualsFresh(
    compoundLayoutFinal,
    compoundLayoutInitial,
    "delete-compound-layout-continuation",
    { byte: 12, deleteBytes: 2, insert: "" },
  );

  const compoundSeparatorInitial = writeSource(
    "compound-separator-initial",
    lines("{ :; }"),
  );
  const compoundSeparatorFinal = writeSource(
    "compound-separator-final",
    lines("{ :\\", "; }"),
  );
  assertIncrementalEqualsFresh(
    compoundSeparatorInitial,
    compoundSeparatorFinal,
    "insert-compound-separator-continuation",
    { byte: 3, deleteBytes: 0, insert: "\\\n" },
  );
  assertIncrementalEqualsFresh(
    compoundSeparatorFinal,
    compoundSeparatorInitial,
    "delete-compound-separator-continuation",
    { byte: 3, deleteBytes: 2, insert: "" },
  );

  const forWordlistInitial = writeSource(
    "for-wordlist-initial",
    lines("for i in \\", "word; do :; done"),
  );
  const forWordlistFinal = writeSource(
    "for-wordlist-final",
    lines("for i in \\", "\\", "word; do :; done"),
  );
  assertIncrementalEqualsFresh(
    forWordlistInitial,
    forWordlistFinal,
    "insert-second-for-wordlist-continuation",
    { byte: 11, deleteBytes: 0, insert: "\\\n" },
  );
  assertIncrementalEqualsFresh(
    forWordlistFinal,
    forWordlistInitial,
    "delete-second-for-wordlist-continuation",
    { byte: 11, deleteBytes: 2, insert: "" },
  );

  const forFormalInitial = writeSource(
    "for-formal-initial",
    lines("for i in a; do :; done"),
  );
  const forFormalFinal = writeSource(
    "for-formal-final",
    lines("for i \\", "in a; do :; done"),
  );
  const forFormalOutput = parseValidCst(forFormalFinal);
  assertCstRange(forFormalOutput, "0:4-0:5", "name: name");
  assertCstRange(forFormalOutput, "0:6-0:7", "line_continuation");
  assertCstRange(forFormalOutput, "1:0-1:2", "in: in");
  assertNotContains(forFormalOutput, "ERROR");
  assertNotContains(forFormalOutput, "MISSING");
  assertIncrementalEqualsFresh(
    forFormalInitial,
    forFormalFinal,
    "insert-for-formal-continuation",
    { byte: 6, deleteBytes: 0, insert: "\\\n" },
  );
  assertIncrementalEqualsFresh(
    forFormalFinal,
    forFormalInitial,
    "delete-for-formal-continuation",
    { byte: 6, deleteBytes: 2, insert: "" },
  );

  const caseSubjectInitial = writeSource(
    "case-subject-initial",
    lines("case x \\", "in x) :;; esac"),
  );
  const caseSubjectFinal = writeSource(
    "case-subject-final",
    lines("case x \\", "\\", "in x) :;; esac"),
  );
  const caseSubjectOutput = parseValidCst(caseSubjectFinal);
  assertCstRange(caseSubjectOutput, "0:5-0:6", "word: word");
  assertCstRange(caseSubjectOutput, "0:7-0:8", "line_continuation");
  assertCstRange(caseSubjectOutput, "1:0-1:1", "line_continuation");
  assertCstRange(caseSubjectOutput, "2:0-2:2", "in: in");
  assertNotContains(caseSubjectOutput, "ERROR");
  assertNotContains(caseSubjectOutput, "MISSING");
  assertIncrementalEqualsFresh(
    caseSubjectInitial,
    caseSubjectFinal,
    "insert-second-case-subject-continuation",
    { byte: 9, deleteBytes: 0, insert: "\\\n" },
  );
  assertIncrementalEqualsFresh(
    caseSubjectFinal,
    caseSubjectInitial,
    "delete-second-case-subject-continuation",
    { byte: 9, deleteBytes: 2, insert: "" },
  );

  const boundaryPairs = [
    {
      edit: { byte: 4, deleteBytes: 0, insert: "\\\n" },
      final: lines("case\\", " x in esac"),
      initial: lines("case x in esac"),
      name: "case-keyword-continuation",
      removal: { byte: 4, deleteBytes: 2, insert: "" },
    },
    {
      edit: { byte: 9, deleteBytes: 0, insert: "\\\n  " },
      final: lines("echo word\\", "  ||next"),
      initial: lines("echo word||next"),
      name: "or-boundary-continuation",
      removal: { byte: 9, deleteBytes: 4, insert: "" },
    },
    {
      edit: { byte: 9, deleteBytes: 0, insert: "\\\n  " },
      final: lines("echo word\\", "  &&next"),
      initial: lines("echo word&&next"),
      name: "and-boundary-continuation",
      removal: { byte: 9, deleteBytes: 4, insert: "" },
    },
    {
      edit: { byte: 17, deleteBytes: 0, insert: "\\\n  " },
      final: lines("case x in pattern\\", "  |other) : ;; esac"),
      initial: lines("case x in pattern|other) : ;; esac"),
      name: "case-pattern-boundary-continuation",
      removal: { byte: 17, deleteBytes: 4, insert: "" },
    },
  ];
  for (const contract of boundaryPairs) {
    const initial = writeSource(`${contract.name}-initial`, contract.initial);
    const final = writeSource(`${contract.name}-final`, contract.final);
    assertIncrementalEqualsFresh(
      initial,
      final,
      `insert-${contract.name}`,
      contract.edit,
    );
    assertIncrementalEqualsFresh(
      final,
      initial,
      `delete-${contract.name}`,
      contract.removal,
    );
  }

  const backquoteEndInitial = writeSource(
    "backquote-end-initial",
    lines("echo `printf x`"),
  );
  const backquoteEndFinal = writeSource(
    "backquote-end-final",
    lines("echo `printf x\\", "`"),
  );
  const backquoteEndOutput = parseValidCst(backquoteEndFinal);
  assertCstRange(backquoteEndOutput, "0:5-1:1", "backquote_substitution");
  assertCstRange(backquoteEndOutput, "0:14-0:15", "line_continuation");
  assertCstRange(backquoteEndOutput, "1:0-1:1", '"\\`"');
  assertNotContains(backquoteEndOutput, "ERROR");
  assertIncrementalEqualsFresh(
    backquoteEndInitial,
    backquoteEndFinal,
    "insert-backquote-end-continuation",
    { byte: 14, deleteBytes: 0, insert: "\\\n" },
  );
  assertIncrementalEqualsFresh(
    backquoteEndFinal,
    backquoteEndInitial,
    "delete-backquote-end-continuation",
    { byte: 14, deleteBytes: 2, insert: "" },
  );

  const substitutionEndInitial = writeSource(
    "command-substitution-end-initial",
    lines("echo $(printf x)"),
  );
  const substitutionEndFinal = writeSource(
    "command-substitution-end-final",
    lines("echo $(printf x\\", ")"),
  );
  const substitutionEndOutput = parseValidCst(substitutionEndFinal);
  assertCstRange(substitutionEndOutput, "0:5-1:1", "command_substitution");
  assertCstRange(substitutionEndOutput, "0:15-0:16", "line_continuation");
  assertCstRange(substitutionEndOutput, "1:0-1:1", '")"');
  assertNotContains(substitutionEndOutput, "ERROR");
  assertIncrementalEqualsFresh(
    substitutionEndInitial,
    substitutionEndFinal,
    "insert-command-substitution-end-continuation",
    { byte: 15, deleteBytes: 0, insert: "\\\n" },
  );
  assertIncrementalEqualsFresh(
    substitutionEndFinal,
    substitutionEndInitial,
    "delete-command-substitution-end-continuation",
    { byte: 15, deleteBytes: 2, insert: "" },
  );

  const functionLayoutInitial = writeSource(
    "function-layout-initial",
    lines(
      "f() {",
      " before",
      " while :; do :; done",
      " value=1",
      " after",
      "}",
    ),
  );
  const functionLayoutFinal = writeSource(
    "function-layout-final",
    lines(
      "f() {",
      " before",
      " while :; do",
      "  :",
      " done",
      " value=1",
      " after",
      "}",
    ),
  );
  for (const output of assertIncrementalEqualsFresh(
    functionLayoutInitial,
    functionLayoutFinal,
    "expand-function-loop-layout",
    { byte: 25, deleteBytes: 5, insert: "o\n  :\n " },
  )) {
    assertCstRange(output, "0:0-7:1", "function_definition");
    assertCstRange(output, "2:1-4:5", "while_clause");
    assertCstRange(output, "5:1-5:8", "assignment_word");
    assertNotContains(output, "ERROR");
  }
});

test("sh: trailing blanks before separating newlines stay layout", () => {
  const blankSeparatorsInitial = writeSource(
    "trailing-blank-separators-initial",
    lines("first one", "second two", "third three"),
  );
  const blankSeparatorsFinal = writeSource(
    "trailing-blank-separators-final",
    lines("first one ", "second two\t", "third three"),
  );
  const blankSeparatorsOutput = parseValidCst(blankSeparatorsFinal);
  assertCstRange(blankSeparatorsOutput, "0:0-0:9", "command: complete_command");
  assertCstRange(blankSeparatorsOutput, "0:9-1:0", "separator: newline_list");
  assertCstRange(
    blankSeparatorsOutput,
    "1:0-1:10",
    "command: complete_command",
  );
  assertCstRange(blankSeparatorsOutput, "1:10-2:0", "separator: newline_list");
  assertCstRange(
    blankSeparatorsOutput,
    "2:0-2:11",
    "command: complete_command",
  );
  for (const recovery of ["ERROR", "MISSING", "_recovery"]) {
    assertNotContains(blankSeparatorsOutput, recovery);
  }
  assertSameLogicalProjection(
    "trailing blanks before separating newlines",
    parseValidCst(blankSeparatorsInitial),
    blankSeparatorsOutput,
  );
  assertIncrementalEqualsFresh(
    blankSeparatorsInitial,
    blankSeparatorsFinal,
    "insert-trailing-blank-separators",
    { byte: 9, deleteBytes: 0, insert: " " },
    { byte: 21, deleteBytes: 0, insert: "\t" },
  );
  assertIncrementalEqualsFresh(
    blankSeparatorsFinal,
    blankSeparatorsInitial,
    "delete-trailing-blank-separators",
    { byte: 9, deleteBytes: 1, insert: "" },
    { byte: 20, deleteBytes: 1, insert: "" },
  );

  const blankTrailingOnly = writeSource(
    "trailing-blank-final-line",
    lines("first one "),
  );
  const blankThenCommand = writeSource(
    "trailing-blank-then-command",
    lines("first one ", "second two"),
  );
  assertIncrementalEqualsFresh(
    blankTrailingOnly,
    blankThenCommand,
    "append-command-after-trailing-blank",
    { byte: 11, deleteBytes: 0, insert: "second two\n" },
  );
  assertIncrementalEqualsFresh(
    blankThenCommand,
    blankTrailingOnly,
    "delete-command-after-trailing-blank",
    { byte: 11, deleteBytes: 11, insert: "" },
  );

  const blankIfBodyInitial = writeSource(
    "trailing-blank-if-body-initial",
    lines("if :; then", "first one", "second two", "fi"),
  );
  const blankIfBodyFinal = writeSource(
    "trailing-blank-if-body-final",
    lines("if :; then", "first one ", "second two ", "fi"),
  );
  const blankIfBodyOutput = parseValidCst(blankIfBodyFinal);
  assertCstRange(blankIfBodyOutput, "1:9-2:0", "separator: separator");
  assertCstRange(blankIfBodyOutput, "2:10-3:0", "terminator: separator");
  assertCstRange(blankIfBodyOutput, "3:0-3:2", "fi_keyword");
  for (const recovery of ["ERROR", "MISSING", "_recovery"]) {
    assertNotContains(blankIfBodyOutput, recovery);
  }
  assertSameLogicalProjection(
    "trailing blanks inside an if body",
    parseValidCst(blankIfBodyInitial),
    blankIfBodyOutput,
  );
  assertIncrementalEqualsFresh(
    blankIfBodyInitial,
    blankIfBodyFinal,
    "insert-trailing-blank-if-body",
    { byte: 20, deleteBytes: 0, insert: " " },
    { byte: 32, deleteBytes: 0, insert: " " },
  );

  const blankRecoveryInitial = writeSource(
    "trailing-blank-recovery-initial",
    lines("first one", "fi"),
  );
  const blankRecoveryFinal = writeSource(
    "trailing-blank-recovery-final",
    lines("first one ", "fi"),
  );
  parseRecoveryAfterEdits(
    blankRecoveryInitial,
    blankRecoveryFinal,
    "insert-trailing-blank-before-stray-closer",
    { byte: 9, deleteBytes: 0, insert: " " },
  );

  const blankHereDocumentInitial = writeSource(
    "trailing-blank-here-document-initial",
    lines("cat <<END", "body", "END", "second"),
  );
  const blankHereDocumentFinal = writeSource(
    "trailing-blank-here-document-final",
    lines("cat <<END ", "body", "END", "second"),
  );
  const blankHereDocumentOutput = parseValidCst(blankHereDocumentFinal);
  assertCstRange(blankHereDocumentOutput, "0:9-3:0", "here_document_sequence");
  assertNotContains(blankHereDocumentOutput, "ERROR");
  assertIncrementalEqualsFresh(
    blankHereDocumentInitial,
    blankHereDocumentFinal,
    "insert-trailing-blank-before-here-document",
    { byte: 9, deleteBytes: 0, insert: " " },
  );
  assertIncrementalEqualsFresh(
    blankHereDocumentFinal,
    blankHereDocumentInitial,
    "delete-trailing-blank-before-here-document",
    { byte: 9, deleteBytes: 1, insert: "" },
  );
});

test("sh: closing layout owns trailing blanks before a continued closer", () => {
  const subshellInitial = writeSource(
    "closing-subshell-initial",
    lines("(echo a \\", ")"),
  );
  const subshellFinal = writeSource(
    "closing-subshell-final",
    lines("(echo a \\", "b )"),
  );
  assertIncrementalEqualsFresh(
    subshellInitial,
    subshellFinal,
    "insert-word-before-continued-subshell-closer",
    { byte: 10, deleteBytes: 0, insert: "b " },
  );

  const tabInitial = writeSource(
    "closing-subshell-tab-initial",
    lines("(echo a\t\\", ")"),
  );
  const tabFinal = writeSource(
    "closing-subshell-tab-final",
    lines("(echo a\t\\", "b )"),
  );
  assertIncrementalEqualsFresh(
    tabInitial,
    tabFinal,
    "insert-word-before-tab-continued-subshell-closer",
    { byte: 10, deleteBytes: 0, insert: "b " },
  );

  const nestedInitial = writeSource(
    "closing-subshell-nested-initial",
    lines("((echo a \\", "))"),
  );
  const nestedFinal = writeSource(
    "closing-subshell-nested-final",
    lines("((echo a \\", "b ))"),
  );
  assertIncrementalEqualsFresh(
    nestedInitial,
    nestedFinal,
    "insert-word-before-nested-subshell-closer",
    { byte: 11, deleteBytes: 0, insert: "b " },
  );
});

test("sh: terms own trailing layout that runs to the end of input", () => {
  const continuationInitial = writeSource(
    "trailing-layout-continuation-initial",
    "\n\\\n\\\n\necho\n\\\n\\\n\n",
  );
  const continuationFinal = writeSource(
    "trailing-layout-continuation-final",
    "\n\n\n\\\n\\\n\necho\n\\\n\\\n\\\\\n\n\n",
  );
  assertIncrementalEqualsFresh(
    continuationInitial,
    continuationFinal,
    "trailing-layout-continuation",
    { byte: 1, deleteBytes: 0, insert: "\n\n" },
    { byte: 17, deleteBytes: 0, insert: "\\\n" },
    { byte: 18, deleteBytes: 0, insert: "\\\n" },
  );

  const blankLineInitial = writeSource(
    "trailing-layout-blank-line-initial",
    "\n\\\n\\\n\necho\n\\\n\\\n\n",
  );
  const blankLineFinal = writeSource(
    "trailing-layout-blank-line-final",
    "\n\\\n\\\n\n\\\n\necho\n\\&\n\\\n\n",
  );
  assertIncrementalEqualsFresh(
    blankLineInitial,
    blankLineFinal,
    "trailing-layout-blank-line",
    { byte: 6, deleteBytes: 0, insert: "\\\n\n" },
    { byte: 19, deleteBytes: 0, insert: "" },
    { byte: 15, deleteBytes: 0, insert: "&" },
  );
});

test("sh: closed commands own blank-led trailing continuation layout", () => {
  const groupInitial = writeSource(
    "closed-group-trailing-initial",
    lines("{ a; }"),
  );
  const groupFinal = writeSource(
    "closed-group-trailing-final",
    "{ a; } \\\n\n",
  );
  assertIncrementalEqualsFresh(
    groupInitial,
    groupFinal,
    "closed-group-trailing",
    { byte: 6, deleteBytes: 0, insert: " \\\n" },
  );

  const assignmentInitial = writeSource(
    "assignment-trailing-initial",
    lines("a=b"),
  );
  const assignmentFinal = writeSource(
    "assignment-trailing-final",
    "a=b \\\n\n",
  );
  assertIncrementalEqualsFresh(
    assignmentInitial,
    assignmentFinal,
    "assignment-trailing",
    { byte: 3, deleteBytes: 0, insert: " \\\n" },
  );
});

test("sh: a continuation before a blank line keeps a compound-list closer reachable", () => {
  for (const [name, source] of [
    ["if", "if true; then : \\\n\nfi\n"],
    ["while", "while true; do : \\\n\ndone\n"],
    ["for", "for x in a; do : \\\n\ndone\n"],
    ["case-item", "case x in a) : \\\n\n;; esac\n"],
    ["case-item-ns", "case x in a) : \\\n\nesac\n"],
    ["brace", "{ : \\\n\n}\n"],
    ["subshell", "( : \\\n\n)\n"],
    ["function", "f() { : \\\n\n}\n"],
    ["condition", "if : \\\n\nthen :; fi\n"],
    ["for-do", "for x in a \\\n\ndo :; done\n"],
  ]) {
    assertValid(writeSource(`compound-closer-${name}`, source), name);
  }

  assertSameLogicalProjection(
    "spaced and glued continuation closers share a public node structure",
    parseValidCst(
      writeSource("compound-closer-spaced", "if true; then : \\\n\nfi\n"),
    ),
    parseValidCst(
      writeSource("compound-closer-glued", "if true; then :\\\n\nfi\n"),
    ),
  );

  const initial = writeSource(
    "compound-closer-initial",
    lines("if true; then", ":", "fi"),
  );
  const final = writeSource(
    "compound-closer-final",
    "if true; then\n: \\\n\nfi\n",
  );
  assertIncrementalEqualsFresh(initial, final, "compound-closer", {
    byte: 15,
    deleteBytes: 0,
    insert: " \\\n",
  });
});

test("sh: an elif consequence owns the continued layout before its else", () => {
  const withElse = writeSource(
    "elif-continued-else",
    "if a; then b; elif c; then d; \\\n else e; fi\n",
  );
  const withCommand = writeSource(
    "elif-continued-command",
    "if a; then b; elif c; then d; \\\n elsXe e; fi\n",
  );
  assertIncrementalEqualsFresh(withElse, withCommand, "elif-continued-else", {
    byte: 36,
    deleteBytes: 0,
    insert: "X",
  });
  assertIncrementalEqualsFresh(
    withCommand,
    withElse,
    "elif-continued-else-restored",
    { byte: 36, deleteBytes: 1, insert: "" },
  );
});

test("sh: word, pipeline, parameter, and arithmetic categories survive edits", () => {
  for (const fixture of [
    {
      name: "separate-pipeline-negation-from-word",
      initial: "!foo\n",
      final: "! foo\n",
      edit: { byte: 1, deleteBytes: 0, insert: " " },
      reverse: { byte: 1, deleteBytes: 1, insert: "" },
      initialBangCount: 0,
    },
    {
      name: "move-pipeline-negation-out-of-command-suffix",
      initial: "echo ! foo\n",
      final: "! foo\n",
      edit: { byte: 0, deleteBytes: 5, insert: "" },
      reverse: { byte: 0, deleteBytes: 0, insert: "echo " },
      initialBangCount: 0,
    },
    {
      name: "continue-layout-after-pipeline-negation",
      initial: "! :\n",
      final: "!\\\n :\n",
      edit: { byte: 1, deleteBytes: 0, insert: "\\\n" },
      reverse: { byte: 1, deleteBytes: 2, insert: "" },
      initialBangCount: 1,
    },
  ]) {
    const initial = writeSource(`${fixture.name}-initial`, fixture.initial);
    const final = writeSource(`${fixture.name}-final`, fixture.final);
    for (const output of assertIncrementalEqualsFresh(
      initial,
      final,
      fixture.name,
      fixture.edit,
    )) {
      assertCstRange(output, "0:0-0:1", "negation: bang");
    }
    for (const output of assertIncrementalEqualsFresh(
      final,
      initial,
      `reverse-${fixture.name}`,
      fixture.reverse,
    )) {
      assertOccurrenceCount(output, "negation: bang", fixture.initialBangCount);
    }
  }

  const descriptorInitial = writeSource(
    "descriptor-initial",
    lines("<input 2x>output"),
  );
  const descriptorFinal = writeSource(
    "descriptor-final",
    lines("<input 2>output"),
  );
  assertIncrementalEqualsFresh(
    descriptorInitial,
    descriptorFinal,
    "descriptor-after-word-edit",
    { byte: 8, deleteBytes: 1, insert: "" },
  );

  const parameterTextInitial = writeSource(
    "parameter-text-initial",
    lines("printf x$%"),
  );
  const parameterTextFinal = writeSource(
    "parameter-text-final",
    lines("printf x$x"),
  );
  assertIncrementalEqualsFresh(
    parameterTextInitial,
    parameterTextFinal,
    "parameter-text-to-expansion",
    { byte: 9, deleteBytes: 1, insert: "x" },
  );
  assertIncrementalEqualsFresh(
    parameterTextFinal,
    parameterTextInitial,
    "parameter-expansion-to-text",
    { byte: 9, deleteBytes: 1, insert: "%" },
  );

  const parameterUnquoted = writeSource(
    "parameter-context-unquoted",
    lines("printf $" + "{x:-*.js}"),
  );
  const parameterQuoted = writeSource(
    "parameter-context-quoted",
    lines('printf "$' + '{x:-*.js}"'),
  );
  const parameterPattern = writeSource(
    "parameter-context-pattern",
    lines('printf "$' + '{x##*.js}"'),
  );
  assertIncrementalEqualsFresh(
    parameterUnquoted,
    parameterQuoted,
    "insert-parameter-outer-quotes",
    { byte: 7, deleteBytes: 0, insert: '"' },
    { byte: 18, deleteBytes: 0, insert: '"' },
  );
  assertIncrementalEqualsFresh(
    parameterQuoted,
    parameterUnquoted,
    "delete-parameter-outer-quotes",
    { byte: 7, deleteBytes: 1, insert: "" },
    { byte: 17, deleteBytes: 1, insert: "" },
  );
  assertIncrementalEqualsFresh(
    parameterQuoted,
    parameterPattern,
    "parameter-value-to-pattern-context",
    { byte: 11, deleteBytes: 2, insert: "##" },
  );
  assertIncrementalEqualsFresh(
    parameterPattern,
    parameterQuoted,
    "parameter-pattern-to-value-context",
    { byte: 11, deleteBytes: 2, insert: ":-" },
  );

  const numericInitial = writeSource(
    "numeric-category-initial",
    lines('printf "$' + '{00}"'),
  );
  const numericFinal = writeSource(
    "numeric-category-final",
    lines('printf "$' + '{01}"'),
  );
  assertIncrementalEqualsFresh(
    numericInitial,
    numericFinal,
    "numeric-source-to-positional-parameter",
    { byte: 11, deleteBytes: 1, insert: "1" },
  );
  assertIncrementalEqualsFresh(
    numericFinal,
    numericInitial,
    "positional-parameter-to-numeric-source",
    { byte: 11, deleteBytes: 1, insert: "0" },
  );

  const arithmeticCategoryInitial = writeSource(
    "arithmetic-category-initial",
    lines(': "$((a | b && c))"'),
  );
  const arithmeticCategoryFinal = writeSource(
    "arithmetic-category-final",
    lines(': "$((a || b && c))"'),
  );
  assertIncrementalEqualsFresh(
    arithmeticCategoryInitial,
    arithmeticCategoryFinal,
    "insert-arithmetic-operator-category-character",
    { byte: 9, deleteBytes: 0, insert: "|" },
  );
  assertIncrementalEqualsFresh(
    arithmeticCategoryFinal,
    arithmeticCategoryInitial,
    "delete-arithmetic-operator-category-character",
    { byte: 9, deleteBytes: 1, insert: "" },
  );

  const missingOperand = writeSource(
    "arithmetic-missing-operand",
    lines(': "$((1 +\\', '))"'),
  );
  const missingOperandOutput = runParse({
    description: "arithmetic missing operand after continuation",
    source: missingOperand,
  }).output;
  assertContains(missingOperandOutput, "command_substitution");
  assertContains(missingOperandOutput, "subshell");
  assertNotContains(missingOperandOutput, "arithmetic_expansion");
  assertCstRange(missingOperandOutput, "0:9-0:10", "line_continuation");
  assert.equal(continuationManifest(missingOperandOutput).length, 1);
});

test("sh: tilde, assignment, and compound-tail classifications remain stable", () => {
  const tildePercentInitial = writeSource(
    "tilde-percent-initial",
    lines(": ~alice/x"),
  );
  const tildePercentFinal = writeSource(
    "tilde-percent-final",
    lines(": ~alice%/x"),
  );
  const [, tildePercentOutput] = assertIncrementalEqualsFresh(
    tildePercentInitial,
    tildePercentFinal,
    "insert-percent-in-literal-tilde-user",
    { byte: 8, deleteBytes: 0, insert: "%" },
  );
  assertIncrementalEqualsFresh(
    tildePercentFinal,
    tildePercentInitial,
    "delete-percent-from-literal-tilde-user",
    { byte: 8, deleteBytes: 1, insert: "" },
  );
  for (const [range, item] of [
    ["0:2-0:9", "tilde_expansion"],
    ["0:3-0:9", "user: tilde_user"],
    ["0:3-0:9", "literal"],
    ["0:9-0:11", "literal `/x`"],
  ]) {
    assertCstRange(tildePercentOutput, range, item);
  }

  const assignmentPercentInitial = writeSource(
    "tilde-assignment-percent-initial",
    lines("A=~alice:x :"),
  );
  const assignmentPercentFinal = writeSource(
    "tilde-assignment-percent-final",
    lines("A=~alice%:x :"),
  );
  const [, assignmentPercentOutput] = assertIncrementalEqualsFresh(
    assignmentPercentInitial,
    assignmentPercentFinal,
    "insert-percent-before-assignment-tilde-colon",
    { byte: 8, deleteBytes: 0, insert: "%" },
  );
  assertIncrementalEqualsFresh(
    assignmentPercentFinal,
    assignmentPercentInitial,
    "delete-percent-before-assignment-tilde-colon",
    { byte: 8, deleteBytes: 1, insert: "" },
  );
  for (const [range, item] of [
    ["0:2-0:11", "value: assignment_value"],
    ["0:2-0:9", "tilde_expansion"],
    ["0:3-0:9", "user: tilde_user"],
    ["0:9-0:11", "literal `:x`"],
  ]) {
    assertCstRange(assignmentPercentOutput, range, item);
  }

  const parameterPercentInitial = writeSource(
    "tilde-parameter-percent-initial",
    lines(": $" + "{v:-~alice/x}"),
  );
  const parameterPercentFinal = writeSource(
    "tilde-parameter-percent-final",
    lines(": $" + "{v:-~alice%/x}"),
  );
  const [, parameterPercentOutput] = assertIncrementalEqualsFresh(
    parameterPercentInitial,
    parameterPercentFinal,
    "insert-percent-in-parameter-word-tilde-user",
    { byte: 13, deleteBytes: 0, insert: "%" },
  );
  assertIncrementalEqualsFresh(
    parameterPercentFinal,
    parameterPercentInitial,
    "delete-percent-from-parameter-word-tilde-user",
    { byte: 13, deleteBytes: 1, insert: "" },
  );
  for (const [range, item] of [
    ["0:2-0:17", "parameter_expansion"],
    ["0:7-0:16", "word: parameter_word"],
    ["0:7-0:14", "tilde_expansion"],
    ["0:8-0:14", "user: tilde_user"],
    ["0:14-0:16", "literal `/x`"],
    ["0:16-0:17", '"}"'],
  ]) {
    assertCstRange(parameterPercentOutput, range, item);
  }

  const nestedUserInitial = writeSource(
    "tilde-nested-user-initial",
    lines(': ~"$(echo ab)"/x'),
  );
  const nestedUserFinal = writeSource(
    "tilde-nested-user-final",
    lines(': ~"$(echo a/b)"/x'),
  );
  const [, nestedUserOutput] = assertIncrementalEqualsFresh(
    nestedUserInitial,
    nestedUserFinal,
    "insert-slash-in-nested-tilde-user-substitution",
    { byte: 12, deleteBytes: 0, insert: "/" },
  );
  assertIncrementalEqualsFresh(
    nestedUserFinal,
    nestedUserInitial,
    "delete-slash-from-nested-tilde-user-substitution",
    { byte: 12, deleteBytes: 1, insert: "" },
  );
  for (const [range, item] of [
    ["0:2-0:3", "literal `~`"],
    ["0:3-0:16", "double_quoted"],
    ["0:4-0:15", "command_substitution"],
    ["0:11-0:14", "literal `a/b`"],
    ["0:16-0:18", "literal `/x`"],
  ]) {
    assertCstRange(nestedUserOutput, range, item);
  }

  for (const [initial, final, delimiter] of [
    [": ~[ab]", ": ~[a/b]", "/"],
    ["x=~[ab]", "x=~[a:b]", ":"],
  ]) {
    const initialSource = writeSource("tilde-bracket-initial", lines(initial));
    const finalSource = writeSource("tilde-bracket-final", lines(final));
    assertIncrementalEqualsFresh(
      initialSource,
      finalSource,
      "insert-tilde-bracket-delimiter",
      { byte: 5, deleteBytes: 0, insert: delimiter },
    );
    assertIncrementalEqualsFresh(
      finalSource,
      initialSource,
      "remove-tilde-bracket-delimiter",
      { byte: 5, deleteBytes: 1, insert: "" },
    );
  }

  const assignmentBoundaryInitial = writeSource(
    "assignment-boundary-initial",
    lines("name=value command"),
  );
  const assignmentBoundaryFinal = writeSource(
    "assignment-boundary-final",
    lines("name=value\\", " command"),
  );
  const assignmentBoundaryOutput = parseValidCst(assignmentBoundaryFinal);
  assertOccurrenceCount(
    assignmentBoundaryOutput,
    "assignment: assignment_word",
    1,
  );
  assertCstRange(
    assignmentBoundaryOutput,
    "0:5-0:10",
    "value: assignment_value",
  );
  assertCstRange(assignmentBoundaryOutput, "0:10-0:11", "line_continuation");
  assertNotContains(assignmentBoundaryOutput, "ERROR");
  assertIncrementalEqualsFresh(
    assignmentBoundaryInitial,
    assignmentBoundaryFinal,
    "insert-assignment-boundary-continuation",
    { byte: 10, deleteBytes: 0, insert: "\\\n" },
  );
  assertIncrementalEqualsFresh(
    assignmentBoundaryFinal,
    assignmentBoundaryInitial,
    "delete-assignment-boundary-continuation",
    { byte: 10, deleteBytes: 2, insert: "" },
  );

  const assignmentNewlineInitial = writeSource(
    "assignment-newline-initial",
    lines("x=a"),
  );
  const assignmentNewlineFinal = writeSource(
    "assignment-newline-final",
    lines("x=a\\", ""),
  );
  const assignmentNewlineOutput = parseValidCst(assignmentNewlineFinal);
  assertOccurrenceCount(
    assignmentNewlineOutput,
    "assignment: assignment_word",
    1,
  );
  assertCstRange(assignmentNewlineOutput, "0:2-0:3", "value: assignment_value");
  assertCstRange(assignmentNewlineOutput, "0:3-0:4", "line_continuation");
  assertCstRange(assignmentNewlineOutput, "1:0-2:0", "trailing: linebreak");
  assertNotContains(assignmentNewlineOutput, "name: cmd_name");
  assertNotContains(assignmentNewlineOutput, "ERROR");
  assertNotContains(assignmentNewlineOutput, "MISSING");
  assertIncrementalEqualsFresh(
    assignmentNewlineInitial,
    assignmentNewlineFinal,
    "insert-assignment-newline-continuation",
    { byte: 3, deleteBytes: 0, insert: "\\\n" },
  );
  assertIncrementalEqualsFresh(
    assignmentNewlineFinal,
    assignmentNewlineInitial,
    "delete-assignment-newline-continuation",
    { byte: 3, deleteBytes: 2, insert: "" },
  );

  const compoundTailInitial = writeSource(
    "compound-tail-initial",
    lines("(:)&", "child_pid=$!"),
  );
  const compoundTailFinal = writeSource(
    "compound-tail-final",
    lines("(:) &", "child_pid=$!"),
  );
  for (const output of assertIncrementalEqualsFresh(
    compoundTailInitial,
    compoundTailFinal,
    "insert-layout-before-asynchronous-separator",
    { byte: 3, deleteBytes: 0, insert: " " },
  )) {
    assertCstRange(output, "0:0-0:3", "subshell");
    assertCstRange(output, "0:4-0:5", "separator_op");
    assertCstRange(output, "1:0-1:12", "command: complete_command");
    assertNotContains(output, "ERROR");
  }
});

test("sh: substitution, redirection, and token boundaries retain ownership", () => {
  const assignmentInitial = writeSource(
    "backquote-assignment-initial",
    lines("worker() {", "  :", "}", 'result="`printf nested`"'),
  );
  const assignmentFinal = writeSource(
    "backquote-assignment-final",
    lines("worker() {", "  :", "}", "result=`printf nested`"),
  );
  assertIncrementalEqualsFresh(
    assignmentInitial,
    assignmentFinal,
    "remove-double-quotes-around-assignment-backquote",
    { byte: 24, deleteBytes: 1, insert: "" },
    { byte: 39, deleteBytes: 1, insert: "" },
  );

  const substitutionLayoutInitial = writeSource(
    "command-substitution-layout-initial",
    lines("echo $(first;)"),
  );
  const substitutionLayoutFinal = writeSource(
    "command-substitution-layout-final",
    lines("echo $(first; )"),
  );
  assertIncrementalEqualsFresh(
    substitutionLayoutInitial,
    substitutionLayoutFinal,
    "insert-layout-before-command-substitution-closer",
    { byte: 13, deleteBytes: 0, insert: " " },
  );

  const backquoteLayoutInitial = writeSource(
    "backquote-layout-initial",
    lines("echo `first;`"),
  );
  const backquoteLayoutFinal = writeSource(
    "backquote-layout-final",
    lines("echo `first; `"),
  );
  assertIncrementalEqualsFresh(
    backquoteLayoutInitial,
    backquoteLayoutFinal,
    "insert-layout-before-backquote-closer",
    { byte: 12, deleteBytes: 0, insert: " " },
  );

  const redirectedCompoundWithout = writeSource(
    "redirected-compound-without-separator",
    lines(': "$({ :; }>g)"'),
  );
  const redirectedCompoundWith = writeSource(
    "redirected-compound-with-separator",
    lines(': "$({ :; }>g;)"'),
  );
  assertIncrementalEqualsFresh(
    redirectedCompoundWithout,
    redirectedCompoundWith,
    "insert-separator-after-redirected-compound-command",
    { byte: 13, deleteBytes: 0, insert: ";" },
  );
  const [, redirectedCompoundOutput] = assertIncrementalEqualsFresh(
    redirectedCompoundWith,
    redirectedCompoundWithout,
    "delete-separator-after-redirected-compound-command",
    { byte: 13, deleteBytes: 1, insert: "" },
  );
  assertCstRange(redirectedCompoundOutput, "0:3-0:14", "command_substitution");
  assertCstRange(
    redirectedCompoundOutput,
    "0:5-0:13",
    "command: complete_command",
  );
  assertCstRange(
    redirectedCompoundOutput,
    "0:5-0:11",
    "body: compound_command",
  );
  assertCstRange(
    redirectedCompoundOutput,
    "0:11-0:13",
    "redirects: redirect_list",
  );
  assertCstDirectChildRange(
    redirectedCompoundOutput,
    "0:3-0:14",
    "command_substitution",
    "0:13-0:14",
    '")"',
  );
  assertNotContains(redirectedCompoundOutput, "recovery");

  const redirectedFunctionWithout = writeSource(
    "redirected-function-without-separator",
    lines(': "$(f(){ :; }>g)"'),
  );
  const redirectedFunctionWith = writeSource(
    "redirected-function-with-separator",
    lines(': "$(f(){ :; }>g;)"'),
  );
  assertIncrementalEqualsFresh(
    redirectedFunctionWithout,
    redirectedFunctionWith,
    "insert-separator-after-redirected-function",
    { byte: 16, deleteBytes: 0, insert: ";" },
  );
  const [, redirectedFunctionOutput] = assertIncrementalEqualsFresh(
    redirectedFunctionWith,
    redirectedFunctionWithout,
    "delete-separator-after-redirected-function",
    { byte: 16, deleteBytes: 1, insert: "" },
  );
  for (const [range, item] of [
    ["0:3-0:17", "command_substitution"],
    ["0:5-0:16", "body: function_definition"],
    ["0:8-0:16", "body: function_body"],
    ["0:8-0:14", "body: compound_command"],
    ["0:14-0:16", "redirects: redirect_list"],
  ]) {
    assertCstRange(redirectedFunctionOutput, range, item);
  }
  assertCstDirectChildRange(
    redirectedFunctionOutput,
    "0:3-0:17",
    "command_substitution",
    "0:16-0:17",
    '")"',
  );
  assertNotContains(redirectedFunctionOutput, "recovery");

  const redirectTargetInitial = writeSource(
    "redirect-target-word-initial",
    lines("<2>x"),
  );
  const redirectTargetFinal = writeSource(
    "redirect-target-word-final",
    lines('<""2>x'),
  );
  assertIncrementalEqualsFresh(
    redirectTargetInitial,
    redirectTargetFinal,
    "insert-empty-quote-before-redirect-target-digit",
    { byte: 1, deleteBytes: 0, insert: '""' },
  );

  const braceWordContinuationInitial = writeSource(
    "brace-word-continuation-initial",
    lines("{a}>x"),
  );
  const braceWordContinuationFinal = writeSource(
    "brace-word-continuation-final",
    lines("{a}\\", ">x"),
  );
  const braceWordContinuationOutput = parseValidCst(braceWordContinuationFinal);
  assertCstRange(braceWordContinuationOutput, "0:0-0:3", "word");
  assertCstRange(braceWordContinuationOutput, "0:3-0:4", "line_continuation");
  assertCstDirectChildRange(
    braceWordContinuationOutput,
    "1:0-1:2",
    "body: io_file",
    "1:0-1:1",
    '">"',
  );
  assertNotContains(braceWordContinuationOutput, "io_location");
  assertIncrementalEqualsFresh(
    braceWordContinuationInitial,
    braceWordContinuationFinal,
    "insert-continuation-after-brace-word",
    { byte: 3, deleteBytes: 0, insert: "\\\n" },
  );

  const functionCloserInitial = writeSource(
    "function-outer-closer-initial",
    lines("{ f()(:); }"),
  );
  const functionCloserFinal = writeSource(
    "function-outer-closer-final",
    lines("{ f()(:) }"),
  );
  assertIncrementalEqualsFresh(
    functionCloserInitial,
    functionCloserFinal,
    "delete-separator-before-function-outer-closer",
    { byte: 8, deleteBytes: 1, insert: "" },
  );

  const outerSubshellInitial = writeSource(
    "recovery-enclosing-subshell-initial",
    lines("(if x; then y; fi); next"),
  );
  const outerSubshellFinal = writeSource(
    "recovery-enclosing-subshell-final",
    lines("(if x; then y); next"),
  );
  parseRecoveryAfterEdits(
    outerSubshellInitial,
    outerSubshellFinal,
    "delete-inner-fi-before-subshell-closer",
    { byte: 13, deleteBytes: 4, insert: "" },
  );

  const outerBackquoteInitial = writeSource(
    "recovery-enclosing-backquote-initial",
    lines("echo `if x; then y; fi`; next"),
  );
  const outerBackquoteFinal = writeSource(
    "recovery-enclosing-backquote-final",
    lines("echo `if x; then y`; next"),
  );
  parseRecoveryAfterEdits(
    outerBackquoteInitial,
    outerBackquoteFinal,
    "delete-inner-fi-before-backquote-closer",
    { byte: 18, deleteBytes: 4, insert: "" },
  );

  const interiorBlankInitial = writeSource(
    "substitution-interior-blank-initial",
    lines("x=$(", "echo hi)"),
  );
  const interiorBlankFinal = writeSource(
    "substitution-interior-blank-final",
    lines("x=$(", " echo hi)"),
  );
  assertIncrementalEqualsFresh(
    interiorBlankInitial,
    interiorBlankFinal,
    "substitution-interior-blank-insert",
    { byte: 5, deleteBytes: 0, insert: " " },
  );
  assertIncrementalEqualsFresh(
    interiorBlankFinal,
    interiorBlankInitial,
    "substitution-interior-blank-delete",
    { byte: 5, deleteBytes: 1, insert: "" },
  );

  const parameterDelimiterInitial = writeSource(
    "parameter-delimiter-initial",
    lines("printf x%"),
  );
  const parameterDelimiterFinal = writeSource(
    "parameter-delimiter-final",
    lines("printf x$x"),
  );
  assertIncrementalEqualsFresh(
    parameterDelimiterInitial,
    parameterDelimiterFinal,
    "parameter-delimiter",
    { byte: 8, deleteBytes: 1, insert: "$x" },
  );

  const literalDollar = writeSource("literal-dollar", lines("printf $"));
  const bracedExpansion = writeSource(
    "braced-expansion",
    lines("printf $" + "{x}"),
  );
  const commandSubstitution = writeSource(
    "command-substitution",
    lines("printf $(x)"),
  );
  const quotedLiteralDollar = writeSource(
    "quoted-literal-dollar",
    lines('printf "$"'),
  );
  const quotedCommandSubstitution = writeSource(
    "quoted-command-substitution",
    lines('printf "$(x)"'),
  );
  assertIncrementalEqualsFresh(
    literalDollar,
    bracedExpansion,
    "insert-braced-expansion-delimiters",
    { byte: 8, deleteBytes: 0, insert: "{x}" },
  );
  assertIncrementalEqualsFresh(
    bracedExpansion,
    literalDollar,
    "delete-braced-expansion-delimiters",
    { byte: 8, deleteBytes: 3, insert: "" },
  );
  assertIncrementalEqualsFresh(
    quotedLiteralDollar,
    quotedCommandSubstitution,
    "insert-command-substitution-delimiters",
    { byte: 9, deleteBytes: 0, insert: "(x)" },
  );
  assertIncrementalEqualsFresh(
    quotedCommandSubstitution,
    quotedLiteralDollar,
    "delete-command-substitution-delimiters",
    { byte: 9, deleteBytes: 3, insert: "" },
  );

  const arithmeticExpansion = writeSource(
    "arithmetic-expansion",
    lines("printf $((x))"),
  );
  assertIncrementalEqualsFresh(
    commandSubstitution,
    arithmeticExpansion,
    "promote-command-substitution-to-arithmetic-expansion",
    { byte: 9, deleteBytes: 0, insert: "(" },
    { byte: 11, deleteBytes: 0, insert: ")" },
  );
  assertIncrementalEqualsFresh(
    arithmeticExpansion,
    commandSubstitution,
    "demote-arithmetic-expansion-to-command-substitution",
    { byte: 9, deleteBytes: 1, insert: "" },
    { byte: 11, deleteBytes: 1, insert: "" },
  );

  const backquoteDelimiterInitial = writeSource(
    "backquote-delimiter-initial",
    lines("echo [inner]"),
  );
  const backquoteDelimiterFinal = writeSource(
    "backquote-delimiter-final",
    lines("echo `inner`"),
  );
  assertIncrementalEqualsFresh(
    backquoteDelimiterInitial,
    backquoteDelimiterFinal,
    "backquote-delimiter",
    { byte: 5, deleteBytes: 1, insert: "`" },
    { byte: 11, deleteBytes: 1, insert: "`" },
  );
});

test("sh: substitution newlines and continuations remain stable", () => {
  const hereSubstitutionInitial = writeSource(
    "here-substitution-initial",
    lines("cmd <<E $(x y)", "body", "E"),
  );
  const hereSubstitutionFinal = writeSource(
    "here-substitution-final",
    lines("cmd <<E $(x", "y)", "body", "E"),
  );
  const hereSubstitutionOutput = parseValidCst(hereSubstitutionFinal);
  assertCstRange(hereSubstitutionOutput, "2:0-3:0", "body: here_document_body");
  assertCstRange(hereSubstitutionOutput, "0:8-1:2", "command_substitution");
  assertIncrementalEqualsFresh(
    hereSubstitutionInitial,
    hereSubstitutionFinal,
    "split-substitution-before-here-document",
    { byte: 11, deleteBytes: 1, insert: "\n" },
  );
  const pipeInitial = writeSource(
    "continuation-pipe-initial",
    lines("first  |second"),
  );
  const pipeFinal = writeSource(
    "continuation-pipe-final",
    lines("first\\", "  |second"),
  );
  for (const output of assertIncrementalEqualsFresh(
    pipeInitial,
    pipeFinal,
    "insert-boundary-continuation-before-pipe",
    { byte: 5, deleteBytes: 0, insert: "\\\n" },
  )) {
    assertNotContains(output, "cmd_suffix");
    assertCstRange(output, "0:5-0:6", "line_continuation");
  }

  const eofInitial = writeSource("continuation-eof-initial", lines("first"));
  const eofFinal = writeSource("continuation-eof-final", "first\\\n");
  for (const output of assertIncrementalEqualsFresh(
    eofInitial,
    eofFinal,
    "insert-trailing-continuation-at-input-end",
    { byte: 5, deleteBytes: 0, insert: "\\" },
  )) {
    assertNotContains(output, "cmd_suffix");
    assert.equal(continuationManifest(output).length, 1);
  }

  const closerInitial = writeSource(
    "continuation-closer-initial",
    lines("x=$( a )"),
  );
  const closerFinal = writeSource(
    "continuation-closer-final",
    lines("x=$( a\\", ")"),
  );
  for (const output of assertIncrementalEqualsFresh(
    closerInitial,
    closerFinal,
    "replace-blank-with-continuation-before-closer",
    { byte: 6, deleteBytes: 1, insert: "\\\n" },
  )) {
    assertNotContains(output, "cmd_suffix");
    assert.equal(continuationManifest(output).length, 1);
  }

  const substitutionDepth = 24;
  let nestedSubstitutions = "'a'";
  for (let depth = 0; depth < substitutionDepth; depth += 1) {
    nestedSubstitutions = `$(${nestedSubstitutions} )`;
  }
  const nestedSubstitutionSource = writeSource(
    "nested-substitution-closers",
    `x=${nestedSubstitutions}\n`,
  );
  assertRepeatedColdParse(
    "valid",
    nestedSubstitutionSource,
    "nested-substitution-closers",
  );

  const arithmeticGroupDepth = 257;
  const deepArithmetic = writeSource(
    "deep-parenthesized-arithmetic",
    `echo $((${"(".repeat(arithmeticGroupDepth)}1${")".repeat(arithmeticGroupDepth)} + 1))\n`,
  );
  const deepArithmeticOutput = parseValidCst(deepArithmetic);
  assertContains(deepArithmeticOutput, "arithmetic_expansion");
  assertNotContains(deepArithmeticOutput, "command_substitution");
});

for (const { name, units, source } of scalingWorkloads()) {
  for (const factor of scalingInputMultipliers) {
    test(`sh: ${name} parses through EOF at scale ${factor}`, () => {
      assertValid(
        writeSource(`${name}-${factor}`, source(units * factor)),
        name,
      );
    });
  }
}
