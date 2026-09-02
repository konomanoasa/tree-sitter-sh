//! POSIX shell grammar for the Tree-sitter parsing library.
//!
//! ```
//! let mut parser = tree_sitter::Parser::new();
//! parser
//!     .set_language(&tree_sitter_sh::LANGUAGE.into())
//!     .expect("POSIX shell grammar must load");
//! let tree = parser
//!     .parse("echo hello\n", None)
//!     .expect("parser must return a tree");
//! assert!(!tree.root_node().has_error());
//! ```

use tree_sitter_language::LanguageFn;

unsafe extern "C" {
    fn tree_sitter_sh() -> *const ();
}

/// The Tree-sitter [`LanguageFn`] for POSIX shell.
pub const LANGUAGE: LanguageFn = unsafe { LanguageFn::from_raw(tree_sitter_sh) };

/// The node type definitions for [`LANGUAGE`].
pub const NODE_TYPES: &str = include_str!("../../src/node-types.json");

/// The syntax highlighting query for [`LANGUAGE`].
pub const HIGHLIGHTS_QUERY: &str = include_str!("../../queries/highlights.scm");

#[cfg(test)]
mod tests {
    #[test]
    fn grammar_loads_and_parses() {
        let mut parser = tree_sitter::Parser::new();
        parser
            .set_language(&super::LANGUAGE.into())
            .expect("POSIX shell grammar must load");
        let tree = parser
            .parse("echo hello\n", None)
            .expect("parser must return a tree");
        assert_eq!(tree.root_node().kind(), "program");
        assert!(!tree.root_node().has_error());
    }
}
