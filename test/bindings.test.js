import assert from "node:assert/strict";
import { test } from "node:test";
import Parser from "tree-sitter";
import SH from "tree-sitter-sh";

test("Node.js language parses a quoted here-document and exposes node metadata and highlights", () => {
  const parser = new Parser();
  parser.setLanguage(SH);
  const source = "cat <<'EOF'\n$HOME\nEOF\n";
  const tree = parser.parse(source);
  assert.equal(tree.rootNode.type, "program");
  assert.equal(tree.rootNode.hasError, false);
  assert.equal(tree.rootNode.text, source);
  assert.ok(
    SH.nodeTypeInfo.some((node) => node.type === "program" && node.named),
  );
  const query = new Parser.Query(SH, SH.HIGHLIGHTS_QUERY);
  assert.deepEqual(
    query
      .captures(tree.rootNode)
      .filter(({ node }) => node.type === "here_document_end_text")
      .map(({ name, node }) => [
        name,
        node.text,
        node.startIndex,
        node.endIndex,
      ]),
    [["label", "EOF", 18, 21]],
  );
});
