import assert from "node:assert/strict";
import { test } from "node:test";
import {
  applyEdits,
  assertContains,
  assertCstDirectChildRange,
  assertCstRange,
  assertIncrementalEqualsFresh,
  assertLineContinuationManifest,
  assertNoLineContinuations,
  assertNotContains,
  assertOccurrenceCount,
  assertRepeatedColdParse,
  assertSameLogicalProjection,
  assertValid,
  cstFingerprint,
  hasRecovery,
  lineContinuationManifest,
  lines,
  parseCst,
  parseRecovery,
  parseRecoveryAfterEdits,
  parseValidCst,
  parseValidTree,
  runParse,
  runQuery,
  writeSource,
} from "./support/parser.js";

test("sh: repairing missing operands and compound bodies matches a fresh parse", () => {
  const prefix = "before alpha\n";
  const suffix = "after omega\n";
  for (const [name, broken, restored, wordOffset, removedLength = 0] of [
    ["if-empty-body", "if :;then |;fi", "if :;then fixed;fi", 10, 1],
    ["if-body-pipes", "if :;then | |;fi", "if :;then fixed;fi", 10, 3],
    ["if-empty-condition", "if |;then :;fi", "if fixed;then :;fi", 3, 1],
    ["if-body-and", "if :;then &&;fi", "if :;then fixed;fi", 10, 2],
    ["if-body-semi-pipe", "if :;then ;|;fi", "if :;then fixed;fi", 10, 2],
    ["while-empty-body", "while :;do |;done", "while :;do fixed;done", 11, 1],
    [
      "while-empty-condition",
      "while |;do :;done",
      "while fixed;do :;done",
      6,
      1,
    ],
    ["until-empty-body", "until :;do &&;done", "until :;do fixed;done", 11, 2],
    ["for-empty-body", "for x;do |;done", "for x;do fixed;done", 9, 1],
    ["brace-empty-body", "{ |; }", "{ fixed; }", 2, 1],
    ["brace-body-pipes", "{ | |; }", "{ fixed; }", 2, 3],
    ["brace-body-and", "{ &&; }", "{ fixed; }", 2, 2],
    ["brace-body-semi-pipe", "{ ;|; }", "{ fixed; }", 2, 2],
    ["subshell-empty-body", "(|;)", "(fixed;)", 1, 1],
    ["subshell-body-semi-pipe", "(;|;)", "(fixed;)", 1, 2],
    ["pipe", "broken | ;", "broken | fixed;", 9],
    ["and", "broken && ;", "broken && fixed;", 10],
    ["or", "broken || &", "broken || fixed&", 10],
    ["negation", "! ;", "! fixed;", 2],
    ["if-pipe", "if :;then :|;fi", "if :;then :|fixed;fi", 12],
    [
      "else-empty-command",
      "if :;then :;else |;fi",
      "if :;then :;else fixed;fi",
      17,
      1,
    ],
    [
      "elif-empty-condition",
      "if :;then :;elif |;then :;fi",
      "if :;then :;elif fixed;then :;fi",
      17,
      1,
    ],
    [
      "while-and",
      "while :; do : && ; done",
      "while :; do : && fixed; done",
      17,
    ],
    ["for-or", "for x;do :||;done", "for x;do :||fixed;done", 12],
    ["brace-pipe", "{ :|; }", "{ :|fixed; }", 4],
    [
      "case-first-pattern",
      "case x in ) :;;esac",
      "case x in fixed) :;;esac",
      10,
    ],
    [
      "case-next-pattern",
      "case x in x|) :;;esac",
      "case x in x|fixed) :;;esac",
      12,
    ],
    [
      "case-both-patterns",
      "case x in |) :;;esac",
      "case x in fixed) :;;esac",
      10,
      1,
    ],
    [
      "case-pipe",
      "case x in x) : | ;; esac",
      "case x in x) : | fixed;; esac",
      17,
    ],
  ]) {
    const initial = writeSource(
      `missing-${name}-operand`,
      `${prefix}${lines(broken)}${suffix}`,
    );
    const { status } = runParse({
      description: `${name}: malformed command`,
      mode: "recovery",
      source: initial,
    });
    assert.equal(status, 1, `${name}: invalid command parsed as valid`);
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

test("sh: case closers and keyword headers survive edits", () => {
  const esacWords = writeSource(
    "case-esac-word-positions",
    lines("case x in (esac) esac=x;; x|esac) : esac;; esac"),
  );
  const esacWordsOutput = parseValidCst(esacWords);
  for (const range of ["0:11-0:15", "0:28-0:32", "0:36-0:40"]) {
    assertCstRange(esacWordsOutput, range, "literal `esac`");
  }
  assertCstRange(esacWordsOutput, "0:17-0:21", "variable_name");
  assertCstRange(esacWordsOutput, "0:43-0:47", "esac_keyword `esac`");

  const emptyNsItem = writeSource(
    "case-esac-after-closed-pattern",
    lines("case x in x) esac"),
  );
  const emptyNsItemOutput = parseValidCst(emptyNsItem);
  assertCstRange(emptyNsItemOutput, "0:10-0:13", "item: case_item_ns");
  assertCstRange(emptyNsItemOutput, "0:13-0:17", "esac_keyword");
  assertNotContains(emptyNsItemOutput, "recovery");

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

  const compoundRegression = writeSource(
    "compound-list-regression",
    lines(
      "regular_file_identity() {",
      "  CURRENT_FILE_ID=",
      '  [ -f "$1" ] && [ ! -L "$1" ] ||',
      "    return 1",
      "  file_owner=$(stat -f '%u' \"$1\" 2>/dev/null) || return 1",
      "  file_mode=$(stat -f '%Lp' \"$1\" 2>/dev/null) || return 1",
      "}",
    ),
  );
  assertValid(compoundRegression);

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

  const caseItemLayout = writeSource(
    "case-item-layout",
    "case value in\n  tab) left\t|| right ;;\n  tight) left||right ;;\nesac\n",
  );
  assertValid(caseItemLayout);

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

  const numericSource = writeSource(
    "unclassified-numeric-parameter-source",
    lines(": $" + "{00}", ': "$' + '{00:-x}"', ': "$' + '{#00}"'),
  );
  const numericOutput = runParse({
    description:
      "unclassified numeric parameter spellings retain anonymous leaves",
    source: numericSource,
  }).output;
  for (const [parentRange, sourceRange] of [
    ["0:2-0:7", "0:4-0:6"],
    ["1:3-1:11", "1:5-1:7"],
    ["2:3-2:9", "2:6-2:8"],
  ]) {
    assertCstDirectChildRange(
      numericOutput,
      parentRange,
      "parameter_expansion",
      sourceRange,
      '"numeric_parameter_source"',
    );
  }
  assertNotContains(numericOutput, "parameter:");
  assertNotContains(numericOutput, "positional_parameter");
  assertNotContains(numericOutput, "special_parameter");

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
  assertCstRange(missingOperandOutput, "0:9-1:0", "line_continuation");
  assertOccurrenceCount(missingOperandOutput, "line_continuation", 1);

  for (const [name, text] of [
    ["arithmetic-prefix-increment", ': "$((++a))"'],
    ["arithmetic-prefix-decrement", ': "$((--a))"'],
    ["arithmetic-infix-increment", ': "$((a++b))"'],
    ["arithmetic-infix-decrement", ': "$((a--b))"'],
  ]) {
    const source = writeSource(name, lines(text));
    const output = runParse({
      description: `${name} falls back to command substitution`,
      source,
    }).output;
    assertContains(output, "command_substitution");
    assertContains(output, "subshell");
    assertNotContains(output, "arithmetic_unary_expression");
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
  assertCstRange(backquoteOutput, "0:6-1:0", "line_continuation");
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

test("sh: arithmetic raw newlines stay layout in every reading", () => {
  const structuredNewline = writeSource(
    "arithmetic-structured-newline",
    lines(': "$((1 +', '2))" "$((1', '))"'),
  );
  const structuredOutput = parseValidTree(structuredNewline);
  assertContains(
    structuredOutput,
    "expression: (arithmetic_binary_expression [0, 6] - [1, 1]",
  );
  assertContains(
    structuredOutput,
    "expression: (arithmetic_number [1, 9] - [1, 10])",
  );

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
    ["0:9-0:10", "literal `/`"],
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
    ["0:9-0:10", "literal `:`"],
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
    ["0:14-0:15", "literal `/`"],
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
    ["0:2-0:16", "tilde_expansion"],
    ["0:3-0:16", "user: tilde_user"],
    ["0:3-0:16", "double_quoted"],
    ["0:4-0:15", "command_substitution"],
    ["0:12-0:13", "literal `/`"],
    ["0:16-0:17", "literal `/`"],
  ]) {
    assertCstRange(nestedUserOutput, range, item);
  }

  for (const [source, prefix] of [
    [": ~[a/b]", "~[a"],
    ["x=~[a:b]", "~[a"],
    [": $" + "{p:-~[a/b]}", "~[a"],
    [": $" + "{p#~[a/b]}", "~[a"],
    ["x=~[[:alpha:]]", "~[["],
    [": ~[[:alpha:]]", "~[[:alpha:]]"],
    [": ~[a:b]", "~[a:b]"],
    [String.raw`: ~[a\/b]`, String.raw`~[a\/b]`],
    [String.raw`x=~[a\:b]`, String.raw`~[a\:b]`],
    [': ~[a"b/c"]', '~[a"b/c"]'],
    ["x=~[a$(printf b:c)]", "~[a$(printf b:c)]"],
    [': "`: ~[/]`"', "~["],
    [': "`x=~[:]`"', "~["],
  ]) {
    const output = parseValidCst(
      writeSource("tilde-bracket-prefix", lines(source)),
    );
    const start = source.indexOf("~");
    const end = start + prefix.length;
    assertOccurrenceCount(output, "tilde_expansion", 1);
    assertCstRange(output, `0:${start}-0:${end}`, "tilde_expansion");
    assertCstRange(output, `0:${start + 1}-0:${end}`, "user: tilde_user");
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
  assertCstRange(assignmentBoundaryOutput, "0:10-1:0", "line_continuation");
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
  assertCstRange(assignmentNewlineOutput, "0:3-1:0", "line_continuation");
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

test("sh: bracket fallback remains stable across complete and incomplete edits", () => {
  const bracketUnclosed = writeSource("bracket-unclosed", lines("printf [abc"));
  const bracketClosed = writeSource("bracket-closed", lines("printf [abc]"));
  const bracketRange = writeSource("bracket-range", lines("printf [a-z]"));
  const bracketUnclosedOutput = parseValidCst(bracketUnclosed);
  assertCstRange(bracketUnclosedOutput, "0:7-0:8", "literal");
  assertCstRange(bracketUnclosedOutput, "0:8-0:11", "literal");
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
  for (const [initial, final, name, edit, member, end, argument, redirect] of [
    [
      arithmeticBracket,
      fallbackBracket,
      "arithmetic-member-to-command-fallback",
      { byte: 10, deleteBytes: 0, insert: " x in x)esac" },
      "command_substitution",
      24,
      "0:26-0:29",
      "0:30-0:34",
    ],
    [
      fallbackBracket,
      arithmeticBracket,
      "command-fallback-member-to-arithmetic",
      { byte: 10, deleteBytes: 12, insert: "" },
      "arithmetic_expansion",
      12,
      "0:14-0:17",
      "0:18-0:22",
    ],
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

test("sh: here-document terminators separate text from continuations and line layout", () => {
  const fixtures = [
    {
      name: "plain",
      source: lines("cat <<EOF", "body", "EOF", "after"),
      end: "2:0-3:0",
      text: ["2:0-2:3"],
      continuations: [],
    },
    {
      name: "quoted",
      source: lines("cat <<'EOF'", "body", "EOF", "after"),
      end: "2:0-3:0",
      text: ["2:0-2:3"],
      continuations: [],
    },
    {
      name: "continued",
      source: lines("cat <<EOF", "body", "\\", "EOF", "after"),
      end: "2:0-4:0",
      text: ["3:0-3:3"],
      continuations: ["2:0-3:0"],
    },
    {
      name: "continued-after-text",
      source: lines("cat <<EOF", "body", "EOF\\", "", "after"),
      end: "2:0-4:0",
      text: ["2:0-2:3"],
      continuations: ["2:3-3:0"],
    },
    {
      name: "stripped-tabs",
      source: lines("cat <<-EOF", "body", "\t\tEOF", "after"),
      end: "2:0-3:0",
      text: ["2:2-2:5"],
      continuations: [],
    },
    {
      name: "quoted-stripped-tabs",
      source: lines("cat <<-'EOF'", "body", "\t\tEOF", "after"),
      end: "2:0-3:0",
      text: ["2:2-2:5"],
      continuations: [],
    },
    {
      name: "continued-stripped-tabs",
      source: lines("cat <<-EOF", "body", "\t\\", "\tEOF", "after"),
      end: "2:0-4:0",
      text: ["3:1-3:4"],
      continuations: ["2:1-3:0"],
    },
    {
      name: "repeated-continuations-and-stripped-tabs",
      source: lines("cat <<-EOF", "body", "\t\\", "\t\t\\", "\tEOF", "after"),
      end: "2:0-5:0",
      text: ["4:1-4:4"],
      continuations: ["2:1-3:0", "3:2-4:0"],
    },
    {
      name: "literal-leading-tab",
      source: lines("cat <<'\tEOF'", "body", "\tEOF", "after"),
      end: "2:0-3:0",
      text: ["2:0-2:4"],
      continuations: [],
    },
    {
      name: "empty",
      source: lines("cat <<''", "body", "", "after"),
      end: "2:0-3:0",
      text: [],
      continuations: [],
    },
    {
      name: "empty-stripped-tabs",
      source: lines("cat <<-''", "body", "\t\t", "after"),
      end: "2:0-3:0",
      text: [],
      continuations: [],
    },
    {
      name: "empty-stripped-tabs-at-eof",
      source: "cat <<-''\nbody\n\t\t",
      end: "2:0-2:2",
      text: [],
      continuations: [],
    },
    {
      name: "quoted-backslash",
      source: lines("cat <<'\\$END'", "body", "\\$END", "after"),
      end: "2:0-3:0",
      text: ["2:0-2:5"],
      continuations: [],
    },
    {
      name: "quoted-trailing-backslash",
      source: lines("cat <<'END\\'", "body", "END\\", "after"),
      end: "2:0-3:0",
      text: ["2:0-2:4"],
      continuations: [],
    },
    {
      name: "escaped-backtick",
      source: lines(': `cat <<"\\\\\\`"', "body", "\\`", "`", "after"),
      end: "2:0-3:0",
      text: ["2:0-2:2"],
      continuations: [],
    },
    {
      name: "quoted-before-backquote-closer",
      source: lines(": `cat <<'EOF'", "body", "EOF`", "after"),
      end: "2:0-2:3",
      text: ["2:0-2:3"],
      continuations: [],
    },
    {
      name: "unicode",
      source: lines("cat <<'終'", "body", "終", "after"),
      end: "2:0-3:0",
      text: ["2:0-2:3"],
      continuations: [],
    },
    {
      name: "non-bmp-unicode-at-eof",
      source: "cat <<'𠮷'\nbody\n𠮷",
      end: "2:0-2:4",
      text: ["2:0-2:4"],
      continuations: [],
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

  const initial = writeSource("terminator-edit-initial", fixtures[0].source);
  const continued = writeSource(
    "terminator-edit-continued",
    fixtures[2].source,
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

  const quoted = writeSource("terminator-edit-quoted", fixtures[1].source);
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
      "single-quote",
      "${value:-'text",
      `\${value:-'text'}`,
      "'}",
      "single_quoted",
    ],
    [
      "dollar-single-quote",
      "${value:-$'text",
      `\${value:-$'text'}`,
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

test("sh: here-document body escape runs fold all enclosing backquotes", () => {
  for (const [
    name,
    initialText,
    finalText,
    offset,
    parameterRange,
    escapeRanges,
  ] of [
    [
      "plain",
      ": `cat <<EOF\n\\$x\nEOF\n`\n",
      ": `cat <<EOF\n\\\\$x\nEOF\n`\n",
      13,
      "1:0-1:3",
      ["1:0-1:2"],
    ],
    [
      "nested",
      ": `: \\`cat <<EOF\n\\\\\\$x\nEOF\n\\`\n`\n",
      ": `: \\`cat <<EOF\n\\\\\\\\$x\nEOF\n\\`\n`\n",
      17,
      "1:0-1:5",
      ["1:0-1:2", "1:2-1:4"],
    ],
    [
      "strip-tabs",
      ": `cat <<-EOF\n\t\\$x\n\tEOF\n`\n",
      ": `cat <<-EOF\n\t\\\\$x\n\tEOF\n`\n",
      15,
      "1:1-1:4",
      ["1:1-1:3"],
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
      for (const range of escapeRanges)
        assertCstRange(output, range, "here_document_escape");
      assertCstRange(output, "1:0-2:0", "body: here_document_body");
      assertCstRange(output, "2:0-3:0", "end: here_document_end");
    }
    assertIncrementalEqualsFresh(
      final,
      initial,
      `${name}-restore-body-expansion`,
      { byte: offset, deleteBytes: 1, insert: "" },
    );
  }

  const quotedTail = writeSource(
    "body-quote-after-enclosing-backquote",
    ': "`cat <<EOF\n\\"\nEOF\n`"\n',
  );
  const quotedTailOutput = parseValidCst(quotedTail);
  assertCstRange(quotedTailOutput, "1:0-1:1", "here_document_text");
  assertCstRange(quotedTailOutput, "1:1-1:2", "here_document_text");
  assertOccurrenceCount(quotedTailOutput, "double_quoted\n", 1);
  assertNotContains(quotedTailOutput, "here_document_escape");

  const quotedDelimiter = writeSource(
    "quoted-body-retains-raw-run",
    ": `cat <<'EOF'\n\\\\$x\nEOF\n`\n",
  );
  const quotedOutput = parseValidCst(quotedDelimiter);
  assertCstRange(quotedOutput, "1:0-1:4", "quoted_here_document_text");
  assertNotContains(quotedOutput, "parameter_expansion");
  assertNotContains(quotedOutput, "here_document_escape");
});

test("sh: paired here-document backslashes retain the newline before the delimiter", () => {
  for (const [name, initialText, finalText, offset, width] of [
    [
      "plain",
      ": `cat <<EOF\nbody\nEOF\n`\n",
      ": `cat <<EOF\n\\\\\\\\\nEOF\n`\n",
      13,
      4,
    ],
    [
      "quoted",
      ': "`cat <<EOF\nbody\nEOF\n`"\n',
      ': "`cat <<EOF\n\\\\\\\\\nEOF\n`"\n',
      14,
      4,
    ],
    [
      "nested",
      ": `: \\`cat <<EOF\nbody\nEOF\n\\`\n`\n",
      ": `: \\`cat <<EOF\n\\\\\\\\\\\\\\\\\nEOF\n\\`\n`\n",
      17,
      8,
    ],
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
      assertNotContains(output, "line_continuation");
    }
    assertIncrementalEqualsFresh(
      final,
      initial,
      `${name}-restore-body-before-paired-run`,
      { byte: offset, deleteBytes: width, insert: "body" },
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
      quoteNode: "dollar_single_quoted",
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
      quoteNode: "double_quoted",
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
      quoteNode: "double_quoted",
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
      quoteNode: "double_quoted",
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
      quoteNode: "double_quoted",
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
      assertCstRange(output, fixture.delimiterRange, fixture.quoteNode);
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

  const backquoteDocument = writeSource(
    "backquote-here-document",
    lines("cat <<\"`printf '%s' END`\"", "body", "`printf %s END`", "after"),
  );
  const backquoteDocumentOutput = parseValidCst(backquoteDocument);
  assertCstRange(backquoteDocumentOutput, "2:0-3:0", "end: here_document_end");
  assertCstRange(
    backquoteDocumentOutput,
    "3:0-3:5",
    "command: complete_command",
  );

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

  const slash = "\\";
  const byteDocument = writeSource(
    "byte-here-document",
    Buffer.concat([
      Buffer.from(`cat <<$'${slash}c?'\nbody\n`),
      Buffer.from([0x7f]),
      Buffer.from(`\ncat <<$'${slash}xC3${slash}xBF'\nbody\n`),
      Buffer.from([0xc3, 0xbf]),
      Buffer.from("\nafter\n"),
    ]),
  );
  const byteDocumentOutput = parseValidCst(byteDocument);
  assertCstRange(byteDocumentOutput, "2:0-3:0", "end: here_document_end");
  assertCstRange(byteDocumentOutput, "5:0-6:0", "end: here_document_end");
  assertCstRange(byteDocumentOutput, "6:0-6:5", "command: complete_command");

  const nulDocument = writeSource(
    "nul-here-document",
    Buffer.from("cat <<EOF #a\0b\nbody\nEOF\nafter\n"),
  );
  const nulDocumentOutput = runParse({
    description: "NUL inside here-document declaration comment",
    source: nulDocument,
  }).output;
  assertCstRange(nulDocumentOutput, "0:10-0:14", "comment: comment");
  assertCstRange(nulDocumentOutput, "2:0-3:0", "end: here_document_end");
  assertCstRange(nulDocumentOutput, "3:0-3:5", "command: complete_command");
  assertCstRange(nulDocumentOutput, "0:0-4:0", "program");

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
    ["0:6-1:0", "line_continuation"],
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
  assertCstRange(continuedTabOutput, "1:1-2:0", "line_continuation");
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
  assertCstRange(continuedDollarOutput, "1:1-2:0", "line_continuation");
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

  const longInitialDelimiter = "A".repeat(2_048);
  const longFinalDelimiter = "B".repeat(2_048);
  const longInitial = writeSource(
    "long-initial",
    `cat <<${longInitialDelimiter}\nbody\n${longInitialDelimiter}\n`,
  );
  const longFinal = writeSource(
    "long-final",
    `cat <<${longFinalDelimiter}\nbody\n${longFinalDelimiter}\nafter`,
  );
  parseRecoveryAfterEdits(
    longInitial,
    longFinal,
    "oversized-delimiter-recovery",
    { byte: 6, deleteBytes: 2048, insert: longFinalDelimiter },
    { byte: 2060, deleteBytes: 2048, insert: longFinalDelimiter },
    { byte: 4109, deleteBytes: 0, insert: "after" },
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
  const fallbackDelimiter = writeSource(
    "command-fallback-here-document-delimiter",
    lines(
      "cat <<$((case x in x)esac)) >out arg",
      "body",
      "$((case x in x)esac))",
      "after",
    ),
  );
  for (const [initial, final, name, edits, member, end, redirect, argument] of [
    [
      arithmeticDelimiter,
      fallbackDelimiter,
      "arithmetic-delimiter-to-command-fallback",
      [
        { byte: 37, deleteBytes: 0, insert: " x in x)esac" },
        { byte: 13, deleteBytes: 0, insert: " x in x)esac" },
      ],
      "command_substitution",
      27,
      "0:28-0:32",
      "0:33-0:36",
    ],
    [
      fallbackDelimiter,
      arithmeticDelimiter,
      "command-fallback-delimiter-to-arithmetic",
      [
        { byte: 49, deleteBytes: 12, insert: "" },
        { byte: 13, deleteBytes: 12, insert: "" },
      ],
      "arithmetic_expansion",
      15,
      "0:16-0:20",
      "0:21-0:24",
    ],
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
      continuations: ["2:0-3:0"],
      quotedBodies: 0,
    },
    {
      name: "quoted-inner-body-remains-opaque-before-continuation",
      source: "cat <<OUT\n$(cat <<'IN'\n$x\n\\\nIN\n)\nOUT\n",
      ends: [["3:0-5:0", "4:0-4:2"]],
      continuations: ["3:0-4:0"],
      quotedBodies: 1,
    },
    {
      name: "quoted-empty-delimiter-inherits-continuation",
      source: "cat <<OUT\n$(cat <<''\n\\\n\n)\nOUT\n",
      ends: [["2:0-4:0", null]],
      continuations: ["2:0-3:0"],
      quotedBodies: 0,
    },
    {
      name: "quoted-inner-preserves-text-before-boundary-continuation",
      source: "cat <<OUT\n$(cat <<'IN'\n\\\\\\\n \nIN\n)\nOUT\n",
      ends: [["4:0-5:0", "4:0-4:2"]],
      continuations: ["2:2-3:0"],
      prefixes: ["2:0-2:2"],
      quotedBodies: 1,
    },
    {
      name: "continuation-removal-precedes-inherited-tab-removal",
      source: "cat <<-OUT\n$(cat <<'IN'\n\t\\\n\tIN\n)\nOUT\n",
      ends: [["2:0-4:0", "3:1-3:3"]],
      continuations: ["2:1-3:0"],
      quotedBodies: 0,
    },
    {
      name: "continuation-removal-ends-with-enclosing-body",
      source: "cat <<OUT\n$(cat <<'IN'\n\\\nIN\n)\nOUT\ncat <<'IN'\n\\\nIN\n",
      ends: [
        ["2:0-4:0", "3:0-3:2"],
        ["8:0-9:0", "8:0-8:2"],
      ],
      continuations: ["2:0-3:0"],
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
  for (const [name, run, edit, continuations, textRange] of [
    [
      "extend-prefix",
      "\\".repeat(5),
      { byte: prefix.length + 1, deleteBytes: 0, insert: "\\\\" },
      ["2:4-3:0"],
      "2:0-2:4",
    ],
    [
      "remove-continuation-by-changing-parity",
      "\\\\",
      { byte: prefix.length + 1, deleteBytes: 1, insert: "" },
      [],
      "2:0-2:2",
    ],
    [
      "replace-counted-backslash-with-text",
      "\\x\\",
      { byte: prefix.length + 1, deleteBytes: 1, insert: "x" },
      ["2:2-3:0"],
      "2:0-2:2",
    ],
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

test("sh: line-continuation and comment contracts", () => {
  const syntaxPhysical = writeSource(
    "syntax-continuation-physical",
    lines('echo "$(printf x\\', ')"', "echo `printf x\\", "`"),
  );
  const syntaxLogical = writeSource(
    "syntax-continuation-logical",
    lines('echo "$(printf x)"', "echo `printf x`"),
  );
  assertLineContinuationManifest(
    "line-continuation-syntax-contract",
    syntaxPhysical,
    syntaxLogical,
    ["0:16-1:0", "2:14-3:0"],
  );

  const literalContinuations = writeSource(
    "literal-line-continuations",
    lines(
      "printf '%s\\n' 'single\\",
      "quote' $'dollar\\",
      "quote'",
      "cat <<'EOF'",
      "quoted\\",
      "body",
      "EOF",
    ),
  );
  assertNoLineContinuations("line-continuation-literals", literalContinuations);

  const terminalAssignmentPhysical = writeSource(
    "terminal-assignment-physical",
    "A=\\\n",
  );
  const terminalAssignmentLogical = writeSource(
    "terminal-assignment-logical",
    "A=",
  );
  const terminalPhysicalOutput = parseValidCst(terminalAssignmentPhysical);
  const terminalLogicalOutput = parseValidCst(terminalAssignmentLogical);
  assertCstRange(terminalPhysicalOutput, "0:2-1:0", "line_continuation");
  assertOccurrenceCount(terminalPhysicalOutput, "line_continuation", 1);
  assertSameLogicalProjection(
    "terminal-assignment-continuation",
    terminalLogicalOutput,
    terminalPhysicalOutput,
  );

  const comments = writeSource(
    "comments",
    lines(
      "  # leading",
      "command # trailing",
      "cat <<EOF # declaration",
      "body",
      "EOF",
    ),
  );
  const commentsOutput = parseValidTree(comments);
  assertContains(commentsOutput, "(comment [0, 2] - [0, 11])");
  assertContains(commentsOutput, "(comment [1, 8] - [1, 18])");
  assertContains(commentsOutput, "comment: (comment [2, 10] - [2, 23])");

  const continuedComments = writeSource(
    "continued-comments",
    lines(
      "\\",
      "# leading",
      "first && \\",
      "# operator",
      "second",
      "cat <<EOF \\",
      "# declaration",
      "body",
      "EOF",
    ),
  );
  const continuedCommentsOutput = parseValidTree(continuedComments);
  for (const expected of [
    "(line_continuation [0, 0] - [1, 0])",
    "(comment [1, 0] - [1, 9])",
    "(line_continuation [2, 9] - [3, 0])",
    "(comment [3, 0] - [3, 10])",
    "(line_continuation [5, 10] - [6, 0])",
    "comment: (comment [6, 0] - [6, 13])",
  ]) {
    assertContains(continuedCommentsOutput, expected);
  }
  assertNotContains(continuedCommentsOutput, "ERROR");

  const nulComment = writeSource(
    "nul-comment",
    Buffer.from("#a\0b\nnext\n", "utf8"),
  );
  const nulCommentOutput = parseValidCst(nulComment, "NUL inside comment");
  assertCstRange(nulCommentOutput, "0:0-0:4", "comment");
  assertCstRange(nulCommentOutput, "1:0-1:4", "command: complete_command");
  assertCstRange(nulCommentOutput, "0:0-2:0", "program");

  const continuedCommentInitial = writeSource(
    "continued-comment-initial",
    lines("first && # comment", "second"),
  );
  const continuedCommentFinal = writeSource(
    "continued-comment-final",
    lines("first && \\", "# comment", "second"),
  );
  assertIncrementalEqualsFresh(
    continuedCommentInitial,
    continuedCommentFinal,
    "insert-comment-boundary-continuation",
    { byte: 9, deleteBytes: 0, insert: "\\\n" },
  );
  assertIncrementalEqualsFresh(
    continuedCommentFinal,
    continuedCommentInitial,
    "delete-comment-boundary-continuation",
    { byte: 9, deleteBytes: 2, insert: "" },
  );

  const commentHorizonInitial = writeSource(
    "comment-horizon-initial",
    lines("while read line; do", '  echo "$line" # note', "done < file"),
  );
  const commentHorizonFinal = writeSource(
    "comment-horizon-final",
    lines("while read line; do", '  echo "$line" # note', "don& < file"),
  );
  parseRecoveryAfterEdits(
    commentHorizonInitial,
    commentHorizonFinal,
    "comment-line-horizon-closer-loss",
    { byte: 45, deleteBytes: 1, insert: "&" },
  );
  assertIncrementalEqualsFresh(
    commentHorizonFinal,
    commentHorizonInitial,
    "comment-line-horizon-closer-return",
    { byte: 45, deleteBytes: 1, insert: "e" },
  );

  const commentChainInitial = writeSource(
    "comment-chain-initial",
    lines(
      "while read line; do",
      "  # first",
      "  # second",
      "",
      "  # third",
      "done < file",
    ),
  );
  const commentChainCloserFinal = writeSource(
    "comment-chain-closer-final",
    lines(
      "while read line; do",
      "  # first",
      "  # second",
      "",
      "  # third",
      "don& < file",
    ),
  );
  parseRecoveryAfterEdits(
    commentChainInitial,
    commentChainCloserFinal,
    "comment-chain-closer-loss",
    { byte: 55, deleteBytes: 1, insert: "&" },
  );
  parseRecoveryAfterEdits(
    commentChainCloserFinal,
    commentChainInitial,
    "comment-chain-closer-return",
    { byte: 55, deleteBytes: 1, insert: "e" },
  );

  const commentChainCommandFinal = writeSource(
    "comment-chain-command-final",
    lines(
      "while read line; do",
      "  # first",
      "  # second",
      "",
      "  : third",
      "done < file",
    ),
  );
  assertIncrementalEqualsFresh(
    commentChainInitial,
    commentChainCommandFinal,
    "comment-chain-command-start",
    { byte: 44, deleteBytes: 1, insert: ":" },
  );
  parseRecoveryAfterEdits(
    commentChainCommandFinal,
    commentChainInitial,
    "comment-chain-comment-return",
    { byte: 44, deleteBytes: 1, insert: "#" },
  );

  const trailingCommentInitial = writeSource(
    "trailing-comment-initial",
    "printf x\n",
  );
  const trailingCommentFinal = writeSource(
    "trailing-comment-final",
    "printf x\n# c",
  );
  assertIncrementalEqualsFresh(
    trailingCommentInitial,
    trailingCommentFinal,
    "append-trailing-comment-at-input-end",
    { byte: 9, deleteBytes: 0, insert: "# c" },
  );
  assertIncrementalEqualsFresh(
    trailingCommentFinal,
    trailingCommentInitial,
    "delete-trailing-comment-at-input-end",
    { byte: 9, deleteBytes: 3, insert: "" },
  );
});

test("sh: commands regain their public structure after a stray separator", () => {
  for (const [name, initial, final, offset, replacement] of [
    ["simple-command", "a; b\n", "a; c\n", 3, "c"],
    ["function", "f() { a; b; }\n", "f() { a; c; }\n", 9, "c"],
    ["if-clause", "if a; then b; c; fi\n", "if a; then b; d; fi\n", 14, "d"],
    [
      "case-item",
      "case x in x) a; b;; esac\n",
      "case x in x) a; c;; esac\n",
      16,
      "c",
    ],
  ]) {
    assertIncrementalEqualsFresh(
      writeSource(`${name}-before-stray-separator`, initial),
      writeSource(`${name}-after-stray-separator`, final),
      `restore-${name}-after-stray-separator`,
      { byte: offset, deleteBytes: 1, insert: ";" },
      { byte: offset, deleteBytes: 1, insert: replacement },
    );
  }
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
  assertCstRange(physicalOutput, "0:8-1:0", "line_continuation");
  assertOccurrenceCount(physicalOutput, "line_continuation", 1);
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
  assertCstRange(repeatedPhysicalOutput, "0:8-1:0", "line_continuation");
  assertCstRange(repeatedPhysicalOutput, "1:1-2:0", "line_continuation");
  assertOccurrenceCount(repeatedPhysicalOutput, "line_continuation", 2);
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
    ["0:9-1:0", "line_continuation"],
    ["2:9-3:0", "line_continuation"],
    ["3:2-3:4", "operator: or_if"],
    ["4:9-5:0", "line_continuation"],
    ["5:2-5:4", "operator: and_if"],
    ["6:17-7:0", "line_continuation"],
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
  assertCstRange(repeatedPipeOutput, "0:5-1:0", "line_continuation");
  assertCstRange(repeatedPipeOutput, "1:0-2:0", "line_continuation");
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
  assertCstRange(closedAndOrPhysicalOutput, "0:7-1:0", "line_continuation");
  assertOccurrenceCount(closedAndOrPhysicalOutput, "line_continuation", 1);
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
  assertCstRange(forFormalOutput, "0:6-1:0", "line_continuation");
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
  assertCstRange(caseSubjectOutput, "0:7-1:0", "line_continuation");
  assertCstRange(caseSubjectOutput, "1:0-2:0", "line_continuation");
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
  assertCstRange(backquoteEndOutput, "0:14-1:0", "line_continuation");
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
  assertCstRange(substitutionEndOutput, "0:15-1:0", "line_continuation");
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

  const nodeStructure = (name, contents) =>
    parseCst(parseValidCst(writeSource(name, contents), name)).map(
      (entry) => entry.content,
    );
  assert.deepEqual(
    nodeStructure("compound-closer-spaced", "if true; then : \\\n\nfi\n"),
    nodeStructure("compound-closer-glued", "if true; then :\\\n\nfi\n"),
    "spaced and glued continuation closers share a public node structure",
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

test("sh: substitution, redirection, and token boundaries retain ownership", () => {
  const backquoteOpeners = writeSource(
    "backquote-opener-boundaries",
    lines(
      "{ `printf brace`; }",
      ": && `printf and-or`",
      "case `printf selector` in",
      "  selector) : ;;",
      "esac",
    ),
  );
  const backquoteOpenersOutput = parseValidTree(backquoteOpeners);
  for (const expected of [
    "(backquote_substitution [0, 2] - [0, 16]",
    "(backquote_substitution [1, 5] - [1, 20]",
    "(backquote_substitution [2, 5] - [2, 22]",
  ]) {
    assertContains(backquoteOpenersOutput, expected);
  }
  assertNotContains(backquoteOpenersOutput, "recovery");

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
  assertCstRange(braceWordContinuationOutput, "0:3-1:0", "line_continuation");
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

  const delimiters = writeSource(
    "source-delimiters",
    lines(
      "printf x$x",
      "echo `inner`",
      "echo `printf \\`nested\\` \\$name`",
      "echo $" + "{x} $(x) $((1))",
    ),
  );
  const delimiterOutput = parseValidCst(delimiters);
  const backquoteToken = '"\\`"';
  const backslashToken = '"\\\\"';
  for (const [range, item] of [
    ["0:8-0:9", '"$"'],
    ["1:5-1:6", backquoteToken],
    ["1:11-1:12", backquoteToken],
    ["2:5-2:6", backquoteToken],
    ["2:13-2:14", backslashToken],
    ["2:14-2:15", backquoteToken],
    ["2:21-2:22", backslashToken],
    ["2:22-2:23", backquoteToken],
    ["2:24-2:25", backslashToken],
    ["2:25-2:26", '"$"'],
    ["2:30-2:31", backquoteToken],
  ]) {
    assertCstRange(delimiterOutput, range, item);
  }
  for (const [parentRange, parentItem, childRange, childItem] of [
    ["3:5-3:9", "parameter_expansion", "3:5-3:6", '"$"'],
    ["3:5-3:9", "parameter_expansion", "3:6-3:7", '"{"'],
    ["3:10-3:14", "command_substitution", "3:10-3:11", '"$"'],
    ["3:10-3:14", "command_substitution", "3:11-3:12", '"("'],
    ["3:15-3:21", "arithmetic_expansion", "3:15-3:16", '"$"'],
    ["3:15-3:21", "arithmetic_expansion", "3:16-3:17", '"("'],
    ["3:15-3:21", "arithmetic_expansion", "3:17-3:18", '"("'],
  ]) {
    assertCstDirectChildRange(
      delimiterOutput,
      parentRange,
      parentItem,
      childRange,
      childItem,
    );
  }
  assertNotContains(delimiterOutput, '"${"');
  assertNotContains(delimiterOutput, '"$("');

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

test("sh: backquote pair runs retain the ordinary tail backslash in the CST", () => {
  for (const [
    name,
    initialText,
    finalText,
    offset,
    tailRange,
    literalRange,
    literalNode,
  ] of [
    [
      "word",
      ": `: \\\\q`\n",
      ": `: \\\\\\q`\n",
      5,
      "0:7-0:8",
      "0:8-0:9",
      "literal",
    ],
    [
      "double-quote",
      ': `: "\\\\q"`\n',
      ': `: "\\\\\\q"`\n',
      6,
      "0:8-0:9",
      "0:9-0:10",
      "double_quote_text",
    ],
    [
      "here-document",
      ": `cat <<EOF\n\\\\q\nEOF\n`\n",
      ": `cat <<EOF\n\\\\\\q\nEOF\n`\n",
      13,
      "1:2-1:3",
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
      assertCstRange(output, tailRange, '"\\\\"');
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

test("sh: embedded command boundaries preserve enclosing source structure", () => {
  for (const [name, source, range, node] of [
    [
      "nested-case",
      lines(": [$(: $(case x in x) :;; esac))]"),
      "0:2-0:33",
      "pattern_bracket_source",
    ],
    [
      "nested-comment",
      lines(": [$(: $(# comment )", ":))]"),
      "0:2-1:4",
      "pattern_bracket_source",
    ],
    [
      "quoted-nested-case",
      lines(': [$(: "$(case x in x) : \'"\';; esac)")]'),
      "0:2-0:39",
      "pattern_bracket_source",
    ],
    [
      "function-case-body",
      lines(": [$(f() case x in x) :;; esac)]"),
      "0:2-0:32",
      "pattern_bracket_source",
    ],
    [
      "function-brace-body",
      lines(": [$(f() { case x in x) :;; esac; })]"),
      "0:2-0:37",
      "pattern_bracket_source",
    ],
    [
      "arithmetic-nested-case",
      lines(': "$(( $(printf %s $(case x in x) printf 1;; esac)) ))"'),
      "0:3-0:54",
      "arithmetic_expansion",
    ],
    [
      "for-name-without-separator",
      lines(": [$(for case do case x in x) :;; esac; done)]"),
      "0:2-0:46",
      "pattern_bracket_source",
    ],
    [
      "for-header-linebreak-and-reserved-wordlist",
      lines(
        ": [$(for x",
        "in case esac do for; do case x in x) :;; esac; done)]",
      ),
      "0:2-1:53",
      "pattern_bracket_source",
    ],
    [
      "closed-subshell-with-redirects",
      lines(": [$(case x in x) (: )2>pre$(:)esac 3<case esac)]"),
      "0:2-0:49",
      "pattern_bracket_source",
    ],
    [
      "closed-brace-group",
      lines(": [$(case x in x) { :; } esac)]"),
      "0:2-0:31",
      "pattern_bracket_source",
    ],
    [
      "closed-if-clause",
      lines(": [$(case x in x) if :; then :; fi esac)]"),
      "0:2-0:41",
      "pattern_bracket_source",
    ],
    [
      "simple-command-redirect-keeps-suffix-word",
      lines(": [$(case x in x) : >case esac;; y) :;; esac)]"),
      "0:2-0:46",
      "pattern_bracket_source",
    ],
  ]) {
    const file = writeSource(name, source);
    const { output } = runParse({ source: file, description: name });
    assertCstRange(output, range, node);
    assertOccurrenceCount(output, node, 1);
  }

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

  for (const [
    name,
    initialText,
    finalText,
    offset,
    quoteRange,
    escapeRanges,
    quoteNode = "dollar_single_quoted",
    parameterRange,
  ] of [
    ["plain", ": `: $'\\''`", ": `: $'\\\\''`", 7, "0:5-0:11", ["0:7-0:10"]],
    [
      "quoted",
      ": \"`: $'\\''`\"",
      ": \"`: $'\\\\''`\"",
      8,
      "0:6-0:12",
      ["0:8-0:11"],
    ],
    [
      "nested",
      ": \"`: \\`: $'\\\\\\''\\``\"",
      ": \"`: \\`: $'\\\\\\\\''\\``\"",
      12,
      "0:10-0:18",
      ["0:12-0:17"],
    ],
    [
      "paired",
      ": `: $'\\\\\\\\\\''`",
      ": `: $'\\\\\\\\\\\\''`",
      7,
      "0:5-0:15",
      ["0:7-0:11", "0:11-0:14"],
    ],
    [
      "control",
      ": `: $'\\c\\\\\\\\'`",
      ": `: $'\\\\c\\\\\\\\'`",
      7,
      "0:5-0:15",
      ["0:7-0:14"],
    ],
    [
      "nested-control",
      ": \"`: \\`: $'\\\\\\c\\\\\\\\\\\\\\\\'\\``\"",
      ": \"`: \\`: $'\\\\\\\\c\\\\\\\\\\\\\\\\'\\``\"",
      12,
      "0:10-0:26",
      ["0:12-0:25"],
    ],
    [
      "escaped-opener",
      ": `: $'\\\\''`",
      ": `: \\$'\\\\''`",
      5,
      "0:5-0:12",
      ["0:8-0:11"],
    ],
    [
      "double-quote-text",
      "echo `a \"$'x'\"`",
      "echo `a \"\\$'x'\"`",
      9,
      "0:8-0:15",
      [],
      "double_quoted",
    ],
    [
      "parameter-word-text",
      'echo `a "$' + "{x:-$'x'}\"`",
      'echo `a "$' + "{x:-\\$'x'}\"`",
      14,
      "0:8-0:21",
      [],
      "double_quoted",
      "0:9-0:20",
    ],
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

test("sh: embedded lookahead inherits parameter quoting and resets it for patterns and commands", () => {
  const cases = [
    {
      name: "arithmetic-quoted-parameter-apostrophe",
      source: 'echo "$(( $' + "{x:-'} $op 1 ))\"",
      range: "0:10-0:23",
      node: "arithmetic_dynamic_expression",
      singleQuotes: 0,
    },
    {
      name: "bracket-quoted-parameter-apostrophe",
      source: ": $" + '{v#["$' + "{x-'}\"]}",
      range: "0:6-0:16",
      node: "pattern_bracket_source",
      singleQuotes: 0,
    },
    {
      name: "arithmetic-nested-quoted-parameter",
      source: ": $(( $" + "{x-$" + "{y-'}} $op 1 ))",
      range: "0:6-0:23",
      node: "arithmetic_dynamic_expression",
      singleQuotes: 0,
    },
    {
      name: "arithmetic-positional-parameter-pattern",
      source: ": $(( $" + "{10#'}'} $op 1 ))",
      range: "0:6-0:21",
      node: "arithmetic_dynamic_expression",
      singleQuotes: 1,
    },
    {
      name: "arithmetic-named-parameter-pattern",
      source: ": $(( $" + "{name%%'}'} $op 1 ))",
      range: "0:6-0:24",
      node: "arithmetic_dynamic_expression",
      singleQuotes: 1,
    },
    {
      name: "arithmetic-command-quoted-parameter",
      source: ': $(( $(printf %s "$' + "{x-'}\") $op 1 ))",
      range: "0:6-0:33",
      node: "arithmetic_dynamic_expression",
      singleQuotes: 0,
    },
    {
      name: "arithmetic-command-single-quoted-parenthesis",
      source: ": $(( $(printf %s ')') $op 1 ))",
      range: "0:6-0:28",
      node: "arithmetic_dynamic_expression",
      singleQuotes: 1,
    },
    {
      name: "bracket-quoted-parameter-literal-dollar-apostrophe",
      source: ": $" + '{v#["$' + "{x-$'}\"]}",
      range: "0:6-0:17",
      node: "pattern_bracket_source",
      singleQuotes: 0,
    },
  ];
  for (const { name, source, range, node, singleQuotes } of cases) {
    const sourcePath = writeSource(name, lines(source));
    const output = parseValidCst(sourcePath);
    assertCstRange(output, range, node);
    assertOccurrenceCount(output, "single_quoted", singleQuotes);
    assertOccurrenceCount(output, "dollar_single_quoted", 0);
  }

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
      source: lines(`: \`: [0-${slash.repeat(2)}z]\``),
      range: "0:6-0:11",
      escapes: ["0:8-0:11"],
    },
    {
      name: "odd-raw-pair-endpoint",
      source: lines(`: \`: [0-${slash.repeat(3)}z]\``),
      range: "0:6-0:11",
      escapes: ["0:8-0:11"],
    },
    {
      name: "complete-raw-pair-endpoint",
      source: lines(`: \`: [0-${slash.repeat(4)}z]\``),
      range: "0:6-0:12",
      escapes: ["0:8-0:12"],
    },
    {
      name: "odd-trailing-member",
      source: lines(`: \`: [0-${slash.repeat(7)}z]\``),
      range: "0:6-0:12",
      escapes: ["0:8-0:12", "0:12-0:15"],
    },
    {
      name: "complete-trailing-member",
      source: lines(`: \`: [0-${slash.repeat(8)}z]\``),
      range: "0:6-0:12",
      escapes: ["0:8-0:12", "0:12-0:16"],
    },
    {
      name: "parameter-pattern-endpoint",
      source: lines(`: \`: \${v#[0-${slash.repeat(4)}z]}\``),
      range: "0:10-0:16",
      escapes: ["0:12-0:16"],
    },
    {
      name: "special-marker-prefixed-range",
      source: lines(`: \`: [:-${slash.repeat(4)}z]\``),
      range: "0:6-0:12",
      escapes: ["0:8-0:12"],
    },
    {
      name: "escaped-range-start",
      source: lines(`: \`: [${slash.repeat(4)}-z]\``),
      range: "0:6-0:12",
      escapes: ["0:6-0:10"],
    },
    {
      name: "nested-backquote-endpoint",
      source: lines(`: \`: \\\`: [0-${slash.repeat(8)}z]\\\`\``),
      range: "0:10-0:20",
      escapes: ["0:12-0:20"],
    },
    {
      name: "nested-backquote-separate-member",
      source: lines(`: \`: \\\`: [0-${slash.repeat(16)}z]\\\`\``),
      range: "0:10-0:20",
      escapes: ["0:12-0:20", "0:20-0:28"],
    },
    {
      name: "escaped-dollar-endpoint",
      source: lines(`: \`: [0-${slash.repeat(3)}$v]\``),
      range: "0:6-0:12",
      escapes: ["0:8-0:12"],
    },
    {
      name: "parameter-after-escaped-endpoint",
      source: lines(`: \`: [0-${slash.repeat(5)}$v]\``),
      range: "0:6-0:12",
      escapes: ["0:8-0:12"],
      parameter: "0:12-0:15",
    },
    {
      name: "escaped-backtick-endpoint",
      source: lines(`: \`: [0-${slash.repeat(3)}\`z]\``),
      range: "0:6-0:12",
      escapes: ["0:8-0:12"],
    },
    {
      name: "enclosing-double-quote-fold",
      source: lines(`: "\`: [0-${slash.repeat(3)}"]\`"`),
      range: "0:7-0:13",
      escapes: ["0:9-0:13"],
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
    if (entry.parameter !== undefined) {
      assertCstRange(output, entry.parameter, "parameter_expansion");
    }
  }

  for (const [initialName, finalName, offset, inserted] of [
    ["escaped-ordinary-endpoint", "complete-raw-pair-endpoint", 8, 2],
    ["complete-raw-pair-endpoint", "complete-trailing-member", 8, 4],
    ["nested-backquote-endpoint", "nested-backquote-separate-member", 12, 8],
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
  for (const [name, plainSource, escapedSource, offset, openerRange] of [
    [
      "unquoted",
      lines(": `: [$(cat <<X", "]", "X", ")]`"),
      lines(": `: [$(cat <<\\\\X", "]", "X", ")]`"),
      14,
      "0:5-0:6",
    ],
    [
      "double-quoted",
      lines(': "`: [$(cat <<X', "]", "X", ')]`"'),
      lines(': "`: [$(cat <<\\\\X', "]", "X", ')]`"'),
      15,
      "0:6-0:7",
    ],
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
  assertValid(
    writeSource("here-backquote-final", lines("cmd <<E `x", "y`", "body", "E")),
  );
  assertValid(
    writeSource("here-escape-final", lines("echo $(cat <<E) x", "body", "E")),
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
    assertCstRange(output, "0:5-1:0", "line_continuation");
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
    assertOccurrenceCount(output, "line_continuation", 1);
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
    assertOccurrenceCount(output, "line_continuation", 1);
  }

  let nestedSubstitutions = "'a'";
  for (let depth = 0; depth < 24; depth += 1) {
    nestedSubstitutions = `$(${nestedSubstitutions} )`;
  }
  const nestedSubstitutionSource = writeSource(
    "nested-substitution-closers",
    `x=${nestedSubstitutions}\n`,
  );
  assertValid(nestedSubstitutionSource);
  assertRepeatedColdParse(
    "valid",
    nestedSubstitutionSource,
    "nested-substitution-closers",
  );

  const deepArithmetic = writeSource(
    "deep-parenthesized-arithmetic",
    `echo $((${"(".repeat(257)}1${")".repeat(257)} + 1))\n`,
  );
  const deepArithmeticOutput = parseValidCst(deepArithmetic);
  assertContains(deepArithmeticOutput, "arithmetic_expansion");
  assertNotContains(deepArithmeticOutput, "command_substitution");

  const embeddedCase = writeSource(
    "embedded-case-inside-arithmetic",
    lines("echo $(( $(case x in a) echo 1;; esac) + 1 ))"),
  );
  const embeddedCaseOutput = parseValidCst(embeddedCase);
  assertContains(embeddedCaseOutput, "arithmetic_expansion");
  assertContains(embeddedCaseOutput, "command_substitution");

  const embeddedDocument = writeSource(
    "embedded-document-inside-arithmetic",
    lines("echo $(( $(cat <<E", " x ) y", "E", ") + 1 ))"),
  );
  const embeddedDocumentOutput = parseValidCst(embeddedDocument);
  assertContains(embeddedDocumentOutput, "arithmetic_expansion");
  assertContains(embeddedDocumentOutput, "here_document_body");
});

function createEditHistoryGenerator() {
  let seed = 1n;
  function next(maximum) {
    seed = BigInt.asUintN(64, seed * 6364136223846793005n + 1n);
    return Number(seed >> 32n) % maximum;
  }
  return function* (fragments, insertions, joinSource) {
    for (let iteration = 0; iteration < 100; iteration++) {
      const parts = [];
      const count = 1 + next(3);
      for (let part = 0; part < count; part++) {
        parts.push(fragments[next(fragments.length)]);
      }
      const initial = joinSource(parts);
      let source = Buffer.from(initial);
      const edits = [];
      for (let step = 0; step < 5; step++) {
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
              deleteBytes: Math.min(next(2) + 1, source.length - position),
              insert: "",
            };
        edits.push(edit);
        source = applyEdits(source, [edit]);
        yield {
          initial,
          source,
          edits: [...edits],
          context: `seed 1, iteration ${iteration}, source ${JSON.stringify(initial)}, edits ${JSON.stringify(edits)}`,
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

test("sh: fixed-seed generated histories converge without line continuations", (context) => {
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
    const incremental = runParse({
      description: history.context,
      mode: "recovery",
      source: initial,
      expectedSource: final,
      edits: history.edits,
    });
    if (
      fresh.status === 0 &&
      !hasRecovery(fresh.output) &&
      !history.source.includes("\\\n")
    ) {
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
  assert.equal(checked, 500);
  assert.ok(compared > 0);
  context.diagnostic(
    `sh: checked ${checked} generated edit states, compared ${compared} valid CSTs`,
  );
});

test("sh: every byte inside a Unicode payload can be deleted and repaired through an edit history", () => {
  const prefix = "printf '";
  const suffix = "' after\n";
  const source = "printf 'é😀' after\n";
  const changed = "printf 'x' after\n";
  const payloadByte = 8;
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
      { byte: payloadByte, deleteBytes: 5, insert: "é😀" },
      { byte: payloadByte, deleteBytes: 6, insert: "x" },
      { byte: payloadByte, deleteBytes: 1, insert: "é😀" },
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
        const end = step === 2 ? 9 : 14;
        assertCstDirectChildRange(
          result.output,
          `0:7-0:${end + 1}`,
          "single_quoted",
          `0:8-0:${end}`,
          `single_quote_content \`${step === 2 ? "x" : "é😀"}\``,
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

test("sh: repairing a function header preserves a following backquote assignment", () => {
  const source = writeSource(
    "repaired-function-before-backquote-assignment",
    "\nworker() {\n  :\n}\nresult=`printf nested`\n",
  );
  const fresh = runParse({ source, description: "original function header" });
  const incremental = runParse({
    source,
    description: "repaired function header",
    edits: [
      { byte: 9, deleteBytes: 3, insert: "Wl35" },
      { byte: 9, deleteBytes: 4, insert: " {\n" },
    ],
  });
  assert.equal(
    cstFingerprint(incremental.output),
    cstFingerprint(fresh.output),
  );
  assertOccurrenceCount(incremental.output, "assignment_word", 1);
});
