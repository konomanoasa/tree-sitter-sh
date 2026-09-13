//! POSIX.1-2024 Shell Command Language support for Tree-sitter.
//!
//! ```
//! let mut parser = tree_sitter::Parser::new();
//! parser.set_language(&tree_sitter_sh::LANGUAGE.into()).unwrap();
//! let tree = parser.parse("echo hello\n", None).unwrap();
//! assert!(!tree.root_node().has_error());
//! ```

use tree_sitter_language::LanguageFn;

unsafe extern "C" {
    fn tree_sitter_sh() -> *const ();
}

/// Grammar entry point for `tree_sitter::Parser::set_language`.
pub const LANGUAGE: LanguageFn = unsafe { LanguageFn::from_raw(tree_sitter_sh) };

/// Public node types and fields in Tree-sitter's JSON format.
pub const NODE_TYPES: &str = include_str!("../../src/node-types.json");

/// Highlight query using standard Tree-sitter and Neovim captures.
pub const HIGHLIGHTS_QUERY: &str = include_str!("../../queries/highlights.scm");

#[cfg(test)]
mod tests {
    use tree_sitter::{Parser, Query, QueryCursor, StreamingIterator};

    #[test]
    fn parses_quoted_here_document_and_highlights_its_terminator() {
        let source = "cat <<'EOF'\n$HOME\nEOF\n";
        let language = super::LANGUAGE.into();
        let mut parser = Parser::new();
        parser.set_language(&language).unwrap();
        let tree = parser.parse(source, None).unwrap();
        let root = tree.root_node();
        assert_eq!(root.kind(), "program");
        assert_eq!(root.byte_range(), 0..source.len());
        assert!(!root.has_error(), "{}", root.to_sexp());

        let query = Query::new(&language, super::HIGHLIGHTS_QUERY).unwrap();
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
}
