const assert = require("node:assert/strict");
const path = require("node:path");
const { test } = require("node:test");
const {
  repositoryDirectory,
  runTreeSitter,
} = require("../scripts/tree-sitter");

const highlightFixture = path.join(
  repositoryDirectory,
  "test",
  "highlight",
  "sh.sh",
);
const inactivePatternsFixture = path.join(
  repositoryDirectory,
  "test",
  "query",
  "inactive-patterns.sh",
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

test("highlight query", () => {
  assertCommand([
    "highlight",
    "--check",
    "--quiet",
    "--scope",
    "source.sh",
    highlightFixture,
  ]);
  assertCommand([
    "query",
    "--test",
    "--scope",
    "source.sh",
    query,
    highlightFixture,
  ]);
});

test("inactive pattern sources remain unhighlighted", () => {
  assertCommand([
    "query",
    "--test",
    "--scope",
    "source.sh",
    query,
    inactivePatternsFixture,
  ]);
});

test("pattern literals span quotes and substitutions in each word context", () => {
  assertCommand([
    "query",
    "--test",
    "--scope",
    "source.sh",
    query,
    path.join(repositoryDirectory, "test", "query", "pattern-literals.txt"),
  ]);
});

test("capture iteration keeps pattern literals inside their owning word", () => {
  const output = assertCommand([
    "query",
    "--captures",
    "--scope",
    "source.sh",
    query,
    path.join(
      repositoryDirectory,
      "test",
      "query",
      "pattern-context-boundaries.txt",
    ),
  ]);
  const captures = output
    .split("\n")
    .filter((line) => line.includes(" - string.regexp,"))
    .map((line) => line.slice(line.indexOf("start:")));
  assert.deepEqual(captures, [
    "start: (0, 0), end: (0, 3), text: `pre`",
    "start: (0, 4), end: (0, 8), text: `tail`",
    "start: (1, 11), end: (1, 14), text: `pre`",
    "start: (1, 15), end: (1, 19), text: `tail`",
    "start: (2, 11), end: (2, 14), text: `pre`",
    "start: (2, 17), end: (2, 21), text: `tail`",
    "start: (3, 18), end: (3, 21), text: `pre`",
    "start: (3, 22), end: (3, 26), text: `tail`",
    "start: (4, 5), end: (4, 8), text: `pre`",
    "start: (4, 9), end: (4, 13), text: `tail`",
    "start: (5, 5), end: (5, 8), text: `pre`",
    "start: (5, 19), end: (5, 22), text: `mid`",
    "start: (5, 23), end: (5, 27), text: `tail`",
    "start: (5, 35), end: (5, 38), text: `end`",
  ]);
});
