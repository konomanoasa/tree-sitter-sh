import assert from "node:assert/strict";
import { test } from "node:test";
import {
  assertCstRange,
  assertIncrementalEqualsFresh,
  assertNodeCount,
  assertSameLogicalProjection,
  continuationManifest,
  parseRecoveryAfterEdits,
  parseValidCst,
  writeSource,
} from "./support/parser.js";

const fixtures = [
  {
    name: "command name",
    marked: "pri§ntf value\n",
    expectedNodes: [{ type: "literal", spelling: "printf" }],
  },
  {
    name: "argument",
    marked: "printf foo§bar\n",
    expectedNodes: [{ type: "literal", spelling: "foobar" }],
  },
  {
    name: "Unicode argument",
    marked: "printf é§🙂a§bc\n",
    expectedNodes: [{ type: "literal", spelling: "é🙂abc" }],
  },
  {
    name: "bullet in argument",
    marked: "printf a§•§b\n",
    expectedNodes: [{ type: "literal", spelling: "a•b" }],
  },
  {
    name: "Unicode separators in argument",
    marked: "printf a\u2028§b\u2029§c\n",
    expectedNodes: [{ type: "literal", spelling: "a\u2028b\u2029c" }],
  },
  {
    name: "Unicode separators in double quote text",
    marked: 'printf "a\u2028§b\u2029§c"\n',
    expectedNodes: [{ type: "double_quote_text", spelling: "a\u2028b\u2029c" }],
  },
  {
    name: "assignment",
    marked: "VA§R=va§lue\n",
    expectedNodes: [
      { type: "variable_name", spelling: "VAR" },
      { type: "literal", spelling: "value" },
    ],
  },
  {
    name: "reserved words",
    marked: "i§f :; th§en :; f§i\n",
    expectedNodes: [
      { type: "if_keyword", spelling: "if" },
      { type: "then_keyword", spelling: "then" },
      { type: "fi_keyword", spelling: "fi" },
    ],
  },
  {
    name: "function name",
    marked: "wor§ker() { :; }\n",
    expectedNodes: [{ type: "fname", spelling: "worker" }],
  },
  {
    name: "for name and words",
    marked: "for al§pha i§n o§ne two; d§o :; do§ne\n",
    expectedNodes: [
      { type: "name", spelling: "alpha" },
      { type: "in_keyword", spelling: "in" },
      { type: "literal", spelling: "one" },
    ],
  },
  {
    name: "control operators",
    marked: "a &§& b |§| c\n",
    expectedNodes: [
      { type: "and_if", spelling: "&&" },
      { type: "or_if", spelling: "||" },
    ],
  },
  {
    name: "case terminators",
    marked: "case x in a) : ;§; b) : ;§& esac\n",
    expectedNodes: [
      { type: "dsemi", spelling: ";;" },
      { type: "semi_and", spelling: ";&" },
    ],
  },
  {
    name: "IO number",
    marked: "printf 1§2>out\n",
    expectedNodes: [{ type: "io_number", spelling: "12" }],
  },
  {
    name: "redirection operator",
    marked: "printf >§>out\n",
    expectedNodes: [{ type: "dgreat", spelling: ">>" }],
  },
  {
    name: "redirection filename",
    marked: "printf >out§put\n",
    expectedNodes: [{ type: "literal", spelling: "output" }],
  },
  {
    name: "parameter name and operand",
    marked: ": $§{va§r:-va§lue}\n",
    expectedNodes: [
      { type: "variable_name", spelling: "var" },
      { type: "literal", spelling: "value" },
    ],
  },
  {
    name: "parameter operator",
    marked: `: \${var:§-value}\n`,
    expectedNodes: [{ type: "parameter_value_operator", spelling: ":-" }],
  },
  {
    name: "parameter length of hash",
    marked: `: \${#§#}\n`,
    expectedNodes: [
      { type: "parameter_length_operator", spelling: "#" },
      { type: "special_parameter", spelling: "#", occurrence: 1 },
    ],
  },
  {
    name: "parameter length of options",
    marked: `: \${#§-}\n`,
    expectedNodes: [
      { type: "parameter_length_operator", spelling: "#" },
      { type: "special_parameter", spelling: "-" },
    ],
  },
  {
    name: "parameter length of status",
    marked: `: \${#§?}\n`,
    expectedNodes: [
      { type: "parameter_length_operator", spelling: "#" },
      { type: "special_parameter", spelling: "?" },
    ],
  },
  {
    name: "longest prefix pattern operator",
    marked: `: \${var#§#value}\n`,
    expectedNodes: [{ type: "parameter_pattern_operator", spelling: "##" }],
  },
  {
    name: "unbraced parameter",
    marked: ": $va§r\n",
    expectedNodes: [{ type: "variable_name", spelling: "var" }],
  },
  {
    name: "numeric parameter spelling",
    marked: `: \${0§0}\n`,
    expectedNodes: [],
  },
  {
    name: "positional parameter",
    marked: `: \${0§1}\n`,
    expectedNodes: [{ type: "positional_parameter", spelling: "01" }],
  },
  { name: "command prefix", marked: ": $§(printf value)\n", expectedNodes: [] },
  { name: "arithmetic prefix", marked: ": $§(§(1 + 2)§)\n", expectedNodes: [] },
  {
    name: "arithmetic number",
    marked: ": $((1§23 + 4))\n",
    expectedNodes: [{ type: "arithmetic_number", spelling: "123" }],
  },
  {
    name: "arithmetic assignment operator",
    marked: ": $((value <§<= 1))\n",
    expectedNodes: [{ type: "arithmetic_operator", spelling: "<<=" }],
  },
  {
    name: "double quote text",
    marked: ': "foo§bar"\n',
    expectedNodes: [{ type: "double_quote_text", spelling: "foobar" }],
  },
  { name: "dollar quote prefix", marked: ": $§'text'\n", expectedNodes: [] },
  {
    name: "pattern character class",
    marked: ": [[:al§pha:]]\n",
    expectedNodes: [
      { type: "pattern_character_class_content_source", spelling: "alpha" },
    ],
  },
  {
    name: "tilde user",
    marked: ": ~roo§t/path\n",
    expectedNodes: [{ type: "literal", spelling: "root" }],
  },
  {
    name: "heredoc operator and delimiter",
    marked: "cat <§<§-E§OF\nbody\nEOF\n",
    expectedNodes: [
      { type: "dlessdash", spelling: "<<-" },
      { type: "literal", spelling: "EOF" },
    ],
  },
  {
    name: "heredoc body and closing word",
    marked: "cat <<EOF\nfoo§bar\nEO§F\n",
    expectedNodes: [
      { type: "here_document_text", spelling: "foobar" },
      { type: "here_document_end_text", spelling: "EOF", occurrence: 1 },
    ],
  },
  { name: "word separator", marked: "printf foo §bar\n", expectedNodes: [] },
  {
    name: "closed compound redirect",
    marked: "( : )§2>out\n",
    expectedNodes: [],
  },
];

function point(source, byte) {
  const prefix = Buffer.from(source).subarray(0, byte);
  let row = 0;
  for (const value of prefix) if (value === 10) row += 1;
  return `${row}:${prefix.length - prefix.lastIndexOf(10) - 1}`;
}

function sourceVariants(marked, repeats) {
  const pieces = marked.split("§");
  const offsets = [];
  let logicalBytes = 0;
  for (const piece of pieces.slice(0, -1)) {
    logicalBytes += Buffer.byteLength(piece);
    offsets.push(logicalBytes);
  }
  const logical = pieces.join("");
  const physical = pieces.join("\\\n".repeat(repeats));
  const slashes = offsets.flatMap((offset, index) =>
    Array.from({ length: repeats }, (_, repeat) => {
      const byte = offset + 2 * (index * repeats + repeat);
      return `${point(physical, byte)}-${point(physical, byte + 1)}`;
    }),
  );
  const mappedRange = (start, end) => {
    const mappedStart =
      start + offsets.filter((offset) => offset <= start).length * repeats * 2;
    const mappedEnd =
      end + offsets.filter((offset) => offset < end).length * repeats * 2;
    return `${point(physical, mappedStart)}-${point(physical, mappedEnd)}`;
  };
  return { logical, physical, offsets, slashes, mappedRange };
}

for (const { name, marked, expectedNodes } of fixtures) {
  test(`continued ${name} preserves formal CST and original byte ranges`, () => {
    const logical = writeSource(`${name}-logical`, marked.replaceAll("§", ""));
    const logicalOutput = parseValidCst(logical);
    for (const repeats of [1, 2]) {
      const variant = sourceVariants(marked, repeats);
      const physical = writeSource(
        `${name}-${repeats}-physical`,
        variant.physical,
      );
      const output = parseValidCst(physical);
      assertSameLogicalProjection(name, logicalOutput, output);
      assert.deepEqual(continuationManifest(output), variant.slashes, name);
      if (name === "numeric parameter spelling") {
        assertNodeCount(output, '"numeric_parameter_source"', 2);
        assertNodeCount(output, "positional_parameter", 0);
        for (const digit of [0, 1]) {
          const offset = variant.logical.indexOf("00") + digit;
          assertCstRange(
            output,
            variant.mappedRange(offset, offset + 1),
            '"numeric_parameter_source"',
          );
        }
      }
      for (const { type, spelling, occurrence = 0 } of expectedNodes) {
        let offset = -1;
        for (let index = 0; index <= occurrence; index += 1) {
          offset = variant.logical.indexOf(spelling, offset + 1);
          assert.notEqual(offset, -1, `${name}: expected spelling is absent`);
        }
        const start = Buffer.byteLength(variant.logical.slice(0, offset));
        const end = start + Buffer.byteLength(spelling);
        assertCstRange(output, variant.mappedRange(start, end), type);
      }
      const edits = variant.offsets.map((offset, index) => ({
        byte: offset + index * repeats * 2,
        deleteBytes: 0,
        insert: "\\\n".repeat(repeats),
      }));
      assertIncrementalEqualsFresh(
        logical,
        physical,
        `${name} insert ${repeats}`,
        ...edits,
      );
      assertIncrementalEqualsFresh(
        physical,
        logical,
        `${name} delete ${repeats}`,
        ...variant.offsets.map((offset) => ({
          byte: offset,
          deleteBytes: repeats * 2,
          insert: "",
        })),
      );
    }
  });
}

test("quoting and native comments retain physical pairs that are not continuations", () => {
  for (const { name, source, type, count } of [
    {
      name: "single quote",
      source: ": 'ab\\\ncd'\n",
      type: "single_quoted",
      count: 1,
    },
    {
      name: "dollar single quote",
      source: ": $'ab\\\ncd'\n",
      type: "dollar_single_quoted",
      count: 1,
    },
    {
      name: "quoted heredoc",
      source: "cat <<'END'\nab\\\ncd\nEND\n",
      type: "quoted_here_document_body",
      count: 1,
    },
    {
      name: "native comment",
      source: "#ab\\\nword\n",
      type: "cmd_name",
      count: 1,
    },
  ]) {
    const output = parseValidCst(writeSource(name, source));
    assert.deepEqual(continuationManifest(output), [], name);
    assertNodeCount(output, type, count);
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
      const expected = [`${point(source, slash)}-${point(source, slash + 1)}`];
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

for (const [name, source] of [
  ["command and redirects", "VAR=value printf é🙂 12>>output && ( : )2>file\n"],
  ["bracket ranges", "printf [a-b--] [a-c-e-g] [a-[:alpha:]] [a-[.x.]]\n"],
  ["case and function", "worker() { case ab in a|[[:alpha:]]) :;; esac; }\n"],
  ["nested expansions", `: "ab\${value:-cd}$(printf ef)$((name <<= 2))gh"\n`],
  ["arithmetic precedence", ": $((value = 0x12 + 3 * 4 < 9 ? 5 : 6))\n"],
  [
    "arithmetic source fragments",
    `: $((0x\${hex} + \${prefix}08 + \${name}123abc))\n`,
  ],
  ["backquote command", "echo `printf ab`\n"],
  ["heredoc lines", `cat <<-END\n\tab\${value}cd\n\tEND\n`],
]) {
  test(`continuations at every ${name} character boundary preserve logical syntax`, () => {
    const logical = writeSource(`${name}-all-boundaries-logical`, source);
    const expected = parseValidCst(logical);
    const characters = Array.from(source);
    let byte = 0;
    for (let index = 0; index <= characters.length; index += 1) {
      const physical =
        characters.slice(0, index).join("") +
        "\\\n" +
        characters.slice(index).join("");
      const description = `${name} continuation at byte ${byte}`;
      const path = writeSource(`${name}-boundary-${index}`, physical);
      const actual = parseValidCst(path, description);
      assertSameLogicalProjection(description, expected, actual);
      assert.deepEqual(continuationManifest(actual), [
        `${point(physical, byte)}-${point(physical, byte + 1)}`,
      ]);
      assertIncrementalEqualsFresh(logical, path, description, {
        byte,
        deleteBytes: 0,
        insert: "\\\n",
      });
      if (index < characters.length)
        byte += Buffer.byteLength(characters[index]);
    }
  });
}
