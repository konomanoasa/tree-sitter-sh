#!/bin/sh

set -eu

repository_directory=$(
  CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd
)
scanner_check_directory=$(
  mktemp -d "${TMPDIR:-/tmp}/tree-sitter-posix-sh-scanner.XXXXXX"
)

cleanup() {
  find "$scanner_check_directory" -depth -delete
}
trap cleanup EXIT HUP INT TERM

case $(uname -s) in
Darwin)
  if ! command -v brew >/dev/null 2>&1; then
    printf '%s\n' "Homebrew is required to resolve LLVM on macOS" >&2
    exit 1
  fi
  llvm_directory=$(brew --prefix llvm)/bin
  clang=$llvm_directory/clang
  clangd=$llvm_directory/clangd
  ;;
*)
  clang=${CC:-clang}
  clangd=${CLANGD:-clangd}
  ;;
esac

for diagnostic_tool in "$clang" "$clangd"; do
  if [ ! -x "$diagnostic_tool" ] && ! command -v "$diagnostic_tool" >/dev/null 2>&1; then
    printf '%s\n' "Required scanner diagnostic is unavailable: $diagnostic_tool" >&2
    exit 1
  fi
done

scanner_source="$repository_directory/src/scanner.c"
scanner_contract_source="$repository_directory/test/scanner/contract.c"
scanner_contract="$scanner_check_directory/scanner-contract"
scanner_reuse_contract="$scanner_check_directory/scanner-reuse-contract"
scanner_reuse_object="$scanner_check_directory/scanner-reuse.o"
compile_commands="$scanner_check_directory/compile_commands.json"

"$clang" \
  -std=c11 \
  -Wall \
  -Wextra \
  -Werror \
  -pedantic \
  -I"$repository_directory/src" \
  -fsyntax-only \
  "$scanner_source"

awk \
  -v directory="$repository_directory" \
  -v compiler="$clang" \
  -v scanner="$scanner_source" \
  -v contract="$scanner_contract_source" '
    function json_escape(value) {
      gsub(/\\/, "\\\\", value)
      gsub(/\"/, "\\\"", value)
      return value
    }

    BEGIN {
      directory = json_escape(directory)
      compiler = json_escape(compiler)
      scanner = json_escape(scanner)
      contract = json_escape(contract)
      printf "[{\"directory\":\"%s\",", directory
      printf "\"arguments\":[\"%s\",\"-std=c11\",", compiler
      printf "\"-Wall\",\"-Wextra\",\"-Werror\",\"-pedantic\","
      printf "\"-I%s/src\",\"-fsyntax-only\",\"%s\"],", directory, scanner
      printf "\"file\":\"%s\"},", scanner
      printf "{\"directory\":\"%s\",", directory
      printf "\"arguments\":[\"%s\",\"-std=c11\",", compiler
      # The contract includes scanner.c so it can exercise private helpers.
      # clangd otherwise counts its non-main-file static helpers toward the
      # diagnostic limit; the real contract build below still uses -Werror.
      printf "\"-Wall\",\"-Wextra\",\"-Werror\",\"-pedantic\",\"-Wno-unused-function\","
      printf "\"-I%s/src\",\"-fsyntax-only\",\"%s\"],", directory, contract
      printf "\"file\":\"%s\"}]\n", contract
    }
  ' >"$compile_commands"

"$clangd" \
  --log=error \
  --compile-commands-dir="$scanner_check_directory" \
  --check="$scanner_source"
"$clangd" \
  --log=error \
  --compile-commands-dir="$scanner_check_directory" \
  --check="$scanner_contract_source"

"$clang" \
  -std=c11 \
  -Wall \
  -Wextra \
  -Werror \
  -pedantic \
  -I"$repository_directory/src" \
  "$scanner_contract_source" \
  -o "$scanner_contract"
"$scanner_contract"

"$clang" \
  -std=c11 \
  -Wall \
  -Wextra \
  -Werror \
  -pedantic \
  -DTREE_SITTER_REUSE_ALLOCATOR \
  -I"$repository_directory/src" \
  "$scanner_contract_source" \
  -o "$scanner_reuse_contract"
"$scanner_reuse_contract"

"$clang" \
  -std=c11 \
  -Wall \
  -Wextra \
  -Werror \
  -pedantic \
  -DTREE_SITTER_REUSE_ALLOCATOR \
  -I"$repository_directory/src" \
  -c \
  "$scanner_source" \
  -o "$scanner_reuse_object"

if nm -u "$scanner_reuse_object" |
  grep -Eq '(^|[[:space:]])_?(malloc|calloc|realloc|free)$'; then
  printf '%s\n' "Scanner bypasses the Tree-sitter allocator" >&2
  exit 1
fi
if ! nm -u "$scanner_reuse_object" |
  grep -Eq '(^|[[:space:]])_?ts_current_(malloc|calloc|realloc|free)$'; then
  printf '%s\n' "Scanner does not reference the Tree-sitter allocator" >&2
  exit 1
fi
