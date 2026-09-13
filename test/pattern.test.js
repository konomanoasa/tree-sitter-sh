import assert from "node:assert/strict";
import { test } from "node:test";
import { nodeIdentity, parseCst, sourceByteOffset } from "./support/cst.js";
import {
  assertCstRange,
  assertNodeCount,
  assertSameLogicalProjection,
  parseValidCst,
  writeSource,
} from "./support/parser.js";

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

function lexicalTexts(output, source, type) {
  const bytes = Buffer.from(source);
  return parseCst(output)
    .filter((entry) => nodeIdentity(entry).type === type)
    .map((entry) => {
      const [start, end] = entry.range
        .split("-")
        .map((point) => sourceByteOffset(source, point));
      return bytes.subarray(start, end).toString().replaceAll("\\\n", "");
    });
}

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
