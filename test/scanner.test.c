#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../src/scanner.c"

#ifdef TREE_SITTER_REUSE_ALLOCATOR
static size_t reuse_malloc_calls;
static size_t reuse_calloc_calls;
static size_t reuse_realloc_calls;
static size_t reuse_free_calls;
static size_t reuse_live_allocations;
static bool reuse_fail_next_calloc;
static bool reuse_fail_next_realloc;

static void *reuse_malloc(size_t size) {
  reuse_malloc_calls += 1;
  void *result = malloc(size);
  if (result != NULL) {
    reuse_live_allocations += 1;
  }
  return result;
}

static void *reuse_calloc(size_t count, size_t size) {
  reuse_calloc_calls += 1;
  if (reuse_fail_next_calloc) {
    reuse_fail_next_calloc = false;
    return NULL;
  }

  void *result = calloc(count, size);
  if (result != NULL) {
    reuse_live_allocations += 1;
  }
  return result;
}

static void *reuse_realloc(void *allocation, size_t size) {
  reuse_realloc_calls += 1;
  if (reuse_fail_next_realloc) {
    reuse_fail_next_realloc = false;
    return NULL;
  }

  if (allocation == NULL) {
    void *result = malloc(size);
    if (result != NULL) {
      reuse_live_allocations += 1;
    }
    return result;
  }
  if (size == 0) {
    free(allocation);
    assert(reuse_live_allocations > 0);
    reuse_live_allocations -= 1;
    return NULL;
  }
  return realloc(allocation, size);
}

static void reuse_free(void *allocation) {
  reuse_free_calls += 1;
  if (allocation != NULL) {
    assert(reuse_live_allocations > 0);
    reuse_live_allocations -= 1;
  }
  free(allocation);
}

void *(*ts_current_malloc)(size_t) = reuse_malloc;
void *(*ts_current_calloc)(size_t, size_t) = reuse_calloc;
void *(*ts_current_realloc)(void *, size_t) = reuse_realloc;
void (*ts_current_free)(void *) = reuse_free;
#endif

struct MockLexer {
  TSLexer lexer;
  const int32_t *input;
  size_t length;
  size_t offset;
  size_t mark;
  uint32_t column;
};

static void mock_advance(TSLexer *lexer, bool skip) {
  (void)skip;
  struct MockLexer *mock = (struct MockLexer *)lexer;
  if (mock->offset >= mock->length) {
    return;
  }

  if (mock->input[mock->offset] == '\n') {
    mock->column = 0;
  } else {
    mock->column += 1;
  }
  mock->offset += 1;
  lexer->lookahead =
    mock->offset < mock->length ? mock->input[mock->offset] : 0;
}

static void mock_mark_end(TSLexer *lexer) {
  struct MockLexer *mock = (struct MockLexer *)lexer;
  mock->mark = mock->offset;
}

static uint32_t mock_get_column(TSLexer *lexer) {
  return ((struct MockLexer *)lexer)->column;
}

static bool mock_eof(const TSLexer *lexer) {
  const struct MockLexer *mock = (const struct MockLexer *)lexer;
  return mock->offset >= mock->length;
}

static void
init_mock_lexer(struct MockLexer *mock, const int32_t *input, size_t length) {
  *mock = (struct MockLexer){
    .lexer =
      {
        .lookahead = length == 0 ? 0 : input[0],
        .advance = mock_advance,
        .mark_end = mock_mark_end,
        .get_column = mock_get_column,
        .eof = mock_eof,
      },
    .input = input,
    .length = length,
  };
}

static void assert_scan_result(
  struct Scanner *scanner,
  const bool *valid_symbols,
  const int32_t *input,
  size_t length,
  bool expected_success,
  TSSymbol expected_symbol,
  size_t expected_mark,
  size_t expected_offset,
  int32_t expected_lookahead
) {
  struct MockLexer mock;
  init_mock_lexer(&mock, input, length);
  bool success =
    tree_sitter_sh_external_scanner_scan(scanner, &mock.lexer, valid_symbols);
  assert(success == expected_success);
  if (success) {
    assert(mock.lexer.result_symbol == expected_symbol);
  }
  assert(mock.mark == expected_mark);
  assert(mock.offset == expected_offset);
  assert(mock.lexer.lookahead == expected_lookahead);
}

static struct HereDocument make_document_bytes(
  const uint8_t *delimiter,
  size_t length,
  bool quoted,
  bool strip_tabs
) {
  uint8_t *copy = NULL;
  if (length > 0) {
    copy = ts_malloc(length);
    assert(copy != NULL);
    memcpy(copy, delimiter, length);
  }
  return (struct HereDocument){
    .delimiter = copy,
    .delimiter_length = length,
    .quoted = quoted,
    .strip_tabs = strip_tabs,
  };
}

static struct HereDocument
make_document(const char *delimiter, bool quoted, bool strip_tabs) {
  return make_document_bytes(
    (const uint8_t *)delimiter,
    strlen(delimiter),
    quoted,
    strip_tabs
  );
}

static struct HereDocument make_repeated_document(size_t length) {
  uint8_t *delimiter = ts_malloc(length);
  assert(delimiter != NULL);
  memset(delimiter, 'D', length);
  return (struct HereDocument){
    .delimiter = delimiter,
    .delimiter_length = length,
    .quoted = true,
    .strip_tabs = true,
  };
}

static void assert_document(
  const struct HereDocument *document,
  const char *delimiter,
  bool quoted,
  bool strip_tabs
) {
  size_t length = strlen(delimiter);
  assert(document->delimiter_length == length);
  assert(memcmp(document->delimiter, delimiter, length) == 0);
  assert(document->quoted == quoted);
  assert(document->strip_tabs == strip_tabs);
}

static void assert_document_bytes(
  const struct HereDocument *document,
  const uint8_t *delimiter,
  size_t delimiter_length,
  bool quoted
) {
  assert(document->delimiter_length == delimiter_length);
  assert(memcmp(document->delimiter, delimiter, delimiter_length) == 0);
  assert(document->quoted == quoted);
  assert(!document->strip_tabs);
}

static void assert_repeated_document(
  const struct HereDocument *document,
  uint8_t delimiter,
  size_t delimiter_length
) {
  assert(document->delimiter_length == delimiter_length);
  for (size_t index = 0; index < delimiter_length; index += 1) {
    assert(document->delimiter[index] == delimiter);
  }
  assert(document->quoted);
  assert(document->strip_tabs);
}

static unsigned snapshot_scanner(
  const struct Scanner *scanner,
  char buffer[TREE_SITTER_SERIALIZATION_BUFFER_SIZE]
) {
  unsigned length =
    tree_sitter_sh_external_scanner_serialize((void *)scanner, buffer);
  assert(length > 0);
  return length;
}

static void assert_scanner_matches_snapshot(
  const struct Scanner *scanner,
  const char expected[TREE_SITTER_SERIALIZATION_BUFFER_SIZE],
  unsigned expected_length
) {
  char actual[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned actual_length = snapshot_scanner(scanner, actual);
  assert(actual_length == expected_length);
  assert(memcmp(actual, expected, expected_length) == 0);
}

static struct Scanner *make_exact_fit_scanner(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  assert(append_captured_document(scanner, make_repeated_document(1010)));
  return scanner;
}

static void assert_all_valid_scan_preserves_state(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);

  scanner->sequence_end_pending = true;
  scanner->backquote_depth = 3;

  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(scanner, before);

  bool valid_symbols[TOKEN_COUNT];
  for (size_t index = 0; index < TOKEN_COUNT; index += 1) {
    valid_symbols[index] = true;
  }

  assert(!tree_sitter_sh_external_scanner_scan(scanner, NULL, valid_symbols));
  assert_scanner_matches_snapshot(scanner, before, before_length);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_state_round_trip(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  struct Scanner *restored = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  assert(restored != NULL);

  scanner->expecting_delimiter = true;
  scanner->delimiter_strips_tabs = true;
  scanner->sequence_end_pending = true;
  scanner->at_here_document_line_start = true;
  scanner->backquote_depth = 7;
  scanner->substitution_depth = 5;
  scanner->body_substitution_depth = 2;
  scanner->body_backquote_depth = 3;

  assert(
    append_captured_document(scanner, make_document("captured", true, false))
  );
  assert(
    append_captured_document(scanner, make_document("second", false, true))
  );
  assert(
    append_pending_document(scanner, make_document("pending", false, false))
  );
  assert(append_document(
    &scanner->active_documents,
    &scanner->active_count,
    make_document("active", true, true)
  ));

  scanner->suspended_frames = ts_calloc(1, sizeof(struct HereDocumentFrame));
  assert(scanner->suspended_frames != NULL);
  scanner->suspended_frame_count = 1;
  scanner->suspended_frames[0].at_line_start = true;
  scanner->suspended_frames[0].body_substitution_depth = 4;
  scanner->suspended_frames[0].body_backquote_depth = 6;
  assert(append_document(
    &scanner->suspended_frames[0].documents,
    &scanner->suspended_frames[0].count,
    make_document("suspended", false, true)
  ));
  scanner->pending_documents[0].declaration_depth = 9;

  char serialized[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned length = snapshot_scanner(scanner, serialized);
  assert((uint8_t)serialized[0] == SCANNER_SERIALIZATION_VERSION);

  tree_sitter_sh_external_scanner_deserialize(restored, serialized, length);

  assert(restored->expecting_delimiter);
  assert(restored->delimiter_strips_tabs);
  assert(restored->sequence_end_pending);
  assert(restored->at_here_document_line_start);
  assert(restored->backquote_depth == 7);
  assert(restored->substitution_depth == 5);
  assert(restored->body_substitution_depth == 2);
  assert(restored->body_backquote_depth == 3);
  assert(restored->captured_count == 2);
  assert_document(&restored->captured_documents[0], "captured", true, false);
  assert(restored->captured_documents[0].declaration_depth == 12);
  assert(restored->captured_documents[1].declaration_depth == 12);
  assert_document(&restored->captured_documents[1], "second", false, true);
  assert(restored->pending_count == 1);
  assert_document(&restored->pending_documents[0], "pending", false, false);
  assert(restored->pending_documents[0].declaration_depth == 9);
  assert(restored->active_count == 1);
  assert_document(&restored->active_documents[0], "active", true, true);
  assert(restored->suspended_frame_count == 1);
  assert(restored->suspended_frames[0].at_line_start);
  assert(restored->suspended_frames[0].body_substitution_depth == 4);
  assert(restored->suspended_frames[0].body_backquote_depth == 6);
  assert(restored->suspended_frames[0].count == 1);
  assert_document(
    &restored->suspended_frames[0].documents[0],
    "suspended",
    false,
    true
  );

  tree_sitter_sh_external_scanner_destroy(restored);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_old_state_is_rejected(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  struct Scanner *restored = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  assert(restored != NULL);

  scanner->backquote_depth = 4;
  char serialized[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned length = snapshot_scanner(scanner, serialized);
  serialized[0] = 11;

  restored->expecting_delimiter = true;
  restored->backquote_depth = 9;
  tree_sitter_sh_external_scanner_deserialize(restored, serialized, length);
  assert(!restored->expecting_delimiter);
  assert(restored->backquote_depth == 0);
  assert(restored->captured_count == 0);

  tree_sitter_sh_external_scanner_destroy(restored);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_exact_fit_state_round_trip(void) {
  struct Scanner *scanner = make_exact_fit_scanner();
  struct Scanner *restored = tree_sitter_sh_external_scanner_create();
  assert(restored != NULL);

  char serialized[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned length = snapshot_scanner(scanner, serialized);
  assert(length == TREE_SITTER_SERIALIZATION_BUFFER_SIZE);

  tree_sitter_sh_external_scanner_deserialize(restored, serialized, length);
  assert(restored->captured_count == 1);
  assert_repeated_document(&restored->captured_documents[0], 'D', 1010);
  assert_scanner_matches_snapshot(restored, serialized, length);

  tree_sitter_sh_external_scanner_destroy(restored);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_delimiter_capture_rejects_oversized_state(void) {
  struct Scanner *scanner = make_exact_fit_scanner();
  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(scanner, before);

  struct HereDocument rejected = make_document("x", false, false);
  assert(!append_captured_document(scanner, rejected));
  assert_scanner_matches_snapshot(scanner, before, before_length);

  clear_document(&rejected);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_captured_to_pending_move_is_capacity_guarded(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  assert(append_captured_document(scanner, make_document("last", true, false)));
  for (size_t index = 0; index < 127; index += 1) {
    assert(append_pending_document(scanner, make_document("x", false, false)));
  }

  assert(move_captured_document_to_pending(scanner));
  assert(scanner->captured_count == 0);
  assert(scanner->pending_count == 128);
  assert_document(&scanner->pending_documents[127], "last", true, false);
  clear_scanner(scanner);

  // Exact fit: 5 header bytes, the captured document (1 flag, 1 depth,
  // 2 length, 629 delimiter), 127 empty pending documents (3 bytes each
  // behind a 1-byte count), and the active and suspended counts reach the
  // 1023-byte capacity. Moving the captured document widens the pending
  // count varint past the freed captured count byte, so the move refuses
  // and leaves the state untouched.
  assert(append_captured_document(scanner, make_repeated_document(629)));
  for (size_t index = 0; index < 127; index += 1) {
    assert(append_pending_document(scanner, make_document("", false, false)));
  }

  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(scanner, before);
  assert(before_length == TREE_SITTER_SERIALIZATION_BUFFER_SIZE);
  assert(!move_captured_document_to_pending(scanner));
  assert(scanner->captured_count == 1);
  assert(scanner->pending_count == 127);
  assert_scanner_matches_snapshot(scanner, before, before_length);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_active_suspend_rejects_oversized_state(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);

  scanner->at_here_document_line_start = true;
  assert(append_document(
    &scanner->active_documents,
    &scanner->active_count,
    make_repeated_document(1011)
  ));

  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(scanner, before);
  assert(before_length == TREE_SITTER_SERIALIZATION_BUFFER_SIZE);
  assert(!suspend_active_documents(scanner));
  assert_scanner_matches_snapshot(scanner, before, before_length);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_backquote_growth_rejects_oversized_state(void) {
  struct Scanner *scanner = make_exact_fit_scanner();
  scanner->backquote_depth = 127;

  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(scanner, before);
  assert(!increase_backquote_depth(scanner));
  assert_scanner_matches_snapshot(scanner, before, before_length);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_strict_scalar_encoding(void) {
  uint8_t bytes[4];
  size_t length = 0;
  assert(encode_utf8_scalar(0x7f, bytes, &length));
  assert(length == 1 && bytes[0] == 0x7f);
  assert(encode_utf8_scalar(0xff, bytes, &length));
  assert(length == 2 && bytes[0] == 0xc3 && bytes[1] == 0xbf);
  assert(encode_utf8_scalar(0x10ffff, bytes, &length));
  assert(length == 4);
  assert(!encode_utf8_scalar(-1, bytes, &length));
  assert(!encode_utf8_scalar(0xd800, bytes, &length));
  assert(!encode_utf8_scalar(0x110000, bytes, &length));

  struct ByteBuffer exact = {.limit = 1};
  assert(!append_codepoint(&exact, 0xff));
  assert(exact.length == 0);
  ts_free(exact.data);
}

static void assert_control_escape_table(void) {
  uint8_t value = 0;
  for (int32_t index = 0; index < 26; index += 1) {
    assert(defined_control_escape_byte('a' + index, &value));
    assert(value == (uint8_t)(index + 1));
    assert(control_escape_byte('a' + index, &value));
    assert(value == (uint8_t)(index + 1));
    assert(defined_control_escape_byte('A' + index, &value));
    assert(value == (uint8_t)(index + 1));
    assert(control_escape_byte('A' + index, &value));
    assert(value == (uint8_t)(index + 1));
  }

  const int32_t punctuation[] = {'[', '\\', ']', '^', '_', '?'};
  const uint8_t expected[] = {0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x7f};
  for (size_t index = 0; index < sizeof(expected); index += 1) {
    assert(defined_control_escape_byte(punctuation[index], &value));
    assert(value == expected[index]);
    assert(control_escape_byte(punctuation[index], &value));
    assert(value == expected[index]);
  }

  const int32_t unspecified[] = {'@', '`', '{', 0xff};
  const uint8_t fallback[] = {0x00, 0x00, 0x1b, 0x1f};
  for (size_t index = 0; index < sizeof(fallback); index += 1) {
    assert(!defined_control_escape_byte(unspecified[index], &value));
    assert(control_escape_byte(unspecified[index], &value));
    assert(value == fallback[index]);
  }
  assert(!control_escape_byte(-1, &value));
}

static bool line_matches_document(
  const uint8_t *delimiter,
  size_t delimiter_length,
  bool quoted,
  bool strip_tabs,
  const int32_t *line,
  size_t line_length
) {
  struct HereDocument document =
    make_document_bytes(delimiter, delimiter_length, quoted, strip_tabs);
  struct MockLexer mock;
  init_mock_lexer(&mock, line, line_length);
  bool result = scan_here_document_end_line(&mock.lexer, &document);
  clear_document(&document);
  return result;
}

static void assert_byte_delimiter_matching(void) {
  const uint8_t utf8_ff[] = {0xc3, 0xbf};
  const int32_t source_ff[] = {0xff, '\n'};
  assert(line_matches_document(
    utf8_ff,
    sizeof(utf8_ff),
    true,
    false,
    source_ff,
    sizeof(source_ff) / sizeof(source_ff[0])
  ));

  const uint8_t invalid_ff[] = {0xff};
  const int32_t bell[] = {0x07, '\n'};
  assert(!line_matches_document(
    invalid_ff,
    sizeof(invalid_ff),
    true,
    false,
    bell,
    sizeof(bell) / sizeof(bell[0])
  ));

  const uint8_t truncated[] = {0xc3};
  assert(!line_matches_document(
    truncated,
    sizeof(truncated),
    true,
    false,
    source_ff,
    sizeof(source_ff) / sizeof(source_ff[0])
  ));
  const uint8_t overlong[] = {0xc0, 0xaf};
  const int32_t slash[] = {'/', '\n'};
  assert(!line_matches_document(
    overlong,
    sizeof(overlong),
    true,
    false,
    slash,
    sizeof(slash) / sizeof(slash[0])
  ));
  const uint8_t nul[] = {0};
  assert(!line_matches_document(nul, sizeof(nul), true, false, NULL, 0));
  const int32_t empty_line[] = {'\n'};
  assert(line_matches_document(NULL, 0, true, false, empty_line, 1));

  const uint8_t del[] = {0x7f};
  const int32_t del_line[] = {0x7f, '\n'};
  const int32_t unit_separator_line[] = {0x1f, '\n'};
  assert(line_matches_document(del, sizeof(del), true, false, del_line, 2));
  assert(!line_matches_document(
    del,
    sizeof(del),
    true,
    false,
    unit_separator_line,
    2
  ));

  const uint8_t joined[] = {'A', 'B'};
  const int32_t joined_line[] = {'A', '\\', '\n', 'B', '\n'};
  assert(line_matches_document(
    joined,
    sizeof(joined),
    false,
    false,
    joined_line,
    sizeof(joined_line) / sizeof(joined_line[0])
  ));
  const int32_t continued_middle_tab[] = {'A', '\\', '\n', '\t', 'B', '\n'};
  assert(!line_matches_document(
    joined,
    sizeof(joined),
    false,
    true,
    continued_middle_tab,
    sizeof(continued_middle_tab) / sizeof(continued_middle_tab[0])
  ));
  const int32_t continued_leading_tab[] = {'\\', '\n', '\t', 'A', 'B', '\n'};
  assert(line_matches_document(
    joined,
    sizeof(joined),
    false,
    true,
    continued_leading_tab,
    sizeof(continued_leading_tab) / sizeof(continued_leading_tab[0])
  ));
  const uint8_t tabbed[] = {'X'};
  const int32_t tabbed_line[] = {'\t', '\t', 'X', '\n'};
  assert(line_matches_document(
    tabbed,
    sizeof(tabbed),
    true,
    true,
    tabbed_line,
    sizeof(tabbed_line) / sizeof(tabbed_line[0])
  ));

  const int32_t nul_suffix[] = {'A', 0, 'X', '\n'};
  const uint8_t a[] = {'A'};
  assert(!line_matches_document(
    a,
    sizeof(a),
    true,
    false,
    nul_suffix,
    sizeof(nul_suffix) / sizeof(nul_suffix[0])
  ));
}

static void assert_nested_here_document_logical_line_tabs(void) {
  struct HereDocument document = make_document("AB", false, true);
  const int32_t input[] = {'A', '\\', '\n', '\t', 'B', '\n'};
  const char expected_source[] = {'A', '\\', '\n', '\t', 'B', '\n'};
  struct MockLexer mock;
  init_mock_lexer(&mock, input, sizeof(input) / sizeof(input[0]));
  struct ByteBuffer source = {0};
  bool is_end = false;
  bool at_end_of_input = false;

  assert(scan_nested_here_document_line(
    &mock.lexer,
    &source,
    &document,
    &is_end,
    &at_end_of_input
  ));
  assert(!is_end);
  assert(!at_end_of_input);
  assert(source.length == sizeof(expected_source));
  assert(memcmp(source.data, expected_source, sizeof(expected_source)) == 0);

  ts_free(source.data);
  clear_document(&document);
}

static void assert_backquote_prefix_classification(void) {
  assert(
    classify_backquote_tick_prefix(1, 1, true, true) ==
    BACKQUOTE_TICK_PREFIX_START
  );
  assert(
    classify_backquote_tick_prefix(2, 1, true, true) ==
    BACKQUOTE_TICK_PREFIX_END
  );
  assert(
    classify_backquote_tick_prefix(2, 3, true, true) ==
    BACKQUOTE_TICK_PREFIX_START
  );
  assert(
    classify_backquote_tick_prefix(3, 3, true, true) ==
    BACKQUOTE_TICK_PREFIX_END
  );
  assert(
    classify_backquote_tick_prefix(3, 7, true, true) ==
    BACKQUOTE_TICK_PREFIX_START
  );
  assert(
    classify_backquote_tick_prefix(3, 5, true, true) ==
    BACKQUOTE_TICK_PREFIX_NONE
  );

  assert(
    classify_backquote_tick_prefix(
      sizeof(size_t) * CHAR_BIT + 1,
      SIZE_MAX,
      true,
      false
    ) == BACKQUOTE_TICK_PREFIX_NONE
  );

  struct BackquoteEscapeRunFold fold = fold_backquote_escape_run(1, 3);
  assert(fold.acting_level == 3 && fold.leftover_count == 0);
  fold = fold_backquote_escape_run(1, 5);
  assert(fold.acting_level == 2 && fold.leftover_count == 2);
  fold = fold_backquote_escape_run(2, 5);
  assert(fold.acting_level == 2 && fold.leftover_count == 2);
  fold = fold_backquote_escape_run(1, 7);
  assert(fold.acting_level == 3 && fold.leftover_count == 1);
}

static bool scan_delimiter_fixture(
  struct Scanner *scanner,
  const int32_t *input,
  size_t length
) {
  struct MockLexer mock;
  init_mock_lexer(&mock, input, length);
  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[LINE_CONTINUATION] = true;
  bool result =
    scan_here_document_delimiter(scanner, &mock.lexer, valid_symbols);
  if (result) {
    assert(mock.lexer.result_symbol == HERE_END_BEGIN);
  }
  return result;
}

static void assert_text_delimiter_fixture(
  struct Scanner *scanner,
  const char *input,
  const char *expected_delimiter,
  bool quoted
) {
  size_t length = strlen(input);
  assert(length > 0 && input[length - 1] == '\n');

  int32_t *characters = malloc(length * sizeof(int32_t));
  assert(characters != NULL);
  for (size_t index = 0; index < length; index += 1) {
    characters[index] = (unsigned char)input[index];
  }

  scanner->expecting_delimiter = true;
  assert(scan_delimiter_fixture(scanner, characters, length));
  assert(scanner->captured_count == 1);
  assert_document(
    &scanner->captured_documents[0],
    expected_delimiter,
    quoted,
    false
  );

  free(characters);
  clear_scanner(scanner);
}

static void assert_substitution_hash_delimiter_words(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);

  assert_text_delimiter_fixture(
    scanner,
    "`printf $x#tag END`\n",
    "`printf $x#tag END`",
    false
  );
  assert_text_delimiter_fixture(
    scanner,
    "`printf ${x}#tag END`\n",
    "`printf ${x}#tag END`",
    false
  );
  assert_text_delimiter_fixture(
    scanner,
    "`printf $(:)#tag END`\n",
    "`printf $(:)#tag END`",
    false
  );
  assert_text_delimiter_fixture(
    scanner,
    "`printf $((1))#tag END`\n",
    "`printf $((1))#tag END`",
    false
  );
  assert_text_delimiter_fixture(
    scanner,
    "`printf $'x'#tag END`\n",
    "`printf x#tag END`",
    true
  );
  assert_text_delimiter_fixture(
    scanner,
    "`printf \\`x\\`#tag END`\n",
    "`printf `x`#tag END`",
    true
  );

  assert_text_delimiter_fixture(
    scanner,
    "`printf ${x}\\\n#tag END`\n",
    "`printf ${x}#tag END`",
    false
  );
  assert_text_delimiter_fixture(
    scanner,
    "`printf $(:)\\\n#tag END`\n",
    "`printf $(:)#tag END`",
    false
  );
  assert_text_delimiter_fixture(
    scanner,
    "`printf $((1))\\\n#tag END`\n",
    "`printf $((1))#tag END`",
    false
  );
  assert_text_delimiter_fixture(
    scanner,
    "`printf \\`x\\`\\\n#tag END`\n",
    "`printf `x`#tag END`",
    true
  );

  assert_text_delimiter_fixture(
    scanner,
    "`ca\\\nse value in <<X) printf END;; esac`\n",
    "`case value in <<X) printf END;; esac`",
    false
  );

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_recursive_backquote_delimiters(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  scanner->expecting_delimiter = true;

  const int32_t quoted_input[] = {
    '`',
    'p',
    'r',
    'i',
    'n',
    't',
    'f',
    ' ',
    '\'',
    '%',
    's',
    '\'',
    ' ',
    'E',
    'N',
    'D',
    '`',
    '\n',
  };
  assert(scan_delimiter_fixture(
    scanner,
    quoted_input,
    sizeof(quoted_input) / sizeof(quoted_input[0])
  ));
  assert(scanner->backquote_depth == 0);
  assert(scanner->captured_count == 1);
  assert_document(
    &scanner->captured_documents[0],
    "`printf %s END`",
    true,
    false
  );
  clear_scanner(scanner);

  scanner->expecting_delimiter = true;
  const int32_t continued_input[] = {
    '`',
    'p',
    'r',
    'i',
    'n',
    't',
    'f',
    ' ',
    'E',
    'O',
    '\\',
    '\n',
    'F',
    '`',
    '\n',
  };
  assert(scan_delimiter_fixture(
    scanner,
    continued_input,
    sizeof(continued_input) / sizeof(continued_input[0])
  ));
  assert_document(
    &scanner->captured_documents[0],
    "`printf EOF`",
    false,
    false
  );
  clear_scanner(scanner);

  scanner->expecting_delimiter = true;
  const int32_t nested_input[] = {
    '`',
    'o',
    'n',
    'e',
    ' ',
    '\\',
    '`',
    't',
    'w',
    'o',
    '\\',
    '`',
    ' ',
    'e',
    'n',
    'd',
    '`',
    '\n',
  };
  assert(scan_delimiter_fixture(
    scanner,
    nested_input,
    sizeof(nested_input) / sizeof(nested_input[0])
  ));
  assert_document(
    &scanner->captured_documents[0],
    "`one `two` end`",
    true,
    false
  );
  clear_scanner(scanner);

  scanner->expecting_delimiter = true;
  const int32_t nested_here_document[] = {
    '`',
    'c',
    'a',
    't',
    ' ',
    '<',
    '<',
    'X',
    '\n',
    'b',
    'o',
    'd',
    'y',
    ' ',
    '`',
    '\n',
    'X',
    '\n',
    'p',
    'r',
    'i',
    'n',
    't',
    'f',
    ' ',
    'E',
    'N',
    'D',
    '`',
    '\n',
  };
  assert(scan_delimiter_fixture(
    scanner,
    nested_here_document,
    sizeof(nested_here_document) / sizeof(nested_here_document[0])
  ));
  const char expected_nested[] = "`cat <<X\nbody `\nX\nprintf END`";
  assert_document(
    &scanner->captured_documents[0],
    expected_nested,
    false,
    false
  );
  clear_scanner(scanner);

  scanner->expecting_delimiter = true;
  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(scanner, before);
  const int32_t incomplete_input[] = {
    '`',
    'o',
    'p',
    'e',
    'n',
    '\n',
  };
  assert(!scan_delimiter_fixture(
    scanner,
    incomplete_input,
    sizeof(incomplete_input) / sizeof(incomplete_input[0])
  ));
  assert_scanner_matches_snapshot(scanner, before, before_length);

  scanner->expecting_delimiter = true;
  scanner->backquote_depth = 1;
  const int32_t ambient_input[] = {
    '\\',
    '`',
    'x',
    '\\',
    '`',
    '\n',
  };
  assert(scan_delimiter_fixture(
    scanner,
    ambient_input,
    sizeof(ambient_input) / sizeof(ambient_input[0])
  ));
  assert(scanner->backquote_depth == 1);
  assert_document(&scanner->captured_documents[0], "`x`", true, false);

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_dollar_single_quote_delimiter_bytes(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  scanner->expecting_delimiter = true;

  const int32_t control_input[] = {
    '$',
    '\'',
    '\\',
    'c',
    '?',
    '\'',
    '\n',
  };
  assert(scan_delimiter_fixture(
    scanner,
    control_input,
    sizeof(control_input) / sizeof(control_input[0])
  ));
  const uint8_t del[] = {0x7f};
  assert_document_bytes(
    &scanner->captured_documents[0],
    del,
    sizeof(del),
    true
  );
  clear_scanner(scanner);

  scanner->expecting_delimiter = true;
  const int32_t utf8_input[] = {
    '$',
    '\'',
    '\\',
    'x',
    'C',
    '3',
    '\\',
    'x',
    'B',
    'F',
    '\'',
    '\n',
  };
  assert(scan_delimiter_fixture(
    scanner,
    utf8_input,
    sizeof(utf8_input) / sizeof(utf8_input[0])
  ));
  const uint8_t utf8_ff[] = {0xc3, 0xbf};
  assert_document_bytes(
    &scanner->captured_documents[0],
    utf8_ff,
    sizeof(utf8_ff),
    true
  );
  clear_scanner(scanner);

  scanner->expecting_delimiter = true;
  const int32_t invalid_input[] = {
    '$',
    '\'',
    '\\',
    'x',
    'F',
    'F',
    '\'',
    '\n',
  };
  assert(scan_delimiter_fixture(
    scanner,
    invalid_input,
    sizeof(invalid_input) / sizeof(invalid_input[0])
  ));
  const uint8_t invalid_ff[] = {0xff};
  assert_document_bytes(
    &scanner->captured_documents[0],
    invalid_ff,
    sizeof(invalid_ff),
    true
  );
  clear_scanner(scanner);

  scanner->expecting_delimiter = true;
  const int32_t unspecified_control_input[] = {
    '$',
    '\'',
    '\\',
    'c',
    '@',
    '\\',
    'c',
    '`',
    '\\',
    'c',
    '{',
    '\\',
    'c',
    0xff,
    '\'',
    '\n',
  };
  assert(scan_delimiter_fixture(
    scanner,
    unspecified_control_input,
    sizeof(unspecified_control_input) / sizeof(unspecified_control_input[0])
  ));
  const uint8_t unspecified_fallback[] = {0x00, 0x00, 0x1b, 0x1f};
  assert_document_bytes(
    &scanner->captured_documents[0],
    unspecified_fallback,
    sizeof(unspecified_fallback),
    true
  );

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_generic_line_continuation_contract(void) {
  const int32_t input[] = {'\\', '\n', 'x'};
  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[LINE_CONTINUATION] = true;

  struct MockLexer continuation;
  init_mock_lexer(&continuation, input, sizeof(input) / sizeof(input[0]));
  assert(scan_line_continuation(&continuation.lexer, valid_symbols));
  assert(continuation.lexer.result_symbol == LINE_CONTINUATION);
  assert(continuation.mark == 2);
  assert(continuation.lexer.lookahead == 'x');
}

static void assert_boundary_line_continuation_contract(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);

  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[WORD_SEPARATOR_BEGIN] = true;

  const int32_t repeated_input[] = {
    '\\',
    '\n',
    '\\',
    '\n',
    '\\',
    '\n',
    'x',
  };
  struct MockLexer classified;
  init_mock_lexer(
    &classified,
    repeated_input,
    sizeof(repeated_input) / sizeof(repeated_input[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &classified.lexer,
    valid_symbols
  ));
  assert(classified.lexer.result_symbol == WORD_SEPARATOR_BEGIN);
  assert(classified.mark == 0);

  valid_symbols[LINE_CONTINUATION] = true;
  struct MockLexer owned;
  init_mock_lexer(
    &owned,
    repeated_input,
    sizeof(repeated_input) / sizeof(repeated_input[0])
  );
  assert(
    tree_sitter_sh_external_scanner_scan(scanner, &owned.lexer, valid_symbols)
  );
  assert(owned.lexer.result_symbol == LINE_CONTINUATION);
  assert(owned.mark == 2);
  valid_symbols[LINE_CONTINUATION] = false;

  bool continuation_symbols[TOKEN_COUNT] = {false};
  continuation_symbols[LINE_CONTINUATION] = true;
  for (size_t index = 0; index < 3; index += 1) {
    struct MockLexer continuation;
    init_mock_lexer(
      &continuation,
      repeated_input + index * 2,
      sizeof(repeated_input) / sizeof(repeated_input[0]) - index * 2
    );
    assert(tree_sitter_sh_external_scanner_scan(
      scanner,
      &continuation.lexer,
      continuation_symbols
    ));
    assert(continuation.lexer.result_symbol == LINE_CONTINUATION);
    assert(continuation.mark == 2);
    assert(continuation.offset == 2);
    assert(continuation.lexer.lookahead == (index < 2 ? '\\' : 'x'));
  }

  const int32_t blank_follower_input[] = {'\\', '\n', ' ', 'x'};
  struct MockLexer pair_before_blank;
  init_mock_lexer(
    &pair_before_blank,
    blank_follower_input,
    sizeof(blank_follower_input) / sizeof(blank_follower_input[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &pair_before_blank.lexer,
    valid_symbols
  ));
  assert(pair_before_blank.lexer.result_symbol == WORD_SEPARATOR_BEGIN);
  assert(pair_before_blank.mark == 0);

  const int32_t wrong_follower_input[] = {'\\', 'x'};
  struct MockLexer wrong_follower;
  init_mock_lexer(
    &wrong_follower,
    wrong_follower_input,
    sizeof(wrong_follower_input) / sizeof(wrong_follower_input[0])
  );
  assert(!tree_sitter_sh_external_scanner_scan(
    scanner,
    &wrong_follower.lexer,
    valid_symbols
  ));
  assert(wrong_follower.mark == 0);
  assert(wrong_follower.offset == 1);
  assert(wrong_follower.lexer.lookahead == 'x');

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_word_separator_classification_contract(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);

  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[LINE_CONTINUATION] = true;
  valid_symbols[WORD_SEPARATOR_BEGIN] = true;
  valid_symbols[ASSIGNMENT_SEPARATOR_BEGIN] = true;
  valid_symbols[REDIRECT_SEPARATOR_BEGIN] = true;

  const int32_t word_input[] = {' ', 's'};
  struct MockLexer word_separator;
  init_mock_lexer(
    &word_separator,
    word_input,
    sizeof(word_input) / sizeof(word_input[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &word_separator.lexer,
    valid_symbols
  ));
  assert(word_separator.lexer.result_symbol == WORD_SEPARATOR_BEGIN);
  assert(word_separator.mark == 1);
  assert(word_separator.lexer.lookahead == 0);

  const int32_t assignment_input[] = {' ', 'a', '='};
  struct MockLexer assignment_separator;
  init_mock_lexer(
    &assignment_separator,
    assignment_input,
    sizeof(assignment_input) / sizeof(assignment_input[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &assignment_separator.lexer,
    valid_symbols
  ));
  assert(
    assignment_separator.lexer.result_symbol == ASSIGNMENT_SEPARATOR_BEGIN
  );
  assert(assignment_separator.mark == 1);
  assert(assignment_separator.lexer.lookahead == '=');

  const int32_t redirect_input[] = {' ', '\t', '<'};
  struct MockLexer redirect_separator;
  init_mock_lexer(
    &redirect_separator,
    redirect_input,
    sizeof(redirect_input) / sizeof(redirect_input[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &redirect_separator.lexer,
    valid_symbols
  ));
  assert(redirect_separator.lexer.result_symbol == REDIRECT_SEPARATOR_BEGIN);
  assert(redirect_separator.mark == 2);
  assert(redirect_separator.lexer.lookahead == '<');

  const int32_t descriptor_redirect_input[] = {' ', '2', '>'};
  struct MockLexer descriptor_redirect;
  init_mock_lexer(
    &descriptor_redirect,
    descriptor_redirect_input,
    sizeof(descriptor_redirect_input) / sizeof(descriptor_redirect_input[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &descriptor_redirect.lexer,
    valid_symbols
  ));
  assert(descriptor_redirect.lexer.result_symbol == REDIRECT_SEPARATOR_BEGIN);
  assert(descriptor_redirect.mark == 1);
  assert(descriptor_redirect.lexer.lookahead == '>');

  bool word_only_symbols[TOKEN_COUNT] = {false};
  word_only_symbols[WORD_SEPARATOR_BEGIN] = true;
  const int32_t assignment_word_input[] = {' ', 'a', '=', 'b'};
  struct MockLexer assignment_as_word;
  init_mock_lexer(
    &assignment_as_word,
    assignment_word_input,
    sizeof(assignment_word_input) / sizeof(assignment_word_input[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &assignment_as_word.lexer,
    word_only_symbols
  ));
  assert(assignment_as_word.lexer.result_symbol == WORD_SEPARATOR_BEGIN);
  assert(assignment_as_word.mark == 1);
  assert(assignment_as_word.lexer.lookahead == '=');

  const int32_t pair_after_blank_input[] = {' ', '\\', '\n', 's'};
  struct MockLexer pair_after_blank;
  init_mock_lexer(
    &pair_after_blank,
    pair_after_blank_input,
    sizeof(pair_after_blank_input) / sizeof(pair_after_blank_input[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &pair_after_blank.lexer,
    valid_symbols
  ));
  assert(pair_after_blank.lexer.result_symbol == WORD_SEPARATOR_BEGIN);
  assert(pair_after_blank.mark == 1);
  assert(pair_after_blank.offset == 4);
  assert(pair_after_blank.lexer.lookahead == 0);

  const int32_t location_redirect_input[] =
    {' ', '{', 'x', '}', '\\', '\n', '>'};
  struct MockLexer location_redirect;
  init_mock_lexer(
    &location_redirect,
    location_redirect_input,
    sizeof(location_redirect_input) / sizeof(location_redirect_input[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &location_redirect.lexer,
    valid_symbols
  ));
  assert(location_redirect.lexer.result_symbol == REDIRECT_SEPARATOR_BEGIN);
  assert(location_redirect.mark == 1);
  assert(location_redirect.offset == 6);
  assert(location_redirect.lexer.lookahead == '>');

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_backquote_prefix_scanner_contract(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  scanner->backquote_depth = 1;

  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[LINE_CONTINUATION] = true;
  valid_symbols[WORD_SEPARATOR_BEGIN] = true;
  valid_symbols[BACKQUOTE_START_PREFIX] = true;

  const int32_t continuation_input[] = {'\\', '\n', 'x'};
  struct MockLexer continuation;
  init_mock_lexer(
    &continuation,
    continuation_input,
    sizeof(continuation_input) / sizeof(continuation_input[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &continuation.lexer,
    valid_symbols
  ));
  assert(continuation.lexer.result_symbol == LINE_CONTINUATION);
  assert(continuation.mark == 2);
  assert(continuation.lexer.lookahead == 'x');
  assert(scanner->backquote_depth == 1);

  bool separator_symbols[TOKEN_COUNT] = {false};
  separator_symbols[WORD_SEPARATOR_BEGIN] = true;
  separator_symbols[BACKQUOTE_START_PREFIX] = true;
  struct MockLexer classified_pair;
  init_mock_lexer(
    &classified_pair,
    continuation_input,
    sizeof(continuation_input) / sizeof(continuation_input[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &classified_pair.lexer,
    separator_symbols
  ));
  assert(classified_pair.lexer.result_symbol == WORD_SEPARATOR_BEGIN);
  assert(classified_pair.mark == 0);
  assert(scanner->backquote_depth == 1);

  const int32_t nested_input[] = {'\\', '`'};
  struct MockLexer nested;
  init_mock_lexer(
    &nested,
    nested_input,
    sizeof(nested_input) / sizeof(nested_input[0])
  );
  assert(
    tree_sitter_sh_external_scanner_scan(scanner, &nested.lexer, valid_symbols)
  );
  assert(nested.lexer.result_symbol == BACKQUOTE_START_PREFIX);
  assert(nested.mark == 1);
  assert(scanner->backquote_depth == 2);

  scanner->backquote_depth = 2;
  memset(valid_symbols, 0, sizeof(valid_symbols));
  valid_symbols[BACKQUOTE_START_PREFIX] = true;
  struct MockLexer closer_boundary;
  init_mock_lexer(
    &closer_boundary,
    nested_input,
    sizeof(nested_input) / sizeof(nested_input[0])
  );
  assert(!tree_sitter_sh_external_scanner_scan(
    scanner,
    &closer_boundary.lexer,
    valid_symbols
  ));
  assert(scanner->backquote_depth == 2);

  memset(valid_symbols, 0, sizeof(valid_symbols));
  valid_symbols[BACKQUOTE_END_PREFIX] = true;
  struct MockLexer closer;
  init_mock_lexer(
    &closer,
    nested_input,
    sizeof(nested_input) / sizeof(nested_input[0])
  );
  assert(
    tree_sitter_sh_external_scanner_scan(scanner, &closer.lexer, valid_symbols)
  );
  assert(closer.lexer.result_symbol == BACKQUOTE_END_PREFIX);
  assert(closer.mark == 1);
  assert(scanner->backquote_depth == 1);

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_backquote_escape_run_scanner_contract(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  scanner->backquote_depth = 1;

  bool run_symbols[TOKEN_COUNT] = {false};
  run_symbols[BACKQUOTE_DOLLAR_PREFIX] = true;
  run_symbols[BACKQUOTE_START_PREFIX] = true;
  run_symbols[BACKQUOTE_END_PREFIX] = true;
  run_symbols[BACKQUOTE_CONTENT_RUN_BEGIN] = true;
  run_symbols[BACKQUOTE_PAIR_RUN_BEGIN] = true;

  const int32_t content_tick[] = {'\\', '\\', '\\', '`'};
  assert_scan_result(
    scanner,
    run_symbols,
    content_tick,
    4,
    true,
    BACKQUOTE_CONTENT_RUN_BEGIN,
    0,
    3,
    '`'
  );
  assert(scanner->backquote_depth == 1);

  const int32_t long_content_tick[] =
    {'\\', '\\', '\\', '\\', '\\', '\\', '\\', '`'};
  assert_scan_result(
    scanner,
    run_symbols,
    long_content_tick,
    8,
    true,
    BACKQUOTE_CONTENT_RUN_BEGIN,
    0,
    7,
    '`'
  );

  const int32_t odd_content_dollar[] = {'\\', '\\', '\\', '$'};
  assert_scan_result(
    scanner,
    run_symbols,
    odd_content_dollar,
    4,
    true,
    BACKQUOTE_CONTENT_RUN_BEGIN,
    0,
    3,
    '$'
  );

  const int32_t even_content_dollar[] = {'\\', '\\', '$'};
  assert_scan_result(
    scanner,
    run_symbols,
    even_content_dollar,
    3,
    true,
    BACKQUOTE_CONTENT_RUN_BEGIN,
    0,
    2,
    '$'
  );

  const int32_t paired_opener[] = {'\\', '\\', '\\', '\\', '\\', '`'};
  assert_scan_result(
    scanner,
    run_symbols,
    paired_opener,
    6,
    true,
    BACKQUOTE_PAIR_RUN_BEGIN,
    0,
    5,
    '`'
  );
  assert(scanner->backquote_depth == 1);

  const int32_t paired_dollar[] = {'\\', '\\', '\\', '\\', '\\', '$'};
  assert_scan_result(
    scanner,
    run_symbols,
    paired_dollar,
    6,
    true,
    BACKQUOTE_PAIR_RUN_BEGIN,
    0,
    5,
    '$'
  );

  scanner->backquote_depth = 2;
  assert_scan_result(
    scanner,
    run_symbols,
    paired_opener,
    6,
    true,
    BACKQUOTE_PAIR_RUN_BEGIN,
    0,
    5,
    '`'
  );
  assert(scanner->backquote_depth == 2);

  scanner->backquote_depth = 1;
  bool prefix_symbols[TOKEN_COUNT] = {false};
  prefix_symbols[BACKQUOTE_DOLLAR_PREFIX] = true;
  prefix_symbols[BACKQUOTE_START_PREFIX] = true;
  prefix_symbols[BACKQUOTE_END_PREFIX] = true;
  assert_scan_result(
    scanner,
    prefix_symbols,
    content_tick,
    4,
    false,
    0,
    0,
    3,
    '`'
  );

  bool end_symbols[TOKEN_COUNT] = {false};
  end_symbols[BACKQUOTE_PAIR_RUN_END] = true;
  const int32_t tail_spelling[] = {'\\', '`'};
  assert_scan_result(
    scanner,
    end_symbols,
    tail_spelling,
    2,
    true,
    BACKQUOTE_PAIR_RUN_END,
    0,
    1,
    '`'
  );

  const int32_t bare_closer[] = {'`'};
  assert_scan_result(
    scanner,
    end_symbols,
    bare_closer,
    1,
    true,
    BACKQUOTE_PAIR_RUN_END,
    0,
    0,
    '`'
  );

  const int32_t continuing_pair[] = {'\\', '\\', 'x'};
  assert_scan_result(
    scanner,
    end_symbols,
    continuing_pair,
    3,
    false,
    0,
    0,
    1,
    '\\'
  );

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void
assert_function_body_boundary_classifies_after_horizontal_layout(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);

  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[FUNCTION_BODY_CONTINUATION_BOUNDARY] = true;

  valid_symbols[LINE_CONTINUATION] = true;
  valid_symbols[NEWLINE] = true;
  valid_symbols[COMMENT_BOUNDARY] = true;

  const int32_t continued_layout_input[] = {'\\', '\n', '{', ' '};
  struct MockLexer continued_layout;
  init_mock_lexer(
    &continued_layout,
    continued_layout_input,
    sizeof(continued_layout_input) / sizeof(continued_layout_input[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &continued_layout.lexer,
    valid_symbols
  ));
  assert(
    continued_layout.lexer.result_symbol == FUNCTION_BODY_CONTINUATION_BOUNDARY
  );
  assert(continued_layout.mark == 0);
  assert(continued_layout.offset == 3);
  assert(continued_layout.lexer.lookahead == ' ');

  const int32_t newline_input[] = {'\n', '{', ' '};
  struct MockLexer newline;
  init_mock_lexer(
    &newline,
    newline_input,
    sizeof(newline_input) / sizeof(newline_input[0])
  );
  assert(
    tree_sitter_sh_external_scanner_scan(scanner, &newline.lexer, valid_symbols)
  );
  assert(newline.lexer.result_symbol == FUNCTION_BODY_CONTINUATION_BOUNDARY);
  assert(newline.mark == 0);
  assert(newline.offset == 2);
  assert(newline.lexer.lookahead == ' ');

  const int32_t comment_input[] = {
    ' ',
    '#',
    'x',
    '\n',
    '\t',
    '{',
    ' ',
  };
  struct MockLexer comment;
  init_mock_lexer(
    &comment,
    comment_input,
    sizeof(comment_input) / sizeof(comment_input[0])
  );
  assert(
    tree_sitter_sh_external_scanner_scan(scanner, &comment.lexer, valid_symbols)
  );
  assert(comment.lexer.result_symbol == FUNCTION_BODY_CONTINUATION_BOUNDARY);
  assert(comment.mark == 0);
  assert(comment.offset == 6);
  assert(comment.lexer.lookahead == ' ');

  const int32_t body_after_layout_input[] = {
    ' ',
    '\\',
    '\n',
    '\t',
    '{',
    ' ',
  };
  struct MockLexer body_after_layout;
  init_mock_lexer(
    &body_after_layout,
    body_after_layout_input,
    sizeof(body_after_layout_input) / sizeof(body_after_layout_input[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &body_after_layout.lexer,
    valid_symbols
  ));
  assert(
    body_after_layout.lexer.result_symbol == FUNCTION_BODY_CONTINUATION_BOUNDARY
  );
  assert(body_after_layout.mark == 0);
  assert(body_after_layout.offset == 5);
  assert(body_after_layout.lexer.lookahead == ' ');

  const int32_t body_input[] = {'{', ' '};
  struct MockLexer body;
  init_mock_lexer(
    &body,
    body_input,
    sizeof(body_input) / sizeof(body_input[0])
  );
  assert(
    tree_sitter_sh_external_scanner_scan(scanner, &body.lexer, valid_symbols)
  );
  assert(body.lexer.result_symbol == FUNCTION_BODY_CONTINUATION_BOUNDARY);
  assert(body.mark == 0);

  const int32_t reserved_body_input[] = {'w', 'h', 'i', 'l', 'e', ' '};
  struct MockLexer reserved_body;
  init_mock_lexer(
    &reserved_body,
    reserved_body_input,
    sizeof(reserved_body_input) / sizeof(reserved_body_input[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &reserved_body.lexer,
    valid_symbols
  ));
  assert(
    reserved_body.lexer.result_symbol == FUNCTION_BODY_CONTINUATION_BOUNDARY
  );
  assert(reserved_body.mark == 0);
  assert(reserved_body.offset == 5);
  assert(reserved_body.lexer.lookahead == ' ');

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_substitution_closers(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  scanner->backquote_depth = 1;

  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[BACKQUOTE_END] = true;
  const int32_t backquote_input[] = {'`'};
  assert_scan_result(
    scanner,
    valid_symbols,
    backquote_input,
    sizeof(backquote_input) / sizeof(backquote_input[0]),
    true,
    BACKQUOTE_END,
    1,
    1,
    0
  );
  assert(scanner->backquote_depth == 0);

  memset(valid_symbols, 0, sizeof(valid_symbols));
  valid_symbols[COMMAND_SUBSTITUTION_CLOSE] = true;
  scanner->substitution_depth = 1;
  const int32_t parenthesis_input[] = {')'};
  assert_scan_result(
    scanner,
    valid_symbols,
    parenthesis_input,
    sizeof(parenthesis_input) / sizeof(parenthesis_input[0]),
    true,
    COMMAND_SUBSTITUTION_CLOSE,
    1,
    1,
    0
  );
  assert(scanner->substitution_depth == 0);

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_case_item_boundary_contract(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);

  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(scanner, before);

  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[CASE_ITEM_END] = true;
  valid_symbols[CASE_ITEM_NS_BOUNDARY] = true;

  const int32_t dsemi_input[] = {' ', ';', ';'};
  const int32_t semi_and_input[] = {' ', ';', '&'};
  const struct {
    const int32_t *input;
    size_t length;
    int32_t lookahead;
  } terminators[] = {
    {dsemi_input, sizeof(dsemi_input) / sizeof(dsemi_input[0]), ';'},
    {semi_and_input, sizeof(semi_and_input) / sizeof(semi_and_input[0]), '&'},
  };
  for (
    size_t index = 0; index < sizeof(terminators) / sizeof(terminators[0]);
    index += 1
  ) {
    assert_scan_result(
      scanner,
      valid_symbols,
      terminators[index].input,
      terminators[index].length,
      true,
      CASE_ITEM_END,
      0,
      2,
      terminators[index].lookahead
    );
    assert_scanner_matches_snapshot(scanner, before, before_length);
  }

  memset(valid_symbols, 0, sizeof(valid_symbols));
  valid_symbols[CASE_ITEM_NS_BOUNDARY] = true;

  const int32_t direct_esac_input[] = {'e', 's', 'a', 'c', ' '};
  assert_scan_result(
    scanner,
    valid_symbols,
    direct_esac_input,
    sizeof(direct_esac_input) / sizeof(direct_esac_input[0]),
    true,
    CASE_ITEM_NS_BOUNDARY,
    0,
    4,
    ' '
  );
  assert_scanner_matches_snapshot(scanner, before, before_length);

  const int32_t continued_esac_input[] = {
    ' ',
    '\\',
    '\n',
    '\t',
    'e',
    's',
    'a',
    'c',
    ' ',
  };
  assert_scan_result(
    scanner,
    valid_symbols,
    continued_esac_input,
    sizeof(continued_esac_input) / sizeof(continued_esac_input[0]),
    false,
    CASE_ITEM_NS_BOUNDARY,
    0,
    4,
    'e'
  );
  assert_scanner_matches_snapshot(scanner, before, before_length);

  memset(valid_symbols, 0, sizeof(valid_symbols));
  valid_symbols[CASE_ITEM_NS_BOUNDARY] = true;
  valid_symbols[NAME_EQUALS_BEGIN] = true;
  valid_symbols[FNAME_BEGIN] = true;

  const int32_t empty_body_esac_input[] = {'e', 's', 'a', 'c', ' '};
  assert_scan_result(
    scanner,
    valid_symbols,
    empty_body_esac_input,
    sizeof(empty_body_esac_input) / sizeof(empty_body_esac_input[0]),
    true,
    CASE_ITEM_NS_BOUNDARY,
    0,
    4,
    ' '
  );
  assert_scanner_matches_snapshot(scanner, before, before_length);

  const int32_t esac_assignment_input[] = {'e', 's', 'a', 'c', '='};
  assert_scan_result(
    scanner,
    valid_symbols,
    esac_assignment_input,
    sizeof(esac_assignment_input) / sizeof(esac_assignment_input[0]),
    true,
    NAME_EQUALS_BEGIN,
    0,
    4,
    '='
  );
  assert_scanner_matches_snapshot(scanner, before, before_length);

  memset(valid_symbols, 0, sizeof(valid_symbols));
  valid_symbols[CASE_ITEM_NS_BOUNDARY] = true;

  const int32_t ordinary_word_input[] = {'e', 's', 'a', 'c', 'x', ' '};
  const int32_t other_closer_input[] = {'f', 'i', ' '};
  const int32_t terminator_input[] = {';', ';'};
  const struct {
    const int32_t *input;
    size_t length;
  } rejected[] = {
    {ordinary_word_input,
      sizeof(ordinary_word_input) / sizeof(ordinary_word_input[0])},
    {other_closer_input,
      sizeof(other_closer_input) / sizeof(other_closer_input[0])},
    {terminator_input, sizeof(terminator_input) / sizeof(terminator_input[0])},
    {NULL, 0},
  };
  for (
    size_t index = 0; index < sizeof(rejected) / sizeof(rejected[0]); index += 1
  ) {
    struct MockLexer rejected_boundary;
    init_mock_lexer(
      &rejected_boundary,
      rejected[index].input,
      rejected[index].length
    );
    assert(!tree_sitter_sh_external_scanner_scan(
      scanner,
      &rejected_boundary.lexer,
      valid_symbols
    ));
    assert(rejected_boundary.mark == 0);
    assert_scanner_matches_snapshot(scanner, before, before_length);
  }

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_lone_separator_ends_the_list(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);

  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[LIST_CONTINUATION] = true;
  valid_symbols[TERMINATOR_AHEAD] = true;

  const int32_t command_input[] = {';', ' ', 'b'};
  assert_scan_result(
    scanner,
    valid_symbols,
    command_input,
    sizeof(command_input) / sizeof(command_input[0]),
    true,
    LIST_CONTINUATION,
    0,
    3,
    0
  );

  const int32_t lone_separator_input[] = {';', ' ', ';'};
  assert_scan_result(
    scanner,
    valid_symbols,
    lone_separator_input,
    sizeof(lone_separator_input) / sizeof(lone_separator_input[0]),
    true,
    TERMINATOR_AHEAD,
    0,
    3,
    0
  );

  const int32_t case_terminator_input[] = {';', ' ', ';', ';'};
  assert_scan_result(
    scanner,
    valid_symbols,
    case_terminator_input,
    sizeof(case_terminator_input) / sizeof(case_terminator_input[0]),
    true,
    LIST_CONTINUATION,
    0,
    4,
    0
  );

  const int32_t fallthrough_terminator_input[] = {';', ' ', ';', '&'};
  assert_scan_result(
    scanner,
    valid_symbols,
    fallthrough_terminator_input,
    sizeof(fallthrough_terminator_input) /
      sizeof(fallthrough_terminator_input[0]),
    true,
    LIST_CONTINUATION,
    0,
    4,
    0
  );

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_separator_continuation_with_pending_documents(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  assert(append_pending_document(scanner, make_document("END", false, false)));

  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[TERM_CONTINUATION] = true;
  valid_symbols[TERMINATOR_AHEAD] = true;
  valid_symbols[RIGHT_BRACE] = true;

  const int32_t same_line_command[] = {';', ' ', 'b', ' '};
  assert_scan_result(
    scanner,
    valid_symbols,
    same_line_command,
    sizeof(same_line_command) / sizeof(same_line_command[0]),
    true,
    TERM_CONTINUATION,
    0,
    3,
    ' '
  );

  const int32_t newline_run[] = {';', '\n'};
  assert_scan_result(
    scanner,
    valid_symbols,
    newline_run,
    sizeof(newline_run) / sizeof(newline_run[0]),
    false,
    TOKEN_COUNT,
    0,
    1,
    '\n'
  );

  const int32_t closing_brace[] = {';', ' ', '}'};
  assert_scan_result(
    scanner,
    valid_symbols,
    closing_brace,
    sizeof(closing_brace) / sizeof(closing_brace[0]),
    true,
    TERMINATOR_AHEAD,
    0,
    3,
    0
  );

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_comment_boundary_contract(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  assert(append_pending_document(scanner, make_document("END", false, false)));

  const int32_t continued_comment[] = {
    ' ',
    ' ',
    '\\',
    '\n',
    '#',
    'x',
    '\n',
  };
  struct MockLexer continued;
  init_mock_lexer(
    &continued,
    continued_comment,
    sizeof(continued_comment) / sizeof(continued_comment[0])
  );
  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[COMMENT_BOUNDARY] = true;
  valid_symbols[HERE_DOCUMENT_LINE_END] = true;
  valid_symbols[LINE_CONTINUATION] = true;
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &continued.lexer,
    valid_symbols
  ));
  assert(continued.lexer.result_symbol == COMMENT_BOUNDARY);
  assert(continued.mark == 0);
  assert(continued.offset == 4);
  assert(continued.lexer.lookahead == '#');

  const int32_t continued_comment_after_blank[] = {'\\', '\n', '#', 'x'};
  struct MockLexer continued_after_blank;
  init_mock_lexer(
    &continued_after_blank,
    continued_comment_after_blank,
    sizeof(continued_comment_after_blank) /
      sizeof(continued_comment_after_blank[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &continued_after_blank.lexer,
    valid_symbols
  ));
  assert(continued_after_blank.lexer.result_symbol == LINE_CONTINUATION);
  assert(continued_after_blank.mark == 2);
  assert(continued_after_blank.offset == 2);
  assert(continued_after_blank.lexer.lookahead == '#');

  size_t boundary_start = continued_after_blank.offset;
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &continued_after_blank.lexer,
    valid_symbols
  ));
  assert(continued_after_blank.lexer.result_symbol == COMMENT_BOUNDARY);
  assert(continued_after_blank.mark == boundary_start);
  assert(continued_after_blank.offset == boundary_start);
  assert(continued_after_blank.lexer.lookahead == '#');

  const int32_t blank_severed_and_if[] = {' ', '&', '\\', '\n', '&'};
  struct MockLexer and_if;
  init_mock_lexer(
    &and_if,
    blank_severed_and_if,
    sizeof(blank_severed_and_if) / sizeof(blank_severed_and_if[0])
  );
  memset(valid_symbols, 0, sizeof(valid_symbols));
  valid_symbols[COMMENT_BOUNDARY] = true;
  valid_symbols[AND_OR_CONTINUATION] = true;
  valid_symbols[LINE_CONTINUATION] = true;
  assert(
    !tree_sitter_sh_external_scanner_scan(scanner, &and_if.lexer, valid_symbols)
  );
  assert(and_if.mark == 0);

  const int32_t severed_and_if[] = {'&', '\\', '\n', '&'};
  struct MockLexer severed_and_if_scan;
  init_mock_lexer(
    &severed_and_if_scan,
    severed_and_if,
    sizeof(severed_and_if) / sizeof(severed_and_if[0])
  );
  assert(!tree_sitter_sh_external_scanner_scan(
    scanner,
    &severed_and_if_scan.lexer,
    valid_symbols
  ));
  assert(severed_and_if_scan.mark == 0);
  assert(scanner->pending_count == 1);

  const int32_t logical_and_if[] = {' ', '&', '&'};
  struct MockLexer logical;
  init_mock_lexer(
    &logical,
    logical_and_if,
    sizeof(logical_and_if) / sizeof(logical_and_if[0])
  );
  assert(
    tree_sitter_sh_external_scanner_scan(scanner, &logical.lexer, valid_symbols)
  );
  assert(logical.lexer.result_symbol == AND_OR_CONTINUATION);
  assert(logical.mark == 0);
  assert(logical.offset == 2);
  assert(logical.lexer.lookahead == '&');

  const int32_t logical_and_if_after_blank[] = {'&', '&'};
  struct MockLexer logical_after_blank;
  init_mock_lexer(
    &logical_after_blank,
    logical_and_if_after_blank,
    sizeof(logical_and_if_after_blank) / sizeof(logical_and_if_after_blank[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &logical_after_blank.lexer,
    valid_symbols
  ));
  assert(logical_after_blank.lexer.result_symbol == AND_OR_CONTINUATION);
  assert(logical_after_blank.mark == 0);
  assert(logical_after_blank.offset == 1);
  assert(logical_after_blank.lexer.lookahead == '&');
  assert(scanner->pending_count == 1);

  const int32_t pending_literal_hash_input[] = {'#', 't', 'a', 'g'};
  struct MockLexer pending_literal_hash;
  init_mock_lexer(
    &pending_literal_hash,
    pending_literal_hash_input,
    sizeof(pending_literal_hash_input) / sizeof(pending_literal_hash_input[0])
  );
  valid_symbols[LITERAL_HASH] = true;
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &pending_literal_hash.lexer,
    valid_symbols
  ));
  assert(pending_literal_hash.lexer.result_symbol == LITERAL_HASH);
  assert(pending_literal_hash.mark == 1);
  assert(pending_literal_hash.offset == 1);
  assert(pending_literal_hash.lexer.lookahead == 't');
  assert(scanner->pending_count == 1);
  valid_symbols[LITERAL_HASH] = false;

  clear_scanner(scanner);

  const int32_t name_continuation[] = {'\\', '\n', 'M', 'E'};
  struct MockLexer name;
  init_mock_lexer(
    &name,
    name_continuation,
    sizeof(name_continuation) / sizeof(name_continuation[0])
  );
  memset(valid_symbols, 0, sizeof(valid_symbols));
  valid_symbols[COMMENT_BOUNDARY] = true;
  valid_symbols[LINE_CONTINUATION] = true;
  assert(
    tree_sitter_sh_external_scanner_scan(scanner, &name.lexer, valid_symbols)
  );
  assert(name.lexer.result_symbol == LINE_CONTINUATION);
  assert(name.mark == 2);
  assert(name.offset == 2);
  assert(name.lexer.lookahead == 'M');

  const int32_t blank_continuation_non_comment[] = {' ', '\\', '\n', 'x'};
  struct MockLexer blank_continuation;
  init_mock_lexer(
    &blank_continuation,
    blank_continuation_non_comment,
    sizeof(blank_continuation_non_comment) /
      sizeof(blank_continuation_non_comment[0])
  );
  assert(!tree_sitter_sh_external_scanner_scan(
    scanner,
    &blank_continuation.lexer,
    valid_symbols
  ));
  assert(blank_continuation.mark == 0);

  const int32_t continued_comment_run[] = {
    '\\',
    '\n',
    '\\',
    '\n',
    '#',
    'x',
    '\n',
  };
  struct MockLexer continuation_run;
  init_mock_lexer(
    &continuation_run,
    continued_comment_run,
    sizeof(continued_comment_run) / sizeof(continued_comment_run[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &continuation_run.lexer,
    valid_symbols
  ));
  assert(continuation_run.lexer.result_symbol == LINE_CONTINUATION);
  assert(continuation_run.mark == 2);
  assert(continuation_run.offset == 2);
  assert(continuation_run.lexer.lookahead == '\\');
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &continuation_run.lexer,
    valid_symbols
  ));
  assert(continuation_run.lexer.result_symbol == LINE_CONTINUATION);
  assert(continuation_run.mark == 4);
  assert(continuation_run.offset == 4);
  assert(continuation_run.lexer.lookahead == '#');

  boundary_start = continuation_run.offset;
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &continuation_run.lexer,
    valid_symbols
  ));
  assert(continuation_run.lexer.result_symbol == COMMENT_BOUNDARY);
  assert(continuation_run.mark == boundary_start);
  assert(continuation_run.offset == boundary_start);
  assert(continuation_run.lexer.lookahead == '#');

  const int32_t trailing_blank_non_comment[] = {
    '\\',
    '\n',
    ' ',
    ' ',
    'x',
  };
  struct MockLexer trailing_blank;
  init_mock_lexer(
    &trailing_blank,
    trailing_blank_non_comment,
    sizeof(trailing_blank_non_comment) / sizeof(trailing_blank_non_comment[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &trailing_blank.lexer,
    valid_symbols
  ));
  assert(trailing_blank.lexer.result_symbol == LINE_CONTINUATION);
  assert(trailing_blank.mark == 2);
  assert(trailing_blank.offset == 2);
  assert(trailing_blank.lexer.lookahead == ' ');

  const int32_t trailing_blank_comment[] = {
    '\\',
    '\n',
    ' ',
    ' ',
    '#',
    'x',
  };
  struct MockLexer trailing_comment;
  init_mock_lexer(
    &trailing_comment,
    trailing_blank_comment,
    sizeof(trailing_blank_comment) / sizeof(trailing_blank_comment[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &trailing_comment.lexer,
    valid_symbols
  ));
  assert(trailing_comment.lexer.result_symbol == LINE_CONTINUATION);
  assert(trailing_comment.mark == 2);
  assert(trailing_comment.offset == 2);
  assert(trailing_comment.lexer.lookahead == ' ');

  const int32_t direct_hash_after_blank[] = {'#', 'x'};
  struct MockLexer hash_after_blank;
  init_mock_lexer(
    &hash_after_blank,
    direct_hash_after_blank,
    sizeof(direct_hash_after_blank) / sizeof(direct_hash_after_blank[0])
  );
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &hash_after_blank.lexer,
    valid_symbols
  ));
  assert(hash_after_blank.lexer.result_symbol == COMMENT_BOUNDARY);
  assert(hash_after_blank.mark == 0);
  assert(hash_after_blank.offset == 0);
  assert(hash_after_blank.lexer.lookahead == '#');

  const int32_t spaced_hash_input[] = {' ', '#', 'x'};
  struct MockLexer spaced_hash;
  init_mock_lexer(
    &spaced_hash,
    spaced_hash_input,
    sizeof(spaced_hash_input) / sizeof(spaced_hash_input[0])
  );
  valid_symbols[LITERAL_HASH] = true;
  valid_symbols[PIPE_CONTINUATION] = true;
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &spaced_hash.lexer,
    valid_symbols
  ));
  assert(spaced_hash.lexer.result_symbol == COMMENT_BOUNDARY);
  assert(spaced_hash.mark == 0);
  assert(spaced_hash.offset == 1);
  assert(spaced_hash.lexer.lookahead == '#');
  valid_symbols[LITERAL_HASH] = false;
  valid_symbols[PIPE_CONTINUATION] = false;

  const int32_t blank_non_comment[] = {' ', '\t', 'x'};
  struct MockLexer blank;
  init_mock_lexer(
    &blank,
    blank_non_comment,
    sizeof(blank_non_comment) / sizeof(blank_non_comment[0])
  );
  assert(
    !tree_sitter_sh_external_scanner_scan(scanner, &blank.lexer, valid_symbols)
  );
  assert(blank.mark == 0);

  const int32_t incomplete_backslash[] = {' ', '\\', 'x'};
  struct MockLexer incomplete;
  init_mock_lexer(
    &incomplete,
    incomplete_backslash,
    sizeof(incomplete_backslash) / sizeof(incomplete_backslash[0])
  );
  assert(!tree_sitter_sh_external_scanner_scan(
    scanner,
    &incomplete.lexer,
    valid_symbols
  ));
  assert(incomplete.mark == 0);

  const int32_t direct_incomplete_backslash[] = {'\\', 'x'};
  struct MockLexer direct_incomplete;
  init_mock_lexer(
    &direct_incomplete,
    direct_incomplete_backslash,
    sizeof(direct_incomplete_backslash) / sizeof(direct_incomplete_backslash[0])
  );
  assert(!tree_sitter_sh_external_scanner_scan(
    scanner,
    &direct_incomplete.lexer,
    valid_symbols
  ));
  assert(direct_incomplete.mark == 0);
  assert(direct_incomplete.offset == 1);
  assert(direct_incomplete.lexer.lookahead == 'x');

  const int32_t direct_comment[] = {'#', 'x', '\n'};
  struct MockLexer direct;
  init_mock_lexer(
    &direct,
    direct_comment,
    sizeof(direct_comment) / sizeof(direct_comment[0])
  );
  memset(valid_symbols, 0, sizeof(valid_symbols));
  valid_symbols[COMMENT_BOUNDARY] = true;
  valid_symbols[COMMENT] = true;
  assert(
    tree_sitter_sh_external_scanner_scan(scanner, &direct.lexer, valid_symbols)
  );
  assert(direct.lexer.result_symbol == COMMENT_BOUNDARY);
  assert(direct.mark == 0);
  assert(direct.offset == 0);
  assert(direct.lexer.lookahead == '#');

  struct MockLexer literal_hash;
  init_mock_lexer(
    &literal_hash,
    direct_comment,
    sizeof(direct_comment) / sizeof(direct_comment[0])
  );
  valid_symbols[LITERAL_HASH] = true;
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &literal_hash.lexer,
    valid_symbols
  ));
  assert(literal_hash.lexer.result_symbol == LITERAL_HASH);
  assert(literal_hash.mark == 1);
  assert(literal_hash.offset == 1);
  assert(literal_hash.lexer.lookahead == 'x');

  struct MockLexer comment;
  init_mock_lexer(
    &comment,
    direct_comment,
    sizeof(direct_comment) / sizeof(direct_comment[0])
  );
  memset(valid_symbols, 0, sizeof(valid_symbols));
  valid_symbols[COMMENT] = true;
  assert(
    tree_sitter_sh_external_scanner_scan(scanner, &comment.lexer, valid_symbols)
  );
  assert(comment.lexer.result_symbol == COMMENT);
  assert(comment.mark == 2);
  assert(comment.offset == 2);
  assert(comment.lexer.lookahead == '\n');

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_trailing_comment_boundary_contract(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);

  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[COMMENT_BOUNDARY] = true;
  valid_symbols[TRAILING_COMMENT_BOUNDARY] = true;

  const int32_t input_end_comment[] = {'#', 'x'};
  assert_scan_result(
    scanner,
    valid_symbols,
    input_end_comment,
    sizeof(input_end_comment) / sizeof(input_end_comment[0]),
    true,
    TRAILING_COMMENT_BOUNDARY,
    0,
    2,
    0
  );

  const int32_t terminated_comment[] = {'#', 'x', '\n'};
  assert_scan_result(
    scanner,
    valid_symbols,
    terminated_comment,
    sizeof(terminated_comment) / sizeof(terminated_comment[0]),
    true,
    COMMENT_BOUNDARY,
    0,
    2,
    '\n'
  );

  valid_symbols[TRAILING_COMMENT_BOUNDARY] = false;
  assert_scan_result(
    scanner,
    valid_symbols,
    input_end_comment,
    sizeof(input_end_comment) / sizeof(input_end_comment[0]),
    true,
    COMMENT_BOUNDARY,
    0,
    0,
    '#'
  );

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_comment_line_end_contract(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);

  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[COMMENT_LINE_END] = true;

  const int32_t next_comment[] = {'\n', '#', 'x', '\n', 'n', 'e', 'x', 't'};
  assert_scan_result(
    scanner,
    valid_symbols,
    next_comment,
    sizeof(next_comment) / sizeof(next_comment[0]),
    true,
    COMMENT_LINE_END,
    1,
    1,
    '#'
  );

  const int32_t layout_before_comment[] = {
    '\n',
    ' ',
    '\t',
    '\n',
    ' ',
    '\\',
    '\n',
    '\t',
    '#',
    'x',
  };
  assert_scan_result(
    scanner,
    valid_symbols,
    layout_before_comment,
    sizeof(layout_before_comment) / sizeof(layout_before_comment[0]),
    true,
    COMMENT_LINE_END,
    1,
    8,
    '#'
  );

  const int32_t command_ahead[] = {'\n', 'n', 'e', 'x', 't'};
  assert_scan_result(
    scanner,
    valid_symbols,
    command_ahead,
    sizeof(command_ahead) / sizeof(command_ahead[0]),
    true,
    COMMENT_LINE_END,
    1,
    5,
    0
  );

  const int32_t input_end[] = {'\n'};
  assert_scan_result(
    scanner,
    valid_symbols,
    input_end,
    sizeof(input_end) / sizeof(input_end[0]),
    true,
    COMMENT_LINE_END,
    1,
    1,
    0
  );

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_arithmetic_boundary_contract(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);

  const int32_t closing_input[] = {'\\', '\n', '\\', '\n', ')'};
  struct MockLexer closing;
  init_mock_lexer(
    &closing,
    closing_input,
    sizeof(closing_input) / sizeof(closing_input[0])
  );
  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[ARITHMETIC_CLOSING_BOUNDARY] = true;
  valid_symbols[ARITHMETIC_OPERAND_BOUNDARY] = true;
  valid_symbols[ARITHMETIC_ADDITIVE_OPERATOR_BOUNDARY] = true;
  assert(
    tree_sitter_sh_external_scanner_scan(scanner, &closing.lexer, valid_symbols)
  );
  assert(closing.lexer.result_symbol == ARITHMETIC_CLOSING_BOUNDARY);
  assert(closing.mark == 0);
  assert(closing.offset == 4);
  assert(closing.lexer.lookahead == ')');

  const int32_t operand_input[] = {'\\', '\n', '\\', '\n', 'a'};
  struct MockLexer operand;
  init_mock_lexer(
    &operand,
    operand_input,
    sizeof(operand_input) / sizeof(operand_input[0])
  );
  assert(
    tree_sitter_sh_external_scanner_scan(scanner, &operand.lexer, valid_symbols)
  );
  assert(operand.lexer.result_symbol == ARITHMETIC_OPERAND_BOUNDARY);
  assert(operand.mark == 0);
  assert(operand.offset == 4);
  assert(operand.lexer.lookahead == 'a');

  const int32_t operator_input[] = {'\\', '\n', '\\', '\n', '+'};
  struct MockLexer operator;
  init_mock_lexer(
    &operator,
    operator_input,
    sizeof(operator_input) / sizeof(operator_input[0])
  );
  memset(valid_symbols, 0, sizeof(valid_symbols));
  valid_symbols[ARITHMETIC_ADDITIVE_OPERATOR_BOUNDARY] = true;
  assert(tree_sitter_sh_external_scanner_scan(
    scanner,
    &operator.lexer,
    valid_symbols
  ));
  assert(operator.lexer.result_symbol == ARITHMETIC_ADDITIVE_OPERATOR_BOUNDARY);
  assert(operator.mark == 0);
  assert(operator.offset == 5);
  assert(operator.lexer.lookahead == 0);

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_arithmetic_left_parenthesis_classification(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);

  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[ARITHMETIC_LEFT_PARENTHESIS] = true;
  valid_symbols[ARITHMETIC_DYNAMIC_LEFT_PARENTHESIS] = true;

  const int32_t structured[] = {'(', '1', ' ', '+', ' ', '2', ')', ')'};
  assert_scan_result(
    scanner,
    valid_symbols,
    structured,
    sizeof(structured) / sizeof(structured[0]),
    true,
    ARITHMETIC_LEFT_PARENTHESIS,
    0,
    7,
    ')'
  );

  const int32_t fragment_structured[] =
    {'(', '$', 'x', ' ', '+', ' ', '1', ')', ')'};
  assert_scan_result(
    scanner,
    valid_symbols,
    fragment_structured,
    sizeof(fragment_structured) / sizeof(fragment_structured[0]),
    true,
    ARITHMETIC_LEFT_PARENTHESIS,
    0,
    8,
    ')'
  );

  const int32_t dynamic[] = {'(', '$', 'x', ' ', '$', 'y', ')', ')'};
  assert_scan_result(
    scanner,
    valid_symbols,
    dynamic,
    sizeof(dynamic) / sizeof(dynamic[0]),
    true,
    ARITHMETIC_DYNAMIC_LEFT_PARENTHESIS,
    0,
    7,
    ')'
  );

  const int32_t substitution[] = {'(', '0', 'x', ')', ')'};
  assert_scan_result(
    scanner,
    valid_symbols,
    substitution,
    sizeof(substitution) / sizeof(substitution[0]),
    false,
    0,
    0,
    4,
    ')'
  );

  memset(valid_symbols, 0, sizeof(valid_symbols));
  valid_symbols[ARITHMETIC_LEFT_PARENTHESIS] = true;
  assert_scan_result(
    scanner,
    valid_symbols,
    dynamic,
    sizeof(dynamic) / sizeof(dynamic[0]),
    false,
    0,
    0,
    7,
    ')'
  );

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_tilde_end_marker(
  enum TokenType symbol,
  int32_t lookahead,
  bool expected
) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);

  const int32_t input[] = {lookahead};
  struct MockLexer mock;
  init_mock_lexer(&mock, input, lookahead == 0 ? 0 : 1);
  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[symbol] = true;

  bool result =
    tree_sitter_sh_external_scanner_scan(scanner, &mock.lexer, valid_symbols);
  assert(result == expected);
  if (expected) {
    assert(mock.lexer.result_symbol == symbol);
    assert(mock.offset == 0);
    assert(mock.mark == 0);
    assert(mock.lexer.lookahead == lookahead);
  }

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_tilde_end_marker_contract(void) {
  const int32_t word_ends[] = {0, '/', ' ', '\t', '\n', ';'};
  for (
    size_t index = 0; index < sizeof(word_ends) / sizeof(word_ends[0]);
    index += 1
  ) {
    assert_tilde_end_marker(WORD_TILDE_END, word_ends[index], true);
    assert_tilde_end_marker(ASSIGNMENT_TILDE_END, word_ends[index], true);
  }

  assert_tilde_end_marker(WORD_TILDE_END, ':', false);
  assert_tilde_end_marker(ASSIGNMENT_TILDE_END, ':', true);
  assert_tilde_end_marker(WORD_TILDE_END, 'x', false);
  assert_tilde_end_marker(ASSIGNMENT_TILDE_END, 'x', false);

  const int32_t grammar_owned_starts[] = {'\'', '"', '$', '`', '\\'};
  for (
    size_t index = 0;
    index < sizeof(grammar_owned_starts) / sizeof(grammar_owned_starts[0]);
    index += 1
  ) {
    assert_tilde_end_marker(WORD_TILDE_END, grammar_owned_starts[index], false);
    assert_tilde_end_marker(
      ASSIGNMENT_TILDE_END,
      grammar_owned_starts[index],
      false
    );
  }
}

static void assert_nul_and_eof_are_distinct(void) {
  const int32_t comment_input[] = {'#', 'a', 0, 'b', '\n'};
  struct MockLexer comment;
  init_mock_lexer(
    &comment,
    comment_input,
    sizeof(comment_input) / sizeof(comment_input[0])
  );
  assert(scan_comment(&comment.lexer));
  assert(comment.lexer.result_symbol == COMMENT);
  assert(comment.offset == 4);
  assert(comment.mark == 4);
  assert(comment.lexer.lookahead == '\n');

  const struct Scanner outside_backquote = {0};
  const int32_t nul_input[] = {0};
  struct MockLexer nul;
  init_mock_lexer(&nul, nul_input, 1);
  assert(!lexer_at_eof(&nul.lexer));
  assert(!is_token_delimiter(&outside_backquote, &nul.lexer));

  struct MockLexer eof;
  init_mock_lexer(&eof, NULL, 0);
  assert(lexer_at_eof(&eof.lexer));
  assert(is_token_delimiter(&outside_backquote, &eof.lexer));
}

static void assert_an_open_backquote_ends_tokens(void) {
  const int32_t backquote_input[] = {'`'};
  struct MockLexer backquote;
  init_mock_lexer(&backquote, backquote_input, 1);

  const struct Scanner outside_backquote = {0};
  assert(!is_token_delimiter(&outside_backquote, &backquote.lexer));

  const struct Scanner inside_backquote = {.backquote_depth = 1};
  assert(is_token_delimiter(&inside_backquote, &backquote.lexer));

  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  scanner->backquote_depth = 1;
  assert(!scan_backquote_start(scanner, &backquote.lexer));
  assert(scanner->backquote_depth == 1);
  scanner->backquote_depth = 0;
  assert(scan_backquote_start(scanner, &backquote.lexer));
  assert(backquote.lexer.result_symbol == BACKQUOTE_START);
  assert(scanner->backquote_depth == 1);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_here_document_line_backslash_parity(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  struct HereDocument document = make_document("E", false, false);
  struct MockLexer mock;
  struct HereDocumentLineStart start;

  const int32_t paired[] = {'x', '\\', '\\', '\n', 'E', '\n'};
  init_mock_lexer(&mock, paired, sizeof(paired) / sizeof(paired[0]));
  assert(
    probe_here_document_line(scanner, &mock.lexer, &document, &start) ==
    HERE_DOCUMENT_LINE_CONTENT
  );
  assert(
    probe_here_document_line(scanner, &mock.lexer, &document, &start) ==
    HERE_DOCUMENT_LINE_DELIMITER
  );

  const int32_t continued[] = {'x', '\\', '\n', 'E', '\n'};
  init_mock_lexer(&mock, continued, sizeof(continued) / sizeof(continued[0]));
  assert(
    probe_here_document_line(scanner, &mock.lexer, &document, &start) ==
    HERE_DOCUMENT_LINE_CONTENT
  );
  assert(
    probe_here_document_line(scanner, &mock.lexer, &document, &start) ==
    HERE_DOCUMENT_LINE_END_OF_INPUT
  );

  const int32_t nested[] = {'x', '\\', '\\', '\n', 'E', '\n'};
  init_mock_lexer(&mock, nested, sizeof(nested) / sizeof(nested[0]));
  bool is_end = false;
  bool at_end_of_input = false;
  assert(scan_nested_here_document_line(
    &mock.lexer,
    NULL,
    &document,
    &is_end,
    &at_end_of_input
  ));
  assert(!is_end);
  assert(!at_end_of_input);
  assert(scan_nested_here_document_line(
    &mock.lexer,
    NULL,
    &document,
    &is_end,
    &at_end_of_input
  ));
  assert(is_end);

  clear_document(&document);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_uncommitted_here_document_delimiter_clears_on_newline(void) {
  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[NEWLINE] = true;
  const int32_t input[] = {'\n'};

  struct Scanner *missing = tree_sitter_sh_external_scanner_create();
  assert(missing != NULL);
  missing->expecting_delimiter = true;
  missing->delimiter_strips_tabs = true;
  assert_scan_result(
    missing,
    valid_symbols,
    input,
    sizeof(input) / sizeof(input[0]),
    true,
    NEWLINE,
    1,
    1,
    0
  );
  assert(!missing->expecting_delimiter);
  assert(!missing->delimiter_strips_tabs);
  assert(missing->captured_count == 0);
  assert(missing->pending_count == 0);
  tree_sitter_sh_external_scanner_destroy(missing);

  struct Scanner *uncommitted = tree_sitter_sh_external_scanner_create();
  assert(uncommitted != NULL);
  assert(
    append_captured_document(uncommitted, make_document("END", false, false))
  );
  assert_scan_result(
    uncommitted,
    valid_symbols,
    input,
    sizeof(input) / sizeof(input[0]),
    true,
    NEWLINE,
    1,
    1,
    0
  );
  assert(uncommitted->captured_count == 0);
  assert(uncommitted->pending_count == 0);
  tree_sitter_sh_external_scanner_destroy(uncommitted);
}

static void assert_io_location_marks_the_full_token(void) {
  const struct Scanner scanner = {0};
  const int32_t direct_input[] = {'{', 'a', '}', '>', 'x'};
  struct MockLexer direct;
  init_mock_lexer(
    &direct,
    direct_input,
    sizeof(direct_input) / sizeof(direct_input[0])
  );

  assert(scan_left_brace_or_io_location(&scanner, &direct.lexer, false, true));
  assert(direct.lexer.result_symbol == IO_LOCATION);
  assert(direct.offset == 3);
  assert(direct.mark == 3);
  assert(direct.lexer.lookahead == '>');

  const int32_t internal_brace_input[] = {'{', 'a', '}', '}', '>', 'x'};
  struct MockLexer internal_brace;
  init_mock_lexer(
    &internal_brace,
    internal_brace_input,
    sizeof(internal_brace_input) / sizeof(internal_brace_input[0])
  );

  assert(
    scan_left_brace_or_io_location(&scanner, &internal_brace.lexer, false, true)
  );
  assert(internal_brace.lexer.result_symbol == IO_LOCATION);
  assert(internal_brace.offset == 4);
  assert(internal_brace.mark == 4);
  assert(internal_brace.lexer.lookahead == '>');

  const int32_t continued_input[] = {'{', 'a', '}', '\\', '\n', '>', 'x'};
  struct MockLexer continued;
  init_mock_lexer(
    &continued,
    continued_input,
    sizeof(continued_input) / sizeof(continued_input[0])
  );

  assert(
    scan_left_brace_or_io_location(&scanner, &continued.lexer, false, true)
  );
  assert(continued.lexer.result_symbol == IO_LOCATION);
  assert(continued.offset == 5);
  assert(continued.mark == 3);
  assert(continued.lexer.lookahead == '>');

  struct MockLexer continuation_after_rewind;
  init_mock_lexer(
    &continuation_after_rewind,
    continued_input + continued.mark,
    sizeof(continued_input) / sizeof(continued_input[0]) - continued.mark
  );
  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[LINE_CONTINUATION] = true;

  assert(
    scan_line_continuation(&continuation_after_rewind.lexer, valid_symbols)
  );
  assert(continuation_after_rewind.lexer.result_symbol == LINE_CONTINUATION);
  assert(continuation_after_rewind.offset == 2);
  assert(continuation_after_rewind.mark == 2);
  assert(continuation_after_rewind.lexer.lookahead == '>');
}

static void assert_redirect_list_begin_contract(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[REDIRECT_LIST_BEGIN] = true;

  const int32_t direct_input[] = {'<', 'i', 'n'};
  assert_scan_result(
    scanner,
    valid_symbols,
    direct_input,
    sizeof(direct_input) / sizeof(direct_input[0]),
    true,
    REDIRECT_LIST_BEGIN,
    0,
    0,
    '<'
  );

  const int32_t spaced_input[] = {' ', '\t', '2', '>', 'o', 'u', 't'};
  assert_scan_result(
    scanner,
    valid_symbols,
    spaced_input,
    sizeof(spaced_input) / sizeof(spaced_input[0]),
    true,
    REDIRECT_LIST_BEGIN,
    0,
    2,
    '2'
  );

  const int32_t continued_input[] = {'\\', '\n', '{', 'f', 'd', '}', '>'};
  assert_scan_result(
    scanner,
    valid_symbols,
    continued_input,
    sizeof(continued_input) / sizeof(continued_input[0]),
    true,
    REDIRECT_LIST_BEGIN,
    0,
    2,
    '{'
  );

  const int32_t rejected_inputs[][2] = {{' ', '&'}, {' ', '\n'}, {' ', 'x'}};
  for (
    size_t index = 0;
    index < sizeof(rejected_inputs) / sizeof(rejected_inputs[0]);
    index += 1
  ) {
    assert_scan_result(
      scanner,
      valid_symbols,
      rejected_inputs[index],
      sizeof(rejected_inputs[index]) / sizeof(rejected_inputs[index][0]),
      false,
      0,
      0,
      1,
      rejected_inputs[index][1]
    );
  }

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_name_equals_begin_contract(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[NAME_EQUALS_BEGIN] = true;

  const int32_t assignment_input[] = {'v', 'a', 'l', 'u', 'e', '=', '1'};
  assert_scan_result(
    scanner,
    valid_symbols,
    assignment_input,
    sizeof(assignment_input) / sizeof(assignment_input[0]),
    true,
    NAME_EQUALS_BEGIN,
    0,
    5,
    '='
  );

  const int32_t continued_assignment_input[] = {
    'v',
    'a',
    'l',
    'u',
    'e',
    '\\',
    '\n',
    '=',
    '1',
  };
  assert_scan_result(
    scanner,
    valid_symbols,
    continued_assignment_input,
    sizeof(continued_assignment_input) / sizeof(continued_assignment_input[0]),
    true,
    NAME_EQUALS_BEGIN,
    0,
    7,
    '='
  );

  const int32_t reserved_input[] = {'c', 'a', 's', 'e', '\\', '\n', ' '};
  valid_symbols[CASE_KEYWORD] = true;
  assert_scan_result(
    scanner,
    valid_symbols,
    reserved_input,
    sizeof(reserved_input) / sizeof(reserved_input[0]),
    true,
    CASE_KEYWORD,
    0,
    6,
    ' '
  );

  const int32_t operand_input[] = {'n', 'a', 'm', 'e', '+'};
  memset(valid_symbols, 0, sizeof(valid_symbols));
  valid_symbols[NAME_EQUALS_BEGIN] = true;
  assert_scan_result(
    scanner,
    valid_symbols,
    operand_input,
    sizeof(operand_input) / sizeof(operand_input[0]),
    false,
    0,
    0,
    4,
    '+'
  );

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void assert_delimiter_scan_resource_rollback(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  scanner->expecting_delimiter = true;

  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(scanner, before);
  size_t length = SCANNER_STATE_CAPACITY + 2;
  int32_t *input = malloc(length * sizeof(int32_t));
  assert(input != NULL);
  for (size_t index = 0; index + 1 < length; index += 1) {
    input[index] = 'x';
  }
  input[length - 1] = '\n';

  assert(!scan_delimiter_fixture(scanner, input, length));
  assert_scanner_matches_snapshot(scanner, before, before_length);
  free(input);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

#ifdef TREE_SITTER_REUSE_ALLOCATOR
static void assert_reuse_allocator_realloc_failure_rolls_back(void) {
  assert(reuse_live_allocations == 0);
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  scanner->expecting_delimiter = true;
  scanner->delimiter_strips_tabs = true;

  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(scanner, before);
  const int32_t input[] = {'x', '\n'};
  size_t realloc_calls_before = reuse_realloc_calls;

  reuse_fail_next_realloc = true;
  assert(
    !scan_delimiter_fixture(scanner, input, sizeof(input) / sizeof(input[0]))
  );
  assert(!reuse_fail_next_realloc);
  assert(reuse_realloc_calls == realloc_calls_before + 1);
  assert_scanner_matches_snapshot(scanner, before, before_length);
  assert(reuse_live_allocations == 1);

  tree_sitter_sh_external_scanner_destroy(scanner);
  assert(reuse_live_allocations == 0);
}

static void assert_reuse_allocator_contract(void) {
  assert(reuse_malloc_calls > 0);
  assert(reuse_calloc_calls > 0);
  assert(reuse_realloc_calls > 0);
  assert(reuse_free_calls > 0);
  assert(reuse_live_allocations == 0);

  reuse_fail_next_calloc = true;
  assert(tree_sitter_sh_external_scanner_create() == NULL);
  assert(!reuse_fail_next_calloc);
  assert(reuse_live_allocations == 0);
}
#endif

int main(void) {
  assert_all_valid_scan_preserves_state();
  assert_state_round_trip();
  assert_old_state_is_rejected();
  assert_exact_fit_state_round_trip();
  assert_delimiter_capture_rejects_oversized_state();
  assert_captured_to_pending_move_is_capacity_guarded();
  assert_active_suspend_rejects_oversized_state();
  assert_backquote_growth_rejects_oversized_state();
  assert_strict_scalar_encoding();
  assert_control_escape_table();
  assert_byte_delimiter_matching();
  assert_nested_here_document_logical_line_tabs();
  assert_here_document_line_backslash_parity();
  assert_backquote_prefix_classification();
  assert_substitution_hash_delimiter_words();
  assert_recursive_backquote_delimiters();
  assert_dollar_single_quote_delimiter_bytes();
  assert_generic_line_continuation_contract();
  assert_boundary_line_continuation_contract();
  assert_word_separator_classification_contract();
  assert_backquote_prefix_scanner_contract();
  assert_backquote_escape_run_scanner_contract();
  assert_function_body_boundary_classifies_after_horizontal_layout();
  assert_substitution_closers();
  assert_case_item_boundary_contract();
  assert_lone_separator_ends_the_list();
  assert_separator_continuation_with_pending_documents();
  assert_comment_boundary_contract();
  assert_trailing_comment_boundary_contract();
  assert_comment_line_end_contract();
  assert_arithmetic_boundary_contract();
  assert_arithmetic_left_parenthesis_classification();
  assert_tilde_end_marker_contract();
  assert_nul_and_eof_are_distinct();
  assert_an_open_backquote_ends_tokens();
  assert_uncommitted_here_document_delimiter_clears_on_newline();
  assert_io_location_marks_the_full_token();
  assert_redirect_list_begin_contract();
  assert_name_equals_begin_contract();
  assert_delimiter_scan_resource_rollback();
#ifdef TREE_SITTER_REUSE_ALLOCATOR
  assert_reuse_allocator_realloc_failure_rolls_back();
  assert_reuse_allocator_contract();
#endif
  return 0;
}
