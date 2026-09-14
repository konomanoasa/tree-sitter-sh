import assert from "node:assert/strict";
import { test } from "node:test";
import {
  assertCstRange,
  assertIncrementalEqualsFresh,
  assertNodeCount,
  assertSameLogicalProjection,
  continuationManifest,
  parseValidCst,
  writeSource,
} from "./support/parser.js";

const fixtures = [
  ["command name", "pri§ntf value\n", [["literal", "printf"]]],
  ["argument", "printf foo§bar\n", [["literal", "foobar"]]],
  ["Unicode argument", "printf é§🙂a§bc\n", [["literal", "é🙂abc"]]],
  ["bullet in argument", "printf a§•§b\n", [["literal", "a•b"]]],
  [
    "Unicode separators in argument",
    "printf a\u2028§b\u2029§c\n",
    [["literal", "a\u2028b\u2029c"]],
  ],
  [
    "Unicode separators in double quote text",
    'printf "a\u2028§b\u2029§c"\n',
    [["double_quote_text", "a\u2028b\u2029c"]],
  ],
  [
    "assignment",
    "VA§R=va§lue\n",
    [
      ["variable_name", "VAR"],
      ["literal", "value"],
    ],
  ],
  [
    "reserved words",
    "i§f :; th§en :; f§i\n",
    [
      ["if_keyword", "if"],
      ["then_keyword", "then"],
      ["fi_keyword", "fi"],
    ],
  ],
  ["function name", "wor§ker() { :; }\n", [["fname", "worker"]]],
  [
    "for name and words",
    "for al§pha i§n o§ne two; d§o :; do§ne\n",
    [
      ["name", "alpha"],
      ["in_keyword", "in"],
      ["literal", "one"],
    ],
  ],
  [
    "control operators",
    "a &§& b |§| c\n",
    [
      ["and_if", "&&"],
      ["or_if", "||"],
    ],
  ],
  [
    "case terminators",
    "case x in a) : ;§; b) : ;§& esac\n",
    [
      ["dsemi", ";;"],
      ["semi_and", ";&"],
    ],
  ],
  ["IO number", "printf 1§2>out\n", [["io_number", "12"]]],
  ["redirection operator", "printf >§>out\n", [["dgreat", ">>"]]],
  ["redirection filename", "printf >out§put\n", [["literal", "output"]]],
  [
    "parameter name and operand",
    ": $§{va§r:-va§lue}\n",
    [
      ["variable_name", "var"],
      ["literal", "value"],
    ],
  ],
  [
    "parameter operator",
    `: \${var:§-value}\n`,
    [["parameter_value_operator", ":-"]],
  ],
  [
    "parameter length of hash",
    `: \${#§#}\n`,
    [
      ["parameter_length_operator", "#"],
      ["special_parameter", "#", 1],
    ],
  ],
  [
    "parameter length of options",
    `: \${#§-}\n`,
    [
      ["parameter_length_operator", "#"],
      ["special_parameter", "-"],
    ],
  ],
  [
    "parameter length of status",
    `: \${#§?}\n`,
    [
      ["parameter_length_operator", "#"],
      ["special_parameter", "?"],
    ],
  ],
  [
    "longest prefix pattern operator",
    `: \${var#§#value}\n`,
    [["parameter_pattern_operator", "##"]],
  ],
  ["unbraced parameter", ": $va§r\n", [["variable_name", "var"]]],
  ["numeric parameter spelling", `: \${0§0}\n`, []],
  ["positional parameter", `: \${0§1}\n`, [["positional_parameter", "01"]]],
  ["command prefix", ": $§(printf value)\n", []],
  ["arithmetic prefix", ": $§(§(1 + 2)§)\n", []],
  ["arithmetic number", ": $((1§23 + 4))\n", [["arithmetic_number", "123"]]],
  [
    "arithmetic assignment operator",
    ": $((value <§<= 1))\n",
    [["arithmetic_operator", "<<="]],
  ],
  ["double quote text", ': "foo§bar"\n', [["double_quote_text", "foobar"]]],
  ["dollar quote prefix", ": $§'text'\n", []],
  [
    "pattern character class",
    ": [[:al§pha:]]\n",
    [["pattern_character_class_content_source", "alpha"]],
  ],
  ["tilde user", ": ~roo§t/path\n", [["literal", "root"]]],
  [
    "heredoc operator and delimiter",
    "cat <§<§-E§OF\nbody\nEOF\n",
    [
      ["dlessdash", "<<-"],
      ["literal", "EOF"],
    ],
  ],
  [
    "heredoc body and closing word",
    "cat <<EOF\nfoo§bar\nEO§F\n",
    [
      ["here_document_text", "foobar"],
      ["here_document_end_text", "EOF", 1],
    ],
  ],
  ["word separator", "printf foo §bar\n", []],
  ["closed compound redirect", "( : )§2>out\n", []],
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

for (const [name, marked, expectedNodes] of fixtures) {
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
      for (const [type, spelling, occurrence = 0] of expectedNodes) {
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
  for (const [name, source, type, count] of [
    ["single quote", ": 'ab\\\ncd'\n", "single_quoted", 1],
    ["dollar single quote", ": $'ab\\\ncd'\n", "dollar_single_quoted", 1],
    [
      "quoted heredoc",
      "cat <<'END'\nab\\\ncd\nEND\n",
      "quoted_here_document_body",
      1,
    ],
    ["native comment", "#ab\\\nword\n", "cmd_name", 1],
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

for (const [name, source] of [
  ["command and redirects", "VAR=value printf é🙂 12>>output && ( : )2>file\n"],
  ["case and function", "worker() { case ab in a|[[:alpha:]]) :;; esac; }\n"],
  ["nested expansions", `: "ab\${value:-cd}$(printf ef)$((name <<= 2))gh"\n`],
  ["arithmetic precedence", ": $((value = 0x12 + 3 * 4 < 9 ? 5 : 6))\n"],
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
