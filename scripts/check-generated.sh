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
    "$repository_directory/$generated_file"
  then
    printf '%s\n' "Generated file is stale: $generated_file" >&2
    stale=1
  fi
done

exit "$stale"
