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

query_current() {
  "$tree_sitter" query \
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

assert_cst_direct_child_range() {
  parent_range=$1
  parent_item=$2
  child_range=$3
  child_item=$4
  file=$5
  if ! awk \
    -v parent_range="$parent_range" \
    -v parent_item="$parent_item" \
    -v child_range="$child_range" \
    -v child_item="$child_item" '
      {
        separator = index($0, " - ")
        if (separator == 0) {
          next
        }

        start = substr($0, 1, separator - 1)
        sub(/[[:space:]]+$/, "", start)
        tail = substr($0, separator + 3)
        match(tail, /^[^[:space:]]+/)
        end = substr(tail, RSTART, RLENGTH)
        content = substr(tail, RLENGTH + 1)
        match(content, /[^[:space:]]/)
        depth = RSTART - 1
        content = substr(content, RSTART)
        if (sub(/^•/, "", content)) {
          depth += 1
        }
        range = start "-" end

        if (!inside && range == parent_range && content == parent_item) {
          inside = 1
          parent_depth = depth
          next
        }
        if (!inside) {
          next
        }
        if (depth <= parent_depth) {
          exit 1
        }
        if (depth == parent_depth + 2 && range == child_range && (child_item == "*" || content == child_item)) {
          found = 1
          exit 0
        }
      }
      END { exit found ? 0 : 1 }
    ' "$file"; then
    printf '%s\n' \
      "Expected $child_item at $child_range directly under $parent_item at $parent_range" >&2
    sed -n '1,200p' "$file" >&2
    exit 1
  fi
}

assert_parse_with_output() {
  source_file=$1
  output_file=$2
  if parse_current \
    --timeout 10000000 \
    "$source_file" \
    >"$output_file" 2>/dev/null; then
    return
  else
    status=$?
  fi
  if [ "$status" -gt 1 ]; then
    fail "Parser failed with status $status while recovering source: $source_file"
  fi
}

assert_parse_contains_all() {
  source_file=$1
  output_file=$2
  shift 2
  assert_parse_with_output "$source_file" "$output_file"
  for expected_item; do
    assert_contains "$expected_item" "$output_file"
  done
}

parse_incremental_and_fresh_with_output() {
  initial_file=$1
  final_file=$2
  test_name=$3
  shift 3

  incremental_output="$runtime_directory/$test_name.incremental"
  fresh_output="$runtime_directory/$test_name.fresh"
  initial_xml="$runtime_directory/$test_name.initial.xml"
  incremental_xml="$runtime_directory/$test_name.incremental.xml"
  fresh_xml="$runtime_directory/$test_name.fresh.xml"
  initial_public="$initial_xml.public"
  incremental_public="$incremental_xml.public"
  fresh_public="$fresh_xml.public"

  if parse_current \
    --timeout 10000000 \
    --edits "$@" \
    -- "$initial_file" \
    >"$incremental_output" 2>/dev/null; then
    :
  else
    status=$?
    if [ "$status" -gt 1 ]; then
      fail "Incremental parser failed with status $status while recovering source: $test_name"
    fi
  fi

  if parse_current \
    --timeout 10000000 \
    "$final_file" \
    >"$fresh_output" 2>/dev/null; then
    :
  else
    status=$?
    if [ "$status" -gt 1 ]; then
      fail "Fresh parser failed with status $status while recovering source: $test_name"
    fi
  fi

  if parse_current \
    --xml \
    --timeout 10000000 \
    --edits "$@" \
    -- "$initial_file" \
    >"$incremental_xml" 2>/dev/null; then
    incremental_status=valid
  else
    status=$?
    if [ "$status" -gt 1 ]; then
      fail "Incremental XML parser failed with status $status while recovering source: $test_name"
    fi
    incremental_status=invalid
  fi

  if parse_current \
    --xml \
    --timeout 10000000 \
    "$final_file" \
    >"$fresh_xml" 2>/dev/null; then
    fresh_status=valid
  else
    status=$?
    if [ "$status" -gt 1 ]; then
      fail "Fresh XML parser failed with status $status while recovering source: $test_name"
    fi
    fresh_status=invalid
  fi

  parse_current \
    --xml \
    --timeout 10000000 \
    "$initial_file" \
    >"$initial_xml" 2>/dev/null || true
  normalize_public_xml "$incremental_xml" "$incremental_public"
  normalize_public_xml "$fresh_xml" "$fresh_public"
  normalize_public_xml "$initial_xml" "$initial_public"

  if [ "$incremental_status" != "$fresh_status" ]; then
    fail "Incremental and fresh statuses differ: $test_name"
  fi

  if ! cmp -s "$incremental_public" "$fresh_public"; then
    diff -u "$fresh_public" "$incremental_public" >&2 || true
    fail "Incremental and fresh public CSTs differ: $test_name"
  fi

  if cmp -s "$initial_public" "$fresh_public"; then
    fail "Incremental recovery regression does not change the public CST: $test_name"
  fi
}

normalize_public_xml() {
  input_file=$1
  output_file=$2
  awk '
    function print_indent(count, cursor) {
      for (cursor = 0; cursor < count; cursor += 1) {
        printf " "
      }
    }

    /^<\?xml / { next }
    /^[[:space:]]*<\/?sources>/ { next }
    /^[[:space:]]*<source / { next }
    /^[[:space:]]*<\/source>/ { next }

    {
      line = $0
      trimmed = line
      sub(/^[[:space:]]*/, "", trimmed)

      if (trimmed ~ /^<(ERROR|MISSING)([[:space:]>]|\/)/) {
        if (trimmed !~ /<\/(ERROR|MISSING)>/ && trimmed !~ /\/>$/) {
          artifact_depth += 1
        }
        next
      }
      if (trimmed ~ /^<\/(ERROR|MISSING)>/) {
        artifact_depth -= 1
        next
      }
      if (trimmed !~ /^</) {
        next
      }

      match(line, /[^[:space:]]/)
      indentation = RSTART - 1 - 4 - artifact_depth * 2
      if (indentation < 0) {
        indentation = 0
      }
      gsub(/>[^<]*</, "><", trimmed)
      print_indent(indentation)
      print trimmed
    }
  ' "$input_file" >"$output_file"
}

assert_incremental_equals_fresh_status() {
  expected_status=$1
  shift
  initial_file=$1
  final_file=$2
  test_name=$3
  shift 3

  initial_output="$runtime_directory/$test_name.initial.xml"
  incremental_output="$runtime_directory/$test_name.incremental.xml"
  fresh_output="$runtime_directory/$test_name.fresh.xml"
  incremental_public="$incremental_output.public"
  fresh_public="$fresh_output.public"
  initial_public="$initial_output.public"

  if parse_current \
    --xml \
    --timeout 10000000 \
    --edits "$@" \
    -- "$initial_file" \
    >"$incremental_output" 2>/dev/null; then
    incremental_status=valid
  else
    incremental_status=invalid
  fi
  if parse_current \
    --xml \
    --timeout 10000000 \
    "$final_file" \
    >"$fresh_output" 2>/dev/null; then
    fresh_status=valid
  else
    fresh_status=invalid
  fi
  parse_current \
    --xml \
    --timeout 10000000 \
    "$initial_file" \
    >"$initial_output" 2>/dev/null || true
  normalize_public_xml "$incremental_output" "$incremental_public"
  normalize_public_xml "$fresh_output" "$fresh_public"
  normalize_public_xml "$initial_output" "$initial_public"

  if [ "$incremental_status" != "$fresh_status" ]; then
    fail "Incremental and fresh statuses differ: $test_name"
  fi

  if [ "$fresh_status" != "$expected_status" ]; then
    fail "Expected $expected_status incremental and fresh parses: $test_name"
  fi

  if ! cmp -s "$incremental_public" "$fresh_public"; then
    diff -u "$fresh_public" "$incremental_public" >&2 || true
    fail "Incremental and fresh public CSTs differ: $test_name"
  fi

  if cmp -s "$initial_public" "$fresh_public"; then
    fail "Incremental regression does not change the public CST: $test_name"
  fi
}

assert_incremental_equals_fresh() {
  assert_incremental_equals_fresh_status valid "$@"
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
compound_query_output="$runtime_directory/recovery-compound.query"
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
query_current \
  "$repository_directory/test/runtime/recovery_contract.scm" \
  "$compound_source" \
  >"$compound_query_output" 2>/dev/null
assert_contains "compound.owner" "$compound_query_output"
assert_contains "compound.recovery" "$compound_query_output"

nested_compound_initial="$runtime_directory/recovery-nested-compound-initial.sh"
nested_compound_final="$runtime_directory/recovery-nested-compound-final.sh"
printf '%s\n' 'if x; then while y; do z; done; fi' \
  >"$nested_compound_initial"
printf '%s\n' 'if x; then while y; do z; fi' >"$nested_compound_final"
parse_incremental_and_fresh_with_output \
  "$nested_compound_initial" \
  "$nested_compound_final" \
  "nested-compound-outer-closer" \
  "26 6"
for nested_compound_output in \
  "$runtime_directory/nested-compound-outer-closer.incremental" \
  "$runtime_directory/nested-compound-outer-closer.fresh"; do
  assert_contains "(while_clause [0, 11] - [0, 26]" \
    "$nested_compound_output"
  assert_contains \
    "recovery: (compound_command_recovery [0, 26] - [0, 26])" \
    "$nested_compound_output"
  assert_contains "(fi_keyword [0, 26] - [0, 28])" \
    "$nested_compound_output"
done

nested_case_recovery="$runtime_directory/recovery-nested-case.sh"
nested_case_recovery_output="$runtime_directory/recovery-nested-case.out"
printf '%s\n' 'case x in x) if y; then z;; esac' \
  >"$nested_case_recovery"
assert_parse_with_output \
  "$nested_case_recovery" \
  "$nested_case_recovery_output"
assert_contains "(if_clause [0, 13] - [0, 25]" \
  "$nested_case_recovery_output"
assert_contains "(separator_recovery [0, 25] - [0, 25])" \
  "$nested_case_recovery_output"
assert_contains \
  "recovery: (compound_command_recovery [0, 25] - [0, 25])" \
  "$nested_case_recovery_output"
assert_contains "terminator: (dsemi [0, 25] - [0, 27])" \
  "$nested_case_recovery_output"
assert_contains "(esac_keyword [0, 28] - [0, 32])" \
  "$nested_case_recovery_output"

same_kind_if_recovery="$runtime_directory/recovery-same-kind-if.sh"
same_kind_if_recovery_output="$runtime_directory/recovery-same-kind-if.out"
printf '%s\n' 'if x; then if y; then z; fi' >"$same_kind_if_recovery"
assert_parse_with_output \
  "$same_kind_if_recovery" \
  "$same_kind_if_recovery_output"
assert_contains "(fi_keyword [0, 25] - [0, 27])" \
  "$same_kind_if_recovery_output"
assert_contains \
  "recovery: (compound_command_recovery [1, 0] - [1, 0])" \
  "$same_kind_if_recovery_output"

same_kind_while_recovery="$runtime_directory/recovery-same-kind-while.sh"
same_kind_while_recovery_output="$runtime_directory/recovery-same-kind-while.out"
printf '%s\n' 'while x; do while y; do z; done' \
  >"$same_kind_while_recovery"
assert_parse_with_output \
  "$same_kind_while_recovery" \
  "$same_kind_while_recovery_output"
assert_contains "(done_keyword [0, 27] - [0, 31])" \
  "$same_kind_while_recovery_output"
assert_contains \
  "recovery: (compound_command_recovery [1, 0] - [1, 0])" \
  "$same_kind_while_recovery_output"

reserved_word_arguments="$runtime_directory/recovery-reserved-word-arguments.sh"
printf '%s\n' \
  'if x; then echo fi done esac; fi' \
  'case x in x) echo fi done esac;; esac' \
  'case fi in fi) :;; esac' \
  'case done in done) :;; esac' \
  'if x; then case fi in fi) :;; esac; fi' \
  'while x; do case done in done) :;; esac; done' \
  >"$reserved_word_arguments"
assert_valid "$reserved_word_arguments"

case_esac_pattern="$runtime_directory/case-esac-pattern.sh"
case_esac_pattern_output="$runtime_directory/case-esac-pattern.out"
printf '%s\n' 'case esac in esac) :;; esac' >"$case_esac_pattern"
assert_parse_with_output "$case_esac_pattern" "$case_esac_pattern_output"
assert_contains "(esac_keyword [0, 13] - [0, 17])" \
  "$case_esac_pattern_output"

parameter_recovery_initial="$runtime_directory/recovery-parameter-initial.sh"
parameter_recovery_final="$runtime_directory/recovery-parameter-final.sh"
printf '%s\n' 'echo ${x}; next' >"$parameter_recovery_initial"
printf '%s\n' 'echo ${x; next' >"$parameter_recovery_final"
parse_incremental_and_fresh_with_output \
  "$parameter_recovery_initial" \
  "$parameter_recovery_final" \
  "parameter-tail-recovery" \
  "8 1"
for parameter_recovery_output in \
  "$runtime_directory/parameter-tail-recovery.incremental" \
  "$runtime_directory/parameter-tail-recovery.fresh"; do
  assert_contains \
    "recovery: (parameter_expansion_recovery [0, 8] - [0, 8])" \
    "$parameter_recovery_output"
  assert_contains "(and_or [0, 10] - [0, 14]" "$parameter_recovery_output"
done

parameter_recovery_query="$runtime_directory/recovery-parameter.query"
query_current \
  "$repository_directory/test/runtime/recovery_contract.scm" \
  "$parameter_recovery_final" \
  >"$parameter_recovery_query" 2>/dev/null
assert_contains "parameter.owner" "$parameter_recovery_query"
assert_contains "parameter.recovery" "$parameter_recovery_query"

parameter_recovery_boundaries="$runtime_directory/recovery-parameter-boundaries.sh"
parameter_recovery_boundaries_output="$runtime_directory/recovery-parameter-boundaries.out"
printf '%s\n' \
  'echo ${x' \
  'next' \
  'echo "${x"; next' \
  'echo $(printf ${x); next' \
  'echo ${00& next' \
  >"$parameter_recovery_boundaries"
assert_parse_with_output \
  "$parameter_recovery_boundaries" \
  "$parameter_recovery_boundaries_output"
assert_contains \
  "recovery: (parameter_expansion_recovery [0, 8] - [0, 8])" \
  "$parameter_recovery_boundaries_output"
assert_contains "(complete_command [1, 0] - [1, 4]" \
  "$parameter_recovery_boundaries_output"
assert_contains \
  "recovery: (parameter_expansion_recovery [2, 9] - [2, 9])" \
  "$parameter_recovery_boundaries_output"
assert_contains \
  "recovery: (parameter_expansion_recovery [3, 17] - [3, 17])" \
  "$parameter_recovery_boundaries_output"
assert_contains \
  "recovery: (parameter_expansion_recovery [4, 9] - [4, 9])" \
  "$parameter_recovery_boundaries_output"

parameter_operator_metacharacters="$runtime_directory/parameter-operator-metacharacters.sh"
parameter_operator_metacharacters_output="$runtime_directory/parameter-operator-metacharacters.out"
printf '%s\n' 'echo ${x:-a;b} ${x:-a&b}' \
  >"$parameter_operator_metacharacters"
assert_valid_with_output \
  "$parameter_operator_metacharacters" \
  "$parameter_operator_metacharacters_output"
assert_not_contains \
  "parameter_expansion_recovery" \
  "$parameter_operator_metacharacters_output"

subshell_initial="$runtime_directory/recovery-subshell-initial.sh"
subshell_final="$runtime_directory/recovery-subshell-final.sh"
printf '%s\n' '(inside)' >"$subshell_initial"
printf '%s\n' '(inside' >"$subshell_final"
parse_incremental_and_fresh_with_output \
  "$subshell_initial" \
  "$subshell_final" \
  "unclosed-subshell" \
  "7 1"
for subshell_output in \
  "$runtime_directory/unclosed-subshell.incremental" \
  "$runtime_directory/unclosed-subshell.fresh"; do
  assert_contains "(subshell [0, 0] - [1, 0]" "$subshell_output"
  assert_contains "(and_or [0, 1] - [0, 7]" "$subshell_output"
  assert_contains \
    "recovery: (compound_command_recovery [1, 0] - [1, 0])" \
    "$subshell_output"
done

group_recovery_query="$runtime_directory/recovery-group.query"
query_current \
  "$repository_directory/test/runtime/recovery_contract.scm" \
  "$subshell_final" \
  >"$group_recovery_query" 2>/dev/null
assert_contains "group.owner" "$group_recovery_query"
assert_contains "group.recovery" "$group_recovery_query"

brace_group_initial="$runtime_directory/recovery-brace-group-initial.sh"
brace_group_final="$runtime_directory/recovery-brace-group-final.sh"
printf '%s\n' '{ inside; }' >"$brace_group_initial"
printf '%s\n' '{ inside;' >"$brace_group_final"
parse_incremental_and_fresh_with_output \
  "$brace_group_initial" \
  "$brace_group_final" \
  "unclosed-brace-group" \
  "9 2"
for brace_group_output in \
  "$runtime_directory/unclosed-brace-group.incremental" \
  "$runtime_directory/unclosed-brace-group.fresh"; do
  assert_contains "(brace_group [0, 0] - [1, 0]" "$brace_group_output"
  assert_contains "(and_or [0, 2] - [0, 8]" "$brace_group_output"
  assert_contains \
    "recovery: (compound_command_recovery [1, 0] - [1, 0])" \
    "$brace_group_output"
done

nested_group_recovery="$runtime_directory/recovery-nested-groups.sh"
nested_group_recovery_output="$runtime_directory/recovery-nested-groups.out"
printf '%s\n' '{ (inside; }' '( { inside; )' \
  >"$nested_group_recovery"
assert_parse_with_output \
  "$nested_group_recovery" \
  "$nested_group_recovery_output"
assert_contains "(subshell [0, 2] - [0, 11]" \
  "$nested_group_recovery_output"
assert_contains \
  "recovery: (compound_command_recovery [0, 11] - [0, 11])" \
  "$nested_group_recovery_output"
assert_contains "(brace_group [1, 2] - [1, 12]" \
  "$nested_group_recovery_output"
assert_contains \
  "recovery: (compound_command_recovery [1, 12] - [1, 12])" \
  "$nested_group_recovery_output"

nested_compound_group_recovery="$runtime_directory/recovery-nested-compound-groups.sh"
nested_compound_group_recovery_output="$runtime_directory/recovery-nested-compound-groups.out"
printf '%s\n' '{ if x; then y; }' '(if x; then y;)' \
  >"$nested_compound_group_recovery"
assert_parse_with_output \
  "$nested_compound_group_recovery" \
  "$nested_compound_group_recovery_output"
assert_contains "(brace_group [0, 0] - [0, 17]" \
  "$nested_compound_group_recovery_output"
assert_contains "(if_clause [0, 2] - [0, 16]" \
  "$nested_compound_group_recovery_output"
assert_contains \
  "recovery: (compound_command_recovery [0, 16] - [0, 16])" \
  "$nested_compound_group_recovery_output"
assert_contains "(subshell [1, 0] - [1, 15]" \
  "$nested_compound_group_recovery_output"
assert_contains "(if_clause [1, 1] - [1, 14]" \
  "$nested_compound_group_recovery_output"
assert_contains \
  "recovery: (compound_command_recovery [1, 14] - [1, 14])" \
  "$nested_compound_group_recovery_output"

unclosed_function_group="$runtime_directory/recovery-function-group.sh"
unclosed_function_group_output="$runtime_directory/recovery-function-group.out"
printf '%s\n' 'f() { inside;' >"$unclosed_function_group"
assert_parse_with_output \
  "$unclosed_function_group" \
  "$unclosed_function_group_output"
assert_contains "(function_definition [0, 0] - [1, 0]" \
  "$unclosed_function_group_output"
assert_contains "(brace_group [0, 4] - [1, 0]" \
  "$unclosed_function_group_output"
assert_contains \
  "recovery: (compound_command_recovery [1, 0] - [1, 0])" \
  "$unclosed_function_group_output"

empty_groups="$runtime_directory/recovery-empty-groups.sh"
empty_groups_output="$runtime_directory/recovery-empty-groups.out"
printf '%s\n' '()' '{ }' >"$empty_groups"
assert_parse_with_output "$empty_groups" "$empty_groups_output"
assert_contains "(subshell [0, 0] - [0, 2]" "$empty_groups_output"
assert_contains \
  "recovery: (compound_command_recovery [0, 1] - [0, 1])" \
  "$empty_groups_output"
assert_contains "(brace_group [1, 0] - [1, 3]" "$empty_groups_output"
assert_contains \
  "recovery: (compound_command_recovery [1, 2] - [1, 2])" \
  "$empty_groups_output"

long_brace_closer="$runtime_directory/recovery-long-brace-closer.sh"
long_brace_closer_output="$runtime_directory/recovery-long-brace-closer.out"
printf '%s\n' 'holder() { first; }suffix' >"$long_brace_closer"
assert_parse_with_output "$long_brace_closer" "$long_brace_closer_output"
assert_contains "(brace_group [0, 9] - [1, 0]" \
  "$long_brace_closer_output"
assert_contains \
  "recovery: (compound_command_recovery [1, 0] - [1, 0])" \
  "$long_brace_closer_output"

right_brace_source="$runtime_directory/recovery-right-brace.sh"
right_brace_output="$runtime_directory/recovery-right-brace.out"
printf '%s\n' "first; }" >"$right_brace_source"
assert_parse_with_output "$right_brace_source" "$right_brace_output"
assert_not_contains "(word [0, 7] - [0, 8]" \
  "$right_brace_output"

parameter_pattern_initial="$runtime_directory/recovery-parameter-pattern-initial.sh"
parameter_pattern_final="$runtime_directory/recovery-parameter-pattern-final.sh"
printf '%s\n' 'echo "${x%foo}"; next' >"$parameter_pattern_initial"
printf '%s\n' 'echo "${x%foo"; next' >"$parameter_pattern_final"
parse_incremental_and_fresh_with_output \
  "$parameter_pattern_initial" \
  "$parameter_pattern_final" \
  "parameter-pattern-tail-recovery" \
  "13 1"
for parameter_pattern_output in \
  "$runtime_directory/parameter-pattern-tail-recovery.incremental" \
  "$runtime_directory/parameter-pattern-tail-recovery.fresh"; do
  assert_contains \
    "(parameter_expansion [0, 6] - [0, 13]" \
    "$parameter_pattern_output"
  assert_contains \
    "recovery: (parameter_expansion_recovery [0, 13] - [0, 13])" \
    "$parameter_pattern_output"
  assert_contains "(and_or [0, 16] - [0, 20]" \
    "$parameter_pattern_output"
done
parameter_pattern_cst="$runtime_directory/recovery-parameter-pattern.cst"
assert_cst_valid_with_output \
  "$parameter_pattern_final" \
  "$parameter_pattern_cst"
assert_cst_direct_child_range \
  "0:5-0:14" \
  "double_quoted" \
  "0:13-0:14" \
  "*" \
  "$parameter_pattern_cst"

empty_body_source="$runtime_directory/recovery-empty-bodies.sh"
empty_body_output="$runtime_directory/recovery-empty-bodies.out"
empty_body_cst="$runtime_directory/recovery-empty-bodies.cst"
printf '%s\n' \
  'if x; then fi' \
  'after_if' \
  'while x; do done' \
  'after_while' \
  'for x; do done' \
  'after_for' \
  '{ }' \
  'after_brace' \
  '()' \
  'after_subshell' \
  >"$empty_body_source"
assert_parse_contains_all \
  "$empty_body_source" \
  "$empty_body_output" \
  "(if_clause [0, 0] - [0, 13]" \
  "recovery: (compound_command_recovery [0, 11] - [0, 11])" \
  "(fi_keyword [0, 11] - [0, 13])" \
  "(complete_command [1, 0] - [1, 8]" \
  "(while_clause [2, 0] - [2, 16]" \
  "recovery: (compound_command_recovery [2, 12] - [2, 12])" \
  "(done_keyword [2, 12] - [2, 16])" \
  "(complete_command [3, 0] - [3, 11]" \
  "(for_clause [4, 0] - [4, 14]" \
  "recovery: (compound_command_recovery [4, 10] - [4, 10])" \
  "(done_keyword [4, 10] - [4, 14])" \
  "(complete_command [5, 0] - [5, 9]" \
  "(brace_group [6, 0] - [6, 3]" \
  "recovery: (compound_command_recovery [6, 2] - [6, 2])" \
  "(complete_command [7, 0] - [7, 11]" \
  "(subshell [8, 0] - [8, 2]" \
  "recovery: (compound_command_recovery [8, 1] - [8, 1])" \
  "(complete_command [9, 0] - [9, 14]"
assert_cst_valid_with_output "$empty_body_source" "$empty_body_cst"
assert_cst_direct_child_range \
  "6:0-6:3" \
  "brace_group" \
  "6:2-6:3" \
  '"}"' \
  "$empty_body_cst"
assert_cst_direct_child_range \
  "8:0-8:2" \
  "subshell" \
  "8:1-8:2" \
  '")"' \
  "$empty_body_cst"

empty_if_initial="$runtime_directory/recovery-empty-if-initial.sh"
empty_if_final="$runtime_directory/recovery-empty-if-final.sh"
printf '%s\n' 'if x; then :; fi' 'next' >"$empty_if_initial"
printf '%s\n' 'if x; then fi' 'next' >"$empty_if_final"
parse_incremental_and_fresh_with_output \
  "$empty_if_initial" \
  "$empty_if_final" \
  "delete-if-body" \
  "11 3"

empty_do_initial="$runtime_directory/recovery-empty-do-initial.sh"
empty_do_final="$runtime_directory/recovery-empty-do-final.sh"
printf '%s\n' 'while x; do :; done' 'next' >"$empty_do_initial"
printf '%s\n' 'while x; do done' 'next' >"$empty_do_final"
parse_incremental_and_fresh_with_output \
  "$empty_do_initial" \
  "$empty_do_final" \
  "delete-do-group-body" \
  "12 3"

empty_brace_initial="$runtime_directory/recovery-empty-brace-initial.sh"
empty_brace_final="$runtime_directory/recovery-empty-brace-final.sh"
printf '%s\n' '{ :; }' 'next' >"$empty_brace_initial"
printf '%s\n' '{ }' 'next' >"$empty_brace_final"
parse_incremental_and_fresh_with_output \
  "$empty_brace_initial" \
  "$empty_brace_final" \
  "delete-brace-body" \
  "2 3"

missing_owner_source="$runtime_directory/recovery-missing-owners.sh"
missing_owner_output="$runtime_directory/recovery-missing-owners.out"
printf '%s\n' \
  'case' \
  'after_case' \
  'case # comment' \
  'after_commented_case' \
  'for' \
  'after_for' \
  'for # comment' \
  'after_commented_for' \
  'f()' \
  'after_function' \
  'g() # comment' \
  'after_commented_function' \
  >"$missing_owner_source"
assert_parse_contains_all \
  "$missing_owner_source" \
  "$missing_owner_output" \
  "(case_clause [0, 0] - [0, 4]" \
  "(complete_command [1, 0] - [1, 10]" \
  "(case_clause [2, 0] - [2, 5]" \
  "(complete_command [3, 0] - [3, 20]" \
  "(for_clause [4, 0] - [4, 3]" \
  "(complete_command [5, 0] - [5, 9]" \
  "(for_clause [6, 0] - [6, 4]" \
  "(complete_command [7, 0] - [7, 19]" \
  "(function_definition [8, 0] - [8, 3]" \
  "name: (fname [8, 0] - [8, 1])" \
  "(complete_command [9, 0] - [9, 14]" \
  "(function_definition [10, 0] - [10, 3]" \
  "name: (fname [10, 0] - [10, 1])" \
  "(complete_command [11, 0] - [11, 24]"

function_body_initial="$runtime_directory/recovery-function-body-initial.sh"
function_body_final="$runtime_directory/recovery-function-body-final.sh"
printf '%s\n' 'f()' '{ :; }' >"$function_body_initial"
printf '%s\n' 'f()' 'next' >"$function_body_final"
parse_incremental_and_fresh_with_output \
  "$function_body_initial" \
  "$function_body_final" \
  "replace-function-body" \
  "4 6 next"

stray_source="$runtime_directory/recovery-stray-closers.sh"
stray_output="$runtime_directory/recovery-stray-closers.out"
printf '%s\n' \
  'first; fi' \
  'after' \
  'first; }' \
  'after_brace' \
  'first; )' \
  'after_parenthesis' \
  'first; ;;' \
  'after_dsemi' \
  'first; ;&' \
  'after_semi_and' \
  >"$stray_source"
assert_parse_contains_all \
  "$stray_source" \
  "$stray_output" \
  "recovery: (command_recovery [0, 7] - [0, 9]" \
  "(fi_keyword [0, 7] - [0, 9])" \
  "(complete_command [1, 0] - [1, 5]" \
  "recovery: (command_recovery [2, 7] - [2, 8])" \
  "(complete_command [3, 0] - [3, 11]" \
  "recovery: (command_recovery [4, 7] - [4, 8])" \
  "(complete_command [5, 0] - [5, 17]" \
  "recovery: (command_recovery [6, 7] - [6, 9]" \
  "(dsemi [6, 7] - [6, 9])" \
  "(complete_command [7, 0] - [7, 11]" \
  "recovery: (command_recovery [8, 7] - [8, 9]" \
  "(semi_and [8, 7] - [8, 9])" \
  "(complete_command [9, 0] - [9, 14]"
assert_not_contains "(word [0, 7] - [0, 9]" "$stray_output"
assert_not_contains "(word [2, 7] - [2, 8]" "$stray_output"
assert_not_contains "(word [4, 7] - [4, 8]" "$stray_output"
assert_not_contains "(word [6, 7] - [6, 9]" "$stray_output"
assert_not_contains "(word [8, 7] - [8, 9]" "$stray_output"

continued_reserved_source="$runtime_directory/recovery-continued-reserved.sh"
continued_reserved_output="$runtime_directory/recovery-continued-reserved.out"
printf '%s\n' 'first; f\' 'i' 'after' >"$continued_reserved_source"
assert_parse_contains_all \
  "$continued_reserved_source" \
  "$continued_reserved_output" \
  "recovery: (command_recovery [0, 7] - [1, 1]" \
  "(fi_keyword [0, 7] - [1, 1]" \
  "(line_continuation [0, 8] - [1, 0])" \
  "(complete_command [2, 0] - [2, 5]"

stray_initial="$runtime_directory/recovery-stray-initial.sh"
stray_final="$runtime_directory/recovery-stray-final.sh"
printf '%s\n' 'first' 'after' >"$stray_initial"
printf '%s\n' 'first; fi' 'after' >"$stray_final"
parse_incremental_and_fresh_with_output \
  "$stray_initial" \
  "$stray_final" \
  "insert-stray-reserved-closer" \
  "5 0 ; fi"

stray_parenthesis_final="$runtime_directory/recovery-stray-parenthesis-final.sh"
printf '%s\n' 'first; )' 'after' >"$stray_parenthesis_final"
parse_incremental_and_fresh_with_output \
  "$stray_initial" \
  "$stray_parenthesis_final" \
  "insert-stray-right-parenthesis" \
  "5 0 ; )"

and_if_semicolon_source="$runtime_directory/recovery-and-if-semicolon.sh"
and_if_semicolon_output="$runtime_directory/recovery-and-if-semicolon.out"
printf '%s\n' 'broken() {' '  first &&;' '}' 'after=$(ok)' \
  >"$and_if_semicolon_source"
assert_parse_contains_all \
  "$and_if_semicolon_source" \
  "$and_if_semicolon_output" \
  "(function_definition [0, 0] - [2, 1]" \
  "recovery: (command_recovery [1, 10] - [1, 10]" \
  "(complete_command [3, 0] - [3, 11]"

and_if_newline_initial="$runtime_directory/recovery-and-if-newline-initial.sh"
and_if_newline_final="$runtime_directory/recovery-and-if-newline-final.sh"
printf '%s\n' 'broken() {' '  first && :' '}' 'after=$(ok)' \
  >"$and_if_newline_initial"
printf '%s\n' 'broken() {' '  first && ' '}' 'after=$(ok)' \
  >"$and_if_newline_final"
parse_incremental_and_fresh_with_output \
  "$and_if_newline_initial" \
  "$and_if_newline_final" \
  "delete-command-after-and-if" \
  "22 1"
for and_if_newline_output in \
  "$runtime_directory/delete-command-after-and-if.incremental" \
  "$runtime_directory/delete-command-after-and-if.fresh"; do
  assert_contains \
    "recovery: (command_recovery [2, 0] - [2, 0])" \
    "$and_if_newline_output"
  assert_contains \
    "(complete_command [3, 0] - [3, 11]" \
    "$and_if_newline_output"
done

backquote_opener_source="$runtime_directory/backquote-opener-boundaries.sh"
backquote_opener_output="$runtime_directory/backquote-opener-boundaries.out"
printf '%s\n' \
  '{ `printf brace`; }' \
  ': && `printf and-or`' \
  'case `printf selector` in' \
  '  selector) : ;;' \
  'esac' \
  >"$backquote_opener_source"
assert_valid_with_output "$backquote_opener_source" "$backquote_opener_output"
assert_contains \
  "(backquote_substitution [0, 2] - [0, 16]" \
  "$backquote_opener_output"
assert_contains \
  "(backquote_substitution [1, 5] - [1, 20]" \
  "$backquote_opener_output"
assert_contains \
  "(backquote_substitution [2, 5] - [2, 22]" \
  "$backquote_opener_output"
assert_not_contains "recovery" "$backquote_opener_output"

backquote_assignment_initial="$runtime_directory/backquote-assignment-initial.sh"
backquote_assignment_final="$runtime_directory/backquote-assignment-final.sh"
printf '%s\n' 'worker() {' '  :' '}' 'result="`printf nested`"' \
  >"$backquote_assignment_initial"
printf '%s\n' 'worker() {' '  :' '}' 'result=`printf nested`' \
  >"$backquote_assignment_final"
assert_valid_incremental_equals_fresh \
  "$backquote_assignment_initial" \
  "$backquote_assignment_final" \
  "remove-double-quotes-around-assignment-backquote" \
  "24 1" \
  "39 1"

command_substitution_layout_initial="$runtime_directory/command-substitution-layout-initial.sh"
command_substitution_layout_final="$runtime_directory/command-substitution-layout-final.sh"
printf '%s\n' 'echo $(first;)' >"$command_substitution_layout_initial"
printf '%s\n' 'echo $(first; )' >"$command_substitution_layout_final"
assert_valid_incremental_equals_fresh \
  "$command_substitution_layout_initial" \
  "$command_substitution_layout_final" \
  "insert-layout-before-command-substitution-closer" \
  "13 0  "

backquote_layout_initial="$runtime_directory/backquote-layout-initial.sh"
backquote_layout_final="$runtime_directory/backquote-layout-final.sh"
printf '%s\n' 'echo `first;`' >"$backquote_layout_initial"
printf '%s\n' 'echo `first; `' >"$backquote_layout_final"
assert_valid_incremental_equals_fresh \
  "$backquote_layout_initial" \
  "$backquote_layout_final" \
  "insert-layout-before-backquote-closer" \
  "12 0  "

outer_closer_source="$runtime_directory/recovery-enclosing-closers.sh"
outer_closer_output="$runtime_directory/recovery-enclosing-closers.out"
outer_closer_cst="$runtime_directory/recovery-enclosing-closers.cst"
printf '%s\n' \
  '(if x; then y); next' \
  'echo `if x; then y`; next' \
  'echo `printf ${x`; next' \
  >"$outer_closer_source"
assert_parse_contains_all \
  "$outer_closer_source" \
  "$outer_closer_output" \
  "(subshell [0, 0] - [0, 14]" \
  "terminator: (separator_recovery [0, 13] - [0, 13])" \
  "recovery: (compound_command_recovery [0, 13] - [0, 13])" \
  "(and_or [0, 16] - [0, 20]" \
  "(backquote_substitution [1, 5] - [1, 19]" \
  "terminator: (separator_recovery [1, 18] - [1, 18])" \
  "recovery: (compound_command_recovery [1, 18] - [1, 18])" \
  "(and_or [1, 21] - [1, 25]" \
  "(backquote_substitution [2, 5] - [2, 17]" \
  "(parameter_expansion_recovery [2, 16] - [2, 16])" \
  "(and_or [2, 19] - [2, 23]"
assert_cst_valid_with_output "$outer_closer_source" "$outer_closer_cst"
assert_cst_direct_child_range \
  "0:0-0:14" \
  "subshell" \
  "0:13-0:14" \
  '")"' \
  "$outer_closer_cst"
assert_cst_direct_child_range \
  "1:5-1:19" \
  "backquote_substitution" \
  "1:18-1:19" \
  "*" \
  "$outer_closer_cst"
assert_cst_direct_child_range \
  "2:5-2:17" \
  "backquote_substitution" \
  "2:16-2:17" \
  "*" \
  "$outer_closer_cst"

missing_command_parenthesis_source="$runtime_directory/recovery-missing-command-parenthesis.sh"
missing_command_parenthesis_output="$runtime_directory/recovery-missing-command-parenthesis.out"
missing_command_parenthesis_cst="$runtime_directory/recovery-missing-command-parenthesis.cst"
printf '%s\n' '(first && )' 'after_missing_command' \
  >"$missing_command_parenthesis_source"
assert_parse_contains_all \
  "$missing_command_parenthesis_source" \
  "$missing_command_parenthesis_output" \
  "(subshell [0, 0] - [0, 11]" \
  "recovery: (command_recovery [0, 10] - [0, 10])" \
  "(complete_command [1, 0] - [1, 21]"
assert_cst_valid_with_output \
  "$missing_command_parenthesis_source" \
  "$missing_command_parenthesis_cst"
assert_cst_direct_child_range \
  "0:0-0:11" \
  "subshell" \
  "0:10-0:11" \
  '")"' \
  "$missing_command_parenthesis_cst"

outer_subshell_initial="$runtime_directory/recovery-enclosing-subshell-initial.sh"
outer_subshell_final="$runtime_directory/recovery-enclosing-subshell-final.sh"
printf '%s\n' '(if x; then y; fi); next' >"$outer_subshell_initial"
printf '%s\n' '(if x; then y); next' >"$outer_subshell_final"
parse_incremental_and_fresh_with_output \
  "$outer_subshell_initial" \
  "$outer_subshell_final" \
  "delete-inner-fi-before-subshell-closer" \
  "13 4"

outer_backquote_initial="$runtime_directory/recovery-enclosing-backquote-initial.sh"
outer_backquote_final="$runtime_directory/recovery-enclosing-backquote-final.sh"
printf '%s\n' 'echo `if x; then y; fi`; next' >"$outer_backquote_initial"
printf '%s\n' 'echo `if x; then y`; next' >"$outer_backquote_final"
parse_incremental_and_fresh_with_output \
  "$outer_backquote_initial" \
  "$outer_backquote_final" \
  "delete-inner-fi-before-backquote-closer" \
  "18 4"

case_pattern_recovery="$runtime_directory/recovery-case-pattern.sh"
case_pattern_recovery_output="$runtime_directory/recovery-case-pattern.out"
printf '%s\n' 'case x in fi' 'next' >"$case_pattern_recovery"
assert_parse_contains_all \
  "$case_pattern_recovery" \
  "$case_pattern_recovery_output" \
  "(case_clause [0, 0] - [0, 12]" \
  "patterns: (pattern_list [0, 10] - [0, 12]" \
  "word: (word [0, 10] - [0, 12]" \
  "recovery: (compound_command_recovery [0, 12] - [0, 12])" \
  "(complete_command [1, 0] - [1, 4]"

case_pattern_initial="$runtime_directory/recovery-case-pattern-initial.sh"
case_pattern_final="$runtime_directory/recovery-case-pattern-final.sh"
printf '%s\n' 'case x in fi) :;; esac' 'next' >"$case_pattern_initial"
printf '%s\n' 'case x in fi' 'next' >"$case_pattern_final"
parse_incremental_and_fresh_with_output \
  "$case_pattern_initial" \
  "$case_pattern_final" \
  "delete-case-pattern-tail" \
  "12 10"

io_location_source="$runtime_directory/recovery-io-location.sh"
io_location_output="$runtime_directory/recovery-io-location.out"
printf '%s\n' "printf {x'}>file" >"$io_location_source"
assert_parse_with_output "$io_location_source" "$io_location_output"
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
assert_parse_with_output "$boundary_source" "$boundary_output"
assert_contains \
  "(here_document_end_recovery [1, 2] - [2, 0])" \
  "$boundary_output"
assert_contains "end: (here_document_end [2, 0] - [3, 0])" "$boundary_output"
assert_contains "(complete_command [5, 0] - [5, 5]" "$boundary_output"

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

continued_dollar_delimiter_initial="$runtime_directory/continued-dollar-delimiter-initial.sh"
continued_dollar_delimiter_final="$runtime_directory/continued-dollar-delimiter-final.sh"
continued_dollar_delimiter_output="$runtime_directory/continued-dollar-delimiter.out"
printf '%s\n' \
  'cat <<$(echo)' \
  'body' \
  '$(echo)' \
  'after' \
  >"$continued_dollar_delimiter_initial"
printf '%s\n' \
  'cat <<$\' \
  '(echo)' \
  'body' \
  '$(echo)' \
  'after' \
  >"$continued_dollar_delimiter_final"
assert_cst_valid_with_output \
  "$continued_dollar_delimiter_final" \
  "$continued_dollar_delimiter_output"
assert_cst_range \
  "0:6 - 1:6" \
  "end: here_end" \
  "$continued_dollar_delimiter_output"
assert_cst_range \
  "0:7 - 1:0" \
  "line_continuation" \
  "$continued_dollar_delimiter_output"
assert_cst_range \
  "3:0 - 4:0" \
  "end: here_document_end" \
  "$continued_dollar_delimiter_output"
assert_valid_incremental_equals_fresh \
  "$continued_dollar_delimiter_initial" \
  "$continued_dollar_delimiter_final" \
  "insert-continuation-after-delimiter-dollar" \
  '7 0 \
'

continued_dollar_quote_initial="$runtime_directory/continued-dollar-quote-initial.sh"
continued_dollar_quote_final="$runtime_directory/continued-dollar-quote-final.sh"
continued_dollar_quote_output="$runtime_directory/continued-dollar-quote.out"
printf '%s\n' "echo \"\$'text'\"" >"$continued_dollar_quote_initial"
printf '%s\n' 'echo "$\' "'text'\"" >"$continued_dollar_quote_final"
assert_cst_valid_with_output \
  "$continued_dollar_quote_final" \
  "$continued_dollar_quote_output"
assert_cst_range \
  "0:7 - 1:0" \
  "line_continuation" \
  "$continued_dollar_quote_output"
assert_not_contains "dollar_single_quoted" "$continued_dollar_quote_output"
assert_valid_incremental_equals_fresh \
  "$continued_dollar_quote_initial" \
  "$continued_dollar_quote_final" \
  "insert-continuation-before-inherited-quote" \
  '7 0 \
'

continued_dollar_parameter_initial="$runtime_directory/continued-dollar-parameter-initial.sh"
continued_dollar_parameter_final="$runtime_directory/continued-dollar-parameter-final.sh"
continued_dollar_parameter_output="$runtime_directory/continued-dollar-parameter.out"
printf '%s\n' "echo \"\${x:-\$'text'}\"" \
  >"$continued_dollar_parameter_initial"
printf '%s\n' 'echo "${x:-$\' "'text'}\"" \
  >"$continued_dollar_parameter_final"
assert_cst_valid_with_output \
  "$continued_dollar_parameter_final" \
  "$continued_dollar_parameter_output"
assert_cst_range \
  "0:12 - 1:0" \
  "line_continuation" \
  "$continued_dollar_parameter_output"
assert_not_contains \
  "dollar_single_quoted" \
  "$continued_dollar_parameter_output"
assert_valid_incremental_equals_fresh \
  "$continued_dollar_parameter_initial" \
  "$continued_dollar_parameter_final" \
  "insert-continuation-in-inherited-parameter-word" \
  '12 0 \
'

continued_dollar_body_initial="$runtime_directory/continued-dollar-body-initial.sh"
continued_dollar_body_final="$runtime_directory/continued-dollar-body-final.sh"
continued_dollar_body_output="$runtime_directory/continued-dollar-body.out"
printf '%s\n' 'cat <<EOF' "\$'text'" 'EOF' \
  >"$continued_dollar_body_initial"
printf '%s\n' 'cat <<EOF' '$\' "'text'" 'EOF' \
  >"$continued_dollar_body_final"
assert_cst_valid_with_output \
  "$continued_dollar_body_final" \
  "$continued_dollar_body_output"
assert_cst_range \
  "1:1 - 2:0" \
  "line_continuation" \
  "$continued_dollar_body_output"
assert_not_contains "dollar_single_quoted" "$continued_dollar_body_output"
assert_valid_incremental_equals_fresh \
  "$continued_dollar_body_initial" \
  "$continued_dollar_body_final" \
  "insert-continuation-in-here-document-body" \
  '11 0 \
'

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
parse_incremental_and_fresh_with_output \
  "$long_initial" \
  "$long_final" \
  "oversized-delimiter-recovery" \
  "6 2048 $long_final_delimiter" \
  "2060 2048 $long_final_delimiter" \
  "4109 0 after"
long_incremental_output="$runtime_directory/oversized-delimiter-recovery.incremental"
long_fresh_output="$runtime_directory/oversized-delimiter-recovery.fresh"
assert_contains \
  "(complete_command [3, 0] - [3, 5]" \
  "$long_incremental_output"
assert_contains \
  "(complete_command [3, 0] - [3, 5]" \
  "$long_fresh_output"

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
  "1:8  - 2:2" \
  "parameter_expansion" \
  "$numeric_parameter_output"
assert_cst_range \
  "1:11 - 2:0" \
  "line_continuation" \
  "$numeric_parameter_output"
assert_not_contains "ERROR" "$numeric_parameter_output"
assert_not_contains "parameter_number" "$numeric_parameter_output"

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
  "insert-unclassified-numeric-parameter-continuation" \
  '11 0 \
'
assert_incremental_equals_fresh \
  "$numeric_unspecified_final" \
  "$numeric_unspecified_initial" \
  "delete-unclassified-numeric-parameter-continuation" \
  "11 2"

numeric_category_initial="$runtime_directory/numeric-category-initial.sh"
numeric_category_final="$runtime_directory/numeric-category-final.sh"
printf '%s\n' 'printf "${00}"' >"$numeric_category_initial"
printf '%s\n' 'printf "${01}"' >"$numeric_category_final"
assert_incremental_equals_fresh \
  "$numeric_category_initial" \
  "$numeric_category_final" \
  "numeric-source-to-positional-parameter" \
  "11 1 1"
assert_incremental_equals_fresh \
  "$numeric_category_final" \
  "$numeric_category_initial" \
  "positional-parameter-to-numeric-source" \
  "11 1 0"

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

case_keyword_initial="$runtime_directory/case-keyword-initial.sh"
case_keyword_final="$runtime_directory/case-keyword-final.sh"
printf '%s\n' 'case x in esac' >"$case_keyword_initial"
printf '%s\n' 'case\' ' x in esac' >"$case_keyword_final"
assert_valid_incremental_equals_fresh \
  "$case_keyword_initial" \
  "$case_keyword_final" \
  "insert-case-keyword-continuation" \
  '4 0 \
'
assert_valid_incremental_equals_fresh \
  "$case_keyword_final" \
  "$case_keyword_initial" \
  "delete-case-keyword-continuation" \
  "4 2"

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

case_body_separator_initial="$runtime_directory/case-body-separator-initial.sh"
case_body_separator_final="$runtime_directory/case-body-separator-final.sh"
printf '%s\n' 'case x in x)echo z;;esac' >"$case_body_separator_initial"
printf '%s\n' 'case x in x)echo \' 'z;;esac' >"$case_body_separator_final"
assert_valid_incremental_equals_fresh \
  "$case_body_separator_initial" \
  "$case_body_separator_final" \
  "insert-case-body-separator-continuation" \
  '17 0 \
'
assert_valid_incremental_equals_fresh \
  "$case_body_separator_final" \
  "$case_body_separator_initial" \
  "delete-case-body-separator-continuation" \
  "17 2"

case_item_following_initial="$runtime_directory/case-item-following-initial.sh"
case_item_following_final="$runtime_directory/case-item-following-final.sh"
printf '%s\n' 'case x in x):;; y):;;esac' >"$case_item_following_initial"
printf '%s\n' 'case x in x):;;\' ' y):;;esac' >"$case_item_following_final"
assert_valid_incremental_equals_fresh \
  "$case_item_following_initial" \
  "$case_item_following_final" \
  "insert-case-item-following-continuation" \
  '15 0 \
'
assert_valid_incremental_equals_fresh \
  "$case_item_following_final" \
  "$case_item_following_initial" \
  "delete-case-item-following-continuation" \
  "15 2"

empty_case_item_initial="$runtime_directory/empty-case-item-initial.sh"
empty_case_item_final="$runtime_directory/empty-case-item-final.sh"
printf '%s\n' 'case x in x):;;esac' >"$empty_case_item_initial"
printf '%s\n' 'case x in x);;esac' >"$empty_case_item_final"
assert_valid_incremental_equals_fresh \
  "$empty_case_item_initial" \
  "$empty_case_item_final" \
  "delete-empty-case-item-body" \
  "12 1"

reserved_for_word_initial="$runtime_directory/reserved-for-word-initial.sh"
reserved_for_word_final="$runtime_directory/reserved-for-word-final.sh"
printf '%s\n' 'for x in ordinary; do :; done' >"$reserved_for_word_initial"
printf '%s\n' 'for x in fi; do :; done' >"$reserved_for_word_final"
assert_valid_incremental_equals_fresh \
  "$reserved_for_word_initial" \
  "$reserved_for_word_final" \
  "replace-for-word-with-reserved-closer-spelling" \
  "9 8 fi"

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
assert_parse_with_output \
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
assert_parse_with_output \
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
