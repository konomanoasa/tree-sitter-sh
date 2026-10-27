# tree-sitter-sh

[![CI](https://github.com/konomanoasa/tree-sitter-sh/actions/workflows/ci.yaml/badge.svg)](https://github.com/konomanoasa/tree-sitter-sh/actions/workflows/ci.yaml)
[![crates.io](https://img.shields.io/crates/v/tree-sitter-sh)](https://crates.io/crates/tree-sitter-sh)
[![npm](https://img.shields.io/npm/v/tree-sitter-sh)](https://www.npmjs.com/package/tree-sitter-sh)

[Tree-sitter](https://tree-sitter.github.io/tree-sitter/) grammar for the
POSIX.1-2024 Shell Command Language.

For syntax error analysis, see [tree-sitter-diagnostic-sh](https://github.com/konomanoasa/tree-sitter-diagnostic-sh).

## Installation

```sh
npm install tree-sitter-sh
```

## Parsing Policy

[Parsing Policy](PARSING_POLICY.md)

## Development

```sh
npm install
npm run build
npm test
```

## Specification

[POSIX.1-2024 Shell Command Language](https://pubs.opengroup.org/onlinepubs/9799919799.2024edition/utilities/V3_chap02.html)

## License

[MIT](LICENSE)

## Used By

- [sh-ts-mode](https://github.com/konomanoasa/sh-ts-mode)
- [shellsmith](https://github.com/konomanoasa/shellsmith)
