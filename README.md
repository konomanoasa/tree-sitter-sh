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

Emacs does not include a Tree-sitter major mode for `posix_sh`; define a
custom major mode that uses the parser.

## Specification

- [POSIX.1-2024 Shell Command Language](https://pubs.opengroup.org/onlinepubs/9799919799.2024edition/utilities/V3_chap02.html)

## License

[MIT](LICENSE)
