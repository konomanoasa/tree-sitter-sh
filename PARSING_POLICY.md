# Parsing Policy

The parser follows [POSIX.1-2024](https://pubs.opengroup.org/onlinepubs/9799919799.2024edition/utilities/V3_chap02.html)
mandatory syntax, excluding optional features and extensions. Syntax not
determined by POSIX follows [dash 0.5.13.5](https://manpages.debian.org/experimental/dash/dash.1.en.html).

These commands are parsed without changing how subsequent source is parsed.

| Command            | Effects not applied                 |
| ------------------ | ----------------------------------- |
| `alias`, `unalias` | Alias substitution and definitions. |
