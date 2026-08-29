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
  const result = runTreeSitter(arguments_, { allowedStatuses: [0, 1] });
  assert.equal(result.status, 0, result.stdout + result.stderr);
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
