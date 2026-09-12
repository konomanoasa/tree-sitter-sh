import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import path from "node:path";
import { test } from "node:test";
import { repositoryDirectory, runTreeSitter } from "../scripts/tree-sitter.js";

const highlightFixture = path.join(
  repositoryDirectory,
  "test",
  "highlight",
  "sh.sh",
);
const query = path.join(repositoryDirectory, "queries", "highlights.scm");

function assertCommand(arguments_) {
  const result = runTreeSitter(arguments_, {
    allowedStatuses: [0, 1],
    // Bound query compilation too; a parser-only timeout cannot interrupt it.
    timeout: 10_000,
  });
  assert.equal(result.status, 0, result.stdout + result.stderr);
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

test("highlight query", () => {
  assertCommand([
    "highlight",
    "--check",
    "--quiet",
    "--scope",
    "source.sh",
    highlightFixture,
  ]);
});

test("assignment patterns have no pattern captures", () => {
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

// Declaration annotations would be here-document body text, so check captures directly.
test("here-document declarations preserve label, quote, and escape captures", () => {
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
