#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../src/scanner.c"

static struct HereDocument make_document(
  const char *delimiter,
  uint32_t source_end_column,
  bool quoted,
  bool strip_tabs
) {
  size_t length = strlen(delimiter);
  char *copy = malloc(length);
  assert(copy != NULL);
  memcpy(copy, delimiter, length);
  return (struct HereDocument){
    .delimiter = copy,
    .delimiter_length = length,
    .source_end_column = source_end_column,
    .quoted = quoted,
    .strip_tabs = strip_tabs,
  };
}

static struct HereDocument
make_repeated_document(size_t length, uint32_t source_end_column) {
  char *delimiter = malloc(length);
  assert(delimiter != NULL);
  memset(delimiter, 'D', length);
  return (struct HereDocument){
    .delimiter = delimiter,
    .delimiter_length = length,
    .source_end_column = source_end_column,
    .quoted = true,
    .strip_tabs = true,
  };
}

static void assert_document(
  const struct HereDocument *document,
  const char *delimiter,
  uint32_t source_end_column,
  bool quoted,
  bool strip_tabs
) {
  size_t length = strlen(delimiter);
  assert(document->delimiter_length == length);
  assert(memcmp(document->delimiter, delimiter, length) == 0);
  assert(document->source_end_column == source_end_column);
  assert(document->quoted == quoted);
  assert(document->strip_tabs == strip_tabs);
}

static void assert_all_valid_scan_preserves_state(void) {
  struct Scanner *scanner = tree_sitter_posix_sh_external_scanner_create();
  assert(scanner != NULL);

  scanner->sequence_end_pending = true;
  scanner->backquote_depth = 3;

  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length =
    tree_sitter_posix_sh_external_scanner_serialize(scanner, before);
  assert(before_length > 0);

  bool valid_symbols[TOKEN_COUNT];
  for (size_t index = 0; index < TOKEN_COUNT; index += 1) {
    valid_symbols[index] = true;
  }

  assert(
    !tree_sitter_posix_sh_external_scanner_scan(scanner, NULL, valid_symbols)
  );

  char after[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned after_length =
    tree_sitter_posix_sh_external_scanner_serialize(scanner, after);
  assert(after_length == before_length);
  assert(memcmp(after, before, before_length) == 0);

  tree_sitter_posix_sh_external_scanner_destroy(scanner);
}

static void assert_inline_state_round_trip(void) {
  struct Scanner *scanner = tree_sitter_posix_sh_external_scanner_create();
  struct Scanner *restored = tree_sitter_posix_sh_external_scanner_create();
  assert(scanner != NULL);
  assert(restored != NULL);

  scanner->expecting_delimiter = true;
  scanner->delimiter_strips_tabs = true;
  scanner->sequence_end_pending = true;
  scanner->at_here_document_line_start = true;
  scanner->backquote_depth = 7;

  assert(append_captured_document(
    scanner,
    make_document("captured", 11, true, false)
  ));
  assert(
    append_captured_document(scanner, make_document("second", 19, false, true))
  );
  assert(
    append_pending_document(scanner, make_document("pending", 23, false, false))
  );
  assert(append_document(
    &scanner->active_documents,
    &scanner->active_count,
    make_document("active", 29, true, true)
  ));

  scanner->suspended_frames = calloc(1, sizeof(struct HereDocumentFrame));
  assert(scanner->suspended_frames != NULL);
  scanner->suspended_frame_count = 1;
  scanner->suspended_frames[0].at_line_start = true;
  assert(append_document(
    &scanner->suspended_frames[0].documents,
    &scanner->suspended_frames[0].count,
    make_document("suspended", 31, false, true)
  ));

  char serialized[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned length =
    tree_sitter_posix_sh_external_scanner_serialize(scanner, serialized);
  assert(length > 2);
  assert((uint8_t)serialized[0] == SCANNER_SERIALIZATION_VERSION);
  assert((uint8_t)serialized[1] == INLINE_SCANNER_STATE);

  tree_sitter_posix_sh_external_scanner_deserialize(
    restored,
    serialized,
    length
  );

  assert(restored->expecting_delimiter);
  assert(restored->delimiter_strips_tabs);
  assert(restored->sequence_end_pending);
  assert(restored->at_here_document_line_start);
  assert(restored->backquote_depth == 7);
  assert(restored->captured_count == 2);
  assert_document(
    &restored->captured_documents[0],
    "captured",
    11,
    true,
    false
  );
  assert_document(&restored->captured_documents[1], "second", 19, false, true);
  assert(restored->pending_count == 1);
  assert_document(&restored->pending_documents[0], "pending", 23, false, false);
  assert(restored->active_count == 1);
  assert_document(&restored->active_documents[0], "active", 29, true, true);
  assert(restored->suspended_frame_count == 1);
  assert(restored->suspended_frames[0].at_line_start);
  assert(restored->suspended_frames[0].count == 1);
  assert_document(
    &restored->suspended_frames[0].documents[0],
    "suspended",
    31,
    false,
    true
  );

  tree_sitter_posix_sh_external_scanner_destroy(restored);
  tree_sitter_posix_sh_external_scanner_destroy(scanner);
}

static void assert_oversized_state_round_trip(void) {
  struct Scanner *scanner = tree_sitter_posix_sh_external_scanner_create();
  struct Scanner *restored = tree_sitter_posix_sh_external_scanner_create();
  assert(scanner != NULL);
  assert(restored != NULL);

  assert(append_captured_document(scanner, make_repeated_document(2048, 4096)));

  char serialized[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned length =
    tree_sitter_posix_sh_external_scanner_serialize(scanner, serialized);
  assert(length == 10);
  assert((uint8_t)serialized[0] == SCANNER_SERIALIZATION_VERSION);
  assert((uint8_t)serialized[1] == INTERNED_SCANNER_STATE);

  tree_sitter_posix_sh_external_scanner_deserialize(
    restored,
    serialized,
    length
  );

  assert(restored->captured_count == 1);
  const struct HereDocument *document = &restored->captured_documents[0];
  assert(document->delimiter_length == 2048);
  assert(document->source_end_column == 4096);
  assert(document->quoted);
  assert(document->strip_tabs);
  for (size_t index = 0; index < document->delimiter_length; index += 1) {
    assert(document->delimiter[index] == 'D');
  }

  tree_sitter_posix_sh_external_scanner_destroy(restored);
  tree_sitter_posix_sh_external_scanner_destroy(scanner);
}

int main(void) {
  assert_all_valid_scan_preserves_state();
  assert_inline_state_round_trip();
  assert_oversized_state_round_trip();
  return 0;
}
