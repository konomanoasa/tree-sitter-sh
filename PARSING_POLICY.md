# Parsing Policy

The parser supports the mandatory syntax of [POSIX.1-2024](https://pubs.opengroup.org/onlinepubs/9799919799.2024edition/utilities/V3_chap02.html). Optional syntax,
optional features, and implementation-specific extensions are not supported.
Where POSIX does not uniquely determine the syntax interpretation, the parser
follows [dash 0.5.13.5](https://manpages.debian.org/experimental/dash/dash.1.en.html). Explicit POSIX requirements take precedence.

The following commands can affect how subsequent source is parsed. The parser
recognizes these commands but does not apply their effects.

| Command            | Effects not applied                                 |
| ------------------ | --------------------------------------------------- |
| `alias`, `unalias` | Alias substitution or changes to alias definitions. |
