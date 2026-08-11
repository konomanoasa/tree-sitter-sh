#!/bin/sh

set -eu

repository_directory=$(
  CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd
)
snapshot_directory=$(
  mktemp -d "${TMPDIR:-/tmp}/tree-sitter-posix-sh-generated.XXXXXX"
)

# ShellCheck cannot see that the trap invokes this callback.
# shellcheck disable=SC2329
cleanup() {
  find "$snapshot_directory" -depth -delete
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

for generated_file in $generated_files; do
  snapshot_file="$snapshot_directory/$generated_file"
  mkdir -p "$(dirname -- "$snapshot_file")"
  cp "$repository_directory/$generated_file" "$snapshot_file"
done

(
  cd "$repository_directory"
  ./node_modules/.bin/tree-sitter generate
)

stale=0
for generated_file in $generated_files; do
  if ! cmp -s \
    "$snapshot_directory/$generated_file" \
    "$repository_directory/$generated_file"; then
    printf '%s\n' "Generated file is stale: $generated_file" >&2
    stale=1
  fi
done

state_limit=40000
parser_size_limit=55000000
state_count=$(
  awk '/^#define STATE_COUNT / { print $3; exit }' \
    "$repository_directory/src/parser.c"
)
parser_size=$(
  wc -c <"$repository_directory/src/parser.c" | tr -d '[:space:]'
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
