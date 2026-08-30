#include <stddef.h>
#include <tree_sitter/tree-sitter-sh.h>

int main(void) {
  return tree_sitter_sh() == NULL;
}
