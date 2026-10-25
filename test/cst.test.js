import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { test } from "node:test";
import nodeTypes from "../src/node-types.json" with { type: "json" };
import {
  assertCstDirectChildRange,
  assertCstRange,
  assertCstSourceContract,
  assertNodeCount,
  assertNotContains,
  assertSameLogicalProjection,
  continuationManifest,
  cstFingerprint,
  logicalProjection,
  namedStructure,
  parseCst,
  sourcePoint,
} from "./support/cst.js";
import {
  assertIncrementalEqualsFresh,
  assertLineContinuationManifest,
  assertRepeatedColdParse,
  assertValid,
  lines,
  parseRecoveryAfterEdits,
  parseValidCst,
  runParse,
  writeSource,
} from "./support/parser.js";

const directory = path.resolve(import.meta.dirname, "corpus");
const header = "==================\n";
const separator = "\n------------------\n";

function corpusCases() {
  return fs
    .readdirSync(directory)
    .sort()
    .flatMap((file) => {
      const parts = fs
        .readFileSync(path.join(directory, file), "utf8")
        .split(header);
      assert.equal(parts.shift(), "", file);
      assert.equal(parts.length % 2, 0, file);
      const cases = [];
      for (let index = 0; index < parts.length; index += 2) {
        const [name, ...attributes] = parts[index].trimEnd().split("\n");
        assert.ok(
          attributes.every(
            (attribute) => attribute === ":cst" || attribute === ":error",
          ),
          name,
        );
        const end = parts[index + 1].indexOf(separator);
        assert.notEqual(end, -1, name);
        cases.push({
          name: `${file}: ${name}`,
          source: parts[index + 1].slice(0, end),
          mode: attributes.includes(":error") ? "recovery" : "valid",
        });
      }
      return cases;
    });
}

const continuation = "line_continuation";

for (const { name, source, mode } of corpusCases()) {
  test(`sh: ${name} preserves source and repeats deterministically`, () => {
    assertRepeatedColdParse(mode, writeSource(name, source), name);
  });
}

test("CST projection preserves punctuation, fields, hierarchy and cardinality", () => {
  const source = [
    "0:0 - 0:3   program",
    "0:0 - 0:3     body: list",
    "0:0 - 0:1       word `a`",
    '0:1 - 0:2       ";"',
    "0:2 - 0:3       word `b`",
  ].join("\n");
  assert.equal(
    namedStructure(source, true),
    "(program body: (list (word) (word)))",
  );
  assert.equal(namedStructure(source, false), "(program (list (word) (word)))");
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

test("CST fingerprints preserve individual continuation ranges and order without ownership", () => {
  const root = "0:0 - 2:3   program";
  const word = "0:0 - 2:3     word";
  const first = `0:3 - 0:4       ${continuation}`;
  const second = `1:0 - 1:1       ${continuation}`;
  const source = [root, word, first, second].join("\n");
  const expected = cstFingerprint(source);
  assert.equal(namedStructure(source, true), "(program (word))");
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
    `0:3 - 0:4       ${continuation}`,
  ].join("\n");
  assertCstSourceContract(output, source);
  for (const changed of [
    output.replace("0:3 - 0:4", "0:3 - 1:0"),
    `0:2 - 0:3     "'"`,
    `${output}\n0:3 - 0:4       ${continuation}`,
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
      `0:6 - 0:7       ${continuation}`,
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

test("sh: public fields retain optional roots and singular source owners", () => {
  for (const { type, field, required, types } of [
    {
      type: "program",
      field: "commands",
      required: false,
      types: ["complete_commands"],
    },
    {
      type: "command",
      field: "body",
      required: true,
      types: ["compound_command", "function_definition", "simple_command"],
    },
    {
      type: "command",
      field: "redirects",
      required: false,
      types: ["redirect_list"],
    },
    {
      type: "function_body",
      field: "body",
      required: true,
      types: ["compound_command"],
    },
    {
      type: "function_body",
      field: "redirects",
      required: false,
      types: ["redirect_list"],
    },
    { type: "here_end", field: "word", required: true, types: ["word"] },
    {
      type: "here_document",
      field: "body",
      required: false,
      types: ["here_document_body", "quoted_here_document_body"],
    },
    {
      type: "here_document",
      field: "end",
      required: true,
      types: ["here_document_end"],
    },
    {
      type: "parameter_expansion",
      field: "parameter",
      required: false,
      types: ["positional_parameter", "special_parameter", "variable_name"],
    },
  ]) {
    const node = nodeTypes.find((node) => node.named && node.type === type);
    assert.ok(node, type);
    assert.ok(node.fields, type);
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

test("sh: pattern bracket ranges require one start and one end", () => {
  const range = nodeTypes.find(
    ({ type, named }) => named && type === "pattern_bracket_range_source",
  );
  assert.ok(range);
  assert.ok(range.fields);
  for (const field of ["start", "end"]) {
    assert.equal(range.fields[field].required, true, field);
    assert.equal(range.fields[field].multiple, false, field);
  }
});

test("sh: CST helpers preserve hierarchy across coordinate widths", () => {
  const caseContents = "case x in a) :;; b) :;& c) :; esac\n";
  for (const [name, contents, layout, rootRange, commandsRange] of [
    ["two-digit-row", caseContents, " \\\n", "0:0-10:0", "0:0-9:4"],
    [
      "three-digit-row",
      caseContents,
      " \\\n".repeat(11),
      "0:0-100:0",
      "0:0-99:4",
    ],
    [
      "mixed-coordinate-widths",
      "x=1 y=2 : z > out 2> err\n",
      " \\\n\t\\\n ",
      "0:0-15:0",
      "0:0-14:4",
    ],
  ]) {
    const logical = parseValidCst(writeSource(`${name}-logical`, contents));
    const physical = parseValidCst(
      writeSource(name, contents.replaceAll(" ", layout)),
    );
    assertSameLogicalProjection(name, logical, physical);
    assertCstDirectChildRange(
      physical,
      rootRange,
      "program",
      commandsRange,
      "commands: complete_commands",
    );
    assert.throws(
      () =>
        assertCstDirectChildRange(
          physical,
          rootRange,
          "program",
          commandsRange,
          "command: complete_command",
        ),
      /directly under program/,
    );
  }
});

test("sh: only the initial BOM is omitted and original byte ranges survive edits", () => {
  const initial = writeSource("without-bom", "printf é\n");
  for (const [name, prefix, range, spelling] of [
    ["initial", "\ufeff", "0:3-0:9", "printf"],
    ["second", "\ufeff\ufeff", "0:3-0:12", "\ufeffprintf"],
    ["after-continuation", "\\\n\ufeff", "1:0-1:9", "\ufeffprintf"],
    ["before-continuation", "\ufeff\\\n", "1:0-1:6", "printf"],
  ]) {
    const source = writeSource(`${name}-bom`, `${prefix}printf é\n`);
    for (const output of assertIncrementalEqualsFresh(initial, source, name, {
      byte: 0,
      deleteBytes: 0,
      insert: prefix,
    })) {
      assertCstRange(output, range, `literal \`${spelling}\``);
    }
    assertIncrementalEqualsFresh(source, initial, `${name} remove`, {
      byte: 0,
      deleteBytes: Buffer.byteLength(prefix),
      insert: "",
    });
  }
});

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
      return `${sourcePoint(physical, byte)}-${sourcePoint(physical, byte + 1)}`;
    }),
  );
  const mappedRange = (start, end) => {
    const mappedStart =
      start + offsets.filter((offset) => offset <= start).length * repeats * 2;
    const mappedEnd =
      end + offsets.filter((offset) => offset < end).length * repeats * 2;
    return `${sourcePoint(physical, mappedStart)}-${sourcePoint(physical, mappedEnd)}`;
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
        `${sourcePoint(physical, byte)}-${sourcePoint(physical, byte + 1)}`,
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
    ["0:16-0:17", "2:14-2:15"],
  );

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
  assertCstRange(terminalPhysicalOutput, "0:2-0:3", "line_continuation");
  assert.equal(continuationManifest(terminalPhysicalOutput).length, 1);
  assertSameLogicalProjection(
    "terminal-assignment-continuation",
    terminalLogicalOutput,
    terminalPhysicalOutput,
  );

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
  const continuedCommentsOutput = parseValidCst(continuedComments);
  for (const [range, type] of [
    ["0:0-0:1", "line_continuation"],
    ["1:0-1:9", "comment"],
    ["2:9-2:10", "line_continuation"],
    ["3:0-3:10", "comment"],
    ["5:10-5:11", "line_continuation"],
    ["6:0-6:13", "comment: comment"],
  ]) {
    assertCstRange(continuedCommentsOutput, range, type);
  }
  assertNotContains(continuedCommentsOutput, "ERROR");

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

test("sh: parser rejects a timeout as structural recovery", () => {
  const commandCount = 10_000;
  const forcedTimeoutMicros = 1;
  const source = writeSource("timeout-guard", ":\n".repeat(commandCount));
  assert.throws(() =>
    runParse({
      description: "timeout guard",
      mode: "recovery",
      source,
      timeoutMicros: forcedTimeoutMicros,
    }),
  );
});

for (const { name, source, expected, continuations } of [
  {
    name: "a continuation before the closers stays in the command-substitution reading",
    source: '\n: "$((1 +\\\n))"\n',
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (double_quoted (command_substitution
      (command_substitution_body (complete_commands (complete_command (list (and_or
      (pipeline (pipe_sequence (command (compound_command (subshell (compound_list
      (term (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (literal))))))))))))))))))))))))))))))))
      (linebreak (newline_list)))`,
    continuations: ["1:9-1:10"],
  },
  {
    name: "assignment-only commands retain command boundaries",
    source: "\nA=one; B=two\nC=three && D=four\nE=five\\\n\nnext\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command prefix:
      (cmd_prefix assignment: (assignment_word name: (variable_name) value:
      (assignment_value (literal))))))))) separator: (separator_op) and_or: (and_or
      pipeline: (pipeline sequence: (pipe_sequence command: (command body:
      (simple_command prefix: (cmd_prefix assignment: (assignment_word name:
      (variable_name) value: (assignment_value (literal))))))))))) separator:
      (newline_list) command: (complete_command body: (list and_or: (and_or
      pipeline: (pipeline sequence: (pipe_sequence command: (command body:
      (simple_command prefix: (cmd_prefix assignment: (assignment_word name:
      (variable_name) value: (assignment_value (literal)))))))) operator: (and_if)
      pipeline: (pipeline sequence: (pipe_sequence command: (command body:
      (simple_command prefix: (cmd_prefix assignment: (assignment_word name:
      (variable_name) value: (assignment_value (literal))))))))))) separator:
      (newline_list) command: (complete_command body: (list and_or: (and_or
      pipeline: (pipeline sequence: (pipe_sequence command: (command body:
      (simple_command prefix: (cmd_prefix assignment: (assignment_word name:
      (variable_name) value: (assignment_value (literal))))))))))) separator:
      (newline_list) command: (complete_command body: (list and_or: (and_or
      pipeline: (pipeline sequence: (pipe_sequence command: (command body:
      (simple_command name: (cmd_name (word (literal))))))))))) trailing:
      (linebreak (newline_list)))`,
    continuations: ["3:6-3:7"],
  },
  {
    name: "a blank-led continuation run after an assignment reaches the end of input",
    source: "\na=b \\\n\n\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command prefix:
      (cmd_prefix assignment: (assignment_word name: (variable_name) value:
      (assignment_value (literal)))))))))))) trailing: (linebreak (newline_list)))`,
    continuations: ["1:4-1:5"],
  },
  {
    name: "a removed newline before a backquote command preserves its body",
    source: "\nprintf `\\\ncommand`\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))) suffix: (cmd_suffix word: (word
      (backquote_substitution body: (backquote_substitution_body (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))))))))))))))))))))))) trailing: (linebreak
      (newline_list)))`,
    continuations: ["1:8-1:9"],
  },
  {
    name: "backquote substitution closers retain boundary escaped newlines",
    source: "\necho `printf x\\\n`\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (backquote_substitution
      (backquote_substitution_body (complete_commands (complete_command (list
      (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))) (cmd_suffix (word (literal))))))))))))))))))))))) (linebreak
      (newline_list)))`,
    continuations: ["1:14-1:15"],
  },
  {
    name: "a word continuation ends before a nested backquote closer",
    source: "\n: `: \\`x\\\n\\``\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (backquote_substitution
      (backquote_substitution_body (complete_commands (complete_command (list
      (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))) (cmd_suffix (word (backquote_substitution
      (backquote_substitution_body (complete_commands (complete_command (list
      (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))))))))))))))))))))))))))))))))))) (linebreak (newline_list)))`,
    continuations: ["1:8-1:9"],
  },
  {
    name: "encoded backquote continuations preserve command layout",
    source: "\n: `: \\\\\n \n`\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (backquote_substitution
      (backquote_substitution_body (complete_commands (complete_command (list
      (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))))))))))) (linebreak (newline_list)))))))))))))) (linebreak
      (newline_list)))`,
    continuations: ["1:6-1:7"],
  },
  {
    name: "encoded continuations preserve I/O numbers before redirection operators",
    source: "\n: `: 2\\\\\n>out`\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (backquote_substitution
      (backquote_substitution_body (complete_commands (complete_command (list
      (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))) (cmd_suffix (io_redirect (io_number) (io_file (filename (word
      (literal)))))))))))))))))))))))))) (linebreak (newline_list)))`,
    continuations: ["1:7-1:8"],
  },
  {
    name: "quoted ancestors remove continuations at inner command boundaries",
    source: '\n: "`: \\`: \\\\\\\\\\\\\\\n :\\``"\n',
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (double_quoted (backquote_substitution
      (backquote_substitution_body (complete_commands (complete_command (list
      (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))) (cmd_suffix (word (backquote_substitution
      (backquote_substitution_body (complete_commands (complete_command (list
      (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))) (cmd_suffix (word (escaped_character)) (word
      (literal)))))))))))))))))))))))))))))))))))) (linebreak (newline_list)))`,
    continuations: ["1:16-1:17"],
  },
  {
    name: "continuations before case in preserve the selector",
    source: "\ncase x\\\n\\\n in x) :;; esac\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (compound_command
      (case_clause (case_keyword) (word (literal)) (in (in_keyword)) (case_list
      (case_item (pattern_list (word (literal))) (compound_list (term (and_or
      (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal)))))))))) (dsemi))) (esac_keyword)))))))))) (linebreak
      (newline_list)))`,
    continuations: ["1:6-1:7", "2:0-2:1"],
  },
  {
    name: "case continuations preserve logical separators and inter-item layout",
    source:
      "\ncase x in x)echo \\\ny;;esac\ncase\\\n x in esac\ncase x in x):;;\\\n y):;& z):;;esac\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (compound_command
      (case_clause (case_keyword) (word (literal)) (in (in_keyword)) (case_list
      (case_item (pattern_list (word (literal))) (compound_list (term (and_or
      (pipeline (pipe_sequence (command (simple_command (cmd_name (word (literal)))
      (cmd_suffix (word (literal)))))))))) (dsemi))) (esac_keyword)))))))))
      (newline_list) (complete_command (list (and_or (pipeline (pipe_sequence
      (command (compound_command (case_clause (case_keyword) (word (literal)) (in
      (in_keyword)) (esac_keyword))))))))) (newline_list) (complete_command (list
      (and_or (pipeline (pipe_sequence (command (compound_command (case_clause
      (case_keyword) (word (literal)) (in (in_keyword)) (case_list (case_item
      (pattern_list (word (literal))) (compound_list (term (and_or (pipeline
      (pipe_sequence (command (simple_command (cmd_name (word (literal))))))))))
      (dsemi)) (case_item (pattern_list (word (literal))) (compound_list (term
      (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal)))))))))) (semi_and)) (case_item (pattern_list (word (literal)))
      (compound_list (term (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal)))))))))) (dsemi)))
      (esac_keyword)))))))))) (linebreak (newline_list)))`,
    continuations: ["1:17-1:18", "3:4-3:5", "5:15-5:16"],
  },
  {
    name: "case reserved-word separators retain alternating continuations",
    source: "\ncase value \\\n \\\n in x) :;; esac\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (compound_command
      (case_clause (case_keyword) word: (word (literal)) in: (in (in_keyword))
      items: (case_list item: (case_item patterns: (pattern_list word: (word
      (literal))) body: (compound_list body: (term and_or: (and_or pipeline:
      (pipeline sequence: (pipe_sequence command: (command body: (simple_command
      name: (cmd_name (word (literal)))))))))) terminator: (dsemi)))
      (esac_keyword)))))))))) trailing: (linebreak (newline_list)))`,
    continuations: ["1:11-1:12", "2:1-2:2"],
  },
  {
    name: "blank-leading case continuations preserve the selector",
    source: "\ncase x \\\n\\\nin x) :;; esac\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (compound_command
      (case_clause (case_keyword) (word (literal)) (in (in_keyword)) (case_list
      (case_item (pattern_list (word (literal))) (compound_list (term (and_or
      (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal)))))))))) (dsemi))) (esac_keyword)))))))))) (linebreak
      (newline_list)))`,
    continuations: ["1:7-1:8", "2:0-2:1"],
  },
  {
    name: "continuations after an empty item precede esac",
    source: "\ncase x in a)\\\n\\\nesac\ncase y in b)\\\n;; esac\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (compound_command
      (case_clause (case_keyword) (word (literal)) (in (in_keyword)) (case_list_ns
      (case_item_ns (pattern_list (word (literal))))) (esac_keyword)))))))))
      (newline_list) (complete_command (list (and_or (pipeline (pipe_sequence
      (command (compound_command (case_clause (case_keyword) (word (literal)) (in
      (in_keyword)) (case_list (case_item (pattern_list (word (literal))) (dsemi)))
      (esac_keyword)))))))))) (linebreak (newline_list)))`,
    continuations: ["1:12-1:13", "2:0-2:1", "4:12-4:13"],
  },
  {
    name: "case pattern alternatives retain boundary escaped newlines",
    source: "\ncase value in\n  first\\\n  | second) : ;;\nesac\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (compound_command
      (case_clause (case_keyword) word: (word (literal)) in: (in (in_keyword))
      (linebreak (newline_list)) items: (case_list item: (case_item patterns:
      (pattern_list word: (word (literal)) word: (word (literal))) body:
      (compound_list body: (term and_or: (and_or pipeline: (pipeline sequence:
      (pipe_sequence command: (command body: (simple_command name: (cmd_name (word
      (literal)))))))))) terminator: (dsemi) (linebreak (newline_list))))
      (esac_keyword)))))))))) trailing: (linebreak (newline_list)))`,
    continuations: ["2:7-2:8"],
  },
  {
    name: "case patterns preserve connector and closing-parenthesis continuations",
    source: "\ncase x in\n  a  \\\n \\\n| b  \\\n \\\n) :;;\nesac\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (compound_command
      (case_clause (case_keyword) word: (word (literal)) in: (in (in_keyword))
      (linebreak (newline_list)) items: (case_list item: (case_item patterns:
      (pattern_list word: (word (literal)) word: (word (literal))) body:
      (compound_list body: (term and_or: (and_or pipeline: (pipeline sequence:
      (pipe_sequence command: (command body: (simple_command name: (cmd_name (word
      (literal)))))))))) terminator: (dsemi) (linebreak (newline_list))))
      (esac_keyword)))))))))) trailing: (linebreak (newline_list)))`,
    continuations: ["2:5-2:6", "3:1-3:2", "4:5-4:6", "5:1-5:2"],
  },
  {
    name: "continuations between a pattern and its closing parenthesis stay layout",
    source: "\ncase x in a\\\n) :;; (b\\\n \\\n) :;; c\\\n|d\\\n) :;; esac\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (compound_command
      (case_clause (case_keyword) (word (literal)) (in (in_keyword)) (case_list
      (case_item (pattern_list (word (literal))) (compound_list (term (and_or
      (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal)))))))))) (dsemi)) (case_item (pattern_list (word (literal)))
      (compound_list (term (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal)))))))))) (dsemi)) (case_item
      (pattern_list (word (literal)) (word (literal))) (compound_list (term (and_or
      (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal)))))))))) (dsemi))) (esac_keyword)))))))))) (linebreak
      (newline_list)))`,
    continuations: ["1:11-1:12", "2:8-2:9", "3:1-3:2", "4:7-4:8", "5:2-5:3"],
  },
  {
    name: "dollar command substitution closers retain boundary escaped newlines",
    source: "\necho $(printf x\\\n)\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (command_substitution
      (command_substitution_body (complete_commands (complete_command (list (and_or
      (pipeline (pipe_sequence (command (simple_command (cmd_name (word (literal)))
      (cmd_suffix (word (literal))))))))))))))))))))))) (linebreak (newline_list)))`,
    continuations: ["1:15-1:16"],
  },
  {
    name: "a continuation before a substitution closer stays layout",
    source: "\nx=$( a\\\n)\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_prefix
      (assignment_word (variable_name) (assignment_value (command_substitution
      (command_substitution_body (complete_commands (complete_command (list (and_or
      (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal)))))))))))))))))))))))) (linebreak (newline_list)))`,
    continuations: ["1:6-1:7"],
  },
  {
    name: "comments can follow a continued connecting operator",
    source: "\nfirst &&\\\n# continued comment\nsecond\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))))))) (and_if) (linebreak (newline_list (comment
      (comment_text)))) (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))))))))))) (linebreak (newline_list)))`,
    continuations: ["1:8-1:9"],
  },
  {
    name: "a hash after a removed newline starts a comment at a token boundary",
    source: "\nprintf value \\\n# continued comment\nafter\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (literal)))))))))) (newline_list (comment
      (comment_text))) (complete_command (list (and_or (pipeline (pipe_sequence
      (command (simple_command (cmd_name (word (literal))))))))))) (linebreak
      (newline_list)))`,
    continuations: ["1:13-1:14"],
  },
  {
    name: "line continuation exposes a comment at each command-start boundary",
    source:
      "\n\\\n# program start\nfirst;\\\n# after semicolon\nsecond &\\\n# after ampersand\n(\n  \\\n# after left parenthesis\n  inside\n)\n",
    expected: `(program (linebreak (newline_list (comment (comment_text))))
      (complete_commands (complete_command (list (and_or (pipeline (pipe_sequence
      (command (simple_command (cmd_name (word (literal))))))))) (separator_op))
      (newline_list (comment (comment_text))) (complete_command (list (and_or
      (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))))))))) (separator_op)) (newline_list (comment (comment_text)))
      (complete_command (list (and_or (pipeline (pipe_sequence (command
      (compound_command (subshell (compound_list (linebreak (newline_list (comment
      (comment_text)))) (term (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))))))))) (separator
      (newline_list)))))))))))) (linebreak (newline_list)))`,
    continuations: ["1:0-1:1", "3:6-3:7", "5:8-5:9", "8:2-8:3"],
  },
  {
    name: "continuation runs before comment lines stay layout",
    source:
      "\n\\\n\\\n# leading\necho;\\\n# after a separator\necho &\\\n\\\n# after an ampersand\necho\n\\\n\\\n# after a newline\n",
    expected: `(program (linebreak (newline_list (comment (comment_text))))
      (complete_commands (complete_command (list (and_or (pipeline (pipe_sequence
      (command (simple_command (cmd_name (word (literal))))))))) (separator_op))
      (newline_list (comment (comment_text))) (complete_command (list (and_or
      (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))))))))) (separator_op)) (newline_list (comment (comment_text)))
      (complete_command (list (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))))))))))) (linebreak (newline_list
      (comment (comment_text)))))`,
    continuations: [
      "1:0-1:1",
      "2:0-2:1",
      "4:5-4:6",
      "6:6-6:7",
      "7:0-7:1",
      "10:0-10:1",
      "11:0-11:1",
    ],
  },
  {
    name: "an EOF comment can follow a removed newline",
    source: "\nprintf value \\\n# comment at EOF",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (literal))))))))))) (comment
      (comment_text)))`,
    continuations: ["1:13-1:14"],
  },
  {
    name: "here-document read-time continuations precede inner comment recognition",
    source: "\ncat <<OUT\n$( : # hello\\\n) ; <<IN ignored\n)\nOUT\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (io_redirect (io_here (dless) (here_end (word
      (literal)))))))))))))) (linebreak (newline_list (here_document_sequence
      (here_document_line_end) (here_document (here_document_body
      (command_substitution (command_substitution_body (complete_commands
      (complete_command (list (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))))))))))) (linebreak (newline_list
      (comment (comment_text))))))) (here_document_end
      (here_document_end_text)))))))`,
    continuations: ["2:12-2:13"],
  },
  {
    name: "quoted backquotes expose continued comments as text and physical pairs",
    source: '\n: "`: \\`: # hello\\\\\\\nworld\n\\``"\n',
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (double_quoted (backquote_substitution
      (backquote_substitution_body (complete_commands (complete_command (list
      (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))) (cmd_suffix (word (backquote_substitution
      (backquote_substitution_body (complete_commands (complete_command (list
      (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))))))))))) (linebreak (newline_list (comment
      (comment_text))))))))))))))))))))))))))))) (linebreak (newline_list)))`,
    continuations: ["1:19-1:20"],
  },
  {
    name: "retained comment prefixes preserve the next logical newline",
    source: '\n: "`: \\"\\`#x\\\\\\\n\\\\\nworld\n\\`\\"`"\n',
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (double_quoted (backquote_substitution
      (backquote_substitution_body (complete_commands (complete_command (list
      (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))) (cmd_suffix (word (double_quoted (backquote_substitution
      (backquote_substitution_body (linebreak (newline_list (comment
      (comment_text)))) (complete_commands (complete_command (list (and_or
      (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))))))))))) (linebreak (newline_list))))))))))))))))))))))))))))
      (linebreak (newline_list)))`,
    continuations: ["1:14-1:15"],
  },
  {
    name: "retained comment prefixes remove the next physical pair",
    source: '\n: "`: \\"\\`#x\\\\\\\n\\\\\\\\\nworld\n\\`\\"`"\n',
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (double_quoted (backquote_substitution
      (backquote_substitution_body (complete_commands (complete_command (list
      (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))) (cmd_suffix (word (double_quoted (backquote_substitution
      (backquote_substitution_body (linebreak (newline_list (comment
      (comment_text)))))))))))))))))))))))))))))) (linebreak (newline_list)))`,
    continuations: ["1:14-1:15", "2:3-2:4"],
  },
  {
    name: "retained comment prefixes can remove a bare source newline",
    source: '\n: "`: \\"\\`#x\\\\\\\n\nworld\n\\`\\"`"\n',
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (double_quoted (backquote_substitution
      (backquote_substitution_body (complete_commands (complete_command (list
      (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))) (cmd_suffix (word (double_quoted (backquote_substitution
      (backquote_substitution_body (linebreak (newline_list (comment
      (comment_text)))))))))))))))))))))))))))))) (linebreak (newline_list)))`,
    continuations: ["1:14-1:15"],
  },
  {
    name: "compound-command closers preserve intervening continuations",
    source: "\nif { :; }  \\\n \\\nthen :; fi\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (compound_command (if_clause
      (if_keyword) condition: (compound_list body: (term and_or: (and_or pipeline:
      (pipeline sequence: (pipe_sequence command: (command body: (compound_command
      (brace_group body: (compound_list body: (term and_or: (and_or pipeline:
      (pipeline sequence: (pipe_sequence command: (command body: (simple_command
      name: (cmd_name (word (literal))))))))) terminator: (separator operator:
      (separator_op))))))))))) (then_keyword) consequence: (compound_list body:
      (term and_or: (and_or pipeline: (pipeline sequence: (pipe_sequence command:
      (command body: (simple_command name: (cmd_name (word (literal)))))))))
      terminator: (separator operator: (separator_op))) (fi_keyword))))))))))
      trailing: (linebreak (newline_list)))`,
    continuations: ["1:11-1:12", "2:1-2:2"],
  },
  {
    name: "a continued closing layout after an elif consequence reaches the next branch",
    source:
      "\nif a; then b; elif c; then d; \\\n else e; fi\nif a; then b; elif c; then d & \\\n elif e; then f; \\\n fi\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (compound_command (if_clause
      (if_keyword) (compound_list (term (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))))))))) (separator (separator_op)))
      (then_keyword) (compound_list (term (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))))))))) (separator (separator_op)))
      (else_part (elif_keyword) (compound_list (term (and_or (pipeline
      (pipe_sequence (command (simple_command (cmd_name (word (literal)))))))))
      (separator (separator_op))) (then_keyword) (compound_list (term (and_or
      (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))))))))) (separator (separator_op))) (else_part (else_keyword)
      (compound_list (term (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))))))))) (separator
      (separator_op))))) (fi_keyword))))))))) (newline_list) (complete_command
      (list (and_or (pipeline (pipe_sequence (command (compound_command (if_clause
      (if_keyword) (compound_list (term (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))))))))) (separator (separator_op)))
      (then_keyword) (compound_list (term (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))))))))) (separator (separator_op)))
      (else_part (elif_keyword) (compound_list (term (and_or (pipeline
      (pipe_sequence (command (simple_command (cmd_name (word (literal)))))))))
      (separator (separator_op))) (then_keyword) (compound_list (term (and_or
      (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))))))))) (separator (separator_op))) (else_part (elif_keyword)
      (compound_list (term (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))))))))) (separator (separator_op)))
      (then_keyword) (compound_list (term (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))))))))) (separator
      (separator_op))))) (fi_keyword)))))))))) (linebreak (newline_list)))`,
    continuations: ["1:30-1:31", "3:31-3:32", "4:17-4:18"],
  },
  {
    name: "a continuation before a blank line keeps a consequence closer reachable",
    source: "\nif true; then\n  echo hi \\\n\nfi\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (compound_command (if_clause
      (if_keyword) condition: (compound_list body: (term and_or: (and_or pipeline:
      (pipeline sequence: (pipe_sequence command: (command body: (simple_command
      name: (cmd_name (word (literal))))))))) terminator: (separator operator:
      (separator_op))) (then_keyword) consequence: (compound_list leading:
      (linebreak (newline_list)) body: (term and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))) suffix: (cmd_suffix word: (word (literal)))))))))
      terminator: (separator newlines: (newline_list))) (fi_keyword))))))))))
      trailing: (linebreak (newline_list)))`,
    continuations: ["2:10-2:11"],
  },
  {
    name: "a continuation between a function header and body remains layout",
    source: "\nlong_function()\\\n{\n  :\n}\nlong_function\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (function_definition name:
      (fname) body: (function_body body: (compound_command (brace_group body:
      (compound_list leading: (linebreak (newline_list)) body: (term and_or:
      (and_or pipeline: (pipeline sequence: (pipe_sequence command: (command body:
      (simple_command name: (cmd_name (word (literal))))))))) terminator:
      (separator newlines: (newline_list))))))))))))) separator: (newline_list)
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))))))))))) trailing: (linebreak (newline_list)))`,
    continuations: ["1:15-1:16"],
  },
  {
    name: "brace compound lists preserve continued operators and final separators",
    source: "\nholder() {\n  first && \\\nsecond\n  third | \\\n  fourth;\n}\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (function_definition name:
      (fname) body: (function_body body: (compound_command (brace_group body:
      (compound_list leading: (linebreak (newline_list)) body: (term and_or:
      (and_or pipeline: (pipeline sequence: (pipe_sequence command: (command body:
      (simple_command name: (cmd_name (word (literal))))))) operator: (and_if)
      pipeline: (pipeline sequence: (pipe_sequence command: (command body:
      (simple_command name: (cmd_name (word (literal)))))))) separator: (separator
      newlines: (newline_list)) and_or: (and_or pipeline: (pipeline sequence:
      (pipe_sequence command: (command body: (simple_command name: (cmd_name (word
      (literal))))) command: (command body: (simple_command name: (cmd_name (word
      (literal))))))))) terminator: (separator operator: (separator_op) linebreak:
      (linebreak (newline_list))))))))))))))) trailing: (linebreak (newline_list)))`,
    continuations: ["2:11-2:12", "4:10-4:11"],
  },
  {
    name: "group closers follow continuation-led layout lines",
    source: "\n{ a\n\\\n}\n(b\n\\\n)\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (compound_command
      (brace_group body: (compound_list body: (term and_or: (and_or pipeline:
      (pipeline sequence: (pipe_sequence command: (command body: (simple_command
      name: (cmd_name (word (literal))))))))) terminator: (separator newlines:
      (newline_list))))))))))) separator: (newline_list) command: (complete_command
      body: (list and_or: (and_or pipeline: (pipeline sequence: (pipe_sequence
      command: (command body: (compound_command (subshell body: (compound_list
      body: (term and_or: (and_or pipeline: (pipeline sequence: (pipe_sequence
      command: (command body: (simple_command name: (cmd_name (word
      (literal))))))))) terminator: (separator newlines: (newline_list))))))))))))
      trailing: (linebreak (newline_list)))`,
    continuations: ["2:0-2:1", "5:0-5:1"],
  },
  {
    name: "only unquoted bodies remove continuations before delimiter lines",
    source: "\ncat <<EOF\n\\\nEOF\ncat <<'END'\nEN\\\nD\nEND\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))) suffix: (cmd_suffix redirect: (io_redirect body:
      (io_here operator: (dless) end: (here_end word: (word (literal)))))))))))))
      separator: (newline_list (here_document_sequence line_end:
      (here_document_line_end) document: (here_document end: (here_document_end
      (here_document_end_text))))) command: (complete_command body: (list and_or:
      (and_or pipeline: (pipeline sequence: (pipe_sequence command: (command body:
      (simple_command name: (cmd_name (word (literal))) suffix: (cmd_suffix
      redirect: (io_redirect body: (io_here operator: (dless) end: (here_end word:
      (word (single_quoted (single_quote_content))))))))))))))) trailing:
      (linebreak (newline_list (here_document_sequence line_end:
      (here_document_line_end) document: (here_document body:
      (quoted_here_document_body (quoted_here_document_text)
      (quoted_here_document_text)) end: (here_document_end
      (here_document_end_text)))))))`,
    continuations: ["2:0-2:1"],
  },
  {
    name: "tab stripping preserves a nonleading tab after a continued line",
    source: "\ncat <<-AB\nA\\\n\tB\nAB\nafter\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))) suffix: (cmd_suffix redirect: (io_redirect body:
      (io_here operator: (dlessdash) end: (here_end word: (word
      (literal))))))))))))) separator: (newline_list (here_document_sequence
      line_end: (here_document_line_end) document: (here_document body:
      (here_document_body (here_document_text)) end: (here_document_end
      (here_document_end_text))))) command: (complete_command body: (list and_or:
      (and_or pipeline: (pipeline sequence: (pipe_sequence command: (command body:
      (simple_command name: (cmd_name (word (literal))))))))))) trailing:
      (linebreak (newline_list)))`,
    continuations: ["2:1-2:2"],
  },
  {
    name: "ancestor continuation removal precedes nested backquote body escape classification",
    source: "\ncat <<OUT\n`cat <<IN\n\\\\\\\n \nIN\n`\nOUT\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (io_redirect (io_here (dless) (here_end (word
      (literal)))))))))))))) (linebreak (newline_list (here_document_sequence
      (here_document_line_end) (here_document (here_document_body
      (backquote_substitution (backquote_substitution_body (complete_commands
      (complete_command (list (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))) (cmd_suffix (io_redirect (io_here
      (dless) (here_end (word (literal)))))))))))))) (linebreak (newline_list
      (here_document_sequence (here_document_line_end) (here_document
      (here_document_body (here_document_text)) (here_document_end
      (here_document_end_text))))))))) (here_document_end
      (here_document_end_text)))))))`,
    continuations: ["3:2-3:3"],
  },
  {
    name: "quoted ancestors join here-document delimiter source before later backquote folding",
    source:
      "\n: \"`: \\`cat <<'$IN'\n\\\\\\\n$IN\n\\``\"\n: \"`: \\`cat <<'\\\\\\\\$IN'\n\\\\\\\\\\\\\\\n$IN\n\\``\"\n: \"`: \\`cat <<'\\\\IN'\n\\\\\\\n\\IN\n\\``\"\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (double_quoted (backquote_substitution
      (backquote_substitution_body (complete_commands (complete_command (list
      (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))) (cmd_suffix (word (backquote_substitution
      (backquote_substitution_body (complete_commands (complete_command (list
      (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))) (cmd_suffix (io_redirect (io_here (dless) (here_end (word
      (single_quoted (single_quote_content))))))))))))))) (linebreak (newline_list
      (here_document_sequence (here_document_line_end) (here_document
      (here_document_end (here_document_end_text))))))))))))))))))))))))))))))
      (newline_list) (complete_command (list (and_or (pipeline (pipe_sequence
      (command (simple_command (cmd_name (word (literal))) (cmd_suffix (word
      (double_quoted (backquote_substitution (backquote_substitution_body
      (complete_commands (complete_command (list (and_or (pipeline (pipe_sequence
      (command (simple_command (cmd_name (word (literal))) (cmd_suffix (word
      (backquote_substitution (backquote_substitution_body (complete_commands
      (complete_command (list (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))) (cmd_suffix (io_redirect (io_here
      (dless) (here_end (word (single_quoted (single_quote_content)))))))))))))))
      (linebreak (newline_list (here_document_sequence (here_document_line_end)
      (here_document (here_document_end
      (here_document_end_text)))))))))))))))))))))))))))))) (newline_list)
      (complete_command (list (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))) (cmd_suffix (word (double_quoted
      (backquote_substitution (backquote_substitution_body (complete_commands
      (complete_command (list (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))) (cmd_suffix (word
      (backquote_substitution (backquote_substitution_body (complete_commands
      (complete_command (list (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))) (cmd_suffix (io_redirect (io_here
      (dless) (here_end (word (single_quoted (single_quote_content)))))))))))))))
      (linebreak (newline_list (here_document_sequence (here_document_line_end)
      (here_document (here_document_end
      (here_document_end_text))))))))))))))))))))))))))))))) (linebreak
      (newline_list)))`,
    continuations: ["2:2-2:3", "6:6-6:7", "10:2-10:3"],
  },
  {
    name: "terminator text excludes leading tabs and line continuations",
    source: "\ncat <<-EOF\n\t\\\n\tEOF\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))) suffix: (cmd_suffix redirect: (io_redirect body:
      (io_here operator: (dlessdash) end: (here_end word: (word
      (literal)))))))))))))) trailing: (linebreak (newline_list
      (here_document_sequence line_end: (here_document_line_end) document:
      (here_document end: (here_document_end (here_document_end_text)))))))`,
    continuations: ["2:1-2:2"],
  },
  {
    name: "a continuation before a here-document delimiter preserves the redirect",
    source: "\ncat <<\\\nEOF\nbody\nEOF\nafter\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))) suffix: (cmd_suffix redirect: (io_redirect body:
      (io_here operator: (dless) end: (here_end word: (word (literal)))))))))))))
      separator: (newline_list (here_document_sequence line_end:
      (here_document_line_end) document: (here_document body: (here_document_body
      (here_document_text)) end: (here_document_end (here_document_end_text)))))
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))))))))))) trailing: (linebreak (newline_list)))`,
    continuations: ["1:6-1:7"],
  },
  {
    name: "tab-stripping here-documents preserve continuations before delimiter words",
    source: "\ncat <<- \\\n\\\n EOF\n\tbody\n\tEOF\nafter\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))) suffix: (cmd_suffix redirect: (io_redirect body:
      (io_here operator: (dlessdash) end: (here_end word: (word
      (literal))))))))))))) separator: (newline_list (here_document_sequence
      line_end: (here_document_line_end) document: (here_document body:
      (here_document_body (here_document_text)) end: (here_document_end
      (here_document_end_text))))) command: (complete_command body: (list and_or:
      (and_or pipeline: (pipeline sequence: (pipe_sequence command: (command body:
      (simple_command name: (cmd_name (word (literal))))))))))) trailing:
      (linebreak (newline_list)))`,
    continuations: ["1:8-1:9", "2:0-2:1"],
  },
  {
    name: "terminal continuations in a delimiter precede its body",
    source: "\ncat <<EOF\\\n\\\n\nbody\nEOF\nafter\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (io_redirect (io_here (dless) (here_end (word
      (literal))))))))))))) (newline_list (here_document_sequence
      (here_document_line_end) (here_document (here_document_body
      (here_document_text)) (here_document_end (here_document_end_text)))))
      (complete_command (list (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))))))))))) (linebreak
      (newline_list)))`,
    continuations: ["1:9-1:10", "2:0-2:1"],
  },
  {
    name: "backquote continuations preserve their physical pairs and document boundaries",
    source:
      "\necho `cat <<\\\\\nEOF\nabc \\\\\n$name\nEOF\n`\necho `echo \\`cat <<\\\\\\\\\nEND\nbody\nEND\n\\``\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))) suffix: (cmd_suffix word: (word
      (backquote_substitution body: (backquote_substitution_body (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))) suffix: (cmd_suffix redirect: (io_redirect body:
      (io_here operator: (dless) end: (here_end word: (word (literal))))))))))))))
      (linebreak (newline_list (here_document_sequence line_end:
      (here_document_line_end) document: (here_document body: (here_document_body
      (here_document_text) (parameter_expansion parameter: (variable_name))) end:
      (here_document_end (here_document_end_text))))))))))))))))) separator:
      (newline_list) command: (complete_command body: (list and_or: (and_or
      pipeline: (pipeline sequence: (pipe_sequence command: (command body:
      (simple_command name: (cmd_name (word (literal))) suffix: (cmd_suffix word:
      (word (backquote_substitution body: (backquote_substitution_body
      (complete_commands command: (complete_command body: (list and_or: (and_or
      pipeline: (pipeline sequence: (pipe_sequence command: (command body:
      (simple_command name: (cmd_name (word (literal))) suffix: (cmd_suffix word:
      (word (backquote_substitution body: (backquote_substitution_body
      (complete_commands command: (complete_command body: (list and_or: (and_or
      pipeline: (pipeline sequence: (pipe_sequence command: (command body:
      (simple_command name: (cmd_name (word (literal))) suffix: (cmd_suffix
      redirect: (io_redirect body: (io_here operator: (dless) end: (here_end word:
      (word (literal)))))))))))))) (linebreak (newline_list (here_document_sequence
      line_end: (here_document_line_end) document: (here_document body:
      (here_document_body (here_document_text)) end: (here_document_end
      (here_document_end_text)))))))))))))))))))))))))))))) trailing: (linebreak
      (newline_list)))`,
    continuations: ["1:13-1:14", "3:5-3:6", "7:22-7:23"],
  },
  {
    name: "nested quoted here-documents inherit enclosing tabs and leading continuations",
    source: "\ncat <<-OUT\n$(cat <<'IN'\n\\\n\tIN\n)\nOUT\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))) suffix: (cmd_suffix redirect: (io_redirect body:
      (io_here operator: (dlessdash) end: (here_end word: (word
      (literal)))))))))))))) trailing: (linebreak (newline_list
      (here_document_sequence line_end: (here_document_line_end) document:
      (here_document body: (here_document_body (command_substitution body:
      (command_substitution_body (complete_commands command: (complete_command
      body: (list and_or: (and_or pipeline: (pipeline sequence: (pipe_sequence
      command: (command body: (simple_command name: (cmd_name (word (literal)))
      suffix: (cmd_suffix redirect: (io_redirect body: (io_here operator: (dless)
      end: (here_end word: (word (single_quoted
      (single_quote_content))))))))))))))) (linebreak (newline_list
      (here_document_sequence line_end: (here_document_line_end) document:
      (here_document end: (here_document_end (here_document_end_text))))))))) end:
      (here_document_end (here_document_end_text)))))))`,
    continuations: ["3:0-3:1"],
  },
  {
    name: "quoted nested bodies keep text before inherited continuations",
    source: "\ncat <<OUT\n$(cat <<'IN'\n\\\\\\\n \nIN\n)\nOUT\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))) suffix: (cmd_suffix redirect: (io_redirect body:
      (io_here operator: (dless) end: (here_end word: (word (literal))))))))))))))
      trailing: (linebreak (newline_list (here_document_sequence line_end:
      (here_document_line_end) document: (here_document body: (here_document_body
      (command_substitution body: (command_substitution_body (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))) suffix: (cmd_suffix redirect: (io_redirect body:
      (io_here operator: (dless) end: (here_end word: (word (single_quoted
      (single_quote_content))))))))))))))) (linebreak (newline_list
      (here_document_sequence line_end: (here_document_line_end) document:
      (here_document body: (quoted_here_document_body (quoted_here_document_text))
      end: (here_document_end (here_document_end_text))))))))) end:
      (here_document_end (here_document_end_text)))))))`,
    continuations: ["3:2-3:3"],
  },
  {
    name: "comments and escaped newlines do not trigger body collection early",
    source:
      "\ncat <<FIRST \\\n  middle <<SECOND # declarations finish here\none\nFIRST\ntwo\nSECOND\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))) suffix: (cmd_suffix redirect: (io_redirect body:
      (io_here operator: (dless) end: (here_end word: (word (literal))))) word:
      (word (literal)) redirect: (io_redirect body: (io_here operator: (dless) end:
      (here_end word: (word (literal)))))))))))))) trailing: (linebreak
      (newline_list (here_document_sequence comment: (comment (comment_text))
      line_end: (here_document_line_end) document: (here_document body:
      (here_document_body (here_document_text)) end: (here_document_end
      (here_document_end_text))) document: (here_document body: (here_document_body
      (here_document_text)) end: (here_document_end (here_document_end_text)))))))`,
    continuations: ["1:12-1:13"],
  },
  {
    name: "continuations before a here-document line end preserve the sequence",
    source:
      "\ncat <<EOF; echo first\\\n\nbody\nEOF\nsecond\ncat <<END; echo third\\\n \\\n\nbody two\nEND\nfourth\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (io_redirect (io_here (dless) (here_end (word
      (literal))))))))))) (separator_op) (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))) (cmd_suffix (word
      (literal)))))))))) (newline_list (here_document_sequence
      (here_document_line_end) (here_document (here_document_body
      (here_document_text)) (here_document_end (here_document_end_text)))))
      (complete_command (list (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal)))))))))) (newline_list)
      (complete_command (list (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))) (cmd_suffix (io_redirect (io_here
      (dless) (here_end (word (literal))))))))))) (separator_op) (and_or (pipeline
      (pipe_sequence (command (simple_command (cmd_name (word (literal)))
      (cmd_suffix (word (literal)))))))))) (newline_list (here_document_sequence
      (here_document_line_end) (here_document (here_document_body
      (here_document_text)) (here_document_end (here_document_end_text)))))
      (complete_command (list (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))))))))))) (linebreak
      (newline_list)))`,
    continuations: ["1:21-1:22", "6:21-6:22", "7:1-7:2"],
  },
  {
    name: "a continuation ending a here-document redirect line keeps a following command",
    source: "\ncat <<EOF \\\n\nEOF\necho after\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))) suffix: (cmd_suffix redirect: (io_redirect body:
      (io_here operator: (dless) end: (here_end word: (word (literal)))))))))))))
      separator: (newline_list (here_document_sequence line_end:
      (here_document_line_end) document: (here_document end: (here_document_end
      (here_document_end_text))))) command: (complete_command body: (list and_or:
      (and_or pipeline: (pipeline sequence: (pipe_sequence command: (command body:
      (simple_command name: (cmd_name (word (literal))) suffix: (cmd_suffix word:
      (word (literal))))))))))) trailing: (linebreak (newline_list)))`,
    continuations: ["1:10-1:11"],
  },
  {
    name: "pipeline negation permits continued layout before its command",
    source: "\n! \\\n  echo\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      negation: (bang) sequence: (pipe_sequence command: (command body:
      (simple_command name: (cmd_name (word (literal))))))))))) trailing:
      (linebreak (newline_list)))`,
    continuations: ["1:2-1:3"],
  },
  {
    name: "line continuations preserve command boundaries before operators",
    source: "\nfirst\\\n  |second\nthird\\\n  ||fourth\nfifth\\\n  &&sixth\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))))) command: (command body: (simple_command name:
      (cmd_name (word (literal)))))))))) separator: (newline_list) command:
      (complete_command body: (list and_or: (and_or pipeline: (pipeline sequence:
      (pipe_sequence command: (command body: (simple_command name: (cmd_name (word
      (literal))))))) operator: (or_if) pipeline: (pipeline sequence:
      (pipe_sequence command: (command body: (simple_command name: (cmd_name (word
      (literal)))))))))) separator: (newline_list) command: (complete_command body:
      (list and_or: (and_or pipeline: (pipeline sequence: (pipe_sequence command:
      (command body: (simple_command name: (cmd_name (word (literal)))))))
      operator: (and_if) pipeline: (pipeline sequence: (pipe_sequence command:
      (command body: (simple_command name: (cmd_name (word (literal)))))))))))
      trailing: (linebreak (newline_list)))`,
    continuations: ["1:5-1:6", "3:5-3:6", "5:5-5:6"],
  },
  {
    name: "command layout preserves leading separator and connector continuations",
    source: "\n\\\na;  \\\n \\\nb &\\\nc; d  \\\n \\\n| e  \\\n \\\n&& f\n\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal)))))))) separator: (separator_op) and_or: (and_or
      pipeline: (pipeline sequence: (pipe_sequence command: (command body:
      (simple_command name: (cmd_name (word (literal)))))))) separator:
      (separator_op) and_or: (and_or pipeline: (pipeline sequence: (pipe_sequence
      command: (command body: (simple_command name: (cmd_name (word
      (literal)))))))) separator: (separator_op) and_or: (and_or pipeline:
      (pipeline sequence: (pipe_sequence command: (command body: (simple_command
      name: (cmd_name (word (literal))))) command: (command body: (simple_command
      name: (cmd_name (word (literal))))))) operator: (and_if) pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))))))))))) trailing: (linebreak (newline_list)))`,
    continuations: [
      "1:0-1:1",
      "2:4-2:5",
      "3:1-3:2",
      "4:3-4:4",
      "5:6-5:7",
      "6:1-6:2",
      "7:5-7:6",
      "8:1-8:2",
    ],
  },
  {
    name: "repeated continuations remain layout at command and connector boundaries",
    source: "\n\\\n\\\n:\nfirst|\\\nnext\nfirst\\\n\\\n&&next\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal)))))))))) separator: (newline_list) command:
      (complete_command body: (list and_or: (and_or pipeline: (pipeline sequence:
      (pipe_sequence command: (command body: (simple_command name: (cmd_name (word
      (literal))))) command: (command body: (simple_command name: (cmd_name (word
      (literal)))))))))) separator: (newline_list) command: (complete_command body:
      (list and_or: (and_or pipeline: (pipeline sequence: (pipe_sequence command:
      (command body: (simple_command name: (cmd_name (word (literal)))))))
      operator: (and_if) pipeline: (pipeline sequence: (pipe_sequence command:
      (command body: (simple_command name: (cmd_name (word (literal)))))))))))
      trailing: (linebreak (newline_list)))`,
    continuations: ["1:0-1:1", "2:0-2:1", "4:6-4:7", "6:5-6:6", "7:0-7:1"],
  },
  {
    name: "line continuation before an empty logical line remains layout",
    source: "\nfirst\n\\\n\nsecond\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal)))))))))) separator: (newline_list) command:
      (complete_command body: (list and_or: (and_or pipeline: (pipeline sequence:
      (pipe_sequence command: (command body: (simple_command name: (cmd_name (word
      (literal))))))))))) trailing: (linebreak (newline_list)))`,
    continuations: ["2:0-2:1"],
  },
  {
    name: "a blank and line continuation before a separating newline share its run",
    source: "\nfirst \\\n\nsecond\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal)))))))))) separator: (newline_list) command:
      (complete_command body: (list and_or: (and_or pipeline: (pipeline sequence:
      (pipe_sequence command: (command body: (simple_command name: (cmd_name (word
      (literal))))))))))) trailing: (linebreak (newline_list)))`,
    continuations: ["1:6-1:7"],
  },
  {
    name: "continuation runs before blank lines stay layout",
    source: "\n\\\n\\\n\necho\n\\\n\\\n\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))))))))))) (linebreak (newline_list)))`,
    continuations: ["1:0-1:1", "2:0-2:1", "5:0-5:1", "6:0-6:1"],
  },
  {
    name: "a continuation-only program stays layout",
    source: "\\\n\\\n",
    expected: `(program)`,
    continuations: ["0:0-0:1", "1:0-1:1"],
  },
  {
    name: "a trailing continuation after a command stays layout at the end of input",
    source: "\nfirst\\\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))))))))))))`,
    continuations: ["1:5-1:6"],
  },
  {
    name: "trailing continuation runs stay layout at the end of input",
    source: "\na \\\n\nb;\\\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal)))))))))) separator: (newline_list) command:
      (complete_command body: (list and_or: (and_or pipeline: (pipeline sequence:
      (pipe_sequence command: (command body: (simple_command name: (cmd_name (word
      (literal))))))))) terminator: (separator_op))))`,
    continuations: ["1:2-1:3", "3:2-3:3"],
  },
  {
    name: "a blank-led continuation run after a closed group reaches the end of input",
    source: "\n{ a; } \\\n\n\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (compound_command
      (brace_group body: (compound_list body: (term and_or: (and_or pipeline:
      (pipeline sequence: (pipe_sequence command: (command body: (simple_command
      name: (cmd_name (word (literal))))))))) terminator: (separator operator:
      (separator_op)))))))))))) trailing: (linebreak (newline_list)))`,
    continuations: ["1:7-1:8"],
  },
  {
    name: "a continuation after for in preserves the wordlist",
    source: "\nfor i in\\\n a; do :; done\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (compound_command (for_clause
      (for_keyword) (name) (in (in_keyword)) (wordlist (word (literal)))
      (sequential_sep) (do_group (do_keyword) (compound_list (term (and_or
      (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))))))))) (separator (separator_op))) (done_keyword)))))))))))
      (linebreak (newline_list)))`,
    continuations: ["1:8-1:9"],
  },
  {
    name: "for wordlist separators retain blank-leading continuation runs",
    source: "\nfor i in \\\n\\\nword; do :; done\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (compound_command
      (for_clause (for_keyword) name: (name) in: (in (in_keyword)) words: (wordlist
      word: (word (literal))) separator: (sequential_sep) body: (do_group
      (do_keyword) body: (compound_list body: (term and_or: (and_or pipeline:
      (pipeline sequence: (pipe_sequence command: (command body: (simple_command
      name: (cmd_name (word (literal))))))))) terminator: (separator operator:
      (separator_op))) (done_keyword))))))))))) trailing: (linebreak
      (newline_list)))`,
    continuations: ["1:9-1:10", "2:0-2:1"],
  },
  {
    name: "blank-leading for continuations preserve the header",
    source: "\nfor i \\\nin a; do :; done\nfor j \\\ndo :; done\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (compound_command (for_clause
      (for_keyword) (name) (in (in_keyword)) (wordlist (word (literal)))
      (sequential_sep) (do_group (do_keyword) (compound_list (term (and_or
      (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))))))))) (separator (separator_op))) (done_keyword))))))))))
      (newline_list) (complete_command (list (and_or (pipeline (pipe_sequence
      (command (compound_command (for_clause (for_keyword) (name) (do_group
      (do_keyword) (compound_list (term (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))))))))) (separator (separator_op)))
      (done_keyword))))))))))) (linebreak (newline_list)))`,
    continuations: ["1:6-1:7", "3:6-3:7"],
  },
  {
    name: "encoded continuations delimit adjacent words and the end of a for list",
    source: "\n: `for x in one\\\\\n ~user\\\\\n ; do :; done`\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (backquote_substitution
      (backquote_substitution_body (complete_commands (complete_command (list
      (and_or (pipeline (pipe_sequence (command (compound_command (for_clause
      (for_keyword) (name) (in (in_keyword)) (wordlist (word (literal)) (word
      (tilde_expansion (tilde_user (literal))))) (sequential_sep) (do_group
      (do_keyword) (compound_list (term (and_or (pipeline (pipe_sequence (command
      (simple_command (cmd_name (word (literal))))))))) (separator (separator_op)))
      (done_keyword))))))))))))))))))))))) (linebreak (newline_list)))`,
    continuations: ["1:16-1:17", "2:7-2:8"],
  },
  {
    name: "raw newlines end redirect attachment while escaped newlines preserve it",
    source: "\n(:)\\\n2>first\\\n>second\n(:)\n>separate\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (compound_command (subshell
      body: (compound_list body: (term and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal)))))))))))) redirects: (redirect_list redirect:
      (io_redirect number: (io_number) body: (io_file filename: (filename word:
      (word (literal))))) redirect: (io_redirect body: (io_file filename: (filename
      word: (word (literal)))))))))))) separator: (newline_list) command:
      (complete_command body: (list and_or: (and_or pipeline: (pipeline sequence:
      (pipe_sequence command: (command body: (compound_command (subshell body:
      (compound_list body: (term and_or: (and_or pipeline: (pipeline sequence:
      (pipe_sequence command: (command body: (simple_command name: (cmd_name (word
      (literal)))))))))))))))))) separator: (newline_list) command:
      (complete_command body: (list and_or: (and_or pipeline: (pipeline sequence:
      (pipe_sequence command: (command body: (simple_command prefix: (cmd_prefix
      redirect: (io_redirect body: (io_file filename: (filename word: (word
      (literal)))))))))))))) trailing: (linebreak (newline_list)))`,
    continuations: ["1:3-1:4", "2:7-2:8"],
  },
  {
    name: "redirection token boundaries preserve intervening continuations",
    source: "\n2\\\n>out command >\\\ntarget\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command prefix:
      (cmd_prefix redirect: (io_redirect number: (io_number) body: (io_file
      filename: (filename word: (word (literal)))))) word: (cmd_word (word
      (literal))) suffix: (cmd_suffix redirect: (io_redirect body: (io_file
      filename: (filename word: (word (literal)))))))))))))) trailing: (linebreak
      (newline_list)))`,
    continuations: ["1:1-1:2", "2:14-2:15"],
  },
  {
    name: "internal blanks in continuation runs separate simple-command elements",
    source:
      "\necho\\\n \\\nargument\necho first\\\n\\\n \\\nsecond\n<input\\\n \\\ncommand\n<input\\\n \\\nNAME=value\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))) suffix: (cmd_suffix word: (word
      (literal)))))))))) separator: (newline_list) command: (complete_command body:
      (list and_or: (and_or pipeline: (pipeline sequence: (pipe_sequence command:
      (command body: (simple_command name: (cmd_name (word (literal))) suffix:
      (cmd_suffix word: (word (literal)) word: (word (literal)))))))))) separator:
      (newline_list) command: (complete_command body: (list and_or: (and_or
      pipeline: (pipeline sequence: (pipe_sequence command: (command body:
      (simple_command prefix: (cmd_prefix redirect: (io_redirect body: (io_file
      filename: (filename word: (word (literal)))))) word: (cmd_word (word
      (literal)))))))))) separator: (newline_list) command: (complete_command body:
      (list and_or: (and_or pipeline: (pipeline sequence: (pipe_sequence command:
      (command body: (simple_command prefix: (cmd_prefix redirect: (io_redirect
      body: (io_file filename: (filename word: (word (literal))))) assignment:
      (assignment_word name: (variable_name) value: (assignment_value
      (literal)))))))))))) trailing: (linebreak (newline_list)))`,
    continuations: [
      "1:4-1:5",
      "2:1-2:2",
      "4:10-4:11",
      "5:0-5:1",
      "6:1-6:2",
      "8:6-8:7",
      "9:1-9:2",
      "11:6-11:7",
      "12:1-12:2",
    ],
  },
  {
    name: "a continuation followed by blank separates command words",
    source: "\necho \\\n  three\nnext\n",
    expected: `(program leading: (linebreak (newline_list)) commands: (complete_commands
      command: (complete_command body: (list and_or: (and_or pipeline: (pipeline
      sequence: (pipe_sequence command: (command body: (simple_command name:
      (cmd_name (word (literal))) suffix: (cmd_suffix word: (word
      (literal)))))))))) separator: (newline_list) command: (complete_command body:
      (list and_or: (and_or pipeline: (pipeline sequence: (pipe_sequence command:
      (command body: (simple_command name: (cmd_name (word (literal)))))))))))
      trailing: (linebreak (newline_list)))`,
    continuations: ["1:5-1:6"],
  },
  {
    name: "a removed newline after a tilde prefix ends the prefix",
    source: "\necho ~user\\\n next\na=~user\\\n cmd\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (tilde_expansion (tilde_user (literal))))
      (word (literal)))))))))) (newline_list) (complete_command (list (and_or
      (pipeline (pipe_sequence (command (simple_command (cmd_prefix
      (assignment_word (variable_name) (assignment_value (tilde_expansion
      (tilde_user (literal)))))) (cmd_word (word (literal))))))))))) (linebreak
      (newline_list)))`,
    continuations: ["1:10-1:11", "3:7-3:8"],
  },
  {
    name: "encoded continuations end argument and assignment tilde users",
    source: "\n: `: ~user\\\\\n :`\n: `A=~user\\\\\n :`\n",
    expected: `(program (linebreak (newline_list)) (complete_commands (complete_command
      (list (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name
      (word (literal))) (cmd_suffix (word (backquote_substitution
      (backquote_substitution_body (complete_commands (complete_command (list
      (and_or (pipeline (pipe_sequence (command (simple_command (cmd_name (word
      (literal))) (cmd_suffix (word (tilde_expansion (tilde_user (literal)))) (word
      (literal)))))))))))))))))))))) (newline_list) (complete_command (list (and_or
      (pipeline (pipe_sequence (command (simple_command (cmd_name (word (literal)))
      (cmd_suffix (word (backquote_substitution (backquote_substitution_body
      (complete_commands (complete_command (list (and_or (pipeline (pipe_sequence
      (command (simple_command (cmd_prefix (assignment_word (variable_name)
      (assignment_value (tilde_expansion (tilde_user (literal)))))) (cmd_word (word
      (literal))))))))))))))))))))))) (linebreak (newline_list)))`,
    continuations: ["1:11-1:12", "3:11-3:12"],
  },
]) {
  test(`sh: ${name} preserves the structure and continuation ranges`, () => {
    const output = parseValidCst(writeSource(name, source));
    assert.equal(
      namedStructure(output, expected.includes(": ")),
      expected.replace(/[ \n]+/g, " "),
      name,
    );
    assert.deepEqual(continuationManifest(output), continuations, name);
  });
}

test("sh: parser resource bounds preserve complete roots and deterministic recovery", async (context) => {
  const completionOnly = { completionOnly: true };
  const arithmeticDepth = 2_000;
  const unmatchedBracketCount = 80_000;
  const bracketMemberCount = 16_000;
  const bracketGroupCount = 4_000;
  const incompleteClassCount = 4_000;
  const continuationCount = 20_000;
  const bodyParameterCount = 16_000;
  const pendingDocumentCount = 600;
  const maximumNestedDocuments = 44;
  const excessDocumentDepth = 300;
  const backquoteDepth = 6;
  const largeValidSources = [
    [
      "deep-arithmetic-expansions",
      `echo ${"$((".repeat(arithmeticDepth)}1${"))".repeat(arithmeticDepth)}\n`,
    ],
    ["unmatched-brackets", `printf ${"[".repeat(unmatchedBracketCount)}\n`],
    ["long-bracket-list", `printf [${"a".repeat(bracketMemberCount)}]\n`],
    ["repeated-brackets", `printf ${"[abc]".repeat(bracketGroupCount)}\n`],
    [
      "repeated-incomplete-special-brackets",
      `printf [${"[:alpha:]".repeat(incompleteClassCount)}\n`,
    ],
    [
      "continued-blank-lines",
      `printf value ${"\\\n".repeat(continuationCount)}after\n`,
    ],
    [
      "continued-pipe-linebreak",
      `first|${"\\\n".repeat(continuationCount)}next\n`,
    ],
    [
      "here-document-dollars",
      `cat <<EOF\n${"$x".repeat(bodyParameterCount)}\nEOF\nafter\n`,
    ],
  ];
  for (const [name, contents] of largeValidSources) {
    await context.test(name, () => {
      assertValid(writeSource(name, contents), name, completionOnly);
    });
  }

  await context.test("queued here-documents", () => {
    const manyDocuments = writeSource(
      "many-documents",
      `cat${" <<X".repeat(pendingDocumentCount)}\n${"body\nX\n".repeat(pendingDocumentCount)}after\n`,
    );
    assertRepeatedColdParse(
      "resource",
      manyDocuments,
      "many-documents",
      completionOnly,
    );
  });

  function nestedDocuments(depth) {
    let contents = "cat <<D0\n";
    for (let counter = 1; counter < depth; counter += 1) {
      contents += `$(cat <<D${counter}\n`;
    }
    contents += "leaf\n";
    for (let counter = depth - 1; counter >= 0; counter -= 1) {
      contents += `D${counter}\n`;
      if (counter > 0) {
        contents += ")\n";
      }
    }
    return `${contents}after\n`;
  }

  await context.test("nested here-documents at capacity", () => {
    const deepDocuments = writeSource(
      "deep-documents",
      nestedDocuments(maximumNestedDocuments),
    );
    assertValid(deepDocuments, "deep-documents", completionOnly);
  });

  await context.test("nested here-documents above capacity", () => {
    const excessDepth = writeSource(
      "excess-document-depth",
      nestedDocuments(maximumNestedDocuments + 1),
    );
    assert.equal(
      runParse({
        ...completionOnly,
        source: excessDepth,
        mode: "resource",
        description: "nested document metadata exceeds 1024 bytes",
      }).status,
      1,
    );
    assertRepeatedColdParse(
      "resource",
      excessDepth,
      "excess-document-depth",
      completionOnly,
    );
  });

  await context.test("deep here-documents complete deterministically", () => {
    const boundedDocuments = writeSource(
      "deep-documents-bounded",
      nestedDocuments(excessDocumentDepth),
    );
    assertRepeatedColdParse(
      "resource",
      boundedDocuments,
      "deep-documents-bounded",
      completionOnly,
    );
  });

  await context.test("nested backquotes", () => {
    let nestedBackquotes = "echo `level1 ";
    for (let level = 2; level <= backquoteDepth; level += 1) {
      nestedBackquotes += `${"\\".repeat(2 ** (level - 1) - 1)}\`level${level} `;
    }
    nestedBackquotes += "leaf";
    for (let level = backquoteDepth; level >= 2; level -= 1) {
      nestedBackquotes += `${"\\".repeat(2 ** (level - 1) - 1)}\` end${level}`;
    }
    nestedBackquotes += "`\n";
    assertValid(
      writeSource("nested-backquotes", nestedBackquotes),
      "nested-backquotes",
      completionOnly,
    );
  });
});
