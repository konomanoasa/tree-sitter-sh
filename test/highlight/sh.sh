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

[a$]
# ^ character
#  ^ punctuation.bracket

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
#                ^ variable
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
#       ^ variable
#        ^ variable.parameter
#         ^ punctuation.delimiter
#           ^^ keyword

    [!a-c]|[[:alpha:]]) printf '%s\n' "$1" ;;
#   ^ punctuation.bracket
#    ^ operator
#     ^ character
#      ^ operator
#       ^ character
#        ^ punctuation.bracket
#         ^ operator
#          ^^ punctuation.bracket
#            ^ punctuation.delimiter
#             ^^^^^ character.special
#                  ^ punctuation.delimiter
#                   ^ punctuation.bracket
#                    ^ punctuation.bracket
#                       ^^^^^^ function.call
#                              ^ punctuation.delimiter
#                               ^^^^ string
#                                   ^ punctuation.delimiter
#                                     ^ punctuation.delimiter
#                                      ^ variable
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
#                     ^ variable
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
#              ^ punctuation.special
#               ^^ punctuation.bracket
#                 ^^^^^ variable
#                       ^^ operator
#                          ^ number
#                            ^ operator
#                              ^ number
#                               ^^ punctuation.bracket
#                                 ^ punctuation.delimiter
#                                    ^ variable
#                                     ^ punctuation.bracket
#                                      ^^^^ variable
#                                          ^^ operator
#                                            ^^^^^^^^ string
#                                                    ^ punctuation.bracket
#                                                     ^ punctuation.delimiter
#                                                       ^ punctuation.delimiter
#                                                        ^ variable
#                                                         ^ variable.builtin
#                                                          ^ punctuation.delimiter

printf '%s\n' "${name#[!a-c]}" file-[[:digit:]]?
# <- function.call
#              ^ variable
#               ^ punctuation.bracket
#                ^^^^ variable
#                    ^ operator
#                     ^ punctuation.bracket
#                      ^ operator
#                       ^ character
#                        ^ operator
#                         ^ character
#                          ^^ punctuation.bracket
#                            ^ punctuation.delimiter
#                              ^^^^^ string.regexp
#                                   ^^ punctuation.bracket
#                                     ^ punctuation.delimiter
#                                      ^^^^^ character.special
#                                           ^ punctuation.delimiter
#                                            ^ punctuation.bracket
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
#              ^ punctuation.special
#               ^ punctuation.bracket
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

cat {fd}>output
# <- function.call
#   ^^^^ string
#       ^ operator
#        ^^^^^^ string

case value in [[.hyphen.]][[=e=]]) : ;; esac
# <- keyword
#    ^^^^^ string
#          ^^ keyword
#             ^ punctuation.bracket
#              ^ punctuation.bracket
#               ^ punctuation.delimiter
#                ^^^^^^ character.special
#                      ^ punctuation.delimiter
#                       ^ punctuation.bracket
#                        ^ punctuation.bracket
#                         ^ punctuation.bracket
#                          ^ punctuation.bracket
#                           ^ punctuation.delimiter
#                            ^ character.special
#                             ^ punctuation.delimiter
#                              ^ punctuation.bracket
#                               ^ punctuation.bracket
#                                ^ punctuation.bracket
#                                  ^ function.call
#                                    ^^ operator
#                                       ^^^^ keyword

printf '%s' \
  value
# ^^^^^ string

cat <<'EOF'
$name \$ text
EOF
# <- label

# command name: asterisk
pre"quoted"mid*tail'quoted'end
# <- string.regexp
#          ^^^ string.regexp
#              ^^^^ string.regexp
#                          ^^^ string.regexp
#   ^^^^^^ !string.regexp

# command name: question mark
pre"quoted"mid?tail'quoted'end
# <- string.regexp
#          ^^^ string.regexp
#              ^^^^ string.regexp
#                          ^^^ string.regexp
#   ^^^^^^ !string.regexp

# command name: bracket expression
pre"quoted"mid[a]tail'quoted'end
# <- string.regexp
#          ^^^ string.regexp
#                ^^^^ string.regexp
#                            ^^^ string.regexp
#   ^^^^^^ !string.regexp

# command after assignment: asterisk
name=value pre"quoted"mid*tail'quoted'end
#          ^^^ string.regexp
#                     ^^^ string.regexp
#                         ^^^^ string.regexp
#                                     ^^^ string.regexp
#              ^^^^^^ !string.regexp

# command after assignment: question mark
name=value pre"quoted"mid?tail'quoted'end
#          ^^^ string.regexp
#                     ^^^ string.regexp
#                         ^^^^ string.regexp
#                                     ^^^ string.regexp
#              ^^^^^^ !string.regexp

# command after assignment: bracket expression
name=value pre"quoted"mid[a]tail'quoted'end
#          ^^^ string.regexp
#                     ^^^ string.regexp
#                           ^^^^ string.regexp
#                                       ^^^ string.regexp
#              ^^^^^^ !string.regexp

# command argument: asterisk
echo pre"quoted"mid*tail'quoted'end
#    ^^^ string.regexp
#               ^^^ string.regexp
#                   ^^^^ string.regexp
#                               ^^^ string.regexp
#        ^^^^^^ !string.regexp

# command argument: question mark
echo pre"quoted"mid?tail'quoted'end
#    ^^^ string.regexp
#               ^^^ string.regexp
#                   ^^^^ string.regexp
#                               ^^^ string.regexp
#        ^^^^^^ !string.regexp

# command argument: bracket expression
echo pre"quoted"mid[a]tail'quoted'end
#    ^^^ string.regexp
#               ^^^ string.regexp
#                     ^^^^ string.regexp
#                                 ^^^ string.regexp
#        ^^^^^^ !string.regexp

# for word list: asterisk
for item in pre"quoted"mid*tail'quoted'end; do :; done
#           ^^^ string.regexp
#                      ^^^ string.regexp
#                          ^^^^ string.regexp
#                                      ^^^ string.regexp
#               ^^^^^^ !string.regexp

# for word list: question mark
for item in pre"quoted"mid?tail'quoted'end; do :; done
#           ^^^ string.regexp
#                      ^^^ string.regexp
#                          ^^^^ string.regexp
#                                      ^^^ string.regexp
#               ^^^^^^ !string.regexp

# for word list: bracket expression
for item in pre"quoted"mid[a]tail'quoted'end; do :; done
#           ^^^ string.regexp
#                      ^^^ string.regexp
#                            ^^^^ string.regexp
#                                        ^^^ string.regexp
#               ^^^^^^ !string.regexp

# redirection filename: asterisk
cat >pre"quoted"mid*tail'quoted'end
#    ^^^ string.regexp
#               ^^^ string.regexp
#                   ^^^^ string.regexp
#                               ^^^ string.regexp
#        ^^^^^^ !string.regexp

# redirection filename: question mark
cat >pre"quoted"mid?tail'quoted'end
#    ^^^ string.regexp
#               ^^^ string.regexp
#                   ^^^^ string.regexp
#                               ^^^ string.regexp
#        ^^^^^^ !string.regexp

# redirection filename: bracket expression
cat >pre"quoted"mid[a]tail'quoted'end
#    ^^^ string.regexp
#               ^^^ string.regexp
#                     ^^^^ string.regexp
#                                 ^^^ string.regexp
#        ^^^^^^ !string.regexp

# Multiple markers share literal captures
echo pre*mid?tail[a]end
#    ^^^ string.regexp
#        ^^^ string.regexp
#            ^^^^ string.regexp
#                   ^^^ string.regexp

# Substitutions separate literals without inheriting pattern captures
echo pre$(printf x)mid*tail${value}end
#    ^^^ string.regexp
#                  ^^^ string.regexp
#                      ^^^^ string.regexp
#                                  ^^^ string.regexp
#         ^^^^^^ !string.regexp
#                ^ !string.regexp
#                          ^^^^^ !string.regexp

# Plain words stay strings
echo pre"quoted"tail
#    ^^^ !string.regexp
#               ^^^^ !string.regexp

# Quoted and escaped markers do not activate patterns
echo pre'*'tail pre\*tail
#    ^^^ !string.regexp
#          ^^^^ !string.regexp
#              ^^^ !string.regexp
#                   ^^^^ !string.regexp

# Assignments do not activate pathname patterns
name=pre*tail
#    ^^^ !string.regexp
#        ^^^^ !string.regexp

# Escaped members retain the brackets around their source
echo [\a]
#    ^ punctuation.bracket
#     ^^ string.escape
#       ^ punctuation.bracket

# Command patterns retain a range ending in an escape
pre[a-\c]
#  ^ punctuation.bracket
#   ^ character
#    ^ operator
#     ^^ string.escape
#       ^ punctuation.bracket

# Commands after assignments retain a quoted range endpoint
name=value pre['a'-c]
#             ^ punctuation.bracket
#              ^ punctuation.delimiter
#               ^ string
#                 ^ operator
#                  ^ character
#                   ^ punctuation.bracket

# Word lists retain collating symbols at both range endpoints
for item in [[.a.]-[.z.]]; do :; done
#           ^^ punctuation.bracket
#             ^ punctuation.delimiter
#              ^ character.special
#                 ^ operator
#                    ^ character.special
#                      ^^ punctuation.bracket

# Filenames retain class markers around a substituted class name
cat >[[:${kind}:]]
#    ^^ punctuation.bracket
#      ^ punctuation.delimiter
#       ^ variable
#         ^^^^ variable
#              ^ punctuation.delimiter
#               ^^ punctuation.bracket

# Case patterns retain collating markers around substitutions
case value in [[.$element.]]) :;; esac
#             ^^ punctuation.bracket
#               ^ punctuation.delimiter
#                ^ variable
#                 ^^^^^^^ variable
#                        ^ punctuation.delimiter
#                         ^^ punctuation.bracket

# Removal patterns retain equivalence markers around substitutions
echo "${name#[[=$element=]]}"
#            ^^ punctuation.bracket
#              ^ punctuation.delimiter
#               ^ variable
#                ^^^^^^^ variable
#                       ^ punctuation.delimiter
#                        ^^ punctuation.bracket

# Here-document delimiters keep labels, quotes, and escapes distinct
cat <<*?[!a-z[:alpha:][.x.][=y=]]
body
*?[!a-z[:alpha:][.x.][=y=]]
# <- label
#^^^^^^^^^^^^^^^^^^^^^^^^^^ label

cat <<[[.a.]-[.z.]]
body
[[.a.]-[.z.]]
# <- label
#^^^^^^^^^^^^ label

cat <<[\a]
body
[a]
# <- label
#^^ label

cat <<['a'-"z"]
body
[a-z]
# <- label
#^^^^ label

cat <<[[:'alpha':][."x".][=$'y'=]]
body
[[:alpha:][.x.][=y=]]
# <- label
#^^^^^^^^^^^^^^^^^^^^ label

cat <<CONTINUED
\
CONTINUED
# <- label
#^^^^^^^^ label

cat <<-STRIPPED
	\
STRIPPED

# Pattern captures stay within their owning word
pre*tail plain
# <- string.regexp
#^^ string.regexp
#   ^^^^ string.regexp
#        ^^^^^ string

name=value pre?tail plain
#    ^^^^^ string
#          ^^^ string.regexp
#              ^^^^ string.regexp
#                   ^^^^^ string

echo plain pre[a]tail plain
#    ^^^^^ string
#          ^^^ string.regexp
#                ^^^^ string.regexp
#                     ^^^^^ string

for item in plain pre*tail plain; do :; done
#           ^^^^^ string
#                 ^^^ string.regexp
#                     ^^^^ string.regexp
#                          ^^^^^ string

cat >pre?tail
# <- function.call
#^^ function.call
#    ^^^ string.regexp
#        ^^^^ string.regexp

echo ~a*b ~a?b ~[!a-z[:alpha:][.x.][=y=]-] ~[[.a.]-[.z.]] ~[-a]
#    ^^^^ string.special.path
#         ^^^^ string.special.path
#              ^^^^^^^^^^^^^^^^^^^^^^^^^^^ string.special.path
#                                          ^^^^^^^^^^^^^^ string.special.path
#                                                         ^^^^^ string.special.path

name=~a*b:~[a-z]
#    ^^^^ string.special.path
#         ^^^^^^ string.special.path

echo ${x:-${y:-~a?b}}
#              ^^^^ string.special.path

echo ~[a-\c] ~[[.$element.]-[."z".]] ~['q'] ~[$(printf x)]
#    ^^^^ string.special.path
#        ^^ string.escape
#          ^ string.special.path
#            ^^^^ string.special.path
#                ^^^^^^^^ variable
#                        ^^^^^ string.special.path
#                             ^ punctuation.delimiter
#                              ^ string
#                               ^ punctuation.delimiter
#                                ^^^ string.special.path
#                                    ^^ string.special.path
#                                      ^ punctuation.delimiter
#                                       ^ string
#                                        ^ punctuation.delimiter
#                                         ^ string.special.path
#                                           ^^ string.special.path
#                                             ^ punctuation.special
#                                              ^ punctuation.bracket
#                                               ^^^^^^ function.call
#                                                      ^ string
#                                                       ^ punctuation.bracket
#                                                        ^ string.special.path

echo ~a*b/pre*tail
#    ^^^^ string.special.path
#        ^^^^ string.regexp
#            ^ character.special
#             ^^^^ string.regexp
