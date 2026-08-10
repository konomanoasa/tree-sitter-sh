#!/bin/sh

# This runner stores exact POSIX sh source, including intentionally incomplete
# input, in single-quoted fixture strings; expansion or quote rewrites would
# change the parser input under test.
# shellcheck disable=SC1003,SC2016

set -eu

repository_directory=$(
  CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd
)
tree_sitter="$repository_directory/node_modules/.bin/tree-sitter"
runtime_directory=$(
  mktemp -d "${TMPDIR:-/tmp}/tree-sitter-posix-sh-runtime.XXXXXX"
)
parser_library="$runtime_directory/parser"

cleanup() {
  find "$runtime_directory" -depth -delete
}
trap cleanup EXIT HUP INT TERM

"$tree_sitter" build \
  --output "$parser_library" \
  "$repository_directory"

parse_current() {
  "$tree_sitter" parse \
    --lib-path "$parser_library" \
    --lang-name posix_sh \
    "$@"
}

fail() {
  printf '%s\n' "$1" >&2
  exit 1
}

assert_contains() {
  expected=$1
  file=$2
  if ! grep -Fq -- "$expected" "$file"; then
    printf '%s\n' "Expected parse output to contain: $expected" >&2
    sed -n '1,200p' "$file" >&2
    exit 1
  fi
}

assert_not_contains() {
  unexpected=$1
  file=$2
  if grep -Fq -- "$unexpected" "$file"; then
    printf '%s\n' "Expected parse output not to contain: $unexpected" >&2
    sed -n '1,200p' "$file" >&2
    exit 1
  fi
}

assert_valid() {
  source_file=$1
  output_file="$runtime_directory/assert-valid.out"
  if ! parse_current \
    --quiet \
    --timeout 10000000 \
    "$source_file" \
    >"$output_file" 2>&1; then
    sed -n '1,200p' "$output_file" >&2
    fail "Expected valid parse: $source_file"
  fi
}

assert_valid_with_output() {
  source_file=$1
  output_file=$2
  if ! parse_current \
    --timeout 10000000 \
    "$source_file" \
    >"$output_file" 2>/dev/null; then
    fail "Expected valid parse: $source_file"
  fi
}

assert_cst_valid_with_output() {
  source_file=$1
  output_file=$2
  if ! parse_current \
    --cst \
    --timeout 10000000 \
    "$source_file" \
    >"$output_file" 2>/dev/null; then
    fail "Expected valid CST parse: $source_file"
  fi
}

assert_cst_range() {
  expected_range=$1
  expected_item=$2
  file=$3
  if ! grep -F -- "$expected_range" "$file" |
    grep -Fq -- "$expected_item"; then
    printf '%s\n' \
      "Expected CST item $expected_item at $expected_range" >&2
    sed -n '1,200p' "$file" >&2
    exit 1
  fi
}

assert_invalid_with_output() {
  source_file=$1
  output_file=$2
  if parse_current "$source_file" >"$output_file" 2>/dev/null; then
    fail "Expected syntax error: $source_file"
  fi
}

normalize_parse_cst() {
  input_file=$1
  output_file=$2
  awk '
    /^[^[:space:](].*[[:space:]]Parse:[[:space:]]/ { next }
    /^[[:space:]]*Edit:[[:space:]]/ { next }
    { print }
  ' "$input_file" >"$output_file"
}

assert_incremental_equals_fresh_status() {
  expected_status=$1
  shift
  initial_file=$1
  final_file=$2
  test_name=$3
  shift 3

  initial_output="$runtime_directory/$test_name.initial"
  incremental_output="$runtime_directory/$test_name.incremental"
  fresh_output="$runtime_directory/$test_name.fresh"
  incremental_cst="$incremental_output.cst"
  fresh_cst="$fresh_output.cst"
  initial_cst="$initial_output.cst"

  if parse_current \
    --edits "$@" \
    -- "$initial_file" \
    >"$incremental_output" 2>/dev/null; then
    incremental_status=valid
  else
    incremental_status=invalid
  fi
  if parse_current \
    "$final_file" \
    >"$fresh_output" 2>/dev/null; then
    fresh_status=valid
  else
    fresh_status=invalid
  fi
  parse_current \
    "$initial_file" \
    >"$initial_output" 2>/dev/null || true
  normalize_parse_cst "$incremental_output" "$incremental_cst"
  normalize_parse_cst "$fresh_output" "$fresh_cst"
  normalize_parse_cst "$initial_output" "$initial_cst"

  if [ "$incremental_status" != "$fresh_status" ]; then
    fail "Incremental and fresh statuses differ: $test_name"
  fi

  if [ "$fresh_status" != "$expected_status" ]; then
    fail "Expected $expected_status incremental and fresh parses: $test_name"
  fi

  if ! cmp -s "$incremental_cst" "$fresh_cst"; then
    diff -u "$fresh_cst" "$incremental_cst" >&2 || true
    fail "Incremental and fresh trees differ: $test_name"
  fi

  if cmp -s "$initial_cst" "$fresh_cst"; then
    fail "Incremental regression does not change the CST: $test_name"
  fi
}

assert_incremental_equals_fresh() {
  assert_incremental_equals_fresh_status valid "$@"
}

assert_invalid_incremental_equals_fresh() {
  assert_incremental_equals_fresh_status invalid "$@"
}

assert_valid_incremental_equals_fresh() {
  final_file=$2
  assert_valid "$final_file"
  assert_incremental_equals_fresh "$@"
}

comments_source="$runtime_directory/comments.sh"
comments_output="$runtime_directory/comments.out"
printf '%s\n' \
  "  # leading" \
  "command # trailing" \
  "cat <<EOF # declaration" \
  "body" \
  "EOF" \
  >"$comments_source"
parse_current "$comments_source" >"$comments_output" 2>/dev/null
assert_contains "(comment [0, 2] - [0, 11])" "$comments_output"
assert_contains "(comment [1, 8] - [1, 18])" "$comments_output"
assert_contains "comment: (comment [2, 10] - [2, 23])" "$comments_output"

continued_comments_source="$runtime_directory/continued-comments.sh"
continued_comments_output="$runtime_directory/continued-comments.out"
printf '%s\n' \
  '\' \
  '# leading' \
  'first && \' \
  '# operator' \
  'second' \
  'cat <<EOF \' \
  '# declaration' \
  'body' \
  'EOF' \
  >"$continued_comments_source"
assert_valid_with_output \
  "$continued_comments_source" \
  "$continued_comments_output"
assert_contains \
  "(line_continuation [0, 0] - [1, 0])" \
  "$continued_comments_output"
assert_contains \
  "(comment [1, 0] - [1, 9])" \
  "$continued_comments_output"
assert_contains \
  "(line_continuation [2, 9] - [3, 0])" \
  "$continued_comments_output"
assert_contains \
  "(comment [3, 0] - [3, 10])" \
  "$continued_comments_output"
assert_contains \
  "(line_continuation [5, 10] - [6, 0])" \
  "$continued_comments_output"
assert_contains \
  "comment: (comment [6, 0] - [6, 13])" \
  "$continued_comments_output"
assert_not_contains "ERROR" "$continued_comments_output"

continued_comment_initial="$runtime_directory/continued-comment-initial.sh"
continued_comment_final="$runtime_directory/continued-comment-final.sh"
printf '%s\n' 'first && # comment' 'second' >"$continued_comment_initial"
printf '%s\n' 'first && \' '# comment' 'second' >"$continued_comment_final"
assert_incremental_equals_fresh \
  "$continued_comment_initial" \
  "$continued_comment_final" \
  "insert-comment-boundary-continuation" \
  '9 0 \
'
assert_incremental_equals_fresh \
  "$continued_comment_final" \
  "$continued_comment_initial" \
  "delete-comment-boundary-continuation" \
  "9 2"

recovery_source="$runtime_directory/recovery-redirection.sh"
recovery_output="$runtime_directory/recovery-redirection.out"
printf '%s\n' "before >;" "after" >"$recovery_source"
assert_valid_with_output "$recovery_source" "$recovery_output"
assert_contains \
  "recovery: (redirection_target_recovery [0, 8] - [0, 8])" \
  "$recovery_output"
assert_contains "(complete_command [1, 0] - [1, 5]" "$recovery_output"

and_or_source="$runtime_directory/recovery-and-or.sh"
and_or_output="$runtime_directory/recovery-and-or.out"
printf '%s\n' "before" "after &&" >"$and_or_source"
assert_valid_with_output "$and_or_source" "$and_or_output"
assert_contains "(complete_command [0, 0] - [0, 6]" "$and_or_output"
assert_contains "operator: (and_if [1, 6] - [1, 8])" "$and_or_output"
assert_contains \
  "recovery: (command_recovery [2, 0] - [2, 0])" \
  "$and_or_output"

compound_source="$runtime_directory/recovery-compound.sh"
compound_output="$runtime_directory/recovery-compound.out"
printf '%s\n' \
  "before" \
  "if condition; then" \
  "  inside" \
  >"$compound_source"
assert_valid_with_output "$compound_source" "$compound_output"
assert_contains "(complete_command [0, 0] - [0, 6]" "$compound_output"
assert_contains "(and_or [2, 2] - [2, 8]" "$compound_output"
assert_contains \
  "recovery: (compound_command_recovery [3, 0] - [3, 0])" \
  "$compound_output"

subshell_source="$runtime_directory/recovery-subshell.sh"
subshell_output="$runtime_directory/recovery-subshell.out"
printf '%s\n' "(inside" >"$subshell_source"
assert_invalid_with_output "$subshell_source" "$subshell_output"
assert_contains "(ERROR [0, 0] - [1, 0]" "$subshell_output"
assert_contains "(and_or [0, 1] - [0, 7]" "$subshell_output"

right_brace_source="$runtime_directory/recovery-right-brace.sh"
right_brace_output="$runtime_directory/recovery-right-brace.out"
printf '%s\n' "first; }" >"$right_brace_source"
assert_invalid_with_output "$right_brace_source" "$right_brace_output"
assert_contains "(ERROR [0, 5] - [1, 0]" "$right_brace_output"
assert_not_contains "(word [0, 7] - [0, 8]" \
  "$right_brace_output"

io_location_source="$runtime_directory/recovery-io-location.sh"
io_location_output="$runtime_directory/recovery-io-location.out"
printf '%s\n' "printf {x'}>file" >"$io_location_source"
assert_invalid_with_output "$io_location_source" "$io_location_output"
assert_not_contains "(io_location " "$io_location_output"

delimiter_source="$runtime_directory/source-delimiters.sh"
delimiter_output="$runtime_directory/source-delimiters.out"
printf '%s\n' \
  'printf x$x' \
  'echo `inner`' \
  'echo `printf \`nested\` \$name`' \
  >"$delimiter_source"
assert_cst_valid_with_output "$delimiter_source" "$delimiter_output"
assert_cst_range '0:8  - 0:9' '"$"' "$delimiter_output"
assert_cst_range '1:5  - 1:6' '"\`"' "$delimiter_output"
assert_cst_range '1:11 - 1:12' '"\`"' "$delimiter_output"
assert_cst_range '2:5  - 2:6' '"\`"' "$delimiter_output"
assert_cst_range '2:13 - 2:14' '"\\"' "$delimiter_output"
assert_cst_range '2:14 - 2:15' '"\`"' "$delimiter_output"
assert_cst_range '2:21 - 2:22' '"\\"' "$delimiter_output"
assert_cst_range '2:22 - 2:23' '"\`"' "$delimiter_output"
assert_cst_range '2:24 - 2:25' '"\\"' "$delimiter_output"
assert_cst_range '2:25 - 2:26' '"$"' "$delimiter_output"
assert_cst_range '2:30 - 2:31' '"\`"' "$delimiter_output"

parameter_delimiter_initial="$runtime_directory/parameter-delimiter-initial.sh"
parameter_delimiter_final="$runtime_directory/parameter-delimiter-final.sh"
printf '%s\n' 'printf x%' >"$parameter_delimiter_initial"
printf '%s\n' 'printf x$x' >"$parameter_delimiter_final"
assert_incremental_equals_fresh \
  "$parameter_delimiter_initial" \
  "$parameter_delimiter_final" \
  "parameter-delimiter" \
  '8 1 $x'

backquote_delimiter_initial="$runtime_directory/backquote-delimiter-initial.sh"
backquote_delimiter_final="$runtime_directory/backquote-delimiter-final.sh"
printf '%s\n' 'echo [inner]' >"$backquote_delimiter_initial"
printf '%s\n' 'echo `inner`' >"$backquote_delimiter_final"
assert_incremental_equals_fresh \
  "$backquote_delimiter_initial" \
  "$backquote_delimiter_final" \
  "backquote-delimiter" \
  '5 1 `' \
  '11 1 `'

boundary_source="$runtime_directory/here-document-boundary.sh"
boundary_output="$runtime_directory/here-document-boundary.out"
printf '%s\n' \
  "cat <<EOF" \
  '$(' \
  "EOF" \
  ")" \
  "EOF" \
  "after" \
  >"$boundary_source"
assert_invalid_with_output "$boundary_source" "$boundary_output"
assert_contains \
  "(here_document_end_recovery [1, 2] - [2, 0])" \
  "$boundary_output"
assert_contains "end: (here_document_end [2, 0] - [3, 0])" "$boundary_output"

continued_delimiter_initial="$runtime_directory/continued-delimiter-initial.sh"
continued_delimiter_final="$runtime_directory/continued-delimiter-final.sh"
continued_delimiter_output="$runtime_directory/continued-delimiter.out"
printf '%s\n' \
  "cat <<EOF" \
  "body" \
  "EOF" \
  "after" \
  >"$continued_delimiter_initial"
printf '%s\n' \
  "cat <<EO\\" \
  "F" \
  "body" \
  "EOF" \
  "after" \
  >"$continued_delimiter_final"
assert_cst_valid_with_output \
  "$continued_delimiter_final" \
  "$continued_delimiter_output"
assert_cst_range \
  "0:6 - 1:1" \
  "end: here_end" \
  "$continued_delimiter_output"
assert_cst_range \
  "0:6 - 1:1" \
  "word: word" \
  "$continued_delimiter_output"
assert_cst_range \
  "0:8 - 1:0" \
  "line_continuation" \
  "$continued_delimiter_output"
assert_cst_range \
  "3:0 - 4:0" \
  "end: here_document_end" \
  "$continued_delimiter_output"
assert_not_contains "ERROR" "$continued_delimiter_output"
assert_incremental_equals_fresh \
  "$continued_delimiter_initial" \
  "$continued_delimiter_final" \
  "insert-continued-here-document-delimiter" \
  '8 0 \
'
assert_incremental_equals_fresh \
  "$continued_delimiter_final" \
  "$continued_delimiter_initial" \
  "delete-continued-here-document-delimiter" \
  "8 2"

delimiter_boundary_initial="$runtime_directory/delimiter-boundary-initial.sh"
delimiter_boundary_final="$runtime_directory/delimiter-boundary-final.sh"
delimiter_boundary_output="$runtime_directory/delimiter-boundary.out"
printf '%s\n' \
  "cat <<EOF" \
  "body" \
  "EOF" \
  "after" \
  >"$delimiter_boundary_initial"
printf '%s\n' \
  "cat <<\\" \
  "EOF" \
  "body" \
  "EOF" \
  "after" \
  >"$delimiter_boundary_final"
assert_cst_valid_with_output \
  "$delimiter_boundary_final" \
  "$delimiter_boundary_output"
assert_cst_range \
  "0:4 - 0:6" \
  "operator: dless" \
  "$delimiter_boundary_output"
assert_cst_range \
  "0:6 - 1:0" \
  "line_continuation" \
  "$delimiter_boundary_output"
assert_cst_range \
  "1:0 - 1:3" \
  "end: here_end" \
  "$delimiter_boundary_output"
assert_cst_range \
  "1:0 - 1:3" \
  "word: word" \
  "$delimiter_boundary_output"
assert_cst_range \
  "3:0 - 4:0" \
  "end: here_document_end" \
  "$delimiter_boundary_output"
assert_cst_range \
  "4:0 - 4:5" \
  "command: complete_command" \
  "$delimiter_boundary_output"
assert_not_contains "ERROR" "$delimiter_boundary_output"
assert_incremental_equals_fresh \
  "$delimiter_boundary_initial" \
  "$delimiter_boundary_final" \
  "insert-here-document-delimiter-boundary-continuation" \
  '6 0 \
'
assert_incremental_equals_fresh \
  "$delimiter_boundary_final" \
  "$delimiter_boundary_initial" \
  "delete-here-document-delimiter-boundary-continuation" \
  "6 2"

nested_initial="$runtime_directory/nested-initial.sh"
nested_final="$runtime_directory/nested-final.sh"
printf '%s\n' \
  "cat <<OUTER" \
  "before" \
  '$(cat <<INNER' \
  "inside" \
  "INNER" \
  ")" \
  "after" \
  "OUTER" \
  >"$nested_initial"
printf '%s\n' \
  "cat <<OUTER" \
  "before" \
  '$(cat <<INNER' \
  "within-value" \
  "INNER" \
  ")" \
  "after" \
  "OUTER" \
  >"$nested_final"
assert_incremental_equals_fresh \
  "$nested_initial" \
  "$nested_final" \
  "nested-here-document" \
  "33 6 within-value"

quoted_initial="$runtime_directory/quoted-initial.sh"
quoted_final="$runtime_directory/quoted-final.sh"
printf '%s\n' \
  "cat <<EOF" \
  '$value' \
  "EOF" \
  >"$quoted_initial"
printf '%s\n' \
  "cat <<'EOF'" \
  '$value' \
  "EOF" \
  >"$quoted_final"
assert_incremental_equals_fresh \
  "$quoted_initial" \
  "$quoted_final" \
  "quoted-here-document" \
  "6 3 'EOF'"

long_initial_delimiter=$(
  awk 'BEGIN { for (counter = 0; counter < 2048; counter += 1) printf "A" }'
)
long_final_delimiter=$(
  awk 'BEGIN { for (counter = 0; counter < 2048; counter += 1) printf "B" }'
)
long_initial="$runtime_directory/long-initial.sh"
long_final="$runtime_directory/long-final.sh"
printf 'cat <<%s\nbody\n%s\n' \
  "$long_initial_delimiter" \
  "$long_initial_delimiter" \
  >"$long_initial"
printf 'cat <<%s\nbody\n%s\nafter' \
  "$long_final_delimiter" \
  "$long_final_delimiter" \
  >"$long_final"
assert_invalid_incremental_equals_fresh \
  "$long_initial" \
  "$long_final" \
  "oversized-delimiter-recovery" \
  "6 2048 $long_final_delimiter" \
  "2060 2048 $long_final_delimiter" \
  "4109 0 after"
long_output="$runtime_directory/long-final.out"
assert_invalid_with_output "$long_final" "$long_output"
assert_contains "(ERROR [0, 4] - [0, 6]" "$long_output"
assert_contains "word: (word [0, 6] - [0, 2054]" "$long_output"
assert_not_contains "missing_here_end" "$long_output"
assert_contains "(complete_command [3, 0] - [3, 5]" "$long_output"

descriptor_initial="$runtime_directory/descriptor-initial.sh"
descriptor_final="$runtime_directory/descriptor-final.sh"
printf '%s\n' "<input 2x>output" >"$descriptor_initial"
printf '%s\n' "<input 2>output" >"$descriptor_final"
assert_incremental_equals_fresh \
  "$descriptor_initial" \
  "$descriptor_final" \
  "descriptor-after-word-edit" \
  "8 1"

parameter_text_initial="$runtime_directory/parameter-text-initial.sh"
parameter_text_final="$runtime_directory/parameter-text-final.sh"
printf '%s\n' 'printf x$%' >"$parameter_text_initial"
printf '%s\n' 'printf x$x' >"$parameter_text_final"
assert_incremental_equals_fresh \
  "$parameter_text_initial" \
  "$parameter_text_final" \
  "parameter-text-to-expansion" \
  "9 1 x"
assert_incremental_equals_fresh \
  "$parameter_text_final" \
  "$parameter_text_initial" \
  "parameter-expansion-to-text" \
  "9 1 %"

parameter_context_unquoted="$runtime_directory/parameter-context-unquoted.sh"
parameter_context_quoted="$runtime_directory/parameter-context-quoted.sh"
parameter_context_pattern="$runtime_directory/parameter-context-pattern.sh"
printf '%s\n' "printf \${x:-*.js}" >"$parameter_context_unquoted"
printf '%s\n' "printf \"\${x:-*.js}\"" >"$parameter_context_quoted"
printf '%s\n' "printf \"\${x##*.js}\"" >"$parameter_context_pattern"
assert_valid_incremental_equals_fresh \
  "$parameter_context_unquoted" \
  "$parameter_context_quoted" \
  "insert-parameter-outer-quotes" \
  '7 0 "' \
  '18 0 "'
assert_valid_incremental_equals_fresh \
  "$parameter_context_quoted" \
  "$parameter_context_unquoted" \
  "delete-parameter-outer-quotes" \
  "7 1" \
  "17 1"
assert_valid_incremental_equals_fresh \
  "$parameter_context_quoted" \
  "$parameter_context_pattern" \
  "parameter-value-to-pattern-context" \
  "11 2 ##"
assert_valid_incremental_equals_fresh \
  "$parameter_context_pattern" \
  "$parameter_context_quoted" \
  "parameter-pattern-to-value-context" \
  "11 2 :-"

numeric_parameter_source="$runtime_directory/numeric-parameter.sh"
numeric_parameter_output="$runtime_directory/numeric-parameter.out"
printf '%s\n' \
  'printf "${001}"' \
  'printf "${0\' \
  '0}"' \
  >"$numeric_parameter_source"
assert_cst_valid_with_output \
  "$numeric_parameter_source" \
  "$numeric_parameter_output"
assert_cst_range \
  "0:10 - 0:13" \
  "parameter: positional_parameter" \
  "$numeric_parameter_output"
assert_cst_range \
  "1:10 - 2:1" \
  "parameter: parameter_number" \
  "$numeric_parameter_output"
assert_cst_range \
  "1:11 - 2:0" \
  "line_continuation" \
  "$numeric_parameter_output"
assert_not_contains "ERROR" "$numeric_parameter_output"

numeric_positional_initial="$runtime_directory/numeric-positional-initial.sh"
numeric_positional_final="$runtime_directory/numeric-positional-final.sh"
printf '%s\n' 'printf "${001}"' >"$numeric_positional_initial"
printf '%s\n' 'printf "${0\' '01}"' >"$numeric_positional_final"
assert_incremental_equals_fresh \
  "$numeric_positional_initial" \
  "$numeric_positional_final" \
  "insert-positional-parameter-continuation" \
  '11 0 \
'
assert_incremental_equals_fresh \
  "$numeric_positional_final" \
  "$numeric_positional_initial" \
  "delete-positional-parameter-continuation" \
  "11 2"

numeric_unspecified_initial="$runtime_directory/numeric-unspecified-initial.sh"
numeric_unspecified_final="$runtime_directory/numeric-unspecified-final.sh"
printf '%s\n' 'printf "${00}"' >"$numeric_unspecified_initial"
printf '%s\n' 'printf "${0\' '0}"' >"$numeric_unspecified_final"
assert_incremental_equals_fresh \
  "$numeric_unspecified_initial" \
  "$numeric_unspecified_final" \
  "insert-parameter-number-continuation" \
  '11 0 \
'
assert_incremental_equals_fresh \
  "$numeric_unspecified_final" \
  "$numeric_unspecified_initial" \
  "delete-parameter-number-continuation" \
  "11 2"

arithmetic_direct="$runtime_directory/arithmetic-direct.sh"
arithmetic_continued="$runtime_directory/arithmetic-continued.sh"
printf '%s\n' 'printf "$((12))"' >"$arithmetic_direct"
printf '%s\n' 'printf "$((1\' '2))"' >"$arithmetic_continued"
assert_incremental_equals_fresh \
  "$arithmetic_direct" \
  "$arithmetic_continued" \
  "insert-arithmetic-number-continuation" \
  '12 0 \
'
assert_incremental_equals_fresh \
  "$arithmetic_continued" \
  "$arithmetic_direct" \
  "delete-arithmetic-number-continuation" \
  "12 2"

arithmetic_equality_initial="$runtime_directory/arithmetic-equality-initial.sh"
arithmetic_equality_final="$runtime_directory/arithmetic-equality-final.sh"
arithmetic_equality_output="$runtime_directory/arithmetic-equality.out"
printf '%s\n' ': "$((a==b))"' >"$arithmetic_equality_initial"
printf '%s\n' ': "$((a=\' '=b))"' >"$arithmetic_equality_final"
assert_cst_valid_with_output \
  "$arithmetic_equality_final" \
  "$arithmetic_equality_output"
assert_cst_range \
  "0:3 - 1:4" \
  "arithmetic_expansion" \
  "$arithmetic_equality_output"
assert_cst_range \
  "0:7 - 1:1" \
  "operator: arithmetic_operator" \
  "$arithmetic_equality_output"
assert_cst_range \
  "0:8 - 1:0" \
  "line_continuation" \
  "$arithmetic_equality_output"
assert_not_contains "command_substitution" "$arithmetic_equality_output"
assert_not_contains "ERROR" "$arithmetic_equality_output"
assert_incremental_equals_fresh \
  "$arithmetic_equality_initial" \
  "$arithmetic_equality_final" \
  "insert-arithmetic-equality-continuation" \
  '8 0 \
'
assert_incremental_equals_fresh \
  "$arithmetic_equality_final" \
  "$arithmetic_equality_initial" \
  "delete-arithmetic-equality-continuation" \
  "8 2"

arithmetic_category_initial="$runtime_directory/arithmetic-category-initial.sh"
arithmetic_category_final="$runtime_directory/arithmetic-category-final.sh"
printf '%s\n' ': "$((a | b && c))"' >"$arithmetic_category_initial"
printf '%s\n' ': "$((a || b && c))"' >"$arithmetic_category_final"
assert_incremental_equals_fresh \
  "$arithmetic_category_initial" \
  "$arithmetic_category_final" \
  "insert-arithmetic-operator-category-character" \
  "9 0 |"
assert_incremental_equals_fresh \
  "$arithmetic_category_final" \
  "$arithmetic_category_initial" \
  "delete-arithmetic-operator-category-character" \
  "9 1"

arithmetic_operand_operator_boundary_initial="$runtime_directory/arithmetic-operand-operator-boundary-initial.sh"
arithmetic_operand_operator_boundary_final="$runtime_directory/arithmetic-operand-operator-boundary-final.sh"
arithmetic_operand_operator_boundary_output="$runtime_directory/arithmetic-operand-operator-boundary.out"
printf '%s\n' ': "$((a==b))"' >"$arithmetic_operand_operator_boundary_initial"
printf '%s\n' ': "$((a\' '==b))"' >"$arithmetic_operand_operator_boundary_final"
assert_cst_valid_with_output \
  "$arithmetic_operand_operator_boundary_final" \
  "$arithmetic_operand_operator_boundary_output"
assert_cst_range \
  "0:3 - 1:5" \
  "arithmetic_expansion" \
  "$arithmetic_operand_operator_boundary_output"
assert_cst_range \
  "0:7 - 1:0" \
  "line_continuation" \
  "$arithmetic_operand_operator_boundary_output"
assert_cst_range \
  "1:0 - 1:2" \
  "operator: arithmetic_operator" \
  "$arithmetic_operand_operator_boundary_output"
assert_not_contains \
  "command_substitution" \
  "$arithmetic_operand_operator_boundary_output"
assert_not_contains "ERROR" "$arithmetic_operand_operator_boundary_output"
assert_incremental_equals_fresh \
  "$arithmetic_operand_operator_boundary_initial" \
  "$arithmetic_operand_operator_boundary_final" \
  "insert-arithmetic-operand-operator-boundary-continuation" \
  '7 0 \
'
assert_incremental_equals_fresh \
  "$arithmetic_operand_operator_boundary_final" \
  "$arithmetic_operand_operator_boundary_initial" \
  "delete-arithmetic-operand-operator-boundary-continuation" \
  "7 2"

arithmetic_parenthesized_initial="$runtime_directory/arithmetic-parenthesized-initial.sh"
arithmetic_parenthesized_final="$runtime_directory/arithmetic-parenthesized-final.sh"
printf '%s\n' ': "$((a + b))"' >"$arithmetic_parenthesized_initial"
printf '%s\n' ': "$(((a + b)))"' >"$arithmetic_parenthesized_final"
assert_incremental_equals_fresh \
  "$arithmetic_parenthesized_initial" \
  "$arithmetic_parenthesized_final" \
  "parenthesize-arithmetic-expression" \
  "6 5 (a + b)"
assert_incremental_equals_fresh \
  "$arithmetic_parenthesized_final" \
  "$arithmetic_parenthesized_initial" \
  "unparenthesize-arithmetic-expression" \
  "6 7 a + b"

arithmetic_opening_layout_initial="$runtime_directory/arithmetic-opening-layout-initial.sh"
arithmetic_opening_layout_final="$runtime_directory/arithmetic-opening-layout-final.sh"
printf '%s\n' ': "$((\' 'a+1))"' >"$arithmetic_opening_layout_initial"
printf '%s\n' ': "$(( \' 'a+1))"' >"$arithmetic_opening_layout_final"
assert_incremental_equals_fresh \
  "$arithmetic_opening_layout_initial" \
  "$arithmetic_opening_layout_final" \
  "insert-arithmetic-opening-layout" \
  "6 0  "
assert_incremental_equals_fresh \
  "$arithmetic_opening_layout_final" \
  "$arithmetic_opening_layout_initial" \
  "delete-arithmetic-opening-layout" \
  "6 1"

arithmetic_opener_initial="$runtime_directory/arithmetic-opener-initial.sh"
arithmetic_opener_final="$runtime_directory/arithmetic-opener-final.sh"
printf '%s\n' ': "$(\' '(a+b))"' >"$arithmetic_opener_initial"
printf '%s\n' ': "$(\' '\' '(a+b))"' >"$arithmetic_opener_final"
assert_incremental_equals_fresh \
  "$arithmetic_opener_initial" \
  "$arithmetic_opener_final" \
  "insert-arithmetic-opener-continuation" \
  '7 0 \
'
assert_incremental_equals_fresh \
  "$arithmetic_opener_final" \
  "$arithmetic_opener_initial" \
  "delete-arithmetic-opener-continuation" \
  "7 2"

arithmetic_negation_initial="$runtime_directory/arithmetic-negation-initial.sh"
arithmetic_negation_final="$runtime_directory/arithmetic-negation-final.sh"
printf '%s\n' ': "$((!a))"' >"$arithmetic_negation_initial"
printf '%s\n' ': "$((! a))"' >"$arithmetic_negation_final"
assert_incremental_equals_fresh \
  "$arithmetic_negation_initial" \
  "$arithmetic_negation_final" \
  "insert-arithmetic-negation-layout" \
  "7 0  "
assert_incremental_equals_fresh \
  "$arithmetic_negation_final" \
  "$arithmetic_negation_initial" \
  "delete-arithmetic-negation-layout" \
  "7 1"

arithmetic_unary_bang_initial="$runtime_directory/arithmetic-unary-bang-initial.sh"
arithmetic_unary_bang_final="$runtime_directory/arithmetic-unary-bang-final.sh"
arithmetic_unary_bang_output="$runtime_directory/arithmetic-unary-bang.out"
printf '%s\n' 'x=$((!-a))' >"$arithmetic_unary_bang_initial"
printf '%s\n' 'x=$((!\' '-a))' >"$arithmetic_unary_bang_final"
assert_cst_valid_with_output \
  "$arithmetic_unary_bang_final" \
  "$arithmetic_unary_bang_output"
assert_contains "assignment: assignment_word" "$arithmetic_unary_bang_output"
assert_cst_range \
  "0:5 - 1:2" \
  "expression: arithmetic_unary_expression" \
  "$arithmetic_unary_bang_output"
assert_cst_range \
  "0:6 - 1:0" \
  "line_continuation" \
  "$arithmetic_unary_bang_output"
assert_cst_range \
  "1:0 - 1:2" \
  "operand: arithmetic_unary_expression" \
  "$arithmetic_unary_bang_output"
assert_not_contains "command_substitution" "$arithmetic_unary_bang_output"
assert_not_contains "ERROR" "$arithmetic_unary_bang_output"
assert_valid_incremental_equals_fresh \
  "$arithmetic_unary_bang_initial" \
  "$arithmetic_unary_bang_final" \
  "insert-arithmetic-unary-bang-continuation" \
  '6 0 \
'
assert_valid_incremental_equals_fresh \
  "$arithmetic_unary_bang_final" \
  "$arithmetic_unary_bang_initial" \
  "delete-arithmetic-unary-bang-continuation" \
  "6 2"

arithmetic_unary_plus_initial="$runtime_directory/arithmetic-unary-plus-initial.sh"
arithmetic_unary_plus_final="$runtime_directory/arithmetic-unary-plus-final.sh"
arithmetic_unary_plus_output="$runtime_directory/arithmetic-unary-plus.out"
printf '%s\n' ': $((+-a))' >"$arithmetic_unary_plus_initial"
printf '%s\n' ': $((+\' '\' '-a))' >"$arithmetic_unary_plus_final"
assert_cst_valid_with_output \
  "$arithmetic_unary_plus_final" \
  "$arithmetic_unary_plus_output"
assert_cst_range \
  "0:5 - 2:2" \
  "expression: arithmetic_unary_expression" \
  "$arithmetic_unary_plus_output"
assert_cst_range \
  "0:6 - 1:0" \
  "line_continuation" \
  "$arithmetic_unary_plus_output"
assert_cst_range \
  "1:0 - 2:0" \
  "line_continuation" \
  "$arithmetic_unary_plus_output"
assert_cst_range \
  "2:0 - 2:2" \
  "operand: arithmetic_unary_expression" \
  "$arithmetic_unary_plus_output"
assert_not_contains "command_substitution" "$arithmetic_unary_plus_output"
assert_not_contains "ERROR" "$arithmetic_unary_plus_output"
assert_valid_incremental_equals_fresh \
  "$arithmetic_unary_plus_initial" \
  "$arithmetic_unary_plus_final" \
  "insert-arithmetic-unary-plus-continuations" \
  '6 0 \
\
'
assert_valid_incremental_equals_fresh \
  "$arithmetic_unary_plus_final" \
  "$arithmetic_unary_plus_initial" \
  "delete-arithmetic-unary-plus-continuations" \
  "6 4"

arithmetic_unary_minus_initial="$runtime_directory/arithmetic-unary-minus-initial.sh"
arithmetic_unary_minus_final="$runtime_directory/arithmetic-unary-minus-final.sh"
printf '%s\n' ': $((-+a))' >"$arithmetic_unary_minus_initial"
printf '%s\n' ': $((-\' '+a))' >"$arithmetic_unary_minus_final"
assert_valid_incremental_equals_fresh \
  "$arithmetic_unary_minus_initial" \
  "$arithmetic_unary_minus_final" \
  "insert-arithmetic-unary-minus-continuation" \
  '6 0 \
'
assert_valid_incremental_equals_fresh \
  "$arithmetic_unary_minus_final" \
  "$arithmetic_unary_minus_initial" \
  "delete-arithmetic-unary-minus-continuation" \
  "6 2"

arithmetic_separated_sign_initial="$runtime_directory/arithmetic-separated-sign-initial.sh"
arithmetic_separated_sign_final="$runtime_directory/arithmetic-separated-sign-final.sh"
printf '%s\n' ': "$((+ +a))"' >"$arithmetic_separated_sign_initial"
printf '%s\n' ': "$((+\' ' +a))"' >"$arithmetic_separated_sign_final"
assert_valid_incremental_equals_fresh \
  "$arithmetic_separated_sign_initial" \
  "$arithmetic_separated_sign_final" \
  "insert-arithmetic-separated-sign-continuation" \
  '7 0 \
'
assert_valid_incremental_equals_fresh \
  "$arithmetic_separated_sign_final" \
  "$arithmetic_separated_sign_initial" \
  "delete-arithmetic-separated-sign-continuation" \
  "7 2"

io_location_initial="$runtime_directory/io-location-initial.sh"
io_location_final="$runtime_directory/io-location-final.sh"
io_location_boundary_output="$runtime_directory/io-location-boundary.out"
printf '%s\n' 'printf {fd}<>x' >"$io_location_initial"
printf '%s\n' 'printf {\' 'fd}<>x' >"$io_location_final"
assert_cst_valid_with_output \
  "$io_location_final" \
  "$io_location_boundary_output"
assert_cst_range \
  "0:7 - 1:3" \
  "location: io_location" \
  "$io_location_boundary_output"
assert_cst_range \
  "0:7 - 0:8" \
  '"{"' \
  "$io_location_boundary_output"
assert_cst_range \
  "0:8 - 1:0" \
  "line_continuation" \
  "$io_location_boundary_output"
assert_cst_range \
  "1:2 - 1:3" \
  '"}"' \
  "$io_location_boundary_output"
assert_not_contains "ERROR" "$io_location_boundary_output"
assert_incremental_equals_fresh \
  "$io_location_initial" \
  "$io_location_final" \
  "insert-io-location-opener-continuation" \
  '8 0 \
'
assert_incremental_equals_fresh \
  "$io_location_final" \
  "$io_location_initial" \
  "delete-io-location-opener-continuation" \
  "8 2"

backquote_initial="$runtime_directory/backquote-continuation-initial.sh"
backquote_final="$runtime_directory/backquote-continuation-final.sh"
backquote_output="$runtime_directory/backquote-continuation.out"
printf '%s\n' 'echo `printf body`' >"$backquote_initial"
printf '%s\n' 'echo `\' 'printf body`' >"$backquote_final"
assert_cst_valid_with_output "$backquote_final" "$backquote_output"
assert_cst_range "0:5  - 0:6" '"\`"' "$backquote_output"
assert_cst_range \
  "0:6  - 1:0" \
  "line_continuation" \
  "$backquote_output"
assert_cst_range "1:11 - 1:12" '"\`"' "$backquote_output"
assert_not_contains "ERROR" "$backquote_output"
assert_incremental_equals_fresh \
  "$backquote_initial" \
  "$backquote_final" \
  "insert-backquote-body-continuation" \
  '6 0 \
'
assert_incremental_equals_fresh \
  "$backquote_final" \
  "$backquote_initial" \
  "delete-backquote-body-continuation" \
  "6 2"

backquote_word_source="$runtime_directory/backquote-word-continuation.sh"
backquote_word_output="$runtime_directory/backquote-word-continuation.out"
printf '%s\n' 'echo `foo\' 'bar`' >"$backquote_word_source"
assert_cst_valid_with_output \
  "$backquote_word_source" \
  "$backquote_word_output"
assert_cst_range "0:6  - 1:3" "literal" "$backquote_word_output"
assert_cst_range \
  "0:9  - 1:0" \
  "line_continuation" \
  "$backquote_word_output"
assert_not_contains "ERROR" "$backquote_word_output"

tilde_initial="$runtime_directory/tilde-continuation-initial.sh"
tilde_final="$runtime_directory/tilde-continuation-final.sh"
tilde_output="$runtime_directory/tilde-continuation.out"
printf '%s\n' 'printf ~user' >"$tilde_initial"
printf '%s\n' 'printf ~\' 'user' >"$tilde_final"
assert_cst_valid_with_output "$tilde_final" "$tilde_output"
assert_cst_range \
  "0:7 - 1:4" \
  "tilde_expansion" \
  "$tilde_output"
assert_cst_range "0:7 - 0:8" '"~"' "$tilde_output"
assert_cst_range "0:8 - 1:0" "line_continuation" "$tilde_output"
assert_cst_range "1:0 - 1:4" "user: tilde_user" "$tilde_output"
assert_not_contains "ERROR" "$tilde_output"
assert_incremental_equals_fresh \
  "$tilde_initial" \
  "$tilde_final" \
  "insert-tilde-user-boundary-continuation" \
  '8 0 \
'
assert_incremental_equals_fresh \
  "$tilde_final" \
  "$tilde_initial" \
  "delete-tilde-user-boundary-continuation" \
  "8 2"

assignment_boundary_initial="$runtime_directory/assignment-boundary-initial.sh"
assignment_boundary_final="$runtime_directory/assignment-boundary-final.sh"
assignment_boundary_output="$runtime_directory/assignment-boundary.out"
printf '%s\n' 'name=value command' >"$assignment_boundary_initial"
printf '%s\n' 'name=value\' ' command' >"$assignment_boundary_final"
assert_cst_valid_with_output \
  "$assignment_boundary_final" \
  "$assignment_boundary_output"
assert_cst_range \
  "0:0  - 1:0" \
  "assignment: assignment_word" \
  "$assignment_boundary_output"
assert_cst_range \
  "0:5  - 1:0" \
  "value: assignment_value" \
  "$assignment_boundary_output"
assert_cst_range \
  "0:10 - 1:0" \
  "line_continuation" \
  "$assignment_boundary_output"
assert_not_contains "ERROR" "$assignment_boundary_output"
assert_incremental_equals_fresh \
  "$assignment_boundary_initial" \
  "$assignment_boundary_final" \
  "insert-assignment-boundary-continuation" \
  '10 0 \
'
assert_incremental_equals_fresh \
  "$assignment_boundary_final" \
  "$assignment_boundary_initial" \
  "delete-assignment-boundary-continuation" \
  "10 2"

name_argument_separator_initial="$runtime_directory/name-argument-separator-initial.sh"
name_argument_separator_final="$runtime_directory/name-argument-separator-final.sh"
printf '%s\n' "echo\\" "\\" "x" >"$name_argument_separator_initial"
printf '%s\n' "echo\\" " \\" "x" >"$name_argument_separator_final"
assert_valid_incremental_equals_fresh \
  "$name_argument_separator_initial" \
  "$name_argument_separator_final" \
  "insert-command-name-argument-continuation-run-blank" \
  '6 0  '
assert_valid_incremental_equals_fresh \
  "$name_argument_separator_final" \
  "$name_argument_separator_initial" \
  "delete-command-name-argument-continuation-run-blank" \
  "6 1"

argument_separator_initial="$runtime_directory/argument-separator-initial.sh"
argument_separator_final="$runtime_directory/argument-separator-final.sh"
printf '%s\n' "echo a\\" "\\" "\\" "b" >"$argument_separator_initial"
printf '%s\n' "echo a\\" "\\" " \\" "b" >"$argument_separator_final"
assert_valid_incremental_equals_fresh \
  "$argument_separator_initial" \
  "$argument_separator_final" \
  "insert-argument-argument-continuation-run-blank" \
  '10 0  '
assert_valid_incremental_equals_fresh \
  "$argument_separator_final" \
  "$argument_separator_initial" \
  "delete-argument-argument-continuation-run-blank" \
  "10 1"

redirect_command_word_separator_initial="$runtime_directory/redirect-command-word-separator-initial.sh"
redirect_command_word_separator_final="$runtime_directory/redirect-command-word-separator-final.sh"
printf '%s\n' ">out\\" "\\" "echo" \
  >"$redirect_command_word_separator_initial"
printf '%s\n' ">out\\" " \\" "echo" \
  >"$redirect_command_word_separator_final"
assert_valid_incremental_equals_fresh \
  "$redirect_command_word_separator_initial" \
  "$redirect_command_word_separator_final" \
  "insert-redirection-command-word-continuation-run-blank" \
  '6 0  '
assert_valid_incremental_equals_fresh \
  "$redirect_command_word_separator_final" \
  "$redirect_command_word_separator_initial" \
  "delete-redirection-command-word-continuation-run-blank" \
  "6 1"

redirect_assignment_separator_initial="$runtime_directory/redirect-assignment-separator-initial.sh"
redirect_assignment_separator_final="$runtime_directory/redirect-assignment-separator-final.sh"
printf '%s\n' ">out\\" "\\" "A=1 command" \
  >"$redirect_assignment_separator_initial"
printf '%s\n' ">out\\" " \\" "A=1 command" \
  >"$redirect_assignment_separator_final"
assert_valid_incremental_equals_fresh \
  "$redirect_assignment_separator_initial" \
  "$redirect_assignment_separator_final" \
  "insert-redirection-assignment-continuation-run-blank" \
  '6 0  '
assert_valid_incremental_equals_fresh \
  "$redirect_assignment_separator_final" \
  "$redirect_assignment_separator_initial" \
  "delete-redirection-assignment-continuation-run-blank" \
  "6 1"

assignment_newline_initial="$runtime_directory/assignment-newline-initial.sh"
assignment_newline_final="$runtime_directory/assignment-newline-final.sh"
assignment_newline_output="$runtime_directory/assignment-newline.out"
printf '%s\n' 'x=a' >"$assignment_newline_initial"
printf '%s\n' "x=a\\" '' >"$assignment_newline_final"
assert_cst_valid_with_output \
  "$assignment_newline_final" \
  "$assignment_newline_output"
assert_cst_range \
  "0:0 - 1:0" \
  "assignment: assignment_word" \
  "$assignment_newline_output"
assert_cst_range \
  "0:2 - 1:0" \
  "value: assignment_value" \
  "$assignment_newline_output"
assert_cst_range \
  "0:3 - 1:0" \
  "line_continuation" \
  "$assignment_newline_output"
assert_cst_range \
  "1:0 - 2:0" \
  "trailing: linebreak" \
  "$assignment_newline_output"
assert_not_contains "name: cmd_name" "$assignment_newline_output"
assert_not_contains "ERROR" "$assignment_newline_output"
assert_not_contains "MISSING" "$assignment_newline_output"
assert_incremental_equals_fresh \
  "$assignment_newline_initial" \
  "$assignment_newline_final" \
  "insert-assignment-newline-continuation" \
  '3 0 \
'
assert_incremental_equals_fresh \
  "$assignment_newline_final" \
  "$assignment_newline_initial" \
  "delete-assignment-newline-continuation" \
  "3 2"

assignment_name_scope_source="$runtime_directory/assignment-name-scope.sh"
assignment_name_scope_output="$runtime_directory/assignment-name-scope.out"
printf '%s\n' \
  'NAME=foo\' \
  '=bar command' \
  'echo NAME\' \
  '=value' \
  'foo-\' \
  '=bar' \
  >"$assignment_name_scope_source"
assert_valid_with_output \
  "$assignment_name_scope_source" \
  "$assignment_name_scope_output"
assert_contains \
  "assignment: (assignment_word [0, 0] - [1, 4]" \
  "$assignment_name_scope_output"
assert_contains \
  "word: (word [2, 5] - [3, 6]" \
  "$assignment_name_scope_output"
assert_contains \
  "name: (cmd_name [4, 0] - [5, 4]" \
  "$assignment_name_scope_output"
assert_not_contains \
  "assignment_word [2," \
  "$assignment_name_scope_output"
assert_not_contains \
  "assignment_word [4," \
  "$assignment_name_scope_output"
assert_not_contains "ERROR" "$assignment_name_scope_output"

quoted_character_initial="$runtime_directory/quoted-character-initial.sh"
quoted_character_final="$runtime_directory/quoted-character-final.sh"
quoted_character_output="$runtime_directory/quoted-character.out"
printf '%s\n' 'X=a\q' >"$quoted_character_initial"
printf '%s\n' 'X=a\' '\q' >"$quoted_character_final"
assert_cst_valid_with_output \
  "$quoted_character_final" \
  "$quoted_character_output"
assert_contains "assignment: assignment_word" "$quoted_character_output"
assert_contains "escaped_character" "$quoted_character_output"
assert_not_contains "ERROR" "$quoted_character_output"
assert_incremental_equals_fresh \
  "$quoted_character_initial" \
  "$quoted_character_final" \
  "insert-noncontinuation-backslash-lookahead" \
  '3 0 \
'
assert_incremental_equals_fresh \
  "$quoted_character_final" \
  "$quoted_character_initial" \
  "delete-noncontinuation-backslash-lookahead" \
  "3 2"

operator_boundary_source="$runtime_directory/operator-boundaries.sh"
operator_boundary_output="$runtime_directory/operator-boundaries.out"
printf '%s\n' \
  'echo word\' \
  '  |next' \
  'echo word\' \
  '  ||next' \
  'echo word\' \
  '  &&next' \
  'case x in pattern\' \
  '  |other) : ;; esac' \
  >"$operator_boundary_source"
assert_cst_valid_with_output \
  "$operator_boundary_source" \
  "$operator_boundary_output"
assert_cst_range "0:9  - 1:0" "line_continuation" "$operator_boundary_output"
assert_cst_range "2:9  - 3:0" "line_continuation" "$operator_boundary_output"
assert_cst_range "3:2  - 3:4" "operator: or_if" "$operator_boundary_output"
assert_cst_range "4:9  - 5:0" "line_continuation" "$operator_boundary_output"
assert_cst_range "5:2  - 5:4" "operator: and_if" "$operator_boundary_output"
assert_cst_range "6:17 - 7:0" "line_continuation" "$operator_boundary_output"
assert_not_contains "ERROR" "$operator_boundary_output"

repeated_pipe_boundary_initial="$runtime_directory/repeated-pipe-boundary-initial.sh"
repeated_pipe_boundary_final="$runtime_directory/repeated-pipe-boundary-final.sh"
repeated_pipe_boundary_output="$runtime_directory/repeated-pipe-boundary.out"
printf '%s\n' 'first|next' >"$repeated_pipe_boundary_initial"
printf '%s\n' 'first\' '\' '  |next' >"$repeated_pipe_boundary_final"
assert_cst_valid_with_output \
  "$repeated_pipe_boundary_final" \
  "$repeated_pipe_boundary_output"
assert_cst_range \
  "0:5 - 1:0" \
  "line_continuation" \
  "$repeated_pipe_boundary_output"
assert_cst_range \
  "1:0 - 2:0" \
  "line_continuation" \
  "$repeated_pipe_boundary_output"
assert_not_contains "ERROR" "$repeated_pipe_boundary_output"
assert_incremental_equals_fresh \
  "$repeated_pipe_boundary_initial" \
  "$repeated_pipe_boundary_final" \
  "insert-repeated-pipe-boundary-continuations" \
  '5 0 \
\
  '
assert_incremental_equals_fresh \
  "$repeated_pipe_boundary_final" \
  "$repeated_pipe_boundary_initial" \
  "delete-repeated-pipe-boundary-continuations" \
  "5 6"

pipe_linebreak_initial="$runtime_directory/pipe-linebreak-initial.sh"
pipe_linebreak_final="$runtime_directory/pipe-linebreak-final.sh"
pipe_linebreak_output="$runtime_directory/pipe-linebreak.out"
printf '%s\n' 'first|next' >"$pipe_linebreak_initial"
printf '%s\n' 'first| \' 'next' >"$pipe_linebreak_final"
assert_cst_valid_with_output \
  "$pipe_linebreak_final" \
  "$pipe_linebreak_output"
assert_cst_range \
  "0:7 - 1:0" \
  "line_continuation" \
  "$pipe_linebreak_output"
assert_not_contains "ERROR" "$pipe_linebreak_output"
assert_incremental_equals_fresh \
  "$pipe_linebreak_initial" \
  "$pipe_linebreak_final" \
  "insert-pipe-linebreak-continuation" \
  '6 0  \
'
assert_incremental_equals_fresh \
  "$pipe_linebreak_final" \
  "$pipe_linebreak_initial" \
  "delete-pipe-linebreak-continuation" \
  "6 3"

compound_layout_initial="$runtime_directory/compound-layout-initial.sh"
compound_layout_final="$runtime_directory/compound-layout-final.sh"
printf '%s\n' 'if { :; } \' 'then :; fi' >"$compound_layout_initial"
printf '%s\n' 'if { :; } \' '\' 'then :; fi' >"$compound_layout_final"
assert_incremental_equals_fresh \
  "$compound_layout_initial" \
  "$compound_layout_final" \
  "insert-compound-layout-continuation" \
  '12 0 \
'
assert_incremental_equals_fresh \
  "$compound_layout_final" \
  "$compound_layout_initial" \
  "delete-compound-layout-continuation" \
  "12 2"

compound_separator_initial="$runtime_directory/compound-separator-initial.sh"
compound_separator_final="$runtime_directory/compound-separator-final.sh"
printf '%s\n' '{ :; }' >"$compound_separator_initial"
printf '%s\n' '{ :\' '; }' >"$compound_separator_final"
assert_incremental_equals_fresh \
  "$compound_separator_initial" \
  "$compound_separator_final" \
  "insert-compound-separator-continuation" \
  '3 0 \
'
assert_incremental_equals_fresh \
  "$compound_separator_final" \
  "$compound_separator_initial" \
  "delete-compound-separator-continuation" \
  "3 2"

for_wordlist_initial="$runtime_directory/for-wordlist-initial.sh"
for_wordlist_final="$runtime_directory/for-wordlist-final.sh"
printf '%s\n' 'for i in \' 'word; do :; done' >"$for_wordlist_initial"
printf '%s\n' 'for i in \' '\' 'word; do :; done' >"$for_wordlist_final"
assert_valid_incremental_equals_fresh \
  "$for_wordlist_initial" \
  "$for_wordlist_final" \
  "insert-second-for-wordlist-continuation" \
  '11 0 \
'
assert_valid_incremental_equals_fresh \
  "$for_wordlist_final" \
  "$for_wordlist_initial" \
  "delete-second-for-wordlist-continuation" \
  "11 2"

for_formal_initial="$runtime_directory/for-formal-initial.sh"
for_formal_final="$runtime_directory/for-formal-final.sh"
for_formal_output="$runtime_directory/for-formal.out"
printf '%s\n' 'for i in a; do :; done' >"$for_formal_initial"
printf '%s\n' "for i \\" 'in a; do :; done' >"$for_formal_final"
assert_cst_valid_with_output "$for_formal_final" "$for_formal_output"
assert_cst_range "0:4  - 0:5" "name: name" "$for_formal_output"
assert_cst_range "0:6  - 1:0" "line_continuation" "$for_formal_output"
assert_cst_range "1:0  - 1:2" "in: in" "$for_formal_output"
assert_not_contains "ERROR" "$for_formal_output"
assert_not_contains "MISSING" "$for_formal_output"
assert_incremental_equals_fresh \
  "$for_formal_initial" \
  "$for_formal_final" \
  "insert-for-formal-continuation" \
  '6 0 \
'
assert_incremental_equals_fresh \
  "$for_formal_final" \
  "$for_formal_initial" \
  "delete-for-formal-continuation" \
  "6 2"

case_subject_initial="$runtime_directory/case-subject-initial.sh"
case_subject_final="$runtime_directory/case-subject-final.sh"
case_subject_output="$runtime_directory/case-subject.out"
printf '%s\n' "case x \\" 'in x) :;; esac' >"$case_subject_initial"
printf '%s\n' "case x \\" "\\" 'in x) :;; esac' >"$case_subject_final"
assert_cst_valid_with_output "$case_subject_final" "$case_subject_output"
assert_cst_range "0:5  - 0:6" "word: word" "$case_subject_output"
assert_cst_range "0:7  - 1:0" "line_continuation" "$case_subject_output"
assert_cst_range "1:0  - 2:0" "line_continuation" "$case_subject_output"
assert_cst_range "2:0  - 2:2" "in: in" "$case_subject_output"
assert_not_contains "ERROR" "$case_subject_output"
assert_not_contains "MISSING" "$case_subject_output"
assert_valid_incremental_equals_fresh \
  "$case_subject_initial" \
  "$case_subject_final" \
  "insert-second-case-subject-continuation" \
  '9 0 \
'
assert_valid_incremental_equals_fresh \
  "$case_subject_final" \
  "$case_subject_initial" \
  "delete-second-case-subject-continuation" \
  "9 2"

or_boundary_initial="$runtime_directory/or-boundary-initial.sh"
or_boundary_final="$runtime_directory/or-boundary-final.sh"
printf '%s\n' 'echo word||next' >"$or_boundary_initial"
printf '%s\n' 'echo word\' '  ||next' >"$or_boundary_final"
assert_incremental_equals_fresh \
  "$or_boundary_initial" \
  "$or_boundary_final" \
  "insert-or-boundary-continuation" \
  '9 0 \
  '
assert_incremental_equals_fresh \
  "$or_boundary_final" \
  "$or_boundary_initial" \
  "delete-or-boundary-continuation" \
  "9 4"

and_boundary_initial="$runtime_directory/and-boundary-initial.sh"
and_boundary_final="$runtime_directory/and-boundary-final.sh"
printf '%s\n' 'echo word&&next' >"$and_boundary_initial"
printf '%s\n' 'echo word\' '  &&next' >"$and_boundary_final"
assert_incremental_equals_fresh \
  "$and_boundary_initial" \
  "$and_boundary_final" \
  "insert-and-boundary-continuation" \
  '9 0 \
  '
assert_incremental_equals_fresh \
  "$and_boundary_final" \
  "$and_boundary_initial" \
  "delete-and-boundary-continuation" \
  "9 4"

case_pattern_boundary_initial="$runtime_directory/case-pattern-boundary-initial.sh"
case_pattern_boundary_final="$runtime_directory/case-pattern-boundary-final.sh"
printf '%s\n' 'case x in pattern|other) : ;; esac' \
  >"$case_pattern_boundary_initial"
printf '%s\n' 'case x in pattern\' '  |other) : ;; esac' \
  >"$case_pattern_boundary_final"
assert_incremental_equals_fresh \
  "$case_pattern_boundary_initial" \
  "$case_pattern_boundary_final" \
  "insert-case-pattern-boundary-continuation" \
  '17 0 \
  '
assert_incremental_equals_fresh \
  "$case_pattern_boundary_final" \
  "$case_pattern_boundary_initial" \
  "delete-case-pattern-boundary-continuation" \
  "17 4"

backquote_end_initial="$runtime_directory/backquote-end-initial.sh"
backquote_end_final="$runtime_directory/backquote-end-final.sh"
backquote_end_output="$runtime_directory/backquote-end.out"
printf '%s\n' 'echo `printf x`' >"$backquote_end_initial"
printf '%s\n' 'echo `printf x\' '`' >"$backquote_end_final"
assert_cst_valid_with_output "$backquote_end_final" "$backquote_end_output"
assert_cst_range \
  "0:5  - 1:1" \
  "backquote_substitution" \
  "$backquote_end_output"
assert_cst_range \
  "0:14 - 1:0" \
  "line_continuation" \
  "$backquote_end_output"
assert_cst_range "1:0  - 1:1" '"\`"' "$backquote_end_output"
assert_not_contains "backquote_end_recovery" "$backquote_end_output"
assert_not_contains "ERROR" "$backquote_end_output"
assert_incremental_equals_fresh \
  "$backquote_end_initial" \
  "$backquote_end_final" \
  "insert-backquote-end-continuation" \
  '14 0 \
'
assert_incremental_equals_fresh \
  "$backquote_end_final" \
  "$backquote_end_initial" \
  "delete-backquote-end-continuation" \
  "14 2"

command_substitution_end_initial="$runtime_directory/command-substitution-end-initial.sh"
command_substitution_end_final="$runtime_directory/command-substitution-end-final.sh"
command_substitution_end_output="$runtime_directory/command-substitution-end.out"
printf '%s\n' 'echo $(printf x)' >"$command_substitution_end_initial"
printf '%s\n' 'echo $(printf x\' ')' >"$command_substitution_end_final"
assert_cst_valid_with_output \
  "$command_substitution_end_final" \
  "$command_substitution_end_output"
assert_cst_range \
  "0:5  - 1:1" \
  "command_substitution" \
  "$command_substitution_end_output"
assert_cst_range \
  "0:15 - 1:0" \
  "line_continuation" \
  "$command_substitution_end_output"
assert_cst_range "1:0  - 1:1" '")"' "$command_substitution_end_output"
assert_not_contains "ERROR" "$command_substitution_end_output"
assert_incremental_equals_fresh \
  "$command_substitution_end_initial" \
  "$command_substitution_end_final" \
  "insert-command-substitution-end-continuation" \
  '15 0 \
'
assert_incremental_equals_fresh \
  "$command_substitution_end_final" \
  "$command_substitution_end_initial" \
  "delete-command-substitution-end-continuation" \
  "15 2"

tilde_word_end_initial="$runtime_directory/tilde-word-end-initial.sh"
tilde_word_end_final="$runtime_directory/tilde-word-end-final.sh"
tilde_word_end_output="$runtime_directory/tilde-word-end.out"
printf '%s\n' 'printf ~username/path' >"$tilde_word_end_initial"
printf '%s\n' 'printf ~username\' '/path' >"$tilde_word_end_final"
assert_cst_valid_with_output "$tilde_word_end_final" "$tilde_word_end_output"
assert_cst_range \
  "0:7  - 0:16" \
  "tilde_expansion" \
  "$tilde_word_end_output"
assert_cst_range \
  "0:8  - 0:16" \
  "user: tilde_user" \
  "$tilde_word_end_output"
assert_cst_range \
  "0:16 - 1:0" \
  "line_continuation" \
  "$tilde_word_end_output"
assert_not_contains "ERROR" "$tilde_word_end_output"
assert_incremental_equals_fresh \
  "$tilde_word_end_initial" \
  "$tilde_word_end_final" \
  "insert-tilde-word-end-continuation" \
  '16 0 \
'
assert_incremental_equals_fresh \
  "$tilde_word_end_final" \
  "$tilde_word_end_initial" \
  "delete-tilde-word-end-continuation" \
  "16 2"

tilde_assignment_end_initial="$runtime_directory/tilde-assignment-end-initial.sh"
tilde_assignment_end_final="$runtime_directory/tilde-assignment-end-final.sh"
tilde_assignment_end_output="$runtime_directory/tilde-assignment-end.out"
printf '%s\n' 'PATH=~username:next command' >"$tilde_assignment_end_initial"
printf '%s\n' 'PATH=~username\' ':next command' >"$tilde_assignment_end_final"
assert_cst_valid_with_output \
  "$tilde_assignment_end_final" \
  "$tilde_assignment_end_output"
assert_cst_range \
  "0:0  - 1:5" \
  "assignment: assignment_word" \
  "$tilde_assignment_end_output"
assert_cst_range \
  "0:5  - 0:14" \
  "tilde_expansion" \
  "$tilde_assignment_end_output"
assert_cst_range \
  "0:14 - 1:0" \
  "line_continuation" \
  "$tilde_assignment_end_output"
assert_not_contains "ERROR" "$tilde_assignment_end_output"
assert_incremental_equals_fresh \
  "$tilde_assignment_end_initial" \
  "$tilde_assignment_end_final" \
  "insert-tilde-assignment-end-continuation" \
  '14 0 \
'
assert_incremental_equals_fresh \
  "$tilde_assignment_end_final" \
  "$tilde_assignment_end_initial" \
  "delete-tilde-assignment-end-continuation" \
  "14 2"

tilde_assignment_colon_initial="$runtime_directory/tilde-assignment-colon-initial.sh"
tilde_assignment_colon_final="$runtime_directory/tilde-assignment-colon-final.sh"
tilde_assignment_colon_output="$runtime_directory/tilde-assignment-colon.out"
printf '%s\n' 'PATH=a:~user/path command' >"$tilde_assignment_colon_initial"
printf '%s\n' 'PATH=a:\' '~user/path command' >"$tilde_assignment_colon_final"
assert_cst_valid_with_output \
  "$tilde_assignment_colon_final" \
  "$tilde_assignment_colon_output"
assert_cst_range \
  "0:0  - 1:10" \
  "assignment: assignment_word" \
  "$tilde_assignment_colon_output"
assert_cst_range \
  "0:7  - 1:0" \
  "line_continuation" \
  "$tilde_assignment_colon_output"
assert_cst_range \
  "1:0  - 1:5" \
  "tilde_expansion" \
  "$tilde_assignment_colon_output"
assert_not_contains "ERROR" "$tilde_assignment_colon_output"
assert_incremental_equals_fresh \
  "$tilde_assignment_colon_initial" \
  "$tilde_assignment_colon_final" \
  "insert-tilde-assignment-colon-continuation" \
  '7 0 \
'
assert_incremental_equals_fresh \
  "$tilde_assignment_colon_final" \
  "$tilde_assignment_colon_initial" \
  "delete-tilde-assignment-colon-continuation" \
  "7 2"

literal_assignment_colon_initial="$runtime_directory/literal-assignment-colon-initial.sh"
literal_assignment_colon_final="$runtime_directory/literal-assignment-colon-final.sh"
literal_assignment_colon_output="$runtime_directory/literal-assignment-colon.out"
printf '%s\n' 'x=a:b :' >"$literal_assignment_colon_initial"
printf '%s\n' "x=a:\\" 'b :' >"$literal_assignment_colon_final"
assert_cst_valid_with_output \
  "$literal_assignment_colon_final" \
  "$literal_assignment_colon_output"
assert_cst_range \
  "0:0 - 1:1" \
  "assignment: assignment_word" \
  "$literal_assignment_colon_output"
assert_cst_range \
  "0:2 - 1:1" \
  "value: assignment_value" \
  "$literal_assignment_colon_output"
assert_cst_range \
  "0:4 - 1:0" \
  "line_continuation" \
  "$literal_assignment_colon_output"
assert_cst_range \
  "1:2 - 1:3" \
  "word: cmd_word" \
  "$literal_assignment_colon_output"
assert_not_contains "name: cmd_name" "$literal_assignment_colon_output"
assert_not_contains "ERROR" "$literal_assignment_colon_output"
assert_not_contains "MISSING" "$literal_assignment_colon_output"
assert_incremental_equals_fresh \
  "$literal_assignment_colon_initial" \
  "$literal_assignment_colon_final" \
  "insert-literal-assignment-colon-continuation" \
  '4 0 \
'
assert_incremental_equals_fresh \
  "$literal_assignment_colon_final" \
  "$literal_assignment_colon_initial" \
  "delete-literal-assignment-colon-continuation" \
  "4 2"

compound_list_regression="$runtime_directory/compound-list-regression.sh"
printf '%s\n' \
  "regular_file_identity() {" \
  "  CURRENT_FILE_ID=" \
  '  [ -f "$1" ] && [ ! -L "$1" ] ||' \
  "    return 1" \
  '  file_owner=$(stat -f '\''%u'\'' "$1" 2>/dev/null) || return 1' \
  '  file_mode=$(stat -f '\''%Lp'\'' "$1" 2>/dev/null) || return 1' \
  "}" \
  >"$compound_list_regression"
assert_valid "$compound_list_regression"

compound_list_initial="$runtime_directory/compound-list-initial.sh"
compound_list_final="$runtime_directory/compound-list-final.sh"
printf '%s\n' \
  "f() {" \
  "  before=" \
  "  first ||" \
  "    second" \
  '  target=$(one) || recover' \
  "}" \
  >"$compound_list_initial"
printf '%s\n' \
  "f() {" \
  "  before=" \
  "  first ||" \
  "    second" \
  '  targets=$(one) || recover' \
  "}" \
  >"$compound_list_final"
assert_valid "$compound_list_final"
assert_incremental_equals_fresh \
  "$compound_list_initial" \
  "$compound_list_final" \
  "compound-list-assignment-edit" \
  "46 0 s"

case_and_or_sample="$repository_directory/test/runtime/case_and_or_functions.sh"
case_and_or_output="$runtime_directory/case-and-or-functions.out"
case_and_or_query_output="$runtime_directory/case-and-or-functions.query"
assert_valid_with_output "$case_and_or_sample" "$case_and_or_output"
"$tree_sitter" query \
  "$repository_directory/test/runtime/function_definitions.scm" \
  "$case_and_or_sample" \
  >"$case_and_or_query_output" 2>/dev/null
assert_contains 'sample_log' "$case_and_or_query_output"
assert_contains 'summarize' "$case_and_or_query_output"

case_item_layout="$runtime_directory/case-item-layout.sh"
printf 'case value in\n  tab) left\t|| right ;;\n  tight) left||right ;;\nesac\n' \
  >"$case_item_layout"
assert_valid "$case_item_layout"

case_item_initial="$runtime_directory/case-item-initial.sh"
case_item_final="$runtime_directory/case-item-final.sh"
printf '%s\n' \
  "summarize() {" \
  "  case value in" \
  "    first) before ;;" \
  "    second) condition ;;" \
  "  esac" \
  "}" \
  >"$case_item_initial"
printf '%s\n' \
  "summarize() {" \
  "  case value in" \
  "    first) before ;;" \
  "    second) condition || invalid_input ;;" \
  "  esac" \
  "}" \
  >"$case_item_final"
assert_valid "$case_item_final"
assert_incremental_equals_fresh \
  "$case_item_initial" \
  "$case_item_final" \
  "case-item-and-or-insertion" \
  "73 0 || invalid_input "
assert_incremental_equals_fresh \
  "$case_item_final" \
  "$case_item_initial" \
  "case-item-and-or-deletion" \
  "73 17"

case_item_boundary_initial="$runtime_directory/case-item-boundary-initial.sh"
case_item_boundary_final="$runtime_directory/case-item-boundary-final.sh"
printf '%s\n' 'case x in x) :\' ';; esac' \
  >"$case_item_boundary_initial"
printf '%s\n' 'case x in x) :\' '\' ';; esac' \
  >"$case_item_boundary_final"
assert_valid_incremental_equals_fresh \
  "$case_item_boundary_initial" \
  "$case_item_boundary_final" \
  "insert-second-case-item-boundary-continuation" \
  '16 0 \
'
assert_valid_incremental_equals_fresh \
  "$case_item_boundary_final" \
  "$case_item_boundary_initial" \
  "delete-second-case-item-boundary-continuation" \
  "16 2"

bracket_unclosed="$runtime_directory/bracket-unclosed.sh"
bracket_closed="$runtime_directory/bracket-closed.sh"
bracket_range="$runtime_directory/bracket-range.sh"
bracket_unclosed_output="$runtime_directory/bracket-unclosed.out"
printf '%s\n' "printf [abc" >"$bracket_unclosed"
printf '%s\n' "printf [abc]" >"$bracket_closed"
printf '%s\n' "printf [a-z]" >"$bracket_range"
assert_cst_valid_with_output \
  "$bracket_unclosed" \
  "$bracket_unclosed_output"
assert_cst_range \
  "0:7  - 0:8" \
  "literal" \
  "$bracket_unclosed_output"
assert_cst_range \
  "0:8  - 0:11" \
  "literal" \
  "$bracket_unclosed_output"
assert_incremental_equals_fresh \
  "$bracket_unclosed" \
  "$bracket_closed" \
  "complete-bracket-expression" \
  "11 0 ]"
assert_incremental_equals_fresh \
  "$bracket_closed" \
  "$bracket_unclosed" \
  "unclose-bracket-expression" \
  "11 1"
assert_incremental_equals_fresh \
  "$bracket_closed" \
  "$bracket_range" \
  "bracket-list-to-range" \
  "9 2 -z"

special_bracket_unclosed="$runtime_directory/special-bracket-unclosed.sh"
special_bracket_closed="$runtime_directory/special-bracket-closed.sh"
printf '%s\n' "printf [[." >"$special_bracket_unclosed"
printf '%s\n' "printf [[.x.]]" >"$special_bracket_closed"
assert_valid_incremental_equals_fresh \
  "$special_bracket_unclosed" \
  "$special_bracket_closed" \
  "complete-collating-symbol-bracket-expression" \
  "10 0 x.]]"
assert_valid_incremental_equals_fresh \
  "$special_bracket_closed" \
  "$special_bracket_unclosed" \
  "unclose-collating-symbol-bracket-expression" \
  "10 4"

special_suffix_initial="$runtime_directory/special-suffix-initial.sh"
special_suffix_final="$runtime_directory/special-suffix-final.sh"
printf '%s\n' "printf [[:alpha:]" >"$special_suffix_initial"
printf '%s\n' "printf [[:alpha:]]" >"$special_suffix_final"
assert_valid_incremental_equals_fresh \
  "$special_suffix_initial" \
  "$special_suffix_final" \
  "complete-special-suffix-outer-bracket" \
  "17 0 ]"
assert_valid_incremental_equals_fresh \
  "$special_suffix_final" \
  "$special_suffix_initial" \
  "restore-special-suffix-literal-prefix" \
  "17 1"

continued_bracket_initial="$runtime_directory/continued-bracket-initial.sh"
continued_bracket_final="$runtime_directory/continued-bracket-final.sh"
continued_bracket_output="$runtime_directory/continued-bracket.out"
printf '%s\n' 'printf []]' >"$continued_bracket_initial"
printf '%s\n' 'printf []\' ']' >"$continued_bracket_final"
assert_cst_valid_with_output \
  "$continued_bracket_final" \
  "$continued_bracket_output"
assert_cst_range \
  "0:7  - 1:1" \
  "pattern_bracket_source" \
  "$continued_bracket_output"
assert_cst_range \
  "0:9  - 1:0" \
  "line_continuation" \
  "$continued_bracket_output"
assert_not_contains "ERROR" "$continued_bracket_output"
assert_valid_incremental_equals_fresh \
  "$continued_bracket_initial" \
  "$continued_bracket_final" \
  "insert-bracket-closer-continuation" \
  '9 0 \
'
assert_valid_incremental_equals_fresh \
  "$continued_bracket_final" \
  "$continued_bracket_initial" \
  "delete-bracket-closer-continuation" \
  "9 2"

invalid_prefix_initial="$runtime_directory/invalid-prefix-initial.sh"
invalid_prefix_final="$runtime_directory/invalid-prefix-final.sh"
printf '%s\n' 'printf [][.]' >"$invalid_prefix_initial"
printf '%s\n' 'printf [\' '][.]' >"$invalid_prefix_final"
assert_valid_incremental_equals_fresh \
  "$invalid_prefix_initial" \
  "$invalid_prefix_final" \
  "insert-invalid-prefix-continuation" \
  '8 0 \
'
assert_valid_incremental_equals_fresh \
  "$invalid_prefix_final" \
  "$invalid_prefix_initial" \
  "delete-invalid-prefix-continuation" \
  "8 2"

operator_suffix_initial="$runtime_directory/operator-suffix-initial.sh"
operator_suffix_final="$runtime_directory/operator-suffix-final.sh"
printf '%s\n' 'printf [a"x"[.]' >"$operator_suffix_initial"
printf '%s\n' 'printf [a"x"*[.]' >"$operator_suffix_final"
assert_valid_incremental_equals_fresh \
  "$operator_suffix_initial" \
  "$operator_suffix_final" \
  "insert-operator-before-completed-bracket-suffix" \
  "12 0 *"
assert_valid_incremental_equals_fresh \
  "$operator_suffix_final" \
  "$operator_suffix_initial" \
  "delete-operator-before-completed-bracket-suffix" \
  "12 1"

incomplete_bracket_initial="$runtime_directory/incomplete-bracket-initial.sh"
incomplete_bracket_final="$runtime_directory/incomplete-bracket-final.sh"
printf '%s\n' 'echo []' >"$incomplete_bracket_initial"
printf '%s\n' 'echo [\' ']' >"$incomplete_bracket_final"
assert_incremental_equals_fresh \
  "$incomplete_bracket_initial" \
  "$incomplete_bracket_final" \
  "insert-incomplete-bracket-continuation" \
  '6 0 \
'
assert_incremental_equals_fresh \
  "$incomplete_bracket_final" \
  "$incomplete_bracket_initial" \
  "delete-incomplete-bracket-continuation" \
  "6 2"

terminal_bracket_initial="$runtime_directory/terminal-bracket-initial.sh"
terminal_bracket_final="$runtime_directory/terminal-bracket-final.sh"
printf '%s\n' 'echo [!]' 'next' >"$terminal_bracket_initial"
printf '%s\n' 'echo [!]\' '' 'next' >"$terminal_bracket_final"
assert_incremental_equals_fresh \
  "$terminal_bracket_initial" \
  "$terminal_bracket_final" \
  "insert-terminal-bracket-continuation" \
  '8 0 \
'
assert_incremental_equals_fresh \
  "$terminal_bracket_final" \
  "$terminal_bracket_initial" \
  "delete-terminal-bracket-continuation" \
  "8 2"

unmatched_brackets="$runtime_directory/unmatched-brackets.sh"
awk 'BEGIN {
  printf "printf "
  for (counter = 0; counter < 80000; counter += 1) printf "["
  printf "\n"
}' >"$unmatched_brackets"
assert_valid "$unmatched_brackets"

continued_unmatched_brackets="$runtime_directory/continued-unmatched-brackets.sh"
awk 'BEGIN {
  printf "printf ["
  for (counter = 0; counter < 12000; counter += 1) {
    printf "x[*?\\\n"
  }
  print "tail"
}' >"$continued_unmatched_brackets"
assert_valid "$continued_unmatched_brackets"

long_bracket_list="$runtime_directory/long-bracket-list.sh"
awk 'BEGIN {
  printf "printf ["
  for (counter = 0; counter < 16000; counter += 1) printf "a"
  printf "]\n"
}' >"$long_bracket_list"
assert_valid "$long_bracket_list"

repeated_brackets="$runtime_directory/repeated-brackets.sh"
awk 'BEGIN {
  printf "printf "
  for (counter = 0; counter < 4000; counter += 1) printf "[abc]"
  printf "\n"
}' >"$repeated_brackets"
assert_valid "$repeated_brackets"

repeated_incomplete_special_brackets="$runtime_directory/repeated-incomplete-special-brackets.sh"
awk 'BEGIN {
  printf "printf ["
  for (counter = 0; counter < 4000; counter += 1) printf "[:alpha:]"
  printf "\n"
}' >"$repeated_incomplete_special_brackets"
assert_valid "$repeated_incomplete_special_brackets"

continued_bracket_ranges="$runtime_directory/continued-bracket-ranges.sh"
awk 'BEGIN {
  printf "printf ["
  for (counter = 0; counter < 4000; counter += 1) {
    printf "a\\\n-\\\nz\\\n"
  }
  print "]"
}' >"$continued_bracket_ranges"
assert_valid "$continued_bracket_ranges"

continued_blank_lines="$runtime_directory/continued-blank-lines.sh"
awk 'BEGIN {
  printf "printf value "
  for (counter = 0; counter < 20000; counter += 1) printf "\\\n"
  print "after"
}' >"$continued_blank_lines"
assert_valid "$continued_blank_lines"

continued_pipe_linebreak="$runtime_directory/continued-pipe-linebreak.sh"
awk 'BEGIN {
  printf "first|"
  for (counter = 0; counter < 20000; counter += 1) printf "\\\n"
  print "next"
}' >"$continued_pipe_linebreak"
assert_valid "$continued_pipe_linebreak"

here_document_dollars="$runtime_directory/here-document-dollars.sh"
awk 'BEGIN {
  print "cat <<EOF"
  for (counter = 0; counter < 16000; counter += 1) printf "$x"
  print ""
  print "EOF"
  print "after"
}' >"$here_document_dollars"
assert_valid "$here_document_dollars"

many_documents="$runtime_directory/many-documents.sh"
awk 'BEGIN {
  printf "cat"
  for (counter = 0; counter < 600; counter += 1) printf " <<X"
  print ""
  for (counter = 0; counter < 600; counter += 1) {
    print "body"
    print "X"
  }
  print "after"
}' >"$many_documents"
assert_invalid_with_output \
  "$many_documents" \
  "$runtime_directory/many-documents.out"

deep_documents="$runtime_directory/deep-documents.sh"
awk 'BEGIN {
  depth = 300
  print "cat <<X"
  for (counter = 1; counter < depth; counter += 1) print "$(cat <<X"
  print "leaf"
  for (counter = depth - 1; counter >= 0; counter -= 1) {
    print "X"
    if (counter > 0) print ")"
  }
  print "after"
}' >"$deep_documents"
assert_invalid_with_output \
  "$deep_documents" \
  "$runtime_directory/deep-documents.out"

scanner_contract="$runtime_directory/scanner-contract"
"${CC:-cc}" \
  -std=c11 \
  -Wall \
  -Wextra \
  -Werror \
  -pedantic \
  -I"$repository_directory/src" \
  "$repository_directory/test/scanner/contract.c" \
  -o "$scanner_contract"
"$scanner_contract"

nested_delimiter_document="$runtime_directory/nested-delimiter-document.sh"
nested_delimiter_output="$runtime_directory/nested-delimiter-document.out"
printf '%s\n' \
  'cat <<$(cat <<\eof' \
  'a here-doc with )' \
  "eof" \
  ")" \
  "after" \
  >"$nested_delimiter_document"
assert_valid_with_output \
  "$nested_delimiter_document" \
  "$nested_delimiter_output"
assert_contains \
  "end: (here_end [0, 6] - [3, 1]" \
  "$nested_delimiter_output"
assert_contains \
  "end: (here_document_end_recovery [5, 0] - [5, 0])" \
  "$nested_delimiter_output"

quote_recovery_initial="$runtime_directory/quote-recovery-initial.sh"
quote_recovery_final="$runtime_directory/quote-recovery-final.sh"
quote_recovery_output="$runtime_directory/quote-recovery.out"
printf '%s\n' \
  "cat <<EOF" \
  '$("open' \
  "EOF" \
  "after" \
  >"$quote_recovery_initial"
printf '%s\n' \
  "cat <<EOF" \
  '$("open")' \
  "EOF" \
  "after" \
  >"$quote_recovery_final"
parse_current \
  "$quote_recovery_initial" \
  >"$quote_recovery_output" 2>/dev/null
assert_contains \
  "(complete_command [3, 0] - [3, 5]" \
  "$quote_recovery_output"
assert_incremental_equals_fresh \
  "$quote_recovery_initial" \
  "$quote_recovery_final" \
  "quote-recovery-at-here-document-end" \
  '17 0 ")'

nested_backquotes="$runtime_directory/nested-backquotes.sh"
awk 'BEGIN {
  depth = 6
  printf "echo `level1 "
  for (level = 2; level <= depth; level += 1) {
    slash_count = 1
    for (power = 1; power < level; power += 1) slash_count *= 2
    slash_count -= 1
    for (slash = 0; slash < slash_count; slash += 1) printf "\\"
    printf "`level%d ", level
  }
  printf "leaf"
  for (level = depth; level >= 2; level -= 1) {
    slash_count = 1
    for (power = 1; power < level; power += 1) slash_count *= 2
    slash_count -= 1
    for (slash = 0; slash < slash_count; slash += 1) printf "\\"
    printf "` end%d", level
  }
  print "`"
}' >"$nested_backquotes"
assert_valid "$nested_backquotes"

small_spaced_continuations="$runtime_directory/small-spaced-continuations.sh"
large_spaced_continuations="$runtime_directory/large-spaced-continuations.sh"
small_direct_continuations="$runtime_directory/small-direct-continuations.sh"
large_direct_continuations="$runtime_directory/large-direct-continuations.sh"
small_arithmetic_layout="$runtime_directory/small-arithmetic-layout.sh"
large_arithmetic_layout="$runtime_directory/large-arithmetic-layout.sh"
small_arithmetic_opener="$runtime_directory/small-arithmetic-opener.sh"
large_arithmetic_opener="$runtime_directory/large-arithmetic-opener.sh"
small_dynamic_arithmetic_layout="$runtime_directory/small-dynamic-arithmetic-layout.sh"
large_dynamic_arithmetic_layout="$runtime_directory/large-dynamic-arithmetic-layout.sh"
small_name_continuations="$runtime_directory/small-name-continuations.sh"
large_name_continuations="$runtime_directory/large-name-continuations.sh"
small_bracket_suffixes="$runtime_directory/small-bracket-suffixes.sh"
large_bracket_suffixes="$runtime_directory/large-bracket-suffixes.sh"
awk 'BEGIN {
  printf "printf value"
  for (counter = 0; counter < 3000; counter += 1) printf " \\\n"
  print "after"
}' >"$small_spaced_continuations"
awk 'BEGIN {
  printf "printf value"
  for (counter = 0; counter < 12000; counter += 1) printf " \\\n"
  print "after"
}' >"$large_spaced_continuations"
awk 'BEGIN {
  printf "printf value "
  for (counter = 0; counter < 3000; counter += 1) printf "\\\n"
  print "after"
}' >"$small_direct_continuations"
awk 'BEGIN {
  printf "printf value "
  for (counter = 0; counter < 12000; counter += 1) printf "\\\n"
  print "after"
}' >"$large_direct_continuations"
awk 'BEGIN {
  printf ": \"$((a"
  for (counter = 0; counter < 2000; counter += 1) printf " \\\n"
  print "+ b))\""
}' >"$small_arithmetic_layout"
awk 'BEGIN {
  printf ": \"$((a"
  for (counter = 0; counter < 8000; counter += 1) printf " \\\n"
  print "+ b))\""
}' >"$large_arithmetic_layout"
awk 'BEGIN {
  printf ": \"$("
  for (counter = 0; counter < 3000; counter += 1) printf "\\\n"
  print "(a + b))\""
}' >"$small_arithmetic_opener"
awk 'BEGIN {
  printf ": \"$("
  for (counter = 0; counter < 12000; counter += 1) printf "\\\n"
  print "(a + b))\""
}' >"$large_arithmetic_opener"
awk 'BEGIN {
  printf ": \"$((1"
  for (counter = 0; counter < 2000; counter += 1) printf " \\\n"
  print "$operator 2))\""
}' >"$small_dynamic_arithmetic_layout"
awk 'BEGIN {
  printf ": \"$((1"
  for (counter = 0; counter < 8000; counter += 1) printf " \\\n"
  print "$operator 2))\""
}' >"$large_dynamic_arithmetic_layout"
awk 'BEGIN {
  printf "NA"
  for (counter = 0; counter < 3000; counter += 1) printf "\\\n"
  print "ME=value command"
}' >"$small_name_continuations"
awk 'BEGIN {
  printf "NA"
  for (counter = 0; counter < 12000; counter += 1) printf "\\\n"
  print "ME=value command"
}' >"$large_name_continuations"
awk 'BEGIN {
  printf "printf "
  for (counter = 0; counter < 3000; counter += 1) {
    printf "[a\"x\"*[.]"
  }
  print ""
}' >"$small_bracket_suffixes"
awk 'BEGIN {
  printf "printf "
  for (counter = 0; counter < 12000; counter += 1) {
    printf "[a\"x\"*[.]"
  }
  print ""
}' >"$large_bracket_suffixes"
assert_valid "$small_spaced_continuations"
assert_valid "$large_spaced_continuations"
assert_valid "$small_direct_continuations"
assert_valid "$large_direct_continuations"
assert_valid "$small_arithmetic_layout"
assert_valid "$large_arithmetic_layout"
assert_valid "$small_arithmetic_opener"
assert_valid "$large_arithmetic_opener"
assert_valid "$small_dynamic_arithmetic_layout"
assert_valid "$large_dynamic_arithmetic_layout"
assert_valid "$small_name_continuations"
assert_valid "$large_name_continuations"
assert_valid "$small_bracket_suffixes"
assert_valid "$large_bracket_suffixes"

measure_parse_milliseconds() {
  measured_source=$1
  start_nanoseconds=$(
    node -e 'process.stdout.write(String(process.hrtime.bigint()))'
  )
  parse_current \
    --quiet \
    --timeout 10000000 \
    "$measured_source" \
    >/dev/null 2>&1
  end_nanoseconds=$(
    node -e 'process.stdout.write(String(process.hrtime.bigint()))'
  )
  printf '%s\n' "$(((end_nanoseconds - start_nanoseconds) / 1000000))"
}

small_milliseconds=$(
  measure_parse_milliseconds "$small_spaced_continuations"
)
large_milliseconds=$(
  measure_parse_milliseconds "$large_spaced_continuations"
)
scaling_limit=$((small_milliseconds * 8 + 100))
if [ "$large_milliseconds" -gt "$scaling_limit" ]; then
  fail \
    "Spaced line-continuation parsing scaled nonlinearly: ${small_milliseconds}ms to ${large_milliseconds}ms"
fi

small_milliseconds=$(
  measure_parse_milliseconds "$small_direct_continuations"
)
large_milliseconds=$(
  measure_parse_milliseconds "$large_direct_continuations"
)
scaling_limit=$((small_milliseconds * 8 + 100))
if [ "$large_milliseconds" -gt "$scaling_limit" ]; then
  fail \
    "Direct line-continuation parsing scaled nonlinearly: ${small_milliseconds}ms to ${large_milliseconds}ms"
fi

small_milliseconds=$(measure_parse_milliseconds "$small_arithmetic_layout")
large_milliseconds=$(measure_parse_milliseconds "$large_arithmetic_layout")
scaling_limit=$((small_milliseconds * 8 + 100))
if [ "$large_milliseconds" -gt "$scaling_limit" ]; then
  fail \
    "Arithmetic layout parsing scaled nonlinearly: ${small_milliseconds}ms to ${large_milliseconds}ms"
fi

small_milliseconds=$(measure_parse_milliseconds "$small_arithmetic_opener")
large_milliseconds=$(measure_parse_milliseconds "$large_arithmetic_opener")
scaling_limit=$((small_milliseconds * 8 + 100))
if [ "$large_milliseconds" -gt "$scaling_limit" ]; then
  fail \
    "Arithmetic opener parsing scaled nonlinearly: ${small_milliseconds}ms to ${large_milliseconds}ms"
fi

small_milliseconds=$(
  measure_parse_milliseconds "$small_dynamic_arithmetic_layout"
)
large_milliseconds=$(
  measure_parse_milliseconds "$large_dynamic_arithmetic_layout"
)
scaling_limit=$((small_milliseconds * 8 + 100))
if [ "$large_milliseconds" -gt "$scaling_limit" ]; then
  fail \
    "Dynamic arithmetic layout parsing scaled nonlinearly: ${small_milliseconds}ms to ${large_milliseconds}ms"
fi

small_milliseconds=$(measure_parse_milliseconds "$small_name_continuations")
large_milliseconds=$(measure_parse_milliseconds "$large_name_continuations")
scaling_limit=$((small_milliseconds * 8 + 100))
if [ "$large_milliseconds" -gt "$scaling_limit" ]; then
  fail \
    "Name continuation parsing scaled nonlinearly: ${small_milliseconds}ms to ${large_milliseconds}ms"
fi

small_milliseconds=$(measure_parse_milliseconds "$small_bracket_suffixes")
large_milliseconds=$(measure_parse_milliseconds "$large_bracket_suffixes")
scaling_limit=$((small_milliseconds * 8 + 100))
if [ "$large_milliseconds" -gt "$scaling_limit" ]; then
  fail \
    "Bracket suffix parsing scaled nonlinearly: ${small_milliseconds}ms to ${large_milliseconds}ms"
fi
