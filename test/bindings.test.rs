use tree_sitter::{
  InputEdit, Parser, Point, Query, QueryCursor, StreamingIterator, Tree,
};
use tree_sitter_sh as grammar;

fn parser() -> Parser {
  let mut parser = Parser::new();
  parser.set_language(&grammar::LANGUAGE.into()).unwrap();
  parser
}

fn cst_snapshot(
  tree: &Tree,
  normalize_continuations: bool,
) -> (Vec<String>, Vec<String>) {
  let mut output = Vec::new();
  let mut continuations = Vec::new();
  let mut cursor = tree.walk();
  let mut depth = 0;
  loop {
    let node = cursor.node();
    let identity = format!(
      "{} {:?} {:?} {:?} {} {} {} {}",
      node.kind(),
      node.byte_range(),
      node.start_position(),
      node.end_position(),
      node.is_named(),
      node.is_extra(),
      node.is_missing(),
      node.is_error()
    );
    if normalize_continuations && node.kind() == "line_continuation" {
      continuations.push(identity);
    } else {
      output.push(format!("{depth} {:?} {identity}", cursor.field_name()));
    }
    if cursor.goto_first_child() {
      depth += 1;
      continue;
    }
    while !cursor.goto_next_sibling() {
      if !cursor.goto_parent() {
        return (output, continuations);
      }
      depth -= 1;
    }
  }
}

fn source_point(source: &[u8], byte: usize) -> Point {
  let prefix = &source[..byte];
  Point {
    row: prefix.iter().filter(|byte| **byte == b'\n').count(),
    column: byte
      - prefix
        .iter()
        .rposition(|byte| *byte == b'\n')
        .map_or(0, |at| at + 1),
  }
}

fn edit_source(
  parser: &mut Parser,
  source: &mut Vec<u8>,
  tree: &mut Tree,
  byte: usize,
  delete: usize,
  insert: &[u8],
) {
  let start_position = source_point(source, byte);
  let old_end_position = source_point(source, byte + delete);
  source.splice(byte..byte + delete, insert.iter().copied());
  tree.edit(&InputEdit {
    start_byte: byte,
    old_end_byte: byte + delete,
    new_end_byte: byte + insert.len(),
    start_position,
    old_end_position,
    new_end_position: source_point(source, byte + insert.len()),
  });
  *tree = parser.parse(&*source, Some(tree)).unwrap();
}

#[test]
fn parses_valid_source() {
  let source = "echo hello\n";
  let language = grammar::LANGUAGE.into();
  let mut parser = Parser::new();
  parser.set_language(&language).unwrap();
  let tree = parser.parse(source, None).unwrap();
  let root = tree.root_node();
  assert_eq!(root.kind(), "program");
  assert_eq!(root.byte_range(), 0..source.len());
  assert!(!root.has_error());
  assert!(grammar::NODE_TYPES.contains("\"program\""));
  Query::new(&language, grammar::HIGHLIGHTS_QUERY).unwrap();
}

#[test]
fn parses_quoted_here_document_and_highlights_its_terminator() {
  let source = "cat <<'EOF'\n$HOME\nEOF\n";
  let language = grammar::LANGUAGE.into();
  let mut parser = Parser::new();
  parser.set_language(&language).unwrap();
  let tree = parser.parse(source, None).unwrap();
  let root = tree.root_node();
  assert_eq!(root.kind(), "program");
  assert_eq!(root.byte_range(), 0..source.len());
  assert!(!root.has_error(), "{}", root.to_sexp());

  let query = Query::new(&language, grammar::HIGHLIGHTS_QUERY).unwrap();
  let mut cursor = QueryCursor::new();
  let mut captures = cursor.captures(&query, root, source.as_bytes());
  let mut terminators = 0;
  while let Some((matched, index)) = captures.next() {
    let capture = matched.captures()[*index];
    if capture.node.kind() == "here_document_end_text" {
      assert_eq!(query.capture_names()[capture.index as usize], "label");
      assert_eq!(capture.node.byte_range(), 18..21);
      assert_eq!(capture.node.utf8_text(source.as_bytes()).unwrap(), "EOF");
      terminators += 1;
    }
  }
  assert_eq!(terminators, 1);
}

#[test]
fn reused_parsers_reset_recovery_and_resource_state() {
  let delimiter = "D".repeat(1024);
  let mut suspended = ":\n".to_owned();
  for index in 0..64 {
    suspended = format!("cat <<D{index}\n$({suspended})\nD{index}\n");
  }
  let suffix = "printf '終😀' \"${x:-値}\"\n";
  let repaired = format!(":\n{suffix}");
  let expected_repair = parser().parse(&repaired, None).unwrap();
  assert!(!expected_repair.root_node().has_error());
  let mut reused = parser();
  for (name, prefix) in [
    ("invalid-parameter", "echo \"${x @}\"\n".to_owned()),
    (
      "unterminated-heredoc",
      "cat <<'END'\nunterminated\n".to_owned(),
    ),
    (
      "delimiter-capacity",
      format!("cat <<{delimiter}\nbody\n{delimiter}\n"),
    ),
    ("suspended-heredoc-capacity", suspended),
  ] {
    let source = prefix.clone() + suffix;
    let cold = parser().parse(&source, None).unwrap();
    assert!(cold.root_node().has_error(), "{name}");
    let mut tree = reused.parse(&source, None).unwrap();
    assert_eq!(
      cst_snapshot(&cold, false),
      cst_snapshot(&tree, false),
      "{name}"
    );
    let repeated = reused.parse(&source, None).unwrap();
    assert_eq!(
      cst_snapshot(&cold, false),
      cst_snapshot(&repeated, false),
      "{name}"
    );
    let mut bytes = source.into_bytes();
    edit_source(&mut reused, &mut bytes, &mut tree, 0, prefix.len(), b":\n");
    assert_eq!(bytes, repaired.as_bytes());
    assert_eq!(
      cst_snapshot(&expected_repair, true),
      cst_snapshot(&tree, true),
      "{name}"
    );
    let fresh_repair = reused.parse(&bytes, None).unwrap();
    assert_eq!(
      cst_snapshot(&expected_repair, false),
      cst_snapshot(&fresh_repair, false),
      "{name}"
    );
  }
}

#[test]
fn repairs_restore_source_contexts_and_unicode_byte_positions() {
  let mut reused = parser();
  for (name, source, marker, replacement, recovery) in [
    (
      "undecodable-quoted-byte",
      "echo '終😀' \\\nvalue\n",
      "😀",
      &b"\x80"[..],
      false,
    ),
    (
      "parameter-closer",
      "echo \"${v:-終😀}\"\n",
      "}",
      &b"@"[..],
      true,
    ),
    (
      "arithmetic-to-subshell",
      "echo $((1 + ${v:-2}))\n",
      "+",
      &b";"[..],
      false,
    ),
    (
      "nested-backquote-quote",
      "echo `echo \\`echo inner\\``\n",
      "inner",
      &b"'"[..],
      true,
    ),
    (
      "suspended-heredoc-terminator",
      "cat <<OUT\n$(cat <<'IN'\n終😀\nIN\n)\nOUT\n",
      "IN\n",
      &b"\n"[..],
      true,
    ),
    (
      "unicode-row-and-column",
      "printf '終😀'\necho after\n",
      "終😀",
      "é\n行".as_bytes(),
      false,
    ),
  ] {
    let expected = parser().parse(source, None).unwrap();
    assert!(!expected.root_node().has_error(), "{name}");
    let mut bytes = source.as_bytes().to_vec();
    let mut tree = reused.parse(&bytes, None).unwrap();
    let byte = source.find(marker).unwrap();
    edit_source(
      &mut reused,
      &mut bytes,
      &mut tree,
      byte,
      marker.len(),
      replacement,
    );
    let cold = parser().parse(&bytes, None).unwrap();
    assert_eq!(cold.root_node().has_error(), recovery, "{name}");
    let repeated = reused.parse(&bytes, None).unwrap();
    assert_eq!(
      cst_snapshot(&cold, false),
      cst_snapshot(&repeated, false),
      "{name}"
    );
    if !cold.root_node().has_error() {
      assert_eq!(
        cst_snapshot(&cold, true),
        cst_snapshot(&tree, true),
        "{name}"
      );
    }
    edit_source(
      &mut reused,
      &mut bytes,
      &mut tree,
      byte,
      replacement.len(),
      marker.as_bytes(),
    );
    assert_eq!(bytes, source.as_bytes());
    assert_eq!(
      cst_snapshot(&expected, true),
      cst_snapshot(&tree, true),
      "{name}"
    );
  }
}
