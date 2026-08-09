# tree-sitter-posix-sh

A [Tree-sitter](https://tree-sitter.github.io/tree-sitter/) grammar for the
POSIX.1-2024 Shell Command Language.

## Usage

### Emacs

Add the grammar repository to `treesit-language-source-alist` and install it:

```elisp
(add-to-list
 'treesit-language-source-alist
 '(posix_sh
   . ("https://github.com/konomanoasa/tree-sitter-posix-sh")))

(treesit-install-language-grammar 'posix_sh)
```

[sh-ts-mode](https://github.com/konomanoasa/sh-ts-mode) provides a Tree-sitter
major mode using this grammar.

## Language server

[sh-language-server](https://github.com/konomanoasa/sh-language-server) uses
this grammar for parsing and provides completion, diagnostics, formatting,
hover information, and semantic tokens.

## Specification

- [POSIX.1-2024 Shell Command Language](https://pubs.opengroup.org/onlinepubs/9799919799.2024edition/utilities/V3_chap02.html)

## License

[MIT](LICENSE)
