import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";
import { after, before, test } from "node:test";
import { repositoryDirectory, runTreeSitter } from "../scripts/tree-sitter.js";
import { assertCaptures, createHighlighter } from "./support/highlight.js";

const captureNames = [
  "character",
  "character.special",
  "comment",
  "function",
  "function.call",
  "keyword",
  "label",
  "number",
  "operator",
  "punctuation.bracket",
  "punctuation.delimiter",
  "punctuation.special",
  "string",
  "string.escape",
  "string.regexp",
  "string.special.path",
  "variable",
  "variable.builtin",
  "variable.parameter",
];
let highlight;

let directory;
before(() => {
  directory = mkdtempSync(path.join(tmpdir(), "tree-sitter-sh-highlight-"));
  highlight = createHighlighter({
    directory,
    root: repositoryDirectory,
    run: assertCommand,
    captureNames,
  });
});
after(() => {
  if (directory) rmSync(directory, { recursive: true, force: true });
});

const highlightFixture = path.join(
  repositoryDirectory,
  "test",
  "highlight",
  "sh.sh",
);
const query = path.join(repositoryDirectory, "queries", "highlights.scm");

function assertCommand(arguments_) {
  const result = runTreeSitter(arguments_, {
    environmentDirectory: path.join(directory, "runtime"),
    allowedStatuses: [0, 1],
    // Bound query compilation too; a parser-only timeout cannot interrupt it.
    timeout: 60_000,
  });
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.doesNotMatch(result.stderr, /Non-standard highlight captures/);
  return result.stdout;
}

function queryCaptures(fixture) {
  const output = assertCommand([
    "query",
    "--captures",
    "--scope",
    "source.sh",
    query,
    fixture,
  ]);
  return [
    ...output.matchAll(
      / - ([^,]+), start: \(([0-9]+), ([0-9]+)\), end: \(([0-9]+), ([0-9]+)\)/g,
    ),
  ].map((match) => ({
    name: match[1],
    start: [Number(match[2]), Number(match[3])],
    end: [Number(match[4]), Number(match[5])],
  }));
}

test("sh: highlight queries satisfy annotated source expectations", () => {
  assertCommand([
    "query",
    "--test",
    "--scope",
    "source.sh",
    query,
    highlightFixture,
  ]);
  assertCommand([
    "highlight",
    "--check",
    "--captures-path",
    path.join(directory, "captures.txt"),
    "--quiet",
    "--scope",
    "source.sh",
    highlightFixture,
  ]);
});

test("sh: assignment patterns have no pattern captures", () => {
  assert.deepEqual(
    queryCaptures(
      path.join(repositoryDirectory, "test", "query", "inactive-patterns.txt"),
    ),
    [
      { name: "variable", start: [0, 0], end: [0, 4] },
      { name: "operator", start: [0, 4], end: [0, 5] },
    ],
  );
});

test("sh: here-document terminators label only text and retain continuations", () => {
  const lines = readFileSync(highlightFixture, "utf8").split("\n");
  const captures = queryCaptures(highlightFixture);
  for (const [declaration, firstRowOffset, expected] of [
    [
      "cat <<*?[!a-z[:alpha:][.x.][=y=]]",
      2,
      [{ name: "label", start: [2, 0], end: [2, 27] }],
    ],
    ["cat <<'EOF'", 2, [{ name: "label", start: [2, 0], end: [2, 3] }]],
    [
      "cat <<CONTINUED",
      1,
      [
        { name: "punctuation.special", start: [1, 0], end: [2, 0] },
        { name: "label", start: [2, 0], end: [2, 9] },
      ],
    ],
    [
      "cat <<-STRIPPED",
      1,
      [
        { name: "punctuation.special", start: [1, 1], end: [2, 0] },
        { name: "label", start: [2, 0], end: [2, 8] },
      ],
    ],
  ]) {
    const declarationRow = lines.indexOf(declaration);
    assert.notEqual(declarationRow, -1, declaration);
    const firstRow = declarationRow + firstRowOffset;
    assert.deepEqual(
      captures.filter(
        (capture) =>
          capture.start[0] < declarationRow + 3 &&
          (capture.end[0] > firstRow ||
            (capture.end[0] === firstRow && capture.end[1] > 0)),
      ),
      expected.map(({ name, start, end }) => ({
        name,
        start: [declarationRow + start[0], start[1]],
        end: [declarationRow + end[0], end[1]],
      })),
      declaration,
    );
  }
});

// Declaration annotations would be here-document body text, so check captures directly.
test("sh: here-document declarations preserve label, quote, and escape captures", () => {
  const lines = readFileSync(highlightFixture, "utf8").split("\n");
  // General string captures can overlap the more specific delimiter labels.
  const captures = queryCaptures(highlightFixture).filter(
    (capture) => capture.name !== "string",
  );
  for (const [input, expected] of [
    ["cat <<*?[!a-z[:alpha:][.x.][=y=]]", { label: [[6, 33]] }],
    ["cat <<[[.a.]-[.z.]]", { label: [[6, 19]] }],
    [
      String.raw`cat <<[\a]`,
      {
        label: [
          [6, 7],
          [9, 10],
        ],
        "string.escape": [[7, 9]],
      },
    ],
    [
      `cat <<['a'-"z"]`,
      {
        label: [
          [6, 7],
          [8, 9],
          [10, 11],
          [12, 13],
          [14, 15],
        ],
        "punctuation.delimiter": [
          [7, 8],
          [9, 10],
          [11, 12],
          [13, 14],
        ],
      },
    ],
    [
      `cat <<[[:'alpha':][."x".][=$'y'=]]`,
      {
        label: [
          [6, 9],
          [10, 15],
          [16, 20],
          [21, 22],
          [23, 27],
          [29, 30],
          [31, 34],
        ],
        "punctuation.delimiter": [
          [9, 10],
          [15, 16],
          [20, 21],
          [22, 23],
          [27, 29],
          [30, 31],
        ],
      },
    ],
  ]) {
    const row = lines.indexOf(input);
    assert.notEqual(row, -1, input);
    const actual = {};
    for (const capture of captures) {
      if (capture.start[0] !== row || capture.start[1] < 6) continue;
      assert.equal(capture.end[0], row, input);
      actual[capture.name] ??= [];
      actual[capture.name].push([capture.start[1], capture.end[1]]);
    }
    const columns = (ranges) =>
      [
        ...new Set(
          ranges.flatMap(([start, end]) =>
            Array.from({ length: end - start }, (_, index) => start + index),
          ),
        ),
      ].sort((left, right) => left - right);
    assert.deepEqual(
      Object.fromEntries(
        Object.entries(actual).map(([name, ranges]) => [name, columns(ranges)]),
      ),
      Object.fromEntries(
        Object.entries(expected).map(([name, ranges]) => [
          name,
          columns(ranges),
        ]),
      ),
      input,
    );
  }
});

const finalCaptureCases = [
  {
    name: "HTML-sensitive Unicode literals retain source bytes and captures",
    source: `printf "é😀<&>'"\n`,
    captures: [
      [0, 6, "function.call"],
      [7, 8, "punctuation.delimiter"],
      [8, 18, "string"],
      [18, 19, "punctuation.delimiter"],
    ],
  },

  {
    name: "empty input has no captures",
    source: "",
    captures: [],
  },
  {
    name: "command names override literals and UTF-8 arguments retain byte ranges",
    source: "printf é😀\n",
    captures: [
      [0, 6, "function.call"],
      [7, 13, "string"],
    ],
  },
  {
    name: "assignment names, operators and quote delimiters keep separate roles",
    source: 'value="é"\n',
    captures: [
      [0, 5, "variable"],
      [5, 6, "operator"],
      [6, 7, "punctuation.delimiter"],
      [7, 9, "string"],
      [9, 10, "punctuation.delimiter"],
    ],
  },
  {
    name: "quoted text and escaped spaces keep distinct final captures",
    source: "printf 'a b' a\\ b\n",
    captures: [
      [0, 6, "function.call"],
      [7, 8, "punctuation.delimiter"],
      [8, 11, "string"],
      [11, 12, "punctuation.delimiter"],
      [13, 14, "string"],
      [14, 16, "string.escape"],
      [16, 17, "string"],
    ],
  },
  {
    name: "arithmetic expansions separate punctuation, variables, operators and numbers",
    source: "echo $((x + 2))\n",
    captures: [
      [0, 4, "function.call"],
      [5, 6, "punctuation.special"],
      [6, 8, "punctuation.bracket"],
      [8, 9, "variable"],
      [10, 11, "operator"],
      [12, 13, "number"],
      [13, 15, "punctuation.bracket"],
    ],
  },
  {
    name: "parameter expansions distinguish positional, special and named variables",
    source: `printf $1 $? \${x}\n`,
    captures: [
      [0, 6, "function.call"],
      [7, 8, "variable"],
      [8, 9, "variable.parameter"],
      [10, 11, "variable"],
      [11, 12, "variable.builtin"],
      [13, 14, "variable"],
      [14, 15, "punctuation.bracket"],
      [15, 16, "variable"],
      [16, 17, "punctuation.bracket"],
    ],
  },
  {
    name: "function declarations override ordinary command names",
    source: 'run() { printf "$1"; }\n',
    captures: [
      [0, 3, "function"],
      [3, 5, "punctuation.bracket"],
      [6, 7, "punctuation.bracket"],
      [8, 14, "function.call"],
      [15, 16, "punctuation.delimiter"],
      [16, 17, "variable"],
      [17, 18, "variable.parameter"],
      [18, 19, "punctuation.delimiter"],
      [19, 20, "punctuation.delimiter"],
      [21, 22, "punctuation.bracket"],
    ],
  },
  {
    name: "quoted here-documents preserve labels and literal expansion text",
    source: "cat <<'EOF'\n$x\nEOF\n",
    captures: [
      [0, 3, "function.call"],
      [4, 6, "operator"],
      [6, 7, "punctuation.delimiter"],
      [7, 10, "label"],
      [10, 11, "punctuation.delimiter"],
      [12, 14, "string"],
      [15, 18, "label"],
    ],
  },
  {
    name: "unquoted here-documents retain expansion captures between labels",
    source: "cat <<EOF\n$x\nEOF\n",
    captures: [
      [0, 3, "function.call"],
      [4, 6, "operator"],
      [6, 9, "label"],
      [10, 12, "variable"],
      [13, 16, "label"],
    ],
  },
  {
    name: "case ranges distinguish pattern characters from operators and command text",
    source: "case x in [a-z]) :;; esac\n",
    captures: [
      [0, 4, "keyword"],
      [5, 6, "string"],
      [7, 9, "keyword"],
      [10, 11, "punctuation.bracket"],
      [11, 12, "character"],
      [12, 13, "operator"],
      [13, 14, "character"],
      [14, 16, "punctuation.bracket"],
      [17, 18, "function.call"],
      [18, 20, "operator"],
      [21, 25, "keyword"],
    ],
  },
  {
    name: "tilde users keep path captures across literal text",
    source: "printf ~user\n",
    captures: [
      [0, 6, "function.call"],
      [7, 12, "string.special.path"],
    ],
  },
  {
    name: "continuations and UTF-8 comments retain their source ranges",
    source: "printf \\\nvalue # é😀\n",
    captures: [
      [0, 6, "function.call"],
      [7, 9, "punctuation.special"],
      [9, 14, "string"],
      [15, 23, "comment"],
    ],
  },
];

for (const grammar of [{ name: "sh", scope: "source.sh" }]) {
  for (const { name, languages, source, captures } of finalCaptureCases) {
    if (languages && !languages.includes(grammar.name)) continue;
    test(`${grammar.name}: ${name}`, () => {
      assertCaptures(source, highlight(grammar.scope, source), captures);
    });
  }
  test(`${grammar.name}: incomplete input preserves source without error colors`, () => {
    for (const source of ["if x; then", 'echo "', "cat <<EOF\nbody\n"]) {
      highlight(grammar.scope, source, false);
    }
  });
}
