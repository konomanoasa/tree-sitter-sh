# comment
# <- comment

name=value
# <- variable
#   ^ operator
#    ^^^^^ string

[a]
# <- punctuation.bracket
#^ character
# ^ punctuation.bracket

name=value [a]
#          ^ punctuation.bracket

for item in [a]; do :; done
#           ^ punctuation.bracket

cat >[a]
#    ^ punctuation.bracket

for item in one *.txt; do
# <- keyword
#   ^^^^ variable
#        ^^ keyword
#           ^^^ string
#               ^ character.special
#                ^^^^ string.regexp
#                    ^ punctuation.delimiter
#                      ^^ keyword

  printf '%s\n' "$item"
# ^^^^^^ function.call
#        ^ punctuation.delimiter
#         ^^^^ string
#             ^ punctuation.delimiter
#               ^ punctuation.delimiter
#                ^ constant
#                 ^^^^ variable
#                     ^ punctuation.delimiter
done
# <- keyword

show() {
# <- function
#   ^^ punctuation.bracket
#      ^ punctuation.bracket

  case "$1" in
# ^^^^ keyword
#      ^ punctuation.delimiter
#       ^ constant
#        ^ variable.parameter
#         ^ punctuation.delimiter
#           ^^ keyword

    [!a-c]|[[:alpha:]]) printf '%s\n' "$1" ;;
#   ^ punctuation.bracket
#    ^ punctuation.special
#     ^ character
#      ^ punctuation.special
#       ^ character
#        ^ punctuation.bracket
#         ^ operator
#          ^^ punctuation.bracket
#            ^ punctuation.bracket
#             ^^^^^ character.special
#                  ^^ punctuation.bracket
#                    ^ punctuation.bracket
#                       ^^^^^^ function.call
#                              ^ punctuation.delimiter
#                               ^^^^ string
#                                   ^ punctuation.delimiter
#                                     ^ punctuation.delimiter
#                                      ^ constant
#                                       ^ variable.parameter
#                                        ^ punctuation.delimiter
#                                          ^^ operator

    *) printf '%s\n' "$1" ;&
#   ^ character.special
#    ^ punctuation.bracket
#      ^^^^^^ function.call
#             ^ punctuation.delimiter
#              ^^^^ string
#                  ^ punctuation.delimiter
#                    ^ punctuation.delimiter
#                     ^ constant
#                      ^ variable.parameter
#                       ^ punctuation.delimiter
#                         ^^ operator

  esac
# ^^^^ keyword
}
# <- punctuation.bracket

printf '%s\n' "$((count += 2 * 3))" "${name:-fallback}" "$@" "$10"
# <- function.call
#      ^ punctuation.delimiter
#       ^^^^ string
#           ^ punctuation.delimiter
#              ^^^ punctuation.bracket
#                 ^^^^^ variable
#                       ^^ operator
#                          ^ number
#                            ^ operator
#                              ^ number
#                               ^^ punctuation.bracket
#                                 ^ punctuation.delimiter
#                                    ^^ punctuation.bracket
#                                      ^^^^ variable
#                                          ^^ operator
#                                            ^^^^^^^^ string
#                                                    ^ punctuation.bracket
#                                                     ^ punctuation.delimiter
#                                                       ^ punctuation.delimiter
#                                                        ^ constant
#                                                         ^ variable.builtin
#                                                          ^ punctuation.delimiter

printf '%s\n' "${name#[!a-c]}" file-[[:digit:]]?
# <- function.call
#              ^^ punctuation.bracket
#                ^^^^ variable
#                    ^ operator
#                     ^ punctuation.bracket
#                      ^ punctuation.special
#                       ^ character
#                        ^ punctuation.special
#                         ^ character
#                          ^^ punctuation.bracket
#                            ^ punctuation.delimiter
#                              ^^^^^ string.regexp
#                                   ^^ punctuation.bracket
#                                     ^ punctuation.bracket
#                                      ^^^^^ character.special
#                                           ^^ punctuation.bracket
#                                             ^ punctuation.bracket
#                                              ^ character.special

printf '%s\n' ~user file\ name
# <- function.call
#             ^^^^^ string.special.path
#                   ^^^^ string
#                       ^^ string.escape
#                         ^^^^ string

printf '%s\n' $'a\n' "a\$b" \*
# <- function.call
#             ^^ punctuation.delimiter
#               ^ string
#                ^^ string.escape
#                  ^ punctuation.delimiter
#                    ^ punctuation.delimiter
#                     ^ string
#                      ^^ string.escape
#                        ^ string
#                         ^ punctuation.delimiter
#                           ^^ string.escape

printf '%s\n' "$(date)" `date`
# <- function.call
#             ^ punctuation.delimiter
#              ^^ punctuation.bracket
#                ^^^^ function.call
#                    ^ punctuation.bracket
#                     ^ punctuation.delimiter
#                       ^ punctuation.delimiter
#                        ^^^^ function.call
#                            ^ punctuation.delimiter

! true && false || true | cat &
# <- operator
# ^^^^ function.call
#      ^^ operator
#         ^^^^^ function.call
#               ^^ operator
#                  ^^^^ function.call
#                       ^ operator
#                         ^^^ function.call
#                             ^ operator

cat 2>output
# <- function.call
#   ^ number
#    ^ operator
#     ^^^^^^ string

printf '%s' \
  value
# ^^^^^ string

cat <<'EOF'
$name \$ text
EOF
