#!/bin/sh

set -eu

repository_directory=$(
  CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd
)
generated_directory=$(
  mktemp -d "${TMPDIR:-/tmp}/tree-sitter-posix-sh-generated.XXXXXX"
)

# ShellCheck cannot see that the trap invokes this callback.
# shellcheck disable=SC2329
cleanup() {
  find "$generated_directory" -depth -delete
}
trap cleanup EXIT HUP INT TERM

generated_files="
src/grammar.json
src/node-types.json
src/parser.c
src/tree_sitter/alloc.h
src/tree_sitter/array.h
src/tree_sitter/parser.h
"

(
  cd "$repository_directory"
  ./node_modules/.bin/tree-sitter generate \
    --output "$generated_directory" \
    grammar.js
)

stale=0
for generated_file in $generated_files; do
  generated_output="$generated_directory/${generated_file#src/}"
  if ! cmp -s \
    "$generated_output" \
    "$repository_directory/$generated_file"; then
    printf '%s\n' "Generated file is stale: $generated_file" >&2
    stale=1
  fi
done

state_limit=30500
parser_size_limit=37500000
external_token_limit=105
state_count=$(
  awk '/^#define STATE_COUNT / { print $3; exit }' \
    "$generated_directory/parser.c"
)
parser_size=$(
  wc -c <"$generated_directory/parser.c" | tr -d '[:space:]'
)
external_token_count=$(
  awk '/^#define EXTERNAL_TOKEN_COUNT / { print $3; exit }' \
    "$generated_directory/parser.c"
)

case $state_count in
'' | *[!0-9]*)
  printf '%s\n' "Could not read STATE_COUNT from src/parser.c" >&2
  stale=1
  ;;
*)
  if [ "$state_count" -gt "$state_limit" ]; then
    printf '%s\n' \
      "Generated parser state budget exceeded: $state_count > $state_limit" >&2
    stale=1
  fi
  ;;
esac

case $external_token_count in
'' | *[!0-9]*)
  printf '%s\n' "Could not read EXTERNAL_TOKEN_COUNT from generated parser.c" >&2
  stale=1
  ;;
*)
  if [ "$external_token_count" -gt "$external_token_limit" ]; then
    printf '%s\n' \
      "Generated external token budget exceeded: $external_token_count > $external_token_limit" >&2
    stale=1
  fi
  ;;
esac

case $parser_size in
'' | *[!0-9]*)
  printf '%s\n' "Could not read the size of src/parser.c" >&2
  stale=1
  ;;
*)
  if [ "$parser_size" -gt "$parser_size_limit" ]; then
    printf '%s\n' \
      "Generated parser size budget exceeded: $parser_size > $parser_size_limit bytes" >&2
    stale=1
  fi
  ;;
esac

exit "$stale"
