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

XDG_CACHE_HOME="$runtime_directory/cache" \
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

parse_timeout_microseconds=10000000

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

assert_occurrence_count() {
  expected_count=$1
  expected_text=$2
  file=$3
  actual_count=$(
    awk -v expected_text="$expected_text" '
      index($0, expected_text) { count += 1 }
      END { print count + 0 }
    ' "$file"
  )
  if [ "$actual_count" -ne "$expected_count" ]; then
    printf '%s\n' \
      "Expected $expected_count occurrences of $expected_text, found $actual_count" >&2
    sed -n '1,200p' "$file" >&2
    exit 1
  fi
}

source_eof_point() (
  eof_source=$1
  eof_byte_count=$(LC_ALL=C wc -c <"$eof_source")
  LC_ALL=C awk -v byte_count="$eof_byte_count" '
    {
      content_bytes += length($0)
      final_column = length($0)
    }
    END {
      if (NR == 0) {
        print "0:0"
      } else if (byte_count == content_bytes + NR) {
        print NR ":0"
      } else {
        print NR - 1 ":" final_column
      }
    }
  ' "$eof_source"
)

parse_root_end_point() (
  root_format=$1
  root_output=$2

  case $root_format in
  tree)
    LC_ALL=C awk '
      NR == 1 {
        line = $0
        if (line !~ /^\(program \[[0-9]+, [0-9]+\] - \[[0-9]+, [0-9]+\][)]?$/) {
          exit
        }
        sub(/^.* - \[/, "", line)
        sub(/\].*$/, "", line)
        gsub(/,[[:space:]]*/, ":", line)
        print line
      }
    ' "$root_output"
    ;;
  cst)
    LC_ALL=C awk '
      NR == 1 {
        separator = index($0, " - ")
        if (separator == 0) {
          exit
        }
        tail = substr($0, separator + 3)
        match(tail, /^[0-9]+:[0-9]+/)
        if (RLENGTH > 0) {
          print substr(tail, RSTART, RLENGTH)
        }
      }
    ' "$root_output"
    ;;
  summary)
    LC_ALL=C awk '
      /"end"[[:space:]]*:[[:space:]]*\{/ {
        inside_end = 1
        next
      }
      inside_end && /"row"[[:space:]]*:/ {
        line = $0
        sub(/^.*"row"[[:space:]]*:[[:space:]]*/, "", line)
        sub(/[^0-9].*$/, "", line)
        end_row = line
        next
      }
      inside_end && /"column"[[:space:]]*:/ {
        line = $0
        sub(/^.*"column"[[:space:]]*:[[:space:]]*/, "", line)
        sub(/[^0-9].*$/, "", line)
        if (end_row != "" && line != "") {
          print end_row ":" line
        }
        exit
      }
    ' "$root_output"
    ;;
  esac
)

assert_resource_cst_root() (
  resource_output=$1
  resource_description=$2

  if ! LC_ALL=C awk '
    NR == 1 && $NF ~ /^(program|ERROR|•program|•ERROR)$/ {
      valid = 1
    }
    END { exit valid ? 0 : 1 }
  ' "$resource_output"; then
    sed -n '1,200p' "$resource_output" >&2
    fail "Resource-bounded parse has no complete program or ERROR root: $resource_description"
  fi
)

assert_parse_consumed_source() (
  consumed_format=$1
  consumed_output=$2
  consumed_description=$3
  consumed_source=$4
  expected_end=$(source_eof_point "$consumed_source")
  actual_end=$(parse_root_end_point "$consumed_format" "$consumed_output")

  if [ -z "$actual_end" ]; then
    fail "Parser output has no readable root range: $consumed_description"
  fi
  if [ "$actual_end" != "$expected_end" ]; then
    fail "Parser stopped at $actual_end before source EOF $expected_end (possible timeout): $consumed_description"
  fi
)

run_parse() {
  parse_mode=$1
  parse_format=$2
  parse_output=$3
  parse_description=$4
  shift 4
  parse_diagnostics="$parse_output.stderr"

  case $parse_format in
  tree)
    if parse_current \
      --timeout "$parse_timeout_microseconds" \
      "$@" \
      >"$parse_output" 2>"$parse_diagnostics"; then
      parse_status=0
    else
      parse_status=$?
    fi
    parse_root=program
    ;;
  cst)
    if parse_current \
      --cst \
      --timeout "$parse_timeout_microseconds" \
      "$@" \
      >"$parse_output" 2>"$parse_diagnostics"; then
      parse_status=0
    else
      parse_status=$?
    fi
    parse_root=program
    ;;
  summary)
    if [ "$parse_mode" != valid ]; then
      fail "JSON summaries are only valid for successful parse checks: $parse_description"
    fi
    if parse_current \
      --quiet \
      --json-summary \
      --timeout "$parse_timeout_microseconds" \
      "$@" \
      >"$parse_output" 2>"$parse_diagnostics"; then
      parse_status=0
    else
      parse_status=$?
    fi
    parse_root='"parse_summaries"'
    ;;
  *)
    fail "Unknown parse output format: $parse_format"
    ;;
  esac

  if [ "$parse_mode" = resource ]; then
    if [ "$parse_format" != cst ]; then
      fail "Resource-bounded checks require CST output: $parse_description"
    fi
    assert_resource_cst_root "$parse_output" "$parse_description"
  elif [ ! -s "$parse_output" ] || ! grep -Fq -- "$parse_root" "$parse_output"; then
    sed -n '1,200p' "$parse_diagnostics" >&2
    fail "Parser produced no complete $parse_format output (possible timeout): $parse_description"
  fi

  case $parse_status in
  0 | 1) ;;
  *)
    sed -n '1,200p' "$parse_diagnostics" >&2
    fail "Parser failed with status $parse_status: $parse_description"
    ;;
  esac

  case $parse_mode in
  valid)
    if [ "$parse_status" -ne 0 ]; then
      sed -n '1,200p' "$parse_output" >&2
      fail "Expected valid parse: $parse_description"
    fi
    ;;
  recovery | resource) ;;
  *) fail "Unknown parse mode: $parse_mode" ;;
  esac

  if [ "$#" -eq 1 ] && [ -f "$1" ]; then
    assert_parse_consumed_source \
      "$parse_format" \
      "$parse_output" \
      "$parse_description" \
      "$1"
  fi
}

assert_timeout_is_not_recovery() {
  timeout_source="$runtime_directory/timeout-guard.sh"
  timeout_output="$runtime_directory/timeout-guard.cst"
  awk 'BEGIN {
    for (counter = 0; counter < 10000; counter += 1) print ":"
  }' >"$timeout_source"

  if (
    parse_timeout_microseconds=1
    run_parse recovery cst "$timeout_output" "timeout guard" "$timeout_source"
  ) >/dev/null 2>&1; then
    fail "A timed-out parse was accepted as structural recovery"
  fi
}

run_query() {
  query_output=$1
  query_path=$2
  shift 2
  query_diagnostics="$query_output.stderr"

  if ! query_current \
    --captures \
    "$query_path" \
    "$@" \
    >"$query_output" 2>"$query_diagnostics"; then
    sed -n '1,200p' "$query_diagnostics" >&2
    fail "Query failed: $query_path"
  fi
}

assert_valid() {
  source_file=$1
  output_file="$runtime_directory/assert-valid.out"
  run_parse valid summary "$output_file" "$source_file" "$source_file"
}

assert_valid_with_output() {
  source_file=$1
  output_file=$2
  run_parse valid tree "$output_file" "$source_file" "$source_file"
}

assert_cst_valid_with_output() {
  source_file=$1
  output_file=$2
  run_parse valid cst "$output_file" "$source_file" "$source_file"
}

assert_cst_range() {
  expected_range=$1
  expected_item=$2
  file=$3
  if ! awk \
    -v expected_range="$expected_range" '
      BEGIN { gsub(/[[:space:]]/, "", expected_range) }

      {
        separator = index($0, " - ")
        if (separator == 0) {
          next
        }
        start = substr($0, 1, separator - 1)
        gsub(/[[:space:]]/, "", start)
        tail = substr($0, separator + 3)
        match(tail, /^[^[:space:]]+/)
        end = substr(tail, RSTART, RLENGTH)
        range = start "-" end
        if (range == expected_range) {
          print
        }
      }
    ' "$file" | grep -Fq -- "$expected_item"; then
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
        depth = RSTART - 1 + length(end)
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
  run_parse recovery tree "$output_file" "$source_file" "$source_file"
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

normalize_cst_fingerprint() {
  cst_input=$1
  cst_fingerprint=$2
  LC_ALL=C awk '
    /^[0-9]+:[0-9]+[[:space:]]+-[[:space:]]+[0-9]+:[0-9]+/ {
      print
    }
  ' "$cst_input" >"$cst_fingerprint"
  if [ ! -s "$cst_fingerprint" ]; then
    fail "CST fingerprint is empty: $cst_input"
  fi
}

assert_cst_outputs_equal() {
  comparison_name=$1
  left_output=$2
  left_status=$3
  right_output=$4
  right_status=$5
  left_fingerprint="$left_output.fingerprint"
  right_fingerprint="$right_output.fingerprint"

  if [ "$left_status" != "$right_status" ]; then
    fail "Parse statuses differ: $comparison_name"
  fi

  normalize_cst_fingerprint "$left_output" "$left_fingerprint"
  normalize_cst_fingerprint "$right_output" "$right_fingerprint"
  if ! cmp -s "$left_fingerprint" "$right_fingerprint"; then
    diff -u "$right_fingerprint" "$left_fingerprint" >&2 || true
    fail "Complete CSTs differ: $comparison_name"
  fi
}

assert_repeated_cold_parse() {
  repeated_mode=$1
  repeated_source=$2
  repeated_name=$3
  first_output="$runtime_directory/$repeated_name.first.cst"
  second_output="$runtime_directory/$repeated_name.second.cst"

  run_parse \
    "$repeated_mode" \
    cst \
    "$first_output" \
    "$repeated_name first cold parse" \
    "$repeated_source"
  first_status=$parse_status
  run_parse \
    "$repeated_mode" \
    cst \
    "$second_output" \
    "$repeated_name second cold parse" \
    "$repeated_source"
  second_status=$parse_status

  assert_cst_outputs_equal \
    "$repeated_name repeated cold parses" \
    "$first_output" \
    "$first_status" \
    "$second_output" \
    "$second_status"
}

assert_cst_fingerprint_distinguishes_anonymous_tokens() {
  semicolon_source="$runtime_directory/fingerprint-semicolon.sh"
  ampersand_source="$runtime_directory/fingerprint-ampersand.sh"
  semicolon_cst="$runtime_directory/fingerprint-semicolon.cst"
  ampersand_cst="$runtime_directory/fingerprint-ampersand.cst"
  semicolon_fingerprint="$semicolon_cst.fingerprint"
  ampersand_fingerprint="$ampersand_cst.fingerprint"

  printf '%s\n' 'a;b' >"$semicolon_source"
  printf '%s\n' 'a&b' >"$ampersand_source"
  run_parse valid cst "$semicolon_cst" "semicolon fingerprint control" "$semicolon_source"
  run_parse valid cst "$ampersand_cst" "ampersand fingerprint control" "$ampersand_source"
  normalize_cst_fingerprint "$semicolon_cst" "$semicolon_fingerprint"
  normalize_cst_fingerprint "$ampersand_cst" "$ampersand_fingerprint"

  if cmp -s "$semicolon_fingerprint" "$ampersand_fingerprint"; then
    fail "Complete CST fingerprints do not distinguish anonymous tokens"
  fi
}

normalize_logical_projection() {
  projection_input=$1
  projection_output=$2
  LC_ALL=C awk '
    {
      if ($0 !~ /^[0-9]+:[0-9]+[[:space:]]+-[[:space:]]+[0-9]+:[0-9]+/) {
        next
      }
      separator = index($0, " - ")
      if (separator == 0) {
        next
      }

      tail = substr($0, separator + 3)
      match(tail, /^[^[:space:]]+/)
      end = substr(tail, RSTART, RLENGTH)
      content = substr(tail, RLENGTH + 1)
      match(content, /[^[:space:]]/)
      depth = RSTART - 1 + length(end)
      content = substr(content, RSTART)
      if (sub(/^•/, "", content)) {
        depth += 1
        content = "!" content
      }

      if (content ~ /^["`]/) {
        next
      }
      sub(/[[:space:]]+`.*`$/, "", content)
      if (content == "line_continuation" || content ~ /: line_continuation$/) {
        next
      }

      if (!have_root) {
        root_depth = depth
        have_root = 1
      }
      print depth - root_depth ":" content
    }
  ' "$projection_input" >"$projection_output"
  if [ ! -s "$projection_output" ]; then
    fail "Logical CST projection is empty: $projection_input"
  fi
}

assert_same_logical_projection() {
  projection_name=$1
  logical_cst=$2
  physical_cst=$3
  logical_projection="$runtime_directory/$projection_name.logical"
  physical_projection="$runtime_directory/$projection_name.physical"

  normalize_logical_projection "$logical_cst" "$logical_projection"
  normalize_logical_projection "$physical_cst" "$physical_projection"
  if ! cmp -s "$logical_projection" "$physical_projection"; then
    diff -u "$logical_projection" "$physical_projection" >&2 || true
    fail "Physical source changes the logical CST: $projection_name"
  fi
}

assert_continued_parse_equivalent() {
  equivalence_name=$1
  expected_status=$2
  logical_source=$3
  physical_source=$4
  logical_cst="$runtime_directory/$equivalence_name.logical.cst"
  physical_cst="$runtime_directory/$equivalence_name.physical.cst"

  run_parse \
    recovery \
    cst \
    "$logical_cst" \
    "$equivalence_name logical source" \
    "$logical_source"
  logical_status=$parse_status
  run_parse \
    recovery \
    cst \
    "$physical_cst" \
    "$equivalence_name physical source" \
    "$physical_source"
  physical_status=$parse_status

  if [ "$logical_status" -ne "$expected_status" ] ||
    [ "$physical_status" -ne "$expected_status" ]; then
    fail "$equivalence_name parse statuses: expected $expected_status, logical $logical_status, physical $physical_status"
  fi
  assert_occurrence_count 1 "line_continuation" "$physical_cst"
  assert_same_logical_projection \
    "$equivalence_name" \
    "$logical_cst" \
    "$physical_cst"
}

extract_line_continuation_manifest() {
  query_output=$1
  manifest_output=$2
  LC_ALL=C awk '
    /capture: [0-9]+ - line\.continuation, start: \([0-9]+, [0-9]+\), end: \([0-9]+, [0-9]+\), text:/ {
      range = $0
      sub(/^.*start: \(/, "", range)
      sub(/\), text:.*$/, "", range)
      gsub(/\), end: \(/, "-", range)
      gsub(/, /, ":", range)
      print range
    }
  ' "$query_output" >"$manifest_output"
}

assert_manifest_source_order() {
  manifest_file=$1
  LC_ALL=C awk '
    {
      endpoint_count = split($0, endpoints, "-")
      start_count = split(endpoints[1], start, ":")
      end_count = split(endpoints[2], end, ":")
      if (NF != 1 || endpoint_count != 2 || start_count != 2 || end_count != 2) {
        exit 1
      }
      if (have_previous && (start[1] < previous_row || (start[1] == previous_row && start[2] <= previous_column))) {
        exit 1
      }
      if (end[1] != start[1] + 1 || end[2] != 0) {
        exit 1
      }
      previous_row = start[1]
      previous_column = start[2]
      have_previous = 1
    }
  ' "$manifest_file" || fail "Line continuations are duplicated, out of order, or have invalid ranges: $manifest_file"
}

assert_line_continuation_manifest() {
  continuation_name=$1
  physical_source=$2
  logical_source=$3
  expected_manifest=$4
  physical_cst="$runtime_directory/$continuation_name.physical.cst"
  logical_cst="$runtime_directory/$continuation_name.logical.cst"
  query_output="$runtime_directory/$continuation_name.query"
  actual_manifest="$runtime_directory/$continuation_name.ranges"
  physical_projection="$runtime_directory/$continuation_name.physical.logical"
  logical_projection="$runtime_directory/$continuation_name.logical.logical"

  run_parse valid cst "$physical_cst" "$continuation_name physical" "$physical_source"
  run_parse valid cst "$logical_cst" "$continuation_name logical" "$logical_source"
  run_query \
    "$query_output" \
    "$repository_directory/test/runtime/contracts.scm" \
    "$physical_source"
  extract_line_continuation_manifest "$query_output" "$actual_manifest"
  assert_manifest_source_order "$actual_manifest"

  if ! cmp -s "$expected_manifest" "$actual_manifest"; then
    diff -u "$expected_manifest" "$actual_manifest" >&2 || true
    fail "Line-continuation count or ranges differ: $continuation_name"
  fi

  normalize_logical_projection "$physical_cst" "$physical_projection"
  normalize_logical_projection "$logical_cst" "$logical_projection"
  if ! cmp -s "$logical_projection" "$physical_projection"; then
    diff -u "$logical_projection" "$physical_projection" >&2 || true
    fail "Line continuations change the logical public CST: $continuation_name"
  fi
}

assert_no_line_continuations() {
  continuation_name=$1
  source_file=$2
  source_cst="$runtime_directory/$continuation_name.cst"
  query_output="$runtime_directory/$continuation_name.query"
  actual_manifest="$runtime_directory/$continuation_name.ranges"

  run_parse valid cst "$source_cst" "$continuation_name" "$source_file"
  run_query \
    "$query_output" \
    "$repository_directory/test/runtime/contracts.scm" \
    "$source_file"
  extract_line_continuation_manifest "$query_output" "$actual_manifest"
  if [ -s "$actual_manifest" ]; then
    sed -n '1,200p' "$actual_manifest" >&2
    fail "Literal backslash-newline source became line_continuation: $continuation_name"
  fi
}

compare_incremental_and_fresh() {
  expected_mode=$1
  shift
  initial_file=$1
  final_file=$2
  test_name=$3
  shift 3

  incremental_output="$runtime_directory/$test_name.incremental"
  fresh_output="$runtime_directory/$test_name.fresh"

  if cmp -s "$initial_file" "$final_file"; then
    fail "Incremental test has identical initial and final sources: $test_name"
  fi

  run_parse \
    "$expected_mode" \
    cst \
    "$incremental_output" \
    "$test_name incremental" \
    --edits "$@" \
    -- "$initial_file"
  incremental_status=$parse_status

  run_parse \
    "$expected_mode" \
    cst \
    "$fresh_output" \
    "$test_name fresh" \
    "$final_file"
  fresh_status=$parse_status

  assert_cst_outputs_equal \
    "$test_name incremental and fresh parses" \
    "$incremental_output" \
    "$incremental_status" \
    "$fresh_output" \
    "$fresh_status"
}

parse_incremental_and_fresh_with_output() {
  compare_incremental_and_fresh recovery "$@"
}

assert_incremental_equals_fresh() {
  compare_incremental_and_fresh valid "$@"
}

assert_timeout_is_not_recovery
assert_cst_fingerprint_distinguishes_anonymous_tokens

# Line-continuation and comment contracts.
syntax_continuation_physical="$runtime_directory/syntax-continuation-physical.sh"
syntax_continuation_logical="$runtime_directory/syntax-continuation-logical.sh"
syntax_continuation_ranges="$runtime_directory/syntax-continuation.ranges"
printf '%s\n' 'echo "$(printf x\' ')"' 'echo `printf x\' '`' \
  >"$syntax_continuation_physical"
printf '%s\n' 'echo "$(printf x)"' 'echo `printf x`' \
  >"$syntax_continuation_logical"
printf '%s\n' '0:16-1:0' '2:14-3:0' >"$syntax_continuation_ranges"
assert_line_continuation_manifest \
  "line-continuation-syntax-contract" \
  "$syntax_continuation_physical" \
  "$syntax_continuation_logical" \
  "$syntax_continuation_ranges"

arithmetic_continuation_physical="$runtime_directory/arithmetic-continuation-physical.sh"
arithmetic_continuation_logical="$runtime_directory/arithmetic-continuation-logical.sh"
arithmetic_continuation_ranges="$runtime_directory/arithmetic-continuation.ranges"
printf '%s\n' ': "$((1 + \' '2))"' ': "$((1 + 2\' '))"' \
  >"$arithmetic_continuation_physical"
printf '%s\n' ': "$((1 + 2))"' ': "$((1 + 2))"' \
  >"$arithmetic_continuation_logical"
printf '%s\n' '0:10-1:0' '2:11-3:0' >"$arithmetic_continuation_ranges"
assert_line_continuation_manifest \
  "line-continuation-arithmetic-contract" \
  "$arithmetic_continuation_physical" \
  "$arithmetic_continuation_logical" \
  "$arithmetic_continuation_ranges"
assert_no_line_continuations \
  "line-continuation-literals" \
  "$repository_directory/test/runtime/literal-line-continuations.source"

terminal_assignment_physical="$runtime_directory/terminal-assignment-physical.sh"
terminal_assignment_logical="$runtime_directory/terminal-assignment-logical.sh"
terminal_assignment_physical_output="$runtime_directory/terminal-assignment-physical.cst"
terminal_assignment_logical_output="$runtime_directory/terminal-assignment-logical.cst"
printf 'A=\\\n' >"$terminal_assignment_physical"
printf 'A=' >"$terminal_assignment_logical"
assert_cst_valid_with_output \
  "$terminal_assignment_physical" \
  "$terminal_assignment_physical_output"
assert_cst_valid_with_output \
  "$terminal_assignment_logical" \
  "$terminal_assignment_logical_output"
assert_cst_range \
  "0:2-1:0" \
  "line_continuation" \
  "$terminal_assignment_physical_output"
assert_occurrence_count \
  1 \
  "line_continuation" \
  "$terminal_assignment_physical_output"
assert_same_logical_projection \
  "terminal-assignment-continuation" \
  "$terminal_assignment_logical_output" \
  "$terminal_assignment_physical_output"

comments_source="$runtime_directory/comments.sh"
comments_output="$runtime_directory/comments.out"
printf '%s\n' \
  "  # leading" \
  "command # trailing" \
  "cat <<EOF # declaration" \
  "body" \
  "EOF" \
  >"$comments_source"
assert_valid_with_output "$comments_source" "$comments_output"
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

nul_comment_source="$runtime_directory/nul-comment.sh"
nul_comment_output="$runtime_directory/nul-comment.cst"
printf '#a\000b\nnext\n' >"$nul_comment_source"
run_parse \
  recovery \
  cst \
  "$nul_comment_output" \
  "NUL inside comment" \
  -- \
  "$nul_comment_source"
assert_cst_range "0:0-0:4" "comment" "$nul_comment_output"
assert_cst_range \
  "1:0-1:4" \
  "command: complete_command" \
  "$nul_comment_output"
assert_cst_range "0:0-2:0" "program" "$nul_comment_output"

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

# Structural recovery and closer ownership.
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
run_query \
  "$compound_query_output" \
  "$repository_directory/test/runtime/contracts.scm" \
  "$compound_source"
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
  assert_cst_range "0:11-0:26" "while_clause" "$nested_compound_output"
  assert_cst_range \
    "0:26-0:26" \
    "recovery: compound_command_recovery" \
    "$nested_compound_output"
  assert_cst_range "0:26-0:28" "fi_keyword" "$nested_compound_output"
done

nested_eof_initial="$runtime_directory/recovery-nested-eof-initial.sh"
nested_eof_final="$runtime_directory/recovery-nested-eof-final.sh"
printf '%s\n' \
  'worker() {' \
  ' if true' \
  ' then' \
  ' printf yes' \
  ' fi' \
  '}' \
  >"$nested_eof_initial"
printf '%s\n' \
  'worker() {' \
  ' if true' \
  ' then' \
  ' printf yes' \
  >"$nested_eof_final"
parse_incremental_and_fresh_with_output \
  "$nested_eof_initial" \
  "$nested_eof_final" \
  "delete-nested-compound-closers" \
  "38 6"
for nested_eof_output in \
  "$runtime_directory/delete-nested-compound-closers.incremental" \
  "$runtime_directory/delete-nested-compound-closers.fresh"; do
  assert_cst_range "0:0-4:0" "function_definition" "$nested_eof_output"
  assert_cst_range "0:9-4:0" "brace_group" "$nested_eof_output"
  assert_cst_range "1:1-4:0" "if_clause" "$nested_eof_output"
  assert_occurrence_count \
    2 \
    "recovery: compound_command_recovery" \
    "$nested_eof_output"
  assert_cst_range \
    "4:0-4:0" \
    "recovery: compound_command_recovery" \
    "$nested_eof_output"
  assert_not_contains "ERROR" "$nested_eof_output"
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
  assert_cst_range \
    "0:8-0:8" \
    "recovery: parameter_expansion_recovery" \
    "$parameter_recovery_output"
  assert_cst_range "0:10-0:14" "and_or" "$parameter_recovery_output"
done

parameter_recovery_query="$runtime_directory/recovery-parameter.query"
run_query \
  "$parameter_recovery_query" \
  "$repository_directory/test/runtime/contracts.scm" \
  "$parameter_recovery_final"
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
  assert_cst_range "0:0-1:0" "subshell" "$subshell_output"
  assert_cst_range "0:1-0:7" "and_or" "$subshell_output"
  assert_cst_range \
    "1:0-1:0" \
    "recovery: compound_command_recovery" \
    "$subshell_output"
done

group_recovery_query="$runtime_directory/recovery-group.query"
run_query \
  "$group_recovery_query" \
  "$repository_directory/test/runtime/contracts.scm" \
  "$subshell_final"
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
  assert_cst_range "0:0-1:0" "brace_group" "$brace_group_output"
  assert_cst_range "0:2-0:8" "and_or" "$brace_group_output"
  assert_cst_range \
    "1:0-1:0" \
    "recovery: compound_command_recovery" \
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
  assert_cst_range \
    "0:6-0:13" \
    "parameter_expansion" \
    "$parameter_pattern_output"
  assert_cst_range \
    "0:13-0:13" \
    "recovery: parameter_expansion_recovery" \
    "$parameter_pattern_output"
  assert_cst_range "0:16-0:20" "and_or" "$parameter_pattern_output"
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
  assert_cst_range \
    "2:0-2:0" \
    "recovery: command_recovery" \
    "$and_if_newline_output"
  assert_cst_range \
    "3:0-3:11" \
    "complete_command" \
    "$and_if_newline_output"
done

# Substitution, redirection, and token-boundary ownership.
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
assert_incremental_equals_fresh \
  "$backquote_assignment_initial" \
  "$backquote_assignment_final" \
  "remove-double-quotes-around-assignment-backquote" \
  "24 1" \
  "39 1"

command_substitution_layout_initial="$runtime_directory/command-substitution-layout-initial.sh"
command_substitution_layout_final="$runtime_directory/command-substitution-layout-final.sh"
printf '%s\n' 'echo $(first;)' >"$command_substitution_layout_initial"
printf '%s\n' 'echo $(first; )' >"$command_substitution_layout_final"
assert_incremental_equals_fresh \
  "$command_substitution_layout_initial" \
  "$command_substitution_layout_final" \
  "insert-layout-before-command-substitution-closer" \
  "13 0  "

backquote_layout_initial="$runtime_directory/backquote-layout-initial.sh"
backquote_layout_final="$runtime_directory/backquote-layout-final.sh"
printf '%s\n' 'echo `first;`' >"$backquote_layout_initial"
printf '%s\n' 'echo `first; `' >"$backquote_layout_final"
assert_incremental_equals_fresh \
  "$backquote_layout_initial" \
  "$backquote_layout_final" \
  "insert-layout-before-backquote-closer" \
  "12 0  "

redirected_compound_without_separator="$runtime_directory/redirected-compound-without-separator.sh"
redirected_compound_with_separator="$runtime_directory/redirected-compound-with-separator.sh"
printf '%s\n' ': "$({ :; }>g)"' \
  >"$redirected_compound_without_separator"
printf '%s\n' ': "$({ :; }>g;)"' \
  >"$redirected_compound_with_separator"
assert_incremental_equals_fresh \
  "$redirected_compound_without_separator" \
  "$redirected_compound_with_separator" \
  "insert-separator-after-redirected-compound-command" \
  "13 0 ;"
assert_incremental_equals_fresh \
  "$redirected_compound_with_separator" \
  "$redirected_compound_without_separator" \
  "delete-separator-after-redirected-compound-command" \
  "13 1"
redirected_compound_output="$runtime_directory/delete-separator-after-redirected-compound-command.fresh"
assert_cst_range \
  "0:3-0:14" \
  "command_substitution" \
  "$redirected_compound_output"
assert_cst_range \
  "0:5-0:13" \
  "command: complete_command" \
  "$redirected_compound_output"
assert_cst_range \
  "0:5-0:11" \
  "body: compound_command" \
  "$redirected_compound_output"
assert_cst_range \
  "0:11-0:13" \
  "redirects: redirect_list" \
  "$redirected_compound_output"
assert_cst_direct_child_range \
  "0:3-0:14" \
  "command_substitution" \
  "0:13-0:14" \
  '")"' \
  "$redirected_compound_output"
assert_not_contains "recovery" "$redirected_compound_output"

redirected_function_without_separator="$runtime_directory/redirected-function-without-separator.sh"
redirected_function_with_separator="$runtime_directory/redirected-function-with-separator.sh"
printf '%s\n' ': "$(f(){ :; }>g)"' \
  >"$redirected_function_without_separator"
printf '%s\n' ': "$(f(){ :; }>g;)"' \
  >"$redirected_function_with_separator"
assert_incremental_equals_fresh \
  "$redirected_function_without_separator" \
  "$redirected_function_with_separator" \
  "insert-separator-after-redirected-function" \
  "16 0 ;"
assert_incremental_equals_fresh \
  "$redirected_function_with_separator" \
  "$redirected_function_without_separator" \
  "delete-separator-after-redirected-function" \
  "16 1"
redirected_function_output="$runtime_directory/delete-separator-after-redirected-function.fresh"
assert_cst_range \
  "0:3-0:17" \
  "command_substitution" \
  "$redirected_function_output"
assert_cst_range \
  "0:5-0:16" \
  "body: function_definition" \
  "$redirected_function_output"
assert_cst_range \
  "0:8-0:16" \
  "body: function_body" \
  "$redirected_function_output"
assert_cst_range \
  "0:8-0:14" \
  "body: compound_command" \
  "$redirected_function_output"
assert_cst_range \
  "0:14-0:16" \
  "redirects: redirect_list" \
  "$redirected_function_output"
assert_cst_direct_child_range \
  "0:3-0:17" \
  "command_substitution" \
  "0:16-0:17" \
  '")"' \
  "$redirected_function_output"
assert_not_contains "recovery" "$redirected_function_output"

redirect_target_word_initial="$runtime_directory/redirect-target-word-initial.sh"
redirect_target_word_final="$runtime_directory/redirect-target-word-final.sh"
printf '%s\n' '<2>x' >"$redirect_target_word_initial"
printf '%s\n' '<""2>x' >"$redirect_target_word_final"
assert_incremental_equals_fresh \
  "$redirect_target_word_initial" \
  "$redirect_target_word_final" \
  "insert-empty-quote-before-redirect-target-digit" \
  '1 0 ""'

io_location_closer_initial="$runtime_directory/io-location-closer-initial.sh"
io_location_closer_final="$runtime_directory/io-location-closer-final.sh"
printf '%s\n' '{a}>x' >"$io_location_closer_initial"
printf '%s\n' '{a}}>x' >"$io_location_closer_final"
assert_incremental_equals_fresh \
  "$io_location_closer_initial" \
  "$io_location_closer_final" \
  "extend-io-location-through-right-brace" \
  '3 0 }'

io_location_continuation_initial="$runtime_directory/io-location-continuation-initial.sh"
io_location_continuation_final="$runtime_directory/io-location-continuation-final.sh"
io_location_continuation_output="$runtime_directory/io-location-continuation.cst"
printf '%s\n' '{a}>x' >"$io_location_continuation_initial"
printf '%s\n' '{a}\' '>x' >"$io_location_continuation_final"
assert_cst_valid_with_output \
  "$io_location_continuation_final" \
  "$io_location_continuation_output"
assert_cst_range \
  "0:0-0:3" \
  "location: io_location" \
  "$io_location_continuation_output"
assert_cst_range \
  "0:3-1:0" \
  "line_continuation" \
  "$io_location_continuation_output"
assert_cst_direct_child_range \
  "1:0-1:2" \
  "body: io_file" \
  "1:0-1:1" \
  '">"' \
  "$io_location_continuation_output"
assert_incremental_equals_fresh \
  "$io_location_continuation_initial" \
  "$io_location_continuation_final" \
  "insert-continuation-after-io-location" \
  '3 0 \
'

function_outer_closer_initial="$runtime_directory/function-outer-closer-initial.sh"
function_outer_closer_final="$runtime_directory/function-outer-closer-final.sh"
printf '%s\n' '{ f()(:); }' >"$function_outer_closer_initial"
printf '%s\n' '{ f()(:) }' >"$function_outer_closer_final"
assert_incremental_equals_fresh \
  "$function_outer_closer_initial" \
  "$function_outer_closer_final" \
  "delete-separator-before-function-outer-closer" \
  "8 1"

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

# Here-document state, delimiter, and body contracts.
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

backquote_here_document_source="$runtime_directory/backquote-here-document.sh"
backquote_here_document_output="$runtime_directory/backquote-here-document.cst"
printf '%s\n' \
  'cat <<"`printf '\''%s'\'' END`"' \
  'body' \
  '`printf %s END`' \
  'after' \
  >"$backquote_here_document_source"
assert_cst_valid_with_output \
  "$backquote_here_document_source" \
  "$backquote_here_document_output"
assert_cst_range \
  "2:0-3:0" \
  "end: here_document_end" \
  "$backquote_here_document_output"
assert_cst_range \
  "3:0-3:5" \
  "command: complete_command" \
  "$backquote_here_document_output"
assert_not_contains \
  "here_document_end_recovery" \
  "$backquote_here_document_output"

backquote_hash_initial="$runtime_directory/backquote-hash-initial.sh"
backquote_hash_final="$runtime_directory/backquote-hash-final.sh"
backquote_hash_output="$runtime_directory/backquote-hash.cst"
printf '%s\n' \
  'cat <<"`printf ${x}X#tag END`"' \
  'body' \
  '`printf ${x}X#tag END`' \
  'after' \
  >"$backquote_hash_initial"
printf '%s\n' \
  'cat <<"`printf ${x}#tag END`"' \
  'body' \
  '`printf ${x}#tag END`' \
  'after' \
  >"$backquote_hash_final"
assert_cst_valid_with_output \
  "$backquote_hash_final" \
  "$backquote_hash_output"
assert_cst_range \
  "0:6-0:29" \
  "end: here_end" \
  "$backquote_hash_output"
assert_cst_range \
  "2:0-3:0" \
  "end: here_document_end" \
  "$backquote_hash_output"
assert_cst_range \
  "3:0-3:5" \
  "command: complete_command" \
  "$backquote_hash_output"
assert_not_contains "ERROR" "$backquote_hash_output"
assert_not_contains \
  "here_document_end_recovery" \
  "$backquote_hash_output"
assert_incremental_equals_fresh \
  "$backquote_hash_initial" \
  "$backquote_hash_final" \
  "delete-backquote-delimiter-word-markers" \
  "48 1" \
  "19 1"
assert_incremental_equals_fresh \
  "$backquote_hash_final" \
  "$backquote_hash_initial" \
  "insert-backquote-delimiter-word-markers" \
  "47 0 X" \
  "19 0 X"

byte_here_document_source="$runtime_directory/byte-here-document.sh"
byte_here_document_output="$runtime_directory/byte-here-document.cst"
printf 'cat <<$'"'"'\\c?'"'"'\nbody\n\177\ncat <<$'"'"'\\xC3\\xBF'"'"'\nbody\n\303\277\nafter\n' \
  >"$byte_here_document_source"
assert_cst_valid_with_output \
  "$byte_here_document_source" \
  "$byte_here_document_output"
assert_cst_range \
  "2:0-3:0" \
  "end: here_document_end" \
  "$byte_here_document_output"
assert_cst_range \
  "5:0-6:0" \
  "end: here_document_end" \
  "$byte_here_document_output"
assert_cst_range \
  "6:0-6:5" \
  "command: complete_command" \
  "$byte_here_document_output"
assert_not_contains \
  "here_document_end_recovery" \
  "$byte_here_document_output"

nul_here_document_source="$runtime_directory/nul-here-document.sh"
nul_here_document_output="$runtime_directory/nul-here-document.cst"
printf 'cat <<EOF #a\000b\nbody\nEOF\nafter\n' >"$nul_here_document_source"
run_parse \
  valid \
  cst \
  "$nul_here_document_output" \
  "NUL inside here-document declaration comment" \
  -- \
  "$nul_here_document_source"
assert_cst_range "0:10-0:14" "comment: comment" "$nul_here_document_output"
assert_cst_range \
  "2:0-3:0" \
  "end: here_document_end" \
  "$nul_here_document_output"
assert_cst_range \
  "3:0-3:5" \
  "command: complete_command" \
  "$nul_here_document_output"
assert_cst_range "0:0-4:0" "program" "$nul_here_document_output"

byte_here_document_mismatch_source="$runtime_directory/byte-here-document-mismatch.sh"
byte_here_document_mismatch_output="$runtime_directory/byte-here-document-mismatch.cst"
printf 'cat <<$'"'"'\\xFF'"'"'\nbody\n\007\nafter\n' \
  >"$byte_here_document_mismatch_source"
assert_cst_valid_with_output \
  "$byte_here_document_mismatch_source" \
  "$byte_here_document_mismatch_output"
assert_cst_range \
  "4:0-4:0" \
  "end: here_document_end_recovery" \
  "$byte_here_document_mismatch_output"

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

missing_delimiter_great="$runtime_directory/missing-delimiter-great.sh"
missing_delimiter_less="$runtime_directory/missing-delimiter-less.sh"
printf '%s\n' 'command << >out' >"$missing_delimiter_great"
printf '%s\n' 'command << <in' >"$missing_delimiter_less"
parse_incremental_and_fresh_with_output \
  "$missing_delimiter_great" \
  "$missing_delimiter_less" \
  "replace-redirection-after-missing-here-document-delimiter" \
  "11 4 <in"
for missing_delimiter_output in \
  "$runtime_directory/replace-redirection-after-missing-here-document-delimiter.incremental" \
  "$runtime_directory/replace-redirection-after-missing-here-document-delimiter.fresh"; do
  assert_cst_range \
    "0:11-0:11" \
    "recovery: missing_here_end" \
    "$missing_delimiter_output"
  assert_cst_range "0:11-0:12" '"<"' "$missing_delimiter_output"
done

continued_tab_initial="$runtime_directory/continued-tab-initial.sh"
continued_tab_final="$runtime_directory/continued-tab-final.sh"
continued_tab_output="$runtime_directory/continued-tab.cst"
printf 'cat <<-AB\nA\\\nB\nAB\nafter\n' >"$continued_tab_initial"
printf 'cat <<-AB\nA\\\n\tB\nAB\nafter\n' >"$continued_tab_final"
assert_cst_valid_with_output "$continued_tab_final" "$continued_tab_output"
assert_cst_range \
  "1:1-2:0" \
  "line_continuation" \
  "$continued_tab_output"
assert_cst_range \
  "3:0-4:0" \
  "end: here_document_end" \
  "$continued_tab_output"
continued_tab_character=$(printf '\t')
assert_incremental_equals_fresh \
  "$continued_tab_initial" \
  "$continued_tab_final" \
  "insert-tab-after-here-document-continuation" \
  "13 0 $continued_tab_character"

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
assert_incremental_equals_fresh \
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
assert_cst_range "3:0-3:5" "complete_command" "$long_incremental_output"
assert_cst_range "3:0-3:5" "complete_command" "$long_fresh_output"

# Word, parameter, arithmetic, and tilde classifications.
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
assert_incremental_equals_fresh \
  "$parameter_context_unquoted" \
  "$parameter_context_quoted" \
  "insert-parameter-outer-quotes" \
  '7 0 "' \
  '18 0 "'
assert_incremental_equals_fresh \
  "$parameter_context_quoted" \
  "$parameter_context_unquoted" \
  "delete-parameter-outer-quotes" \
  "7 1" \
  "17 1"
assert_incremental_equals_fresh \
  "$parameter_context_quoted" \
  "$parameter_context_pattern" \
  "parameter-value-to-pattern-context" \
  "11 2 ##"
assert_incremental_equals_fresh \
  "$parameter_context_pattern" \
  "$parameter_context_quoted" \
  "parameter-pattern-to-value-context" \
  "11 2 :-"

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

arithmetic_missing_operand_source="$runtime_directory/arithmetic-missing-operand.sh"
arithmetic_missing_operand_output="$runtime_directory/arithmetic-missing-operand.cst"
printf '%s\n' ': "$((1 +\' '))"' >"$arithmetic_missing_operand_source"
run_parse \
  recovery \
  cst \
  "$arithmetic_missing_operand_output" \
  "arithmetic missing operand after continuation" \
  "$arithmetic_missing_operand_source"
assert_cst_range \
  "0:3-0:9" \
  "ERROR" \
  "$arithmetic_missing_operand_output"
assert_cst_range \
  "0:9-1:0" \
  "line_continuation" \
  "$arithmetic_missing_operand_output"
assert_cst_range \
  "1:0-1:2" \
  "double_quote_text" \
  "$arithmetic_missing_operand_output"
assert_occurrence_count \
  1 \
  "line_continuation" \
  "$arithmetic_missing_operand_output"

arithmetic_prefix_increment_logical="$runtime_directory/arithmetic-prefix-increment-logical.sh"
arithmetic_prefix_increment_physical="$runtime_directory/arithmetic-prefix-increment-physical.sh"
printf '%s\n' ': "$((++a))"' >"$arithmetic_prefix_increment_logical"
printf '%s\n' ': "$((+\' '+a))"' >"$arithmetic_prefix_increment_physical"
assert_continued_parse_equivalent \
  "arithmetic-prefix-increment" \
  1 \
  "$arithmetic_prefix_increment_logical" \
  "$arithmetic_prefix_increment_physical"

arithmetic_prefix_decrement_logical="$runtime_directory/arithmetic-prefix-decrement-logical.sh"
arithmetic_prefix_decrement_physical="$runtime_directory/arithmetic-prefix-decrement-physical.sh"
printf '%s\n' ': "$((--a))"' >"$arithmetic_prefix_decrement_logical"
printf '%s\n' ': "$((-\' '-a))"' >"$arithmetic_prefix_decrement_physical"
assert_continued_parse_equivalent \
  "arithmetic-prefix-decrement" \
  1 \
  "$arithmetic_prefix_decrement_logical" \
  "$arithmetic_prefix_decrement_physical"

arithmetic_infix_increment_logical="$runtime_directory/arithmetic-infix-increment-logical.sh"
arithmetic_infix_increment_physical="$runtime_directory/arithmetic-infix-increment-physical.sh"
printf '%s\n' ': "$((a++b))"' >"$arithmetic_infix_increment_logical"
printf '%s\n' ': "$((a+\' '+b))"' >"$arithmetic_infix_increment_physical"
assert_continued_parse_equivalent \
  "arithmetic-infix-increment" \
  0 \
  "$arithmetic_infix_increment_logical" \
  "$arithmetic_infix_increment_physical"

arithmetic_infix_decrement_logical="$runtime_directory/arithmetic-infix-decrement-logical.sh"
arithmetic_infix_decrement_physical="$runtime_directory/arithmetic-infix-decrement-physical.sh"
printf '%s\n' ': "$((a--b))"' >"$arithmetic_infix_decrement_logical"
printf '%s\n' ': "$((a-\' '-b))"' >"$arithmetic_infix_decrement_physical"
assert_continued_parse_equivalent \
  "arithmetic-infix-decrement" \
  0 \
  "$arithmetic_infix_decrement_logical" \
  "$arithmetic_infix_decrement_physical"

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

arithmetic_lvalue_initial="$runtime_directory/arithmetic-lvalue-initial.sh"
arithmetic_lvalue_parenthesized="$runtime_directory/arithmetic-lvalue-parenthesized.sh"
printf '%s\n' ': "$((name = 1))"' >"$arithmetic_lvalue_initial"
printf '%s\n' ': "$(((name) = 1))"' >"$arithmetic_lvalue_parenthesized"
assert_incremental_equals_fresh \
  "$arithmetic_lvalue_initial" \
  "$arithmetic_lvalue_parenthesized" \
  "parenthesize-arithmetic-assignment-lvalue" \
  "6 0 (" \
  "11 0 )"
assert_incremental_equals_fresh \
  "$arithmetic_lvalue_parenthesized" \
  "$arithmetic_lvalue_initial" \
  "unparenthesize-arithmetic-assignment-lvalue" \
  "6 1" \
  "10 1"
arithmetic_lvalue_output="$runtime_directory/parenthesize-arithmetic-assignment-lvalue.fresh"
assert_cst_range \
  "0:3-0:18" \
  "arithmetic_expansion" \
  "$arithmetic_lvalue_output"
assert_cst_range \
  "0:6-0:16" \
  "expression: arithmetic_assignment_expression" \
  "$arithmetic_lvalue_output"
assert_cst_range \
  "0:6-0:12" \
  "left: parenthesized_arithmetic" \
  "$arithmetic_lvalue_output"
assert_cst_range \
  "0:7-0:11" \
  "expression: arithmetic_variable" \
  "$arithmetic_lvalue_output"
assert_cst_range \
  "0:13-0:14" \
  "operator: arithmetic_operator" \
  "$arithmetic_lvalue_output"
assert_cst_range \
  "0:15-0:16" \
  "right: arithmetic_number" \
  "$arithmetic_lvalue_output"

arithmetic_non_lvalue_initial="$runtime_directory/arithmetic-non-lvalue-initial.sh"
arithmetic_non_lvalue_final="$runtime_directory/arithmetic-non-lvalue-final.sh"
printf '%s\n' ': "$(((name) = 2))"' >"$arithmetic_non_lvalue_initial"
printf '%s\n' ': "$(((name + 1) = 2))"' >"$arithmetic_non_lvalue_final"
parse_incremental_and_fresh_with_output \
  "$arithmetic_non_lvalue_initial" \
  "$arithmetic_non_lvalue_final" \
  "make-parenthesized-arithmetic-non-lvalue" \
  "11 0  + 1"
assert_incremental_equals_fresh \
  "$arithmetic_non_lvalue_final" \
  "$arithmetic_non_lvalue_initial" \
  "restore-parenthesized-arithmetic-lvalue" \
  "11 4"
for arithmetic_non_lvalue_output in \
  "$runtime_directory/make-parenthesized-arithmetic-non-lvalue.incremental" \
  "$runtime_directory/make-parenthesized-arithmetic-non-lvalue.fresh"; do
  assert_cst_range \
    "0:3-0:22" \
    "ERROR" \
    "$arithmetic_non_lvalue_output"
  assert_cst_range \
    "0:6-0:16" \
    "parenthesized_arithmetic_source" \
    "$arithmetic_non_lvalue_output"
  assert_cst_range \
    "0:17-0:18" \
    '"="' \
    "$arithmetic_non_lvalue_output"
  assert_not_contains \
    "arithmetic_assignment_expression" \
    "$arithmetic_non_lvalue_output"
done

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
assert_incremental_equals_fresh \
  "$arithmetic_unary_bang_initial" \
  "$arithmetic_unary_bang_final" \
  "insert-arithmetic-unary-bang-continuation" \
  '6 0 \
'
assert_incremental_equals_fresh \
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
assert_incremental_equals_fresh \
  "$arithmetic_unary_plus_initial" \
  "$arithmetic_unary_plus_final" \
  "insert-arithmetic-unary-plus-continuations" \
  '6 0 \
\
'
assert_incremental_equals_fresh \
  "$arithmetic_unary_plus_final" \
  "$arithmetic_unary_plus_initial" \
  "delete-arithmetic-unary-plus-continuations" \
  "6 4"

arithmetic_unary_minus_initial="$runtime_directory/arithmetic-unary-minus-initial.sh"
arithmetic_unary_minus_final="$runtime_directory/arithmetic-unary-minus-final.sh"
printf '%s\n' ': $((-+a))' >"$arithmetic_unary_minus_initial"
printf '%s\n' ': $((-\' '+a))' >"$arithmetic_unary_minus_final"
assert_incremental_equals_fresh \
  "$arithmetic_unary_minus_initial" \
  "$arithmetic_unary_minus_final" \
  "insert-arithmetic-unary-minus-continuation" \
  '6 0 \
'
assert_incremental_equals_fresh \
  "$arithmetic_unary_minus_final" \
  "$arithmetic_unary_minus_initial" \
  "delete-arithmetic-unary-minus-continuation" \
  "6 2"

arithmetic_separated_sign_initial="$runtime_directory/arithmetic-separated-sign-initial.sh"
arithmetic_separated_sign_final="$runtime_directory/arithmetic-separated-sign-final.sh"
printf '%s\n' ': "$((+ +a))"' >"$arithmetic_separated_sign_initial"
printf '%s\n' ': "$((+\' ' +a))"' >"$arithmetic_separated_sign_final"
assert_incremental_equals_fresh \
  "$arithmetic_separated_sign_initial" \
  "$arithmetic_separated_sign_final" \
  "insert-arithmetic-separated-sign-continuation" \
  '7 0 \
'
assert_incremental_equals_fresh \
  "$arithmetic_separated_sign_final" \
  "$arithmetic_separated_sign_initial" \
  "delete-arithmetic-separated-sign-continuation" \
  "7 2"

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

tilde_percent_initial="$runtime_directory/tilde-percent-initial.sh"
tilde_percent_final="$runtime_directory/tilde-percent-final.sh"
printf '%s\n' ': ~alice/x' >"$tilde_percent_initial"
printf '%s\n' ': ~alice%/x' >"$tilde_percent_final"
assert_incremental_equals_fresh \
  "$tilde_percent_initial" \
  "$tilde_percent_final" \
  "insert-percent-in-literal-tilde-user" \
  "8 0 %"
assert_incremental_equals_fresh \
  "$tilde_percent_final" \
  "$tilde_percent_initial" \
  "delete-percent-from-literal-tilde-user" \
  "8 1"
tilde_percent_output="$runtime_directory/insert-percent-in-literal-tilde-user.fresh"
assert_cst_range \
  "0:2-0:9" \
  "tilde_expansion" \
  "$tilde_percent_output"
assert_cst_range \
  "0:3-0:9" \
  "user: tilde_user" \
  "$tilde_percent_output"
assert_cst_range \
  "0:3-0:9" \
  "literal" \
  "$tilde_percent_output"
assert_cst_range "0:9-0:10" '"/"' "$tilde_percent_output"

tilde_assignment_percent_initial="$runtime_directory/tilde-assignment-percent-initial.sh"
tilde_assignment_percent_final="$runtime_directory/tilde-assignment-percent-final.sh"
printf '%s\n' 'A=~alice:x :' >"$tilde_assignment_percent_initial"
printf '%s\n' 'A=~alice%:x :' >"$tilde_assignment_percent_final"
assert_incremental_equals_fresh \
  "$tilde_assignment_percent_initial" \
  "$tilde_assignment_percent_final" \
  "insert-percent-before-assignment-tilde-colon" \
  "8 0 %"
assert_incremental_equals_fresh \
  "$tilde_assignment_percent_final" \
  "$tilde_assignment_percent_initial" \
  "delete-percent-before-assignment-tilde-colon" \
  "8 1"
tilde_assignment_percent_output="$runtime_directory/insert-percent-before-assignment-tilde-colon.fresh"
assert_cst_range \
  "0:2-0:11" \
  "value: assignment_value" \
  "$tilde_assignment_percent_output"
assert_cst_range \
  "0:2-0:9" \
  "tilde_expansion" \
  "$tilde_assignment_percent_output"
assert_cst_range \
  "0:3-0:9" \
  "user: tilde_user" \
  "$tilde_assignment_percent_output"
assert_cst_range \
  "0:9-0:10" \
  '":"' \
  "$tilde_assignment_percent_output"

tilde_parameter_percent_initial="$runtime_directory/tilde-parameter-percent-initial.sh"
tilde_parameter_percent_final="$runtime_directory/tilde-parameter-percent-final.sh"
printf '%s\n' ': ${v:-~alice/x}' >"$tilde_parameter_percent_initial"
printf '%s\n' ': ${v:-~alice%/x}' >"$tilde_parameter_percent_final"
assert_incremental_equals_fresh \
  "$tilde_parameter_percent_initial" \
  "$tilde_parameter_percent_final" \
  "insert-percent-in-parameter-word-tilde-user" \
  "13 0 %"
assert_incremental_equals_fresh \
  "$tilde_parameter_percent_final" \
  "$tilde_parameter_percent_initial" \
  "delete-percent-from-parameter-word-tilde-user" \
  "13 1"
tilde_parameter_percent_output="$runtime_directory/insert-percent-in-parameter-word-tilde-user.fresh"
assert_cst_range \
  "0:2-0:17" \
  "parameter_expansion" \
  "$tilde_parameter_percent_output"
assert_cst_range \
  "0:7-0:16" \
  "word: parameter_word" \
  "$tilde_parameter_percent_output"
assert_cst_range \
  "0:7-0:14" \
  "tilde_expansion" \
  "$tilde_parameter_percent_output"
assert_cst_range \
  "0:8-0:14" \
  "user: tilde_user" \
  "$tilde_parameter_percent_output"
assert_cst_range \
  "0:14-0:15" \
  '"/"' \
  "$tilde_parameter_percent_output"
assert_cst_range \
  "0:16-0:17" \
  '"}"' \
  "$tilde_parameter_percent_output"

tilde_nested_user_initial="$runtime_directory/tilde-nested-user-initial.sh"
tilde_nested_user_final="$runtime_directory/tilde-nested-user-final.sh"
printf '%s\n' ': ~"$(echo ab)"/x' >"$tilde_nested_user_initial"
printf '%s\n' ': ~"$(echo a/b)"/x' >"$tilde_nested_user_final"
assert_incremental_equals_fresh \
  "$tilde_nested_user_initial" \
  "$tilde_nested_user_final" \
  "insert-slash-in-nested-tilde-user-substitution" \
  "12 0 /"
assert_incremental_equals_fresh \
  "$tilde_nested_user_final" \
  "$tilde_nested_user_initial" \
  "delete-slash-from-nested-tilde-user-substitution" \
  "12 1"
tilde_nested_user_output="$runtime_directory/insert-slash-in-nested-tilde-user-substitution.fresh"
assert_cst_range \
  "0:2-0:16" \
  "tilde_expansion" \
  "$tilde_nested_user_output"
assert_cst_range \
  "0:3-0:16" \
  "user: tilde_user" \
  "$tilde_nested_user_output"
assert_cst_range \
  "0:3-0:16" \
  "double_quoted" \
  "$tilde_nested_user_output"
assert_cst_range \
  "0:4-0:15" \
  "command_substitution" \
  "$tilde_nested_user_output"
assert_cst_range "0:12-0:13" '"/"' "$tilde_nested_user_output"
assert_cst_range "0:16-0:17" '"/"' "$tilde_nested_user_output"

assignment_boundary_initial="$runtime_directory/assignment-boundary-initial.sh"
assignment_boundary_final="$runtime_directory/assignment-boundary-final.sh"
assignment_boundary_output="$runtime_directory/assignment-boundary.out"
printf '%s\n' 'name=value command' >"$assignment_boundary_initial"
printf '%s\n' 'name=value\' ' command' >"$assignment_boundary_final"
assert_cst_valid_with_output \
  "$assignment_boundary_final" \
  "$assignment_boundary_output"
assert_occurrence_count \
  1 \
  "assignment: assignment_word" \
  "$assignment_boundary_output"
assert_cst_range \
  "0:5  - 0:10" \
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

assignment_newline_initial="$runtime_directory/assignment-newline-initial.sh"
assignment_newline_final="$runtime_directory/assignment-newline-final.sh"
assignment_newline_output="$runtime_directory/assignment-newline.out"
printf '%s\n' 'x=a' >"$assignment_newline_initial"
printf '%s\n' "x=a\\" '' >"$assignment_newline_final"
assert_cst_valid_with_output \
  "$assignment_newline_final" \
  "$assignment_newline_output"
assert_occurrence_count \
  1 \
  "assignment: assignment_word" \
  "$assignment_newline_output"
assert_cst_range \
  "0:2 - 0:3" \
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

compound_tail_initial="$runtime_directory/compound-tail-initial.sh"
compound_tail_final="$runtime_directory/compound-tail-final.sh"
printf '%s\n' '(:)&' 'child_pid=$!' >"$compound_tail_initial"
printf '%s\n' '(:) &' 'child_pid=$!' >"$compound_tail_final"
assert_incremental_equals_fresh \
  "$compound_tail_initial" \
  "$compound_tail_final" \
  "insert-layout-before-asynchronous-separator" \
  '3 0  '
for compound_tail_output in \
  "$runtime_directory/insert-layout-before-asynchronous-separator.incremental" \
  "$runtime_directory/insert-layout-before-asynchronous-separator.fresh"; do
  assert_cst_range "0:0-0:3" "subshell" "$compound_tail_output"
  assert_cst_range "0:4-0:5" "separator_op" "$compound_tail_output"
  assert_cst_range \
    "1:0-1:12" \
    "command: complete_command" \
    "$compound_tail_output"
  assert_not_contains "ERROR" "$compound_tail_output"
  assert_not_contains "command_recovery" "$compound_tail_output"
done

# Command separators and compound-list boundaries.
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
assert_incremental_equals_fresh \
  "$for_wordlist_initial" \
  "$for_wordlist_final" \
  "insert-second-for-wordlist-continuation" \
  '11 0 \
'
assert_incremental_equals_fresh \
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
assert_incremental_equals_fresh \
  "$case_subject_initial" \
  "$case_subject_final" \
  "insert-second-case-subject-continuation" \
  '9 0 \
'
assert_incremental_equals_fresh \
  "$case_subject_final" \
  "$case_subject_initial" \
  "delete-second-case-subject-continuation" \
  "9 2"

case_keyword_initial="$runtime_directory/case-keyword-initial.sh"
case_keyword_final="$runtime_directory/case-keyword-final.sh"
printf '%s\n' 'case x in esac' >"$case_keyword_initial"
printf '%s\n' 'case\' ' x in esac' >"$case_keyword_final"
assert_incremental_equals_fresh \
  "$case_keyword_initial" \
  "$case_keyword_final" \
  "insert-case-keyword-continuation" \
  '4 0 \
'
assert_incremental_equals_fresh \
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

function_layout_initial="$runtime_directory/function-layout-initial.sh"
function_layout_final="$runtime_directory/function-layout-final.sh"
printf '%s\n' \
  'f() {' \
  ' before' \
  ' while :; do :; done' \
  ' value=1' \
  ' after' \
  '}' \
  >"$function_layout_initial"
printf '%s\n' \
  'f() {' \
  ' before' \
  ' while :; do' \
  '  :' \
  ' done' \
  ' value=1' \
  ' after' \
  '}' \
  >"$function_layout_final"
assert_incremental_equals_fresh \
  "$function_layout_initial" \
  "$function_layout_final" \
  "expand-function-loop-layout" \
  '25 5 o
  :
 '
for function_layout_output in \
  "$runtime_directory/expand-function-loop-layout.incremental" \
  "$runtime_directory/expand-function-loop-layout.fresh"; do
  assert_cst_range "0:0-7:1" "function_definition" "$function_layout_output"
  assert_cst_range "2:1-4:5" "while_clause" "$function_layout_output"
  assert_cst_range "5:1-5:8" "assignment_word" "$function_layout_output"
  assert_not_contains "ERROR" "$function_layout_output"
  assert_not_contains "command_recovery" "$function_layout_output"
done

compound_list_branch_initial="$runtime_directory/compound-list-branch-initial.sh"
compound_list_branch_final="$runtime_directory/compound-list-branch-final.sh"
printf '%s\n' \
  'f() {' \
  ' a=' \
  ' if :; then' \
  '  :' \
  ' else' \
  '  :' \
  ' fi' \
  ' b=' \
  '}' \
  >"$compound_list_branch_initial"
printf '%s\n' \
  'f() {' \
  ' a=' \
  ' if :; then' \
  '  :' \
  ' else' \
  '  :' \
  ' fi' \
  ' while :; do :; done' \
  ' b=' \
  '}' \
  >"$compound_list_branch_final"
assert_incremental_equals_fresh \
  "$compound_list_branch_initial" \
  "$compound_list_branch_final" \
  "insert-compound-list-loop" \
  '40 0  while :; do :; done
'
for compound_list_branch_output in \
  "$runtime_directory/insert-compound-list-loop.incremental" \
  "$runtime_directory/insert-compound-list-loop.fresh"; do
  assert_cst_range \
    "0:0-9:1" \
    "function_definition" \
    "$compound_list_branch_output"
  assert_cst_range "0:4-9:1" "brace_group" "$compound_list_branch_output"
  assert_cst_direct_child_range \
    "0:4-9:1" \
    "brace_group" \
    "0:5-9:0" \
    "body: compound_list" \
    "$compound_list_branch_output"
  assert_cst_direct_child_range \
    "0:5-9:0" \
    "body: compound_list" \
    "1:1-8:3" \
    "body: term" \
    "$compound_list_branch_output"
  assert_cst_range "2:1-6:3" "if_clause" "$compound_list_branch_output"
  assert_cst_range "7:1-7:20" "while_clause" "$compound_list_branch_output"
  assert_occurrence_count 2 "assignment: assignment_word" \
    "$compound_list_branch_output"
  assert_cst_range \
    "1:1-1:3" \
    "assignment: assignment_word" \
    "$compound_list_branch_output"
  assert_cst_range \
    "8:1-8:3" \
    "assignment: assignment_word" \
    "$compound_list_branch_output"
  assert_cst_range "0:0-10:0" "program" "$compound_list_branch_output"
  assert_not_contains "ERROR" "$compound_list_branch_output"
  assert_not_contains "command_recovery" "$compound_list_branch_output"
  assert_not_contains "compound_command_recovery" \
    "$compound_list_branch_output"
done

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

function_structure_sample="$repository_directory/test/runtime/function-structure.source"
function_structure_output="$runtime_directory/function-structure.out"
function_structure_query="$runtime_directory/function-structure.query"
assert_valid_with_output "$function_structure_sample" "$function_structure_output"
run_query \
  "$function_structure_query" \
  "$repository_directory/test/runtime/contracts.scm" \
  "$function_structure_sample"
assert_contains 'sample_log' "$function_structure_query"
assert_contains 'summarize' "$function_structure_query"

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
assert_incremental_equals_fresh \
  "$case_item_boundary_initial" \
  "$case_item_boundary_final" \
  "insert-second-case-item-boundary-continuation" \
  '16 0 \
'
assert_incremental_equals_fresh \
  "$case_item_boundary_final" \
  "$case_item_boundary_initial" \
  "delete-second-case-item-boundary-continuation" \
  "16 2"

case_body_separator_initial="$runtime_directory/case-body-separator-initial.sh"
case_body_separator_final="$runtime_directory/case-body-separator-final.sh"
printf '%s\n' 'case x in x)echo z;;esac' >"$case_body_separator_initial"
printf '%s\n' 'case x in x)echo \' 'z;;esac' >"$case_body_separator_final"
assert_incremental_equals_fresh \
  "$case_body_separator_initial" \
  "$case_body_separator_final" \
  "insert-case-body-separator-continuation" \
  '17 0 \
'
assert_incremental_equals_fresh \
  "$case_body_separator_final" \
  "$case_body_separator_initial" \
  "delete-case-body-separator-continuation" \
  "17 2"

case_item_following_initial="$runtime_directory/case-item-following-initial.sh"
case_item_following_final="$runtime_directory/case-item-following-final.sh"
printf '%s\n' 'case x in x):;; y):;;esac' >"$case_item_following_initial"
printf '%s\n' 'case x in x):;;\' ' y):;;esac' >"$case_item_following_final"
assert_incremental_equals_fresh \
  "$case_item_following_initial" \
  "$case_item_following_final" \
  "insert-case-item-following-continuation" \
  '15 0 \
'
assert_incremental_equals_fresh \
  "$case_item_following_final" \
  "$case_item_following_initial" \
  "delete-case-item-following-continuation" \
  "15 2"

empty_case_item_initial="$runtime_directory/empty-case-item-initial.sh"
empty_case_item_final="$runtime_directory/empty-case-item-final.sh"
printf '%s\n' 'case x in x):;;esac' >"$empty_case_item_initial"
printf '%s\n' 'case x in x);;esac' >"$empty_case_item_final"
assert_incremental_equals_fresh \
  "$empty_case_item_initial" \
  "$empty_case_item_final" \
  "delete-empty-case-item-body" \
  "12 1"

reserved_for_word_initial="$runtime_directory/reserved-for-word-initial.sh"
reserved_for_word_final="$runtime_directory/reserved-for-word-final.sh"
printf '%s\n' 'for x in ordinary; do :; done' >"$reserved_for_word_initial"
printf '%s\n' 'for x in fi; do :; done' >"$reserved_for_word_final"
assert_incremental_equals_fresh \
  "$reserved_for_word_initial" \
  "$reserved_for_word_final" \
  "replace-for-word-with-reserved-closer-spelling" \
  "9 8 fi"

# Bracket fallback, resource bounds, and scaling guards.
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
assert_incremental_equals_fresh \
  "$special_bracket_unclosed" \
  "$special_bracket_closed" \
  "complete-collating-symbol-bracket-expression" \
  "10 0 x.]]"
assert_incremental_equals_fresh \
  "$special_bracket_closed" \
  "$special_bracket_unclosed" \
  "unclose-collating-symbol-bracket-expression" \
  "10 4"

special_suffix_initial="$runtime_directory/special-suffix-initial.sh"
special_suffix_final="$runtime_directory/special-suffix-final.sh"
printf '%s\n' "printf [[:alpha:]" >"$special_suffix_initial"
printf '%s\n' "printf [[:alpha:]]" >"$special_suffix_final"
assert_incremental_equals_fresh \
  "$special_suffix_initial" \
  "$special_suffix_final" \
  "complete-special-suffix-outer-bracket" \
  "17 0 ]"
assert_incremental_equals_fresh \
  "$special_suffix_final" \
  "$special_suffix_initial" \
  "restore-special-suffix-literal-prefix" \
  "17 1"

operator_suffix_initial="$runtime_directory/operator-suffix-initial.sh"
operator_suffix_final="$runtime_directory/operator-suffix-final.sh"
printf '%s\n' 'printf [a"x"[.]' >"$operator_suffix_initial"
printf '%s\n' 'printf [a"x"*[.]' >"$operator_suffix_final"
assert_incremental_equals_fresh \
  "$operator_suffix_initial" \
  "$operator_suffix_final" \
  "insert-operator-before-completed-bracket-suffix" \
  "12 0 *"
assert_incremental_equals_fresh \
  "$operator_suffix_final" \
  "$operator_suffix_initial" \
  "delete-operator-before-completed-bracket-suffix" \
  "12 1"

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
assert_repeated_cold_parse resource "$many_documents" "many-documents"

deep_documents="$runtime_directory/deep-documents.sh"
awk 'BEGIN {
  depth = 150
  print "cat <<X"
  for (counter = 1; counter < depth; counter += 1) print "$(cat <<X"
  print "leaf"
  for (counter = depth - 1; counter >= 0; counter -= 1) {
    print "X"
    if (counter > 0) print ")"
  }
  print "after"
}' >"$deep_documents"
assert_valid "$deep_documents"

deep_documents_bounded="$runtime_directory/deep-documents-bounded.sh"
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
}' >"$deep_documents_bounded"
parse_timeout_microseconds=30000000
assert_repeated_cold_parse \
  resource \
  "$deep_documents_bounded" \
  "deep-documents-bounded"
parse_timeout_microseconds=10000000

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
assert_parse_with_output \
  "$quote_recovery_initial" \
  "$quote_recovery_output"
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
small_dynamic_arithmetic_layout="$runtime_directory/small-dynamic-arithmetic-layout.sh"
large_dynamic_arithmetic_layout="$runtime_directory/large-dynamic-arithmetic-layout.sh"
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
measure_parse_milliseconds() {
  measured_source=$1
  measured_output="$runtime_directory/measured-parse.out"
  start_nanoseconds=$(
    node -e 'process.stdout.write(String(process.hrtime.bigint()))'
  )
  run_parse \
    valid \
    summary \
    "$measured_output" \
    "$measured_source performance" \
    "$measured_source"
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

small_milliseconds=$(measure_parse_milliseconds "$small_bracket_suffixes")
large_milliseconds=$(measure_parse_milliseconds "$large_bracket_suffixes")
scaling_limit=$((small_milliseconds * 8 + 100))
if [ "$large_milliseconds" -gt "$scaling_limit" ]; then
  fail \
    "Bracket suffix parsing scaled nonlinearly: ${small_milliseconds}ms to ${large_milliseconds}ms"
fi
