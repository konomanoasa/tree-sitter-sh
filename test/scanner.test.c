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
static size_t reuse_allocation_calls;
static size_t reuse_fail_allocation_call;
static bool reuse_fail_next_calloc;
static bool reuse_fail_next_realloc;

static bool reuse_allocation_fails(void) {
  reuse_allocation_calls += 1;
  return reuse_fail_allocation_call !=
    0 &&
    reuse_allocation_calls == reuse_fail_allocation_call;
}

static void *reuse_malloc(size_t size) {
  reuse_malloc_calls += 1;
  if (reuse_allocation_fails()) {
    return NULL;
  }
  void *result = malloc(size);
  if (result != NULL) {
    reuse_live_allocations += 1;
  }
  return result;
}

static void *reuse_calloc(size_t count, size_t size) {
  reuse_calloc_calls += 1;
  if (reuse_allocation_fails() || reuse_fail_next_calloc) {
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
  if (reuse_allocation_fails() || reuse_fail_next_realloc) {
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

static void test_lookahead_replays_raw_cuts_in_distinct_quote_views(void) {
  const int32_t source[] = {'\'', '\\', '\n', 'x', '\'', '\\', '\n', 'y'};
  struct MockLexer mock;
  init_mock_lexer(&mock, source, sizeof(source) / sizeof(source[0]));
  struct NativeInput native = {.lexer = &mock.lexer};
  struct SourceStage stage = source_stage(SOURCE_REMOVE_CONTINUATIONS);
  struct SourceSnapshot initial = {.stages = &stage, .stage_count = 1};
  struct LogicalLexer input;
  assert(logical_init(&input, &native, &initial));
  struct LookaheadLexer lookahead;
  assert(lookahead_init(&lookahead, &input.lexer));
  TSLexer *lexer = &lookahead.lexer;
  lexer->advance(lexer, false);
  assert(lexer->lookahead == 'x');
  size_t parent = lookahead.position;
  assert(lookahead_local_policy(lexer, true));
  size_t quoted = lookahead.position;
  const int32_t quoted_source[] = {'\\', '\n', 'x', '\''};
  for (size_t index = 0; index < 4; index += 1) {
    assert(lexer->lookahead == quoted_source[index]);
    lexer->advance(lexer, false);
  }
  assert(lexer->lookahead == '\\');
  assert(lookahead_local_policy(lexer, false));
  assert(lexer->lookahead == 'y');
  lexer->mark_end(lexer);
  assert(input.mark == 5);
  lexer->advance(lexer, false);
  assert(lexer->eof(lexer));
  size_t views = lookahead.view_count;
  lookahead_seek(&lookahead, parent);
  assert(lexer->lookahead == 'x');
  assert(lookahead_local_policy(lexer, true));
  assert(lookahead.position == quoted);
  assert(lookahead.view_count == views);
  assert(lexer->lookahead == '\\');
  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  assert(input.mark == 2);
  assert(mock.offset == 8);
  assert(mock.mark == 0);
  lookahead_clear(&lookahead);
  logical_clear(&input);
  native_input_clear(&native);
}

struct SourceFixture {
  int32_t *characters;
  struct MockLexer mock;
  struct NativeInput native;
  struct LogicalLexer input;
  struct LookaheadLexer lookahead;
};

static void init_source_fixture(
  struct SourceFixture *fixture,
  const char *source,
  struct SourceStage *stages,
  size_t count
) {
  *fixture = (struct SourceFixture){0};
  size_t length = strlen(source);
  fixture->characters = ts_malloc(length * sizeof(*fixture->characters));
  assert(fixture->characters != NULL);
  for (size_t index = 0; index < length; index += 1) {
    fixture->characters[index] = (unsigned char)source[index];
  }
  init_mock_lexer(&fixture->mock, fixture->characters, length);
  fixture->native.lexer = &fixture->mock.lexer;
  struct SourceSnapshot initial = {.stages = stages, .stage_count = count};
  assert(logical_init(&fixture->input, &fixture->native, &initial));
  assert(lookahead_init(&fixture->lookahead, &fixture->input.lexer));
}

static void clear_source_fixture(struct SourceFixture *fixture) {
  lookahead_clear(&fixture->lookahead);
  logical_clear(&fixture->input);
  native_input_clear(&fixture->native);
  ts_free(fixture->characters);
}

static void test_delimiter_source_views_preserve_quotedness_and_bytes(void) {
  const struct {
    const char *source;
    const char *delimiter;
    bool quoted;
  } fixtures[] = {
    {"E\\\nOF ", "EOF", false},
    {"'E\\\nOF' ", "E\\\nOF", true},
    {"\"E\\\nOF\" ", "EOF", true},
    {"a\\ b ", "a b", true},
    {"`printf \\`x\\`#tag END` ", "`printf `x`#tag END`", true},
    {"`printf \\\\\nEOF` ", "`printf EOF`", false},
  };
  for (
    size_t index = 0; index < sizeof(fixtures) / sizeof(fixtures[0]); index += 1
  ) {
    struct SourceStage stage = source_stage(SOURCE_REMOVE_CONTINUATIONS);
    struct SourceFixture fixture;
    init_source_fixture(&fixture, fixtures[index].source, &stage, 1);
    struct HereDocument document = {0};
    assert(
      read_here_document_delimiter(
        &fixture.lookahead.lexer,
        NULL,
        0,
        false,
        &document
      ) == DELIMITER_READ_WORD
    );
    size_t length = strlen(fixtures[index].delimiter);
    assert(document.delimiter_length == length);
    assert(memcmp(document.delimiter, fixtures[index].delimiter, length) == 0);
    assert(document.quoted == fixtures[index].quoted);
    assert(fixture.lookahead.lexer.lookahead == ' ');
    clear_document(&document);
    clear_source_fixture(&fixture);
  }
}

static void
test_embedded_readers_apply_source_views_before_comments_and_closers(void) {
  const struct {
    const char *source;
    size_t stages;
    int32_t next;
  } fixtures[] = {
    {"x=$((1 + $((2)))) ) tail", 1, ' '},
    {"x=$((echo one); (echo two)) ) tail", 1, ' '},
    {"printf '\\\n)'; : ) tail", 1, ' '},
    {": `printf '%s' x` ) tail", 1, ' '},
    {"case x in x) : `case y in esac`;; z) :;; esac)tail", 1, 't'},
    {": #x\\\n)ignored\n) tail", 1, 'i'},
    {": #x\\\n)ignored\n) tail", 2, ' '},
    {"\"`printf #x\\\n) ignored\n`\" ) tail", 1, ' '},
  };
  for (
    size_t index = 0; index < sizeof(fixtures) / sizeof(fixtures[0]); index += 1
  ) {
    struct SourceStage stages[] = {
      source_stage(SOURCE_REMOVE_CONTINUATIONS),
      source_stage(SOURCE_REMOVE_CONTINUATIONS),
    };
    struct SourceFixture fixture;
    init_source_fixture(
      &fixture,
      fixtures[index].source,
      stages,
      fixtures[index].stages
    );
    assert(
      skip_embedded_construct(NULL, &fixture.lookahead.lexer, ')') ==
      ARITHMETIC_VALIDATION_VALID
    );
    assert(fixture.lookahead.lexer.lookahead == fixtures[index].next);
    clear_source_fixture(&fixture);
  }
}

static void test_here_document_readers_stop_at_backquote_boundaries(void) {
  for (unsigned quoted = 0; quoted < 2; quoted += 1) {
    struct SourceStage stages[] = {
      source_stage(SOURCE_REMOVE_CONTINUATIONS),
      source_stage(SOURCE_BACKQUOTE_DECODE),
      source_stage(SOURCE_REMOVE_CONTINUATIONS),
    };
    struct SourceFixture fixture;
    init_source_fixture(&fixture, "`tail", stages, 3);
    struct HereDocument document = {
      .delimiter = (uint8_t *)"END",
      .delimiter_length = 3,
      .quoted = quoted != 0,
    };
    assert(fixture.lookahead.lexer.lookahead == SOURCE_BACKQUOTE_BOUNDARY);
    size_t position = fixture.lookahead.position;
    struct CommandStart start;
    assert(
      read_here_document_line(
        NULL,
        &fixture.lookahead.lexer,
        &document,
        NULL,
        &start
      ) == HERE_DOCUMENT_LINE_END_OF_INPUT
    );
    assert(fixture.lookahead.position == position);
    assert(!fixture.lookahead.failed);
    clear_source_fixture(&fixture);
  }
}

static void test_here_document_readers_keep_the_owning_source_view(void) {
  const struct {
    const char *source;
    const char *delimiter;
    bool quoted;
    bool tabs;
    enum HereDocumentLineKind kind;
    const char *first_word;
  } fixtures[] = {
    {"E\\\nOF\nrest", "EOF", false, false, HERE_DOCUMENT_LINE_DELIMITER, ""},
    {"E\\\nOF\nrest", "EOF", true, false, HERE_DOCUMENT_LINE_CONTENT, ""},
    {"\t\\\n\tEOF\nrest",
      "EOF",
      false,
      true,
      HERE_DOCUMENT_LINE_DELIMITER,
      NULL},
    {"then\nrest", "END", true, false, HERE_DOCUMENT_LINE_CONTENT, "then"},
  };
  for (
    size_t index = 0; index < sizeof(fixtures) / sizeof(fixtures[0]); index += 1
  ) {
    struct SourceStage stage = source_stage(SOURCE_REMOVE_CONTINUATIONS);
    struct SourceFixture fixture;
    init_source_fixture(&fixture, fixtures[index].source, &stage, 1);
    struct HereDocument document = {
      .delimiter = (uint8_t *)fixtures[index].delimiter,
      .delimiter_length = strlen(fixtures[index].delimiter),
      .quoted = fixtures[index].quoted,
      .strip_tabs = fixtures[index].tabs,
    };
    struct CommandStart start = {0};
    assert(
      read_here_document_line(
        NULL,
        &fixture.lookahead.lexer,
        &document,
        NULL,
        &start
      ) == fixtures[index].kind
    );
    if (fixtures[index].first_word != NULL) {
      assert(strcmp(start.first_word, fixtures[index].first_word) == 0);
    }
    clear_source_fixture(&fixture);
  }

  struct SourceStage stages[] = {
    source_stage(SOURCE_REMOVE_CONTINUATIONS),
    source_stage(SOURCE_REMOVE_CONTINUATIONS),
    source_stage(SOURCE_STRIP_LEADING_TABS),
    source_stage(SOURCE_REMOVE_CONTINUATIONS),
    source_stage(SOURCE_BACKQUOTE_DECODE),
    source_stage(SOURCE_REMOVE_CONTINUATIONS),
  };
  for (size_t index = 0; index < 4; index += 1)
    stages[index].disabled = true;
  struct SourceFixture fixture;
  init_source_fixture(&fixture, "\\\\\n world\n", stages, 6);
  struct HereDocument document =
    {.delimiter = (uint8_t *)"END", .delimiter_length = 3, .quoted = true};
  struct SourceContext context = {
    .opener = QUOTED_HERE_DOCUMENT_BODY_START,
    .stage_count = 1,
    .count = 1,
  };
  struct Scanner scanner = {
    .active_documents = &document,
    .active_count = 1,
    .contexts = &context,
    .context_count = 1
  };
  struct CommandStart start;
  assert(
    read_here_document_line(
      &scanner,
      &fixture.lookahead.lexer,
      &document,
      NULL,
      &start
    ) == HERE_DOCUMENT_LINE_LAYOUT
  );
  assert(start.first_character == 0);
  assert(fixture.lookahead.lexer.lookahead == ' ');
  clear_source_fixture(&fixture);
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
  char guarded[TREE_SITTER_SERIALIZATION_BUFFER_SIZE + 2];
  memset(guarded, 0x5a, sizeof(guarded));
  unsigned length =
    tree_sitter_sh_external_scanner_serialize((void *)scanner, guarded + 1);
  assert(length > 0 && length <= TREE_SITTER_SERIALIZATION_BUFFER_SIZE);
  assert(guarded[0] == 0x5a);
  for (size_t index = length + 1; index < sizeof(guarded); index++) {
    assert(guarded[index] == 0x5a);
  }
  memcpy(buffer, guarded + 1, length);
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

static size_t
fill_pending_document_to_capacity(struct Scanner *scanner, size_t index) {
  char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned used = snapshot_scanner(scanner, state);
  struct HereDocument *document = &scanner->pending_documents[index];
  size_t length = document->delimiter_length + sizeof(state) - used;
  assert(document->delimiter_length >= 128 && length < 16384);
  uint8_t *delimiter = ts_realloc(document->delimiter, length);
  assert(delimiter != NULL);
  memset(delimiter, 'D', length);
  document->delimiter = delimiter;
  document->delimiter_length = length;
  assert(snapshot_scanner(scanner, state) == sizeof(state));
  return length;
}

static struct Scanner *make_exact_fit_scanner(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  assert(append_pending_document(scanner, make_repeated_document(512)));
  fill_pending_document_to_capacity(scanner, 0);
  return scanner;
}

static void test_disabled_and_rejected_recovery_scans_preserve_state(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);

  scanner->sequence_end_pending = true;
  scanner->backquote_depth = 3;
  assert(append_pending_document(scanner, make_document("EOF", false, false)));

  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(scanner, before);

  bool valid_symbols[TOKEN_COUNT];
  for (size_t index = 0; index < TOKEN_COUNT; index += 1) {
    valid_symbols[index] = true;
  }

  const struct {
    int32_t source[8];
    size_t length;
  } inputs[] = {
    {{'2', '>', 'x'}, 3},
    {{'|'}, 1},
    {{'\\'}, 1},
    {{0}, 0},
  };
  for (
    size_t index = 0; index < sizeof(inputs) / sizeof(inputs[0]); index += 1
  ) {
    struct MockLexer mock;
    init_mock_lexer(&mock, inputs[index].source, inputs[index].length);
    assert(
      !tree_sitter_sh_external_scanner_scan(scanner, &mock.lexer, valid_symbols)
    );
    assert_scanner_matches_snapshot(scanner, before, before_length);
    bool disabled_symbols[TOKEN_COUNT] = {false};
    init_mock_lexer(&mock, inputs[index].source, inputs[index].length);
    assert(!tree_sitter_sh_external_scanner_scan(
      scanner,
      &mock.lexer,
      disabled_symbols
    ));
    assert_scanner_matches_snapshot(scanner, before, before_length);
  }
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void
test_recovery_emits_newlines_without_reclassifying_continuations(void) {
  const int32_t input[] = {'\n', '\\', '\n', '\\', '\n', '\n', 'x'};
  const struct {
    enum TokenType symbol;
    size_t length;
  } expected[] = {
    {LOGICAL_NEWLINE_BEGIN, 0},
    {LOGICAL_NEWLINE_BEGIN_CHARACTER, 1},
    {SOURCE_BEGIN, 0},
    {LINE_CONTINUATION, 1},
    {REMOVED_NEWLINE, 1},
    {LINE_CONTINUATION, 1},
    {REMOVED_NEWLINE, 1},
    {LOGICAL_NEWLINE_BEGIN, 0},
    {LOGICAL_NEWLINE_BEGIN_CHARACTER, 1},
  };
  struct Scanner scanner = {0};
  bool valid[TOKEN_COUNT];
  for (size_t index = 0; index < TOKEN_COUNT; index += 1) {
    valid[index] = true;
  }
  size_t offset = 0;
  char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned state_length = 0;
  for (
    size_t index = 0; index < sizeof(expected) / sizeof(expected[0]); index += 1
  ) {
    struct MockLexer lexer;
    init_mock_lexer(
      &lexer,
      input + offset,
      sizeof(input) / sizeof(input[0]) - offset
    );
    assert(tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid));
    assert(lexer.lexer.result_symbol == expected[index].symbol);
    assert(lexer.mark == expected[index].length);
    offset += lexer.mark;
    state_length = snapshot_scanner(&scanner, state);
    tree_sitter_sh_external_scanner_deserialize(&scanner, state, state_length);
    assert_scanner_matches_snapshot(&scanner, state, state_length);
  }
  assert(offset == 6);
  assert(!scanner.emission.active);
  struct MockLexer lexer;
  init_mock_lexer(&lexer, input + offset, 1);
  assert(tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid));
  assert(lexer.lexer.result_symbol == WORD_INITIAL_LITERAL_BEGIN);
  assert(lexer.mark == 0);
  assert(scanner.emission.symbol == WORD_INITIAL_LITERAL_BEGIN_WHOLE);
  assert(scanner.emission.remaining == 1);
  clear_scanner(&scanner);
}

static void
test_recovery_preserves_reserved_and_punctuation_token_identity(void) {
  const struct {
    int32_t source[8];
    size_t length;
    enum TokenType begin;
    enum TokenType piece;
    size_t width;
  } cases[] = {
    {{'{', ' '}, 2, LEFT_BRACE, LEFT_BRACE_CHARACTER, 1},
    {{'}', '\n'}, 2, RIGHT_BRACE, RIGHT_BRACE_CHARACTER, 1},
    {{'!', ' '}, 2, PIPELINE_NEGATION, BANG_BEGIN_PIECE, 1},
    {{'(', 'x'},
      2,
      PUNCT_LEFT_PARENTHESIS,
      PUNCT_LEFT_PARENTHESIS_CHARACTER,
      1},
    {{')', 'x'},
      2,
      PUNCT_RIGHT_PARENTHESIS,
      PUNCT_RIGHT_PARENTHESIS_CHARACTER,
      1},
    {{';', 'x'}, 2, PUNCT_SEMICOLON, PUNCT_SEMICOLON_CHARACTER, 1},
    {{'&', 'x'}, 2, PUNCT_AMPERSAND, PUNCT_AMPERSAND_CHARACTER, 1},
    {{'i', 'f', ' '}, 3, IF_KEYWORD, IF_KEYWORD_BEGIN_WHOLE, 2},
    {{'t', 'h', 'e', 'n', ';'}, 5, THEN_KEYWORD, THEN_KEYWORD_BEGIN_WHOLE, 4},
    {{'e', 'l', 'i', 'f', '\n'}, 5, ELIF_KEYWORD, ELIF_KEYWORD_BEGIN_WHOLE, 4},
    {{'e', 'l', 's', 'e', ' '}, 5, ELSE_KEYWORD, ELSE_KEYWORD_BEGIN_WHOLE, 4},
    {{'f', 'i', ';'}, 3, FI_KEYWORD, FI_KEYWORD_BEGIN_WHOLE, 2},
    {{'f', 'o', 'r', ' '}, 4, FOR_KEYWORD, FOR_KEYWORD_BEGIN_WHOLE, 3},
    {{'i', 'n', '\n'}, 3, IN_KEYWORD, IN_KEYWORD_BEGIN_WHOLE, 2},
    {{'d', 'o', '\n'}, 3, DO_KEYWORD, DO_KEYWORD_BEGIN_WHOLE, 2},
    {{'d', 'o', 'n', 'e', ';'}, 5, DONE_KEYWORD, DONE_KEYWORD_BEGIN_WHOLE, 4},
    {{'c', 'a', 's', 'e', ' '}, 5, CASE_KEYWORD, CASE_KEYWORD_BEGIN_WHOLE, 4},
    {{'e', 's', 'a', 'c', '\n'}, 5, ESAC_KEYWORD, ESAC_KEYWORD_BEGIN_WHOLE, 4},
    {{'w', 'h', 'i', 'l', 'e', ' '},
      6,
      WHILE_KEYWORD,
      WHILE_KEYWORD_BEGIN_WHOLE,
      5},
    {{'u', 'n', 't', 'i', 'l', ' '},
      6,
      UNTIL_KEYWORD,
      UNTIL_KEYWORD_BEGIN_WHOLE,
      5},
  };
  bool valid[TOKEN_COUNT];
  for (size_t index = 0; index < TOKEN_COUNT; index += 1) {
    valid[index] = true;
  }
  for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
    struct Scanner scanner = {0};
    struct MockLexer lexer;
    init_mock_lexer(&lexer, cases[index].source, cases[index].length);
    assert(tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid));
    assert(lexer.lexer.result_symbol == cases[index].begin);
    assert(lexer.mark == 0);
    char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    unsigned length = snapshot_scanner(&scanner, state);
    tree_sitter_sh_external_scanner_deserialize(&scanner, state, length);
    init_mock_lexer(&lexer, cases[index].source, cases[index].length);
    assert(tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid));
    assert(lexer.lexer.result_symbol == cases[index].piece);
    assert(lexer.mark == cases[index].width);
    assert(!scanner.emission.active);
    clear_scanner(&scanner);
  }

  const int32_t continued[] = {'f', '\\', '\n', 'i', ';'};
  const struct {
    enum TokenType symbol;
    size_t width;
  } expected[] = {
    {FI_KEYWORD, 0},
    {FI_KEYWORD_BEGIN_PIECE, 1},
    {LINE_CONTINUATION, 1},
    {REMOVED_NEWLINE, 1},
    {FI_KEYWORD_BEGIN_PIECE, 1},
  };
  struct Scanner scanner = {0};
  size_t offset = 0;
  for (
    size_t index = 0; index < sizeof(expected) / sizeof(expected[0]); index += 1
  ) {
    struct MockLexer lexer;
    init_mock_lexer(
      &lexer,
      continued + offset,
      sizeof(continued) / sizeof(continued[0]) - offset
    );
    assert(tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid));
    assert(lexer.lexer.result_symbol == expected[index].symbol);
    assert(lexer.mark == expected[index].width);
    offset += lexer.mark;
    char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    unsigned length = snapshot_scanner(&scanner, state);
    tree_sitter_sh_external_scanner_deserialize(&scanner, state, length);
  }
  assert(offset == 4);
  assert(!scanner.emission.active);
  clear_scanner(&scanner);
}

static void
test_parenthesis_recovery_defers_substitution_state_to_grammar(void) {
  const enum TokenType ends[] = {
    COMMAND_SUBSTITUTION_END,
    ARITHMETIC_EXPANSION_END
  };
  for (size_t index = 0; index < sizeof(ends) / sizeof(ends[0]); index += 1) {
    struct Scanner scanner = {0};
    assert(scanner_source_ready(&scanner));
    assert(source_context_push(&scanner, DQ_OPEN));
    assert(source_context_push(&scanner, COMMAND_OPEN));
    scanner.substitution_depth = 1;
    bool valid[TOKEN_COUNT];
    for (size_t token = 0; token < TOKEN_COUNT; token += 1) {
      valid[token] = true;
    }
    const int32_t source[] = {')'};
    const enum TokenType expected[] = {
      PUNCT_RIGHT_PARENTHESIS,
      PUNCT_RIGHT_PARENTHESIS_CHARACTER
    };
    for (
      size_t part = 0; part < sizeof(expected) / sizeof(expected[0]); part += 1
    ) {
      struct MockLexer lexer;
      init_mock_lexer(&lexer, source, 1);
      assert(
        tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid)
      );
      assert(lexer.lexer.result_symbol == expected[part]);
      assert(lexer.mark == part);
      assert(scanner.context_count == 2);
      assert(scanner.substitution_depth == 1);
      char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
      unsigned length = snapshot_scanner(&scanner, state);
      tree_sitter_sh_external_scanner_deserialize(&scanner, state, length);
    }
    struct MockLexer lexer;
    init_mock_lexer(&lexer, NULL, 0);
    assert(
      !tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid)
    );
    assert(scanner.context_count == 2);
    memset(valid, 0, sizeof(valid));
    valid[ends[index]] = true;
    init_mock_lexer(&lexer, NULL, 0);
    assert(tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid));
    assert(lexer.lexer.result_symbol == ends[index]);
    assert(lexer.mark == 0);
    assert(scanner.context_count == 1);
    assert(scanner.contexts[0].opener == DQ_OPEN);
    assert(
      scanner.substitution_depth ==
      (ends[index] == COMMAND_SUBSTITUTION_END ? 0 : 1)
    );
    clear_scanner(&scanner);
  }
}

static void test_state_round_trip(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  struct Scanner *restored = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  assert(restored != NULL);

  char initial[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned initial_length = snapshot_scanner(scanner, initial);

  assert(scanner_source_ready(scanner));
  assert(source_context_push(scanner, SQ_OPEN));
  scanner->emission = (struct LexicalEmission){
    .symbol = SINGLE_QUOTE_CONTENT_BEGIN,
    .remaining = 128,
    .active = true,
  };

  scanner->expecting_delimiter = true;
  scanner->delimiter_strips_tabs = true;
  scanner->sequence_end_pending = true;
  scanner->at_here_document_line_start = true;
  scanner->backquote_depth = 7;
  scanner->substitution_depth = 5;
  scanner->body_backquote_depth = 3;

  assert(append_pending_document(scanner, make_document("first", true, false)));
  assert(
    append_pending_document(scanner, make_document("second", false, true))
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
  scanner->suspended_frames[0].body_backquote_depth = 6;
  assert(append_document(
    &scanner->suspended_frames[0].documents,
    &scanner->suspended_frames[0].count,
    make_document("suspended", false, true)
  ));
  scanner->pending_documents[1].declaration_depth = 9;

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
  assert(restored->body_backquote_depth == 3);
  assert(restored->source.stage_count == 1);
  assert(restored->source.stages[0].disabled);
  assert(restored->context_count == 1);
  assert(restored->contexts[0].opener == SQ_OPEN);
  assert(restored->contexts[0].stage_count == 1);
  assert(restored->contexts[0].count == 1);
  assert(!restored->contexts[0].local_disabled);
  assert(restored->emission.active);
  assert(restored->emission.symbol == SINGLE_QUOTE_CONTENT_BEGIN);
  assert(restored->emission.remaining == 128);
  assert(!restored->emission.removed_newline);
  assert(restored->pending_count == 2);
  assert_document(&restored->pending_documents[0], "first", true, false);
  assert(restored->pending_documents[0].declaration_depth == 12);
  assert_document(&restored->pending_documents[1], "second", false, true);
  assert(restored->pending_documents[1].declaration_depth == 9);
  assert(restored->active_count == 1);
  assert_document(&restored->active_documents[0], "active", true, true);
  assert(restored->suspended_frame_count == 1);
  assert(restored->suspended_frames[0].at_line_start);
  assert(restored->suspended_frames[0].body_backquote_depth == 6);
  assert(restored->suspended_frames[0].count == 1);
  assert_document(
    &restored->suspended_frames[0].documents[0],
    "suspended",
    false,
    true
  );
  assert_scanner_matches_snapshot(restored, serialized, length);

  tree_sitter_sh_external_scanner_deserialize(restored, NULL, 0);
  assert_scanner_matches_snapshot(restored, initial, initial_length);

  tree_sitter_sh_external_scanner_destroy(restored);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void test_equal_source_contexts_preserve_each_policy_restore(void) {
  struct Scanner scanner = {0};
  assert(scanner_source_ready(&scanner));
  scanner.source.stages[0].disabled = true;
  for (size_t index = 0; index < 2000; index += 1) {
    assert(source_context_push(&scanner, COMMAND_OPEN));
  }
  assert(scanner.context_count == 2);
  assert(scanner.contexts[0].count == 1);
  assert(scanner.contexts[0].local_disabled);
  assert(scanner.contexts[1].count == 1999);
  assert(!scanner.contexts[1].local_disabled);
  assert(source_context_push(&scanner, PARAMETER_OPEN));
  assert(source_context_push(&scanner, COMMAND_OPEN));
  assert(scanner.context_count == 4);
  assert(source_context_pop(&scanner));
  assert(source_context_pop(&scanner));
  assert(scanner.context_count == 2);

  char serialized[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned length = snapshot_scanner(&scanner, serialized);
  struct Scanner restored = {0};
  tree_sitter_sh_external_scanner_deserialize(&restored, serialized, length);
  assert_scanner_matches_snapshot(&restored, serialized, length);
  assert(restored.contexts[1].count == 1999);
  for (size_t index = 0; index < 1999; index += 1) {
    assert(source_context_pop(&restored));
    assert(!restored.source.stages[0].disabled);
  }
  assert(restored.context_count == 1);
  assert(source_context_pop(&restored));
  assert(restored.context_count == 0);
  assert(restored.source.stages[0].disabled);
  assert(!source_context_pop(&restored));
  clear_scanner(&restored);
  clear_scanner(&scanner);
}

static void test_source_context_counts_round_trip_and_reject_overflow(void) {
  const size_t counts[] = {1, 127, 128, 2000, SIZE_MAX};
  for (size_t index = 0; index < sizeof(counts) / sizeof(*counts); index += 1) {
    struct Scanner scanner = {0};
    assert(scanner_source_ready(&scanner));
    assert(source_context_push(&scanner, COMMAND_OPEN));
    scanner.contexts[0].count = counts[index];
    char serialized[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    unsigned length = snapshot_scanner(&scanner, serialized);
    struct Scanner restored = {0};
    tree_sitter_sh_external_scanner_deserialize(&restored, serialized, length);
    assert_scanner_matches_snapshot(&restored, serialized, length);
    assert(restored.context_count == 1);
    assert(restored.contexts[0].count == counts[index]);
    if (counts[index] == SIZE_MAX) {
      assert(!source_context_push(&restored, COMMAND_OPEN));
      assert_scanner_matches_snapshot(&restored, serialized, length);
    }
    clear_scanner(&restored);
    clear_scanner(&scanner);
  }

  struct Scanner scanner = {0};
  struct Scanner empty = {0};
  struct Scanner restored = {0};
  char empty_state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned empty_length = snapshot_scanner(&empty, empty_state);
  assert(scanner_source_ready(&scanner));
  assert(source_context_push(&scanner, COMMAND_OPEN));
  char serialized[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned length = snapshot_scanner(&scanner, serialized);
  assert(serialized[length - 3] == 1);
  serialized[length - 3] = 0;
  tree_sitter_sh_external_scanner_deserialize(&restored, serialized, length);
  assert_scanner_matches_snapshot(&restored, empty_state, empty_length);

  assert(source_context_push(&scanner, PARAMETER_OPEN));
  scanner.contexts[0].count = SIZE_MAX - 1;
  length = snapshot_scanner(&scanner, serialized);
  assert(!source_context_push(&scanner, COMMAND_OPEN));
  assert_scanner_matches_snapshot(&scanner, serialized, length);
  assert(serialized[length - 3] == 1);
  serialized[length - 3] = 2;
  tree_sitter_sh_external_scanner_deserialize(&restored, serialized, length);
  assert_scanner_matches_snapshot(&restored, empty_state, empty_length);
  clear_scanner(&restored);
  clear_scanner(&scanner);
}

static void test_here_document_line_layout_keeps_following_source_owners(void) {
  const struct {
    const char *source;
    bool enabled;
    bool accepted;
  } cases[] = {
    {" \n", true, true},
    {"\t \n", true, true},
    {" #comment\n", true, false},
    {" argument\n", true, false},
    {" >file\n", true, false},
    {" \n", false, false},
  };
  for (size_t index = 0; index < sizeof(cases) / sizeof(*cases); index += 1) {
    struct Scanner scanner = {0};
    assert(scanner_source_ready(&scanner));
    assert(
      append_pending_document(&scanner, make_document("END", false, false))
    );
    char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    unsigned length = snapshot_scanner(&scanner, before);
    struct SourceStage stage = source_stage(SOURCE_REMOVE_CONTINUATIONS);
    struct SourceFixture fixture;
    init_source_fixture(&fixture, cases[index].source, &stage, 1);
    bool valid[TOKEN_COUNT] = {false};
    valid[HERE_DOCUMENT_LINE_LAYOUT_BEGIN] = cases[index].enabled;
    assert(
      scan_dispatch(&scanner, &fixture.lookahead.lexer, valid) ==
      cases[index].accepted
    );
    assert_scanner_matches_snapshot(&scanner, before, length);
    if (cases[index].accepted) {
      assert(
        fixture.lookahead.lexer.result_symbol == HERE_DOCUMENT_LINE_LAYOUT_BEGIN
      );
      assert(fixture.input.mark == 0);
    } else {
      assert(fixture.lookahead.position == 0);
      assert(fixture.lookahead.lexer.lookahead == cases[index].source[0]);
    }
    assert(!fixture.lookahead.failed);
    clear_source_fixture(&fixture);
    clear_scanner(&scanner);
  }
}

static void test_here_document_line_start_only_emits_valid_tokens(void) {
  const struct {
    const char *source;
    bool content_valid;
    bool end_valid;
    enum TokenType expected;
  } fixtures[] = {
    {"body\n", false, false, TOKEN_COUNT},
    {"EOF\n", false, false, TOKEN_COUNT},
    {"body\n", false, true, TOKEN_COUNT},
    {"EOF\n", false, true, HERE_DOCUMENT_END_BEGIN},
    {"body\n", true, false, HERE_DOCUMENT_CONTENT_LINE_START},
    {"EOF\n", true, false, HERE_DOCUMENT_CONTENT_LINE_START},
    {"body\n", true, true, HERE_DOCUMENT_CONTENT_LINE_START},
    {"EOF\n", true, true, HERE_DOCUMENT_END_BEGIN},
  };
  for (size_t quoted = 0; quoted < 2; quoted += 1) {
    for (
      size_t index = 0; index < sizeof(fixtures) / sizeof(fixtures[0]);
      index += 1
    ) {
      struct Scanner scanner = {0};
      assert(append_pending_document(
        &scanner,
        make_document("EOF", quoted != 0, false)
      ));
      assert(activate_startable_pending_documents(&scanner));
      scanner.at_here_document_line_start = true;
      char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
      unsigned before_length = snapshot_scanner(&scanner, before);
      int32_t input[5];
      size_t length = strlen(fixtures[index].source);
      for (size_t character = 0; character < length; character += 1) {
        input[character] = (unsigned char)fixtures[index].source[character];
      }
      struct MockLexer mock;
      init_mock_lexer(&mock, input, length);
      enum TokenType end =
        quoted != 0 ? QUOTED_HERE_DOCUMENT_END_BEGIN : HERE_DOCUMENT_END_BEGIN;
      bool valid_symbols[TOKEN_COUNT] = {false};
      valid_symbols[HERE_DOCUMENT_CONTENT_LINE_START] =
        fixtures[index].content_valid;
      valid_symbols[end] = fixtures[index].end_valid;
      bool accepted = tree_sitter_sh_external_scanner_scan(
        &scanner,
        &mock.lexer,
        valid_symbols
      );
      assert(accepted == (fixtures[index].expected != TOKEN_COUNT));
      if (accepted) {
        enum TokenType expected =
          fixtures[index].expected == HERE_DOCUMENT_END_BEGIN
          ? end
          : fixtures[index].expected;
        assert(mock.lexer.result_symbol == expected);
        assert(valid_symbols[mock.lexer.result_symbol]);
        assert(!scanner.at_here_document_line_start);
        assert(mock.mark == 0);
        assert(scanner.active_count == 1);
        assert_document(
          &scanner.active_documents[0],
          "EOF",
          quoted != 0,
          false
        );
      } else {
        assert_scanner_matches_snapshot(&scanner, before, before_length);
      }
      clear_scanner(&scanner);
    }
  }
}

static void assert_boundary_scan(
  struct Scanner *scanner,
  const char *source,
  const bool *valid,
  enum TokenType expected
) {
  int32_t input[64];
  size_t length = strlen(source);
  assert(length <= sizeof(input) / sizeof(input[0]));
  for (size_t index = 0; index < length; index += 1) {
    input[index] = (unsigned char)source[index];
  }
  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(scanner, before);
  struct MockLexer lexer;
  init_mock_lexer(&lexer, input, length);
  bool accepted =
    tree_sitter_sh_external_scanner_scan(scanner, &lexer.lexer, valid);
  assert(accepted == (expected != TOKEN_COUNT));
  if (accepted) {
    assert(lexer.lexer.result_symbol == expected);
    assert(lexer.mark == 0);
  }
  assert_scanner_matches_snapshot(scanner, before, before_length);
}

static void test_newline_boundaries_require_a_valid_enclosing_terminator(void) {
  const struct {
    const char *source;
    enum TokenType closer;
    bool closes;
  } cases[] = {
    {"\n)\n", PUNCT_RIGHT_PARENTHESIS, true},
    {"\n}\n", RIGHT_BRACE, true},
    {"\nthen\n", THEN_KEYWORD, true},
    {"\nelif\n", ELIF_KEYWORD, true},
    {"\nelse\n", ELSE_KEYWORD, true},
    {"\nfi\n", FI_KEYWORD, true},
    {"\ndo\n", DO_KEYWORD, true},
    {"\ndone\n", DONE_KEYWORD, true},
    {"\nesac\n", ESAC_KEYWORD, true},
    {"\n;;\n", CASE_ITEM_END, true},
    {"\n;&\n", CASE_ITEM_END, true},
    {"\nf\\\ni\n", FI_KEYWORD, true},
    {"\n;\\\n;\n", CASE_ITEM_END, true},
    {"\n}word\n", RIGHT_BRACE, false},
    {"\nfinally\n", FI_KEYWORD, false},
    {"\n;\n", CASE_ITEM_END, false},
    {"\n&\n", CASE_ITEM_END, false},
    {"\n|\n", CASE_ITEM_END, false},
    {"\n&&\n", CASE_ITEM_END, false},
    {"\n||\n", CASE_ITEM_END, false},
  };
  for (unsigned active = 0; active < 2; active += 1) {
    for (unsigned enabled = 0; enabled < 2; enabled += 1) {
      for (
        size_t index = 0; index < sizeof(cases) / sizeof(*cases); index += 1
      ) {
        struct Scanner scanner = {0};
        assert(scanner_source_ready(&scanner));
        if (active != 0) {
          assert(append_pending_document(
            &scanner,
            make_document("END", false, false)
          ));
          assert(activate_startable_pending_documents(&scanner));
          assert(source_context_push(&scanner, HERE_DOCUMENT_BODY_START));
          assert(source_context_push(&scanner, COMMAND_OPEN));
          scanner.substitution_depth = 1;
        }
        bool valid[TOKEN_COUNT] = {false};
        valid[TERM_CONTINUATION] = true;
        valid[TERM_BOUNDARY] = true;
        valid[SEPARATOR_NEWLINE] = true;
        valid[cases[index].closer] = enabled != 0;
        bool closes = enabled != 0 && cases[index].closes;
        enum TokenType expected = closes
          ? active != 0 ? TOKEN_COUNT : TERM_BOUNDARY
          : SEPARATOR_NEWLINE;
        assert_boundary_scan(&scanner, cases[index].source, valid, expected);
        clear_scanner(&scanner);
      }
    }
  }
}

static void test_layout_boundaries_do_not_turn_invalid_source_into_end(void) {
  const struct {
    const char *source;
    enum TokenType continuation;
    enum TokenType closer;
    enum TokenType expected;
  } cases[] = {
    {"\n", TERM_CONTINUATION, TOKEN_COUNT, TOKEN_COUNT},
    {"\n\n", TERM_CONTINUATION, TOKEN_COUNT, TERM_BOUNDARY},
    {" \n)\n", TERM_CONTINUATION, TOKEN_COUNT, TERM_CONTINUATION},
    {" \n)\n", TERM_CONTINUATION, PUNCT_RIGHT_PARENTHESIS, TERM_BOUNDARY},
    {" # note\n|\n", TERM_CONTINUATION, TOKEN_COUNT, TERM_CONTINUATION},
    {";;\n", TERM_CONTINUATION, TOKEN_COUNT, TOKEN_COUNT},
    {";&\n", TERM_CONTINUATION, TOKEN_COUNT, TOKEN_COUNT},
    {";;\n", TERM_CONTINUATION, CASE_ITEM_END, CASE_ITEM_END},
    {";&\n", TERM_CONTINUATION, CASE_ITEM_END, CASE_ITEM_END},
  };
  for (size_t index = 0; index < sizeof(cases) / sizeof(*cases); index += 1) {
    struct Scanner scanner = {0};
    assert(scanner_source_ready(&scanner));
    bool valid[TOKEN_COUNT] = {false};
    valid[cases[index].continuation] = true;
    valid[TERM_BOUNDARY] = true;
    valid[SEPARATOR_NEWLINE] = true;
    if (cases[index].closer != TOKEN_COUNT) {
      valid[cases[index].closer] = true;
    }
    assert_boundary_scan(
      &scanner,
      cases[index].source,
      valid,
      cases[index].expected
    );
    clear_scanner(&scanner);
  }
}

static void test_document_newline_boundaries_preserve_document_ownership(void) {
  const struct {
    const char *source;
    bool active;
    enum TokenType continuation;
    enum TokenType expected;
  } cases[] = {
    {"\necho next\n", false, TERM_CONTINUATION, TOKEN_COUNT},
    {" \necho next\n", false, TERM_CONTINUATION, TOKEN_COUNT},
    {"\nEND\necho next\n", true, TERM_CONTINUATION, TOKEN_COUNT},
    {" \nEND\necho next\n", true, TERM_CONTINUATION, TOKEN_COUNT},
  };
  for (size_t index = 0; index < sizeof(cases) / sizeof(*cases); index += 1) {
    struct Scanner scanner = {0};
    assert(scanner_source_ready(&scanner));
    assert(
      append_pending_document(&scanner, make_document("END", false, false))
    );
    if (cases[index].active) {
      assert(activate_startable_pending_documents(&scanner));
      assert(source_context_push(&scanner, HERE_DOCUMENT_BODY_START));
      assert(source_context_push(&scanner, COMMAND_OPEN));
      scanner.substitution_depth = 1;
    }
    bool valid[TOKEN_COUNT] = {false};
    valid[cases[index].continuation] = true;
    valid[TERM_BOUNDARY] = true;
    valid[SEPARATOR_NEWLINE] = true;
    valid[PRE_NEWLINE_BLANK] = true;
    assert_boundary_scan(
      &scanner,
      cases[index].source,
      valid,
      cases[index].expected
    );
    clear_scanner(&scanner);
  }
}

static void test_old_state_is_rejected(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  struct Scanner *restored = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  assert(restored != NULL);

  scanner->backquote_depth = 4;
  char serialized[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned length = snapshot_scanner(scanner, serialized);
  serialized[0] = SCANNER_SERIALIZATION_VERSION - 1;

  restored->expecting_delimiter = true;
  restored->backquote_depth = 9;
  tree_sitter_sh_external_scanner_deserialize(restored, serialized, length);
  assert(!restored->expecting_delimiter);
  assert(restored->backquote_depth == 0);
  assert(restored->pending_count == 0);

  tree_sitter_sh_external_scanner_destroy(restored);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void test_exact_fit_state_round_trip(void) {
  struct Scanner *scanner = make_exact_fit_scanner();
  struct Scanner *restored = tree_sitter_sh_external_scanner_create();
  assert(restored != NULL);
  size_t delimiter_length = scanner->pending_documents[0].delimiter_length;

  char serialized[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned length = snapshot_scanner(scanner, serialized);
  assert(length == TREE_SITTER_SERIALIZATION_BUFFER_SIZE);

  tree_sitter_sh_external_scanner_deserialize(restored, serialized, length);
  assert(restored->pending_count == 1);
  assert_repeated_document(
    &restored->pending_documents[0],
    'D',
    delimiter_length
  );
  assert_scanner_matches_snapshot(restored, serialized, length);

  tree_sitter_sh_external_scanner_destroy(restored);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void test_pending_document_rejects_oversized_state(void) {
  struct Scanner *scanner = make_exact_fit_scanner();
  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(scanner, before);

  struct HereDocument rejected = make_document("x", false, false);
  assert(!append_pending_document(scanner, rejected));
  assert_scanner_matches_snapshot(scanner, before, before_length);

  clear_document(&rejected);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void test_pending_activation_fits_after_depth_growth(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  scanner->substitution_depth = 16384;
  scanner->backquote_depth = 16384;
  assert(append_pending_document(scanner, make_repeated_document(512)));
  size_t delimiter_length = fill_pending_document_to_capacity(scanner, 0);

  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(scanner, before);
  assert(before_length == TREE_SITTER_SERIALIZATION_BUFFER_SIZE);

  assert(activate_startable_pending_documents(scanner));
  assert(scanner->pending_count == 0);
  assert(scanner->active_count == 1);
  assert(scanner->body_backquote_depth == 16384);
  assert_repeated_document(
    &scanner->active_documents[0],
    'D',
    delimiter_length
  );
  char after[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  assert(snapshot_scanner(scanner, after) == before_length - 1);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void test_pending_activation_fits_after_suspension(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  assert(append_pending_document(scanner, make_repeated_document(503)));
  assert(activate_startable_pending_documents(scanner));
  scanner->at_here_document_line_start = true;
  scanner->substitution_depth = 1;
  assert(append_pending_document(scanner, make_repeated_document(200)));
  assert(append_pending_document(scanner, make_repeated_document(200)));
  size_t last_length = fill_pending_document_to_capacity(scanner, 1);

  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(scanner, before);
  assert(before_length == TREE_SITTER_SERIALIZATION_BUFFER_SIZE);
  assert(activate_startable_pending_documents(scanner));
  assert(scanner->pending_count == 0);
  assert(scanner->active_count == 2);
  assert_repeated_document(&scanner->active_documents[0], 'D', 200);
  assert_repeated_document(&scanner->active_documents[1], 'D', last_length);
  assert(scanner->suspended_frame_count == 1);
  assert(scanner->suspended_frames[0].count == 1);
  assert(scanner->suspended_frames[0].at_line_start);
  assert_repeated_document(
    &scanner->suspended_frames[0].documents[0],
    'D',
    503
  );
  char after[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned after_length = snapshot_scanner(scanner, after);
  assert(after_length == TREE_SITTER_SERIALIZATION_BUFFER_SIZE);
  struct Scanner restored = {0};
  tree_sitter_sh_external_scanner_deserialize(&restored, after, after_length);
  assert_scanner_matches_snapshot(&restored, after, after_length);
  clear_scanner(&restored);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void test_pending_activation_rejects_oversized_state(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  assert(append_pending_document(scanner, make_repeated_document(503)));
  assert(activate_startable_pending_documents(scanner));
  scanner->at_here_document_line_start = true;
  scanner->substitution_depth = 1;
  assert(append_pending_document(scanner, make_repeated_document(400)));
  fill_pending_document_to_capacity(scanner, 0);
  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(scanner, before);
  assert(before_length == TREE_SITTER_SERIALIZATION_BUFFER_SIZE);
  assert(!activate_startable_pending_documents(scanner));
  assert_scanner_matches_snapshot(scanner, before, before_length);
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void test_strict_scalar_encoding(void) {
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

static void test_control_escape_table(void) {
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
  struct Scanner scanner = {0};
  assert(append_pending_document(
    &scanner,
    make_document_bytes(delimiter, delimiter_length, quoted, strip_tabs)
  ));
  assert(activate_startable_pending_documents(&scanner));
  struct MockLexer mock;
  init_mock_lexer(&mock, line, line_length);
  bool body_valid[TOKEN_COUNT] = {false};
  body_valid
    [quoted ? QUOTED_HERE_DOCUMENT_BODY_START : HERE_DOCUMENT_BODY_START] =
      true;
  assert(
    tree_sitter_sh_external_scanner_scan(&scanner, &mock.lexer, body_valid)
  );
  assert(mock.mark == 0);
  init_mock_lexer(&mock, line, line_length);
  bool end_valid[TOKEN_COUNT] = {false};
  end_valid[quoted ? QUOTED_HERE_DOCUMENT_END_BEGIN : HERE_DOCUMENT_END_BEGIN] =
    true;
  bool result =
    tree_sitter_sh_external_scanner_scan(&scanner, &mock.lexer, end_valid);
  clear_scanner(&scanner);
  return result;
}

static void test_byte_delimiter_matching(void) {
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

static bool scan_delimiter_fixture(
  struct Scanner *scanner,
  const int32_t *input,
  size_t length
) {
  struct MockLexer mock;
  init_mock_lexer(&mock, input, length);
  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[HERE_END_BEGIN] = true;
  bool result =
    tree_sitter_sh_external_scanner_scan(scanner, &mock.lexer, valid_symbols);
  if (result) {
    assert(mock.lexer.result_symbol == HERE_END_BEGIN);
    assert(mock.mark == 0);
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
  assert(scanner->pending_count == 1);
  assert_document(
    &scanner->pending_documents[0],
    expected_delimiter,
    quoted,
    false
  );

  free(characters);
  clear_scanner(scanner);
}

static void test_double_quoted_parameter_delimiters(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);

  const char *fixtures[][2] = {
    {"\"${x:-'EOF'}\"\n", "${x:-'EOF'}"},
    {"\"${x:-'}\"\n", "${x:-'}"},
    {"\"${x:-a\\qb}\"\n", "${x:-a\\qb}"},
    {"\"${x:-a\\}b}\"\n", "${x:-a}b}"},
    {"\"${x:-${y:-'EOF'}}\"\n", "${x:-${y:-'EOF'}}"},
    {"\"${x:-$(printf 'EOF')}\"\n", "${x:-$(printf EOF)}"},
    {"\"${x#'EOF'}\"\n", "${x#EOF}"},
    {"\"${12%%'EOF'}\"\n", "${12%%EOF}"},
    {"\"${?#'EOF'}\"\n", "${?#EOF}"},
    {"\"${x:-${y#'EOF'}'tail'}\"\n", "${x:-${y#EOF}'tail'}"},
  };
  for (
    size_t index = 0; index < sizeof(fixtures) / sizeof(fixtures[0]); index += 1
  ) {
    assert_text_delimiter_fixture(
      scanner,
      fixtures[index][0],
      fixtures[index][1],
      true
    );
  }

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void test_arithmetic_delimiters_preserve_nested_quote_context(void) {
  struct Scanner *scanner = tree_sitter_sh_external_scanner_create();
  assert(scanner != NULL);
  const struct {
    const char *source;
    const char *delimiter;
    bool quoted;
  } fixtures[] = {
    {"\"$((1+${x:-'2'}))\"\n", "$((1+${x:-'2'}))", true},
    {"$((1+${x:-'2'}))\n", "$((1+${x:-'2'}))", false},
    {"$((1+${x:-'}))\n", "$((1+${x:-'}))", false},
    {"$((1+${x:-a\\qb}))\n", "$((1+${x:-a\\qb}))", false},
    {"$((1+${x:-a\\}b}))\n", "$((1+${x:-a}b}))", true},
    {"$((1+${x:-\\$y}))\n", "$((1+${x:-$y}))", true},
    {"$((1+${x#'2'}))\n", "$((1+${x#2}))", true},
    {"$((1+$((2+${x:-'3'}))))\n", "$((1+$((2+${x:-'3'}))))", false},
    {"$((1+$(printf '2')))\n", "$((1+$(printf 2)))", true},
    {"$((1+`printf \\\"2\\\"`))\n", "$((1+`printf 2`))", true},
  };
  for (
    size_t index = 0; index < sizeof(fixtures) / sizeof(fixtures[0]); index += 1
  ) {
    assert_text_delimiter_fixture(
      scanner,
      fixtures[index].source,
      fixtures[index].delimiter,
      fixtures[index].quoted
    );
  }
  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void test_substitution_hash_delimiter_words(void) {
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

static void test_dollar_single_quote_escape_units(void) {
  const struct {
    int32_t source[9];
    size_t length;
    size_t end;
    int32_t following;
  } cases[] = {
    {{'\\', 'x', '1', '2', '3', 'g'}, 6, 5, 'g'},
    {{'\\', 'x', 'a', 'B', 'C', 'D', 'E', 'f', '\''}, 9, 8, '\''},
    {{'\\', 'x', '4', '1', '\''}, 5, 4, '\''},
    {{'\\', 'x', 'F', 'g'}, 4, 3, 'g'},
    {{'\\', '1', '2', '3', '4'}, 5, 4, '4'},
    {{'\\', 'x', '1', '2', '3'}, 5, 5, 0},
  };
  for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
    struct MockLexer mock;
    init_mock_lexer(&mock, cases[index].source, cases[index].length);
    assert(scan_dollar_single_quote_token(&mock.lexer));
    assert(mock.lexer.result_symbol == DOLLAR_SINGLE_QUOTE_ESCAPE);
    assert(mock.offset == cases[index].end);
    assert(mock.mark == cases[index].end);
    assert(mock.lexer.lookahead == cases[index].following);
    assert(mock_eof(&mock.lexer) == (cases[index].end == cases[index].length));
  }
}

static void test_dollar_single_quote_delimiter_bytes(void) {
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
  assert_document_bytes(&scanner->pending_documents[0], del, sizeof(del), true);
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
    &scanner->pending_documents[0],
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
    &scanner->pending_documents[0],
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
    &scanner->pending_documents[0],
    unspecified_fallback,
    sizeof(unspecified_fallback),
    true
  );

  tree_sitter_sh_external_scanner_destroy(scanner);
}

static void test_delimiter_scan_resource_rollback(void) {
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
static void test_delimiter_allocation_failures_preserve_state(void) {
  const char delimiter[] = "$((1))"
                           "abcdefghijklmnopqrstuvwxyz"
                           "abcdefghijklmnopqrstuvwxyz"
                           "abcdefghijklmnopqrstuvwxyz";
  int32_t input[sizeof(delimiter)];
  for (size_t index = 0; index + 1 < sizeof(delimiter); index += 1) {
    input[index] = (unsigned char)delimiter[index];
  }
  input[sizeof(delimiter) - 1] = '\n';
  bool valid_symbols[TOKEN_COUNT] = {false};
  valid_symbols[HERE_END_BEGIN] = true;
  size_t allocation_count = 0;
  for (size_t failure = 0; failure <= allocation_count; failure += 1) {
    assert(reuse_live_allocations == 0);
    struct Scanner scanner = {
      .expecting_delimiter = true,
      .delimiter_strips_tabs = true,
    };
    char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    unsigned length = snapshot_scanner(&scanner, before);
    struct MockLexer mock;
    init_mock_lexer(&mock, input, sizeof(input) / sizeof(input[0]));
    size_t calls = reuse_allocation_calls;
    if (failure > 0) {
      reuse_fail_allocation_call = calls + failure;
    }
    bool accepted = tree_sitter_sh_external_scanner_scan(
      &scanner,
      &mock.lexer,
      valid_symbols
    );
    if (failure == 0) {
      allocation_count = reuse_allocation_calls - calls;
      assert(allocation_count > 0);
      assert(accepted);
      assert(mock.lexer.result_symbol == HERE_END_BEGIN);
      assert(scanner.pending_count == 1);
      assert_document(&scanner.pending_documents[0], delimiter, false, true);
    } else {
      assert(reuse_allocation_calls >= reuse_fail_allocation_call);
      reuse_fail_allocation_call = 0;
      assert(!accepted);
      assert_scanner_matches_snapshot(&scanner, before, length);
    }
    clear_scanner(&scanner);
    assert(reuse_live_allocations == 0);
  }
}

static void test_pending_activation_allocation_failures_preserve_state(void) {
  size_t allocation_count = 0;
  for (size_t failure = 0; failure <= allocation_count; failure += 1) {
    assert(reuse_live_allocations == 0);
    struct Scanner scanner = {0};
    assert(
      append_pending_document(&scanner, make_document("retained", false, false))
    );
    scanner.substitution_depth = 1;
    assert(
      append_pending_document(&scanner, make_document("suspended", true, false))
    );
    assert(activate_startable_pending_documents(&scanner));
    scanner.substitution_depth = 2;
    assert(
      append_pending_document(&scanner, make_document("activated", false, true))
    );
    char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    unsigned length = snapshot_scanner(&scanner, before);
    size_t calls = reuse_allocation_calls;
    if (failure > 0) {
      reuse_fail_allocation_call = calls + failure;
    }
    bool accepted = activate_startable_pending_documents(&scanner);
    if (failure == 0) {
      allocation_count = reuse_allocation_calls - calls;
      assert(allocation_count > 0);
      assert(accepted);
      assert(scanner.pending_count == 1);
      assert_document(&scanner.pending_documents[0], "retained", false, false);
      assert(scanner.active_count == 1);
      assert_document(&scanner.active_documents[0], "activated", false, true);
      assert(scanner.suspended_frame_count == 1);
      assert(scanner.suspended_frames[0].count == 1);
      assert_document(
        &scanner.suspended_frames[0].documents[0],
        "suspended",
        true,
        false
      );
    } else {
      assert(reuse_allocation_calls >= reuse_fail_allocation_call);
      reuse_fail_allocation_call = 0;
      assert(!accepted);
      assert_scanner_matches_snapshot(&scanner, before, length);
    }
    clear_scanner(&scanner);
    assert(reuse_live_allocations == 0);
  }
}

static void test_reuse_allocator_realloc_failure_rolls_back(void) {
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

static void test_reuse_allocator_contract(void) {
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

static void assert_source_step(
  struct Scanner *scanner,
  const int32_t *input,
  size_t length,
  enum TokenType symbol,
  size_t mark
) {
  bool valid[TOKEN_COUNT] = {false};
  valid[symbol] = true;
  struct MockLexer lexer;
  init_mock_lexer(&lexer, input, length);
  assert(tree_sitter_sh_external_scanner_scan(scanner, &lexer.lexer, valid));
  assert(lexer.lexer.result_symbol == symbol);
  assert(lexer.mark == mark);
  char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned state_length = snapshot_scanner(scanner, state);
  tree_sitter_sh_external_scanner_deserialize(scanner, state, state_length);
  assert_scanner_matches_snapshot(scanner, state, state_length);
}

static void
test_separators_preserve_operator_identity_and_pending_documents(void) {
  const struct {
    const char *source;
    enum TokenType expected;
    enum TokenType piece;
    size_t width;
  } cases[] = {
    {";", PUNCT_SEMICOLON, PUNCT_SEMICOLON_CHARACTER, 1},
    {"; echo next\n", PUNCT_SEMICOLON, PUNCT_SEMICOLON_CHARACTER, 1},
    {";\nfi\n", PUNCT_SEMICOLON, PUNCT_SEMICOLON_CHARACTER, 1},
    {"; )\n", PUNCT_SEMICOLON, PUNCT_SEMICOLON_CHARACTER, 1},
    {"&", PUNCT_AMPERSAND, PUNCT_AMPERSAND_CHARACTER, 1},
    {"& # note\necho next\n", PUNCT_AMPERSAND, PUNCT_AMPERSAND_CHARACTER, 1},
    {"& )\n", PUNCT_AMPERSAND, PUNCT_AMPERSAND_CHARACTER, 1},
    {";;", DSEMI_BEGIN, DSEMI_BEGIN_PIECE, 2},
    {";&", SEMI_AND_BEGIN, SEMI_AND_BEGIN_PIECE, 2},
    {"&&", AND_IF_BEGIN, AND_IF_BEGIN_PIECE, 2},
    {";\\\n;", DSEMI_BEGIN, DSEMI_BEGIN_PIECE, 4},
    {";\\\n&", SEMI_AND_BEGIN, SEMI_AND_BEGIN_PIECE, 4},
    {"&\\\n&", AND_IF_BEGIN, AND_IF_BEGIN_PIECE, 4},
  };
  for (unsigned document = 0; document < 3; document += 1) {
    for (unsigned recovery = 0; recovery < 2; recovery += 1) {
      for (
        size_t index = 0; index < sizeof(cases) / sizeof(*cases); index += 1
      ) {
        struct Scanner scanner = {0};
        assert(scanner_source_ready(&scanner));
        if (document != 0) {
          assert(append_pending_document(
            &scanner,
            make_document("END", false, false)
          ));
          if (document == 2) {
            assert(activate_startable_pending_documents(&scanner));
            assert(source_context_push(&scanner, HERE_DOCUMENT_BODY_START));
            assert(source_context_push(&scanner, COMMAND_OPEN));
            scanner.substitution_depth = 1;
          }
        }
        int32_t input[32];
        size_t length = strlen(cases[index].source);
        assert(length <= sizeof(input) / sizeof(*input));
        for (size_t offset = 0; offset < length; offset += 1) {
          input[offset] = (unsigned char)cases[index].source[offset];
        }
        bool valid[TOKEN_COUNT];
        for (size_t symbol = 0; symbol < TOKEN_COUNT; symbol += 1) {
          valid[symbol] = recovery != 0;
        }
        valid[PUNCT_SEMICOLON] = true;
        valid[PUNCT_AMPERSAND] = true;
        char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
        unsigned before_length = snapshot_scanner(&scanner, before);
        struct MockLexer lexer;
        init_mock_lexer(&lexer, input, length);
        bool accepted =
          tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid);
        assert(accepted == (recovery != 0 || cases[index].width == 1));
        if (accepted) {
          assert(lexer.lexer.result_symbol == cases[index].expected);
          assert(lexer.mark == 0);
          char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
          unsigned state_length = snapshot_scanner(&scanner, state);
          tree_sitter_sh_external_scanner_deserialize(
            &scanner,
            state,
            state_length
          );
          if (cases[index].width == 4) {
            assert_source_step(&scanner, input, length, cases[index].piece, 1);
            assert_source_step(
              &scanner,
              input + 1,
              length - 1,
              LINE_CONTINUATION,
              1
            );
            assert_source_step(
              &scanner,
              input + 2,
              length - 2,
              REMOVED_NEWLINE,
              1
            );
            assert_source_step(
              &scanner,
              input + 3,
              length - 3,
              cases[index].piece,
              1
            );
          } else {
            assert_source_step(
              &scanner,
              input,
              length,
              cases[index].piece,
              cases[index].width
            );
          }
        }
        assert_scanner_matches_snapshot(&scanner, before, before_length);
        clear_scanner(&scanner);
      }
    }
  }
}

static void
test_logical_lexical_emission_keeps_individual_physical_pairs(void) {
  const int32_t input[] = {'f', 'o', '\\', '\n', '\\', '\n', 'o', ' '};
  struct Scanner scanner = {0};
  const struct {
    enum TokenType symbol;
    size_t mark;
  } steps[] = {
    {WORD_INITIAL_LITERAL_BEGIN, 0},
    {WORD_INITIAL_LITERAL_BEGIN_PIECE, 2},
    {LINE_CONTINUATION, 1},
    {REMOVED_NEWLINE, 1},
    {LINE_CONTINUATION, 1},
    {REMOVED_NEWLINE, 1},
    {WORD_INITIAL_LITERAL_BEGIN_PIECE, 1},
  };
  size_t offset = 0;
  for (size_t index = 0; index < sizeof(steps) / sizeof(steps[0]); index += 1) {
    assert_source_step(
      &scanner,
      input + offset,
      sizeof(input) / sizeof(input[0]) - offset,
      steps[index].symbol,
      steps[index].mark
    );
    offset += steps[index].mark;
    if (offset == 7) {
      assert(!scanner.emission.active);
      assert(scanner.emission.remaining == 0);
      assert(scanner.emission.symbol == 0);
    }
  }
  assert(offset == 7);
  assert(!scanner.emission.active);
  assert(scanner.context_count == 0);
  clear_scanner(&scanner);
}

static void
test_word_entry_classifies_and_owns_source_in_normal_and_recovery_scans(void) {
  const struct {
    const char *source;
    enum TokenType symbol;
    enum TokenType piece;
    size_t remaining;
    size_t first_width;
  } fixtures[] = {
    {"echo ",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_WHOLE,
      4,
      4},
    {"e\\\ncho ",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_PIECE,
      6,
      1},
    {"esacx;",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_WHOLE,
      5,
      5},
    {"fi\\\nx;",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_PIECE,
      5,
      2},
    {"donex\n",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_WHOLE,
      5,
      5},
    {"thenx ",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_WHOLE,
      5,
      5},
    {"elifx ",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_WHOLE,
      5,
      5},
    {"elsex ",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_WHOLE,
      5,
      5},
    {"dox ",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_WHOLE,
      3,
      3},
    {"ifx ",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_WHOLE,
      3,
      3},
    {"inx ",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_WHOLE,
      3,
      3},
    {"forx ",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_WHOLE,
      4,
      4},
    {"casex ",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_WHOLE,
      5,
      5},
    {"whilex ",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_WHOLE,
      6,
      6},
    {"untilx ",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_WHOLE,
      6,
      6},
    {"{x ", WORD_INITIAL_LITERAL_BEGIN, WORD_INITIAL_LITERAL_BEGIN_WHOLE, 2, 2},
    {"}x ", WORD_INITIAL_LITERAL_BEGIN, WORD_INITIAL_LITERAL_BEGIN_WHOLE, 2, 2},
    {"}\\\nx ",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_PIECE,
      4,
      1},
    {"!x ", WORD_INITIAL_LITERAL_BEGIN, WORD_INITIAL_LITERAL_BEGIN_WHOLE, 2, 2},
    {"'x' ", WORD_INITIAL_SQ_OPEN, WORD_INITIAL_SQ_OPEN_CHARACTER, 1, 1},
    {"\"x\" ", WORD_INITIAL_DQ_OPEN, WORD_INITIAL_DQ_OPEN_CHARACTER, 1, 1},
    {"${x} ",
      WORD_INITIAL_DOLLAR_EXPANSION_START,
      WORD_INITIAL_DOLLAR_EXPANSION_START_CHARACTER,
      1,
      1},
    {"$'x' ",
      WORD_INITIAL_DOLLAR_SQ_DOLLAR,
      WORD_INITIAL_DOLLAR_SQ_DOLLAR_CHARACTER,
      1,
      1},
    {"`x` ",
      WORD_INITIAL_BACKQUOTE_START,
      WORD_INITIAL_BACKQUOTE_START_CHARACTER,
      1,
      1},
    {"\\x ",
      WORD_INITIAL_ESCAPED_CHARACTER_BEGIN,
      WORD_INITIAL_ESCAPED_CHARACTER_BEGIN_PIECE,
      2,
      1},
    {"* ",
      WORD_INITIAL_PATTERN_STAR_BEGIN,
      WORD_INITIAL_PATTERN_STAR_BEGIN_PIECE,
      1,
      1},
    {"? ",
      WORD_INITIAL_PATTERN_QUESTION_BEGIN,
      WORD_INITIAL_PATTERN_QUESTION_BEGIN_PIECE,
      1,
      1},
    {"[ab] ",
      WORD_INITIAL_PATTERN_BRACKET_OPEN,
      WORD_INITIAL_PATTERN_BRACKET_OPEN_CHARACTER,
      1,
      1},
    {"[ab ",
      WORD_INITIAL_FALLBACK_LITERAL_BEGIN,
      WORD_INITIAL_FALLBACK_LITERAL_BEGIN_PIECE,
      3,
      3},
  };
  for (size_t recovery = 0; recovery < 2; recovery += 1) {
    for (
      size_t index = 0; index < sizeof(fixtures) / sizeof(fixtures[0]);
      index += 1
    ) {
      int32_t input[16];
      size_t length = strlen(fixtures[index].source);
      assert(length <= sizeof(input) / sizeof(input[0]));
      for (size_t character = 0; character < length; character += 1) {
        input[character] = (unsigned char)fixtures[index].source[character];
      }
      struct Scanner scanner = {0};
      bool valid[TOKEN_COUNT] = {false};
      for (size_t symbol = 0; symbol < TOKEN_COUNT; symbol += 1) {
        valid[symbol] = recovery !=
          0 ||
          (symbol >=
            WORD_INITIAL_LITERAL_BEGIN &&
            symbol <= WORD_INITIAL_BACKQUOTE_START);
      }
      valid[WORD_INITIAL_LITERAL_BEGIN_WHOLE] = true;
      struct MockLexer lexer;
      init_mock_lexer(&lexer, input, length);
      assert(
        tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid)
      );
      assert(lexer.lexer.result_symbol == fixtures[index].symbol);
      assert(lexer.mark == 0);
      assert(scanner.emission.active);
      assert(scanner.emission.remaining == fixtures[index].remaining);
      assert(scanner.context_count == 0);
      char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
      unsigned state_length = snapshot_scanner(&scanner, state);
      tree_sitter_sh_external_scanner_deserialize(
        &scanner,
        state,
        state_length
      );
      assert_scanner_matches_snapshot(&scanner, state, state_length);

      bool disabled[TOKEN_COUNT] = {false};
      disabled[fixtures[index].symbol] = true;
      init_mock_lexer(&lexer, input, length);
      assert(
        !tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, disabled)
      );
      assert_scanner_matches_snapshot(&scanner, state, state_length);

      enum TokenType piece = fixtures[index].piece;
      valid[piece] = true;
      init_mock_lexer(&lexer, input, length);
      assert(
        tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid)
      );
      assert(lexer.lexer.result_symbol == piece);
      assert(lexer.mark == fixtures[index].first_width);
      assert(
        scanner.emission.remaining == fixtures[index].remaining - lexer.mark
      );
      state_length = snapshot_scanner(&scanner, state);
      tree_sitter_sh_external_scanner_deserialize(
        &scanner,
        state,
        state_length
      );
      assert_scanner_matches_snapshot(&scanner, state, state_length);
      clear_scanner(&scanner);
    }
  }
}

static void
test_whole_source_requires_its_classification_and_nonempty_range(void) {
  const struct {
    const char *source;
    enum TokenType begin;
    enum TokenType whole;
    enum TokenType piece;
    size_t width;
  } fixtures[] = {
    {"echo ",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_WHOLE,
      WORD_INITIAL_LITERAL_BEGIN_PIECE,
      4},
    {"word\\\n next",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_WHOLE,
      WORD_INITIAL_LITERAL_BEGIN_PIECE,
      4},
    {"fi;", FI_KEYWORD, FI_KEYWORD_BEGIN_WHOLE, FI_KEYWORD_BEGIN_PIECE, 2},
    {"fi\\\n;", FI_KEYWORD, FI_KEYWORD_BEGIN_WHOLE, FI_KEYWORD_BEGIN_PIECE, 2},
  };
  for (
    size_t index = 0; index < sizeof(fixtures) / sizeof(*fixtures); index += 1
  ) {
    const size_t length = strlen(fixtures[index].source);
    int32_t source[16];
    assert(length <= sizeof(source) / sizeof(*source));
    for (size_t offset = 0; offset < length; offset += 1) {
      source[offset] = (unsigned char)fixtures[index].source[offset];
    }
    struct Scanner scanner = {0};
    bool valid[TOKEN_COUNT] = {false};
    valid[fixtures[index].whole] = true;
    struct MockLexer lexer;
    init_mock_lexer(&lexer, source, length);
    assert(
      !tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid)
    );
    assert(!scanner.emission.active);
    valid[fixtures[index].begin] = true;
    init_mock_lexer(&lexer, source, 0);
    assert(
      !tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid)
    );
    init_mock_lexer(&lexer, source, length);
    assert(tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid));
    assert(lexer.lexer.result_symbol == fixtures[index].begin);
    assert(lexer.mark == 0);
    assert(scanner.emission.symbol == fixtures[index].whole);
    assert(scanner.emission.remaining == fixtures[index].width);
    char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    unsigned state_length = snapshot_scanner(&scanner, state);
    tree_sitter_sh_external_scanner_deserialize(&scanner, state, state_length);
    assert_scanner_matches_snapshot(&scanner, state, state_length);

    valid[fixtures[index].whole] = false;
    valid[fixtures[index].piece] = true;
    init_mock_lexer(&lexer, source, length);
    assert(
      !tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid)
    );
    assert_scanner_matches_snapshot(&scanner, state, state_length);
    valid[fixtures[index].whole] = true;
    init_mock_lexer(&lexer, source, fixtures[index].width - 1);
    assert(
      !tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid)
    );
    assert_scanner_matches_snapshot(&scanner, state, state_length);
    init_mock_lexer(&lexer, source, length);
    assert(tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid));
    assert(lexer.lexer.result_symbol == fixtures[index].whole);
    assert(lexer.mark == fixtures[index].width);
    assert(!scanner.emission.active);
    assert(scanner.context_count == 0);
    clear_scanner(&scanner);
  }
}

static void test_tilde_word_entry_uses_the_grammar_owned_lexical_context(void) {
  const int32_t source[] = {'~', 'h', 'o', 'm', 'e', ' '};
  const struct {
    bool tilde;
    bool recovery;
    enum TokenType symbol;
    enum TokenType emission;
    size_t remaining;
  } fixtures[] = {
    {false, false, WORD_INITIAL_LITERAL_BEGIN, WORD_INITIAL_LITERAL_BEGIN, 5},
    {true, false, WORD_TILDE_START, WORD_TILDE_START, 1},
    {true, true, WORD_TILDE_START, WORD_TILDE_START, 1},
  };
  for (
    size_t index = 0; index < sizeof(fixtures) / sizeof(fixtures[0]); index += 1
  ) {
    struct Scanner scanner = {0};
    bool valid[TOKEN_COUNT] = {false};
    for (size_t symbol = 0; symbol < TOKEN_COUNT; symbol += 1) {
      valid[symbol] = fixtures[index].recovery;
    }
    valid[WORD_INITIAL_LITERAL_BEGIN] = true;
    valid[WORD_TILDE_START] = fixtures[index].tilde;
    struct MockLexer lexer;
    init_mock_lexer(&lexer, source, sizeof(source) / sizeof(source[0]));
    assert(tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid));
    assert(lexer.lexer.result_symbol == fixtures[index].symbol);
    assert(lexer.mark == 0);
    assert(scanner.emission.symbol == fixtures[index].emission);
    assert(scanner.emission.remaining == fixtures[index].remaining);
    assert(scanner.context_count == 0);
    clear_scanner(&scanner);
  }
}

static void test_pending_emission_rejects_disabled_and_stale_source(void) {
  const int32_t source[] = {'x', '\\', '\n', 'y', ' '};
  struct Scanner scanner = {0};
  assert_source_step(&scanner, source, 5, LITERAL_BEGIN, 0);
  assert_source_step(&scanner, source, 5, LITERAL_BEGIN_PIECE, 1);
  assert_source_step(&scanner, source + 1, 4, LINE_CONTINUATION, 1);
  assert(scanner.emission.removed_newline);
  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(&scanner, before);
  const int32_t stale[] = {'x', 'y', ' '};
  const struct {
    const int32_t *input;
    size_t length;
    enum TokenType valid;
  } rejected[] = {
    {source + 2, 3, LITERAL_BEGIN_PIECE},
    {source + 2, 3, LINE_CONTINUATION},
    {stale, 3, REMOVED_NEWLINE},
    {stale, 0, REMOVED_NEWLINE},
  };
  for (
    size_t index = 0; index < sizeof(rejected) / sizeof(rejected[0]); index += 1
  ) {
    bool valid[TOKEN_COUNT] = {false};
    valid[rejected[index].valid] = true;
    struct MockLexer lexer;
    init_mock_lexer(&lexer, rejected[index].input, rejected[index].length);
    assert(
      !tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid)
    );
    assert_scanner_matches_snapshot(&scanner, before, before_length);
  }
  assert_source_step(&scanner, source + 2, 3, REMOVED_NEWLINE, 1);
  assert_source_step(&scanner, source + 3, 2, LITERAL_BEGIN_PIECE, 1);
  assert(!scanner.emission.active);
  clear_scanner(&scanner);
}

static void test_lexical_source_rejects_another_begin_after_restore(void) {
  const struct {
    const char *source;
    enum TokenType begin;
    enum TokenType piece;
    enum TokenType other;
    size_t width;
    size_t contexts;
  } fixtures[] = {
    {"fi ",
      FI_KEYWORD,
      FI_KEYWORD_BEGIN_PIECE,
      LOGICAL_BLANK_BEGIN_PIECE,
      2,
      0},
    {")file",
      PUNCT_RIGHT_PARENTHESIS,
      PUNCT_RIGHT_PARENTHESIS_CHARACTER,
      PUNCT_GREATER_CHARACTER,
      1,
      0},
    {"echo ",
      WORD_INITIAL_LITERAL_BEGIN,
      WORD_INITIAL_LITERAL_BEGIN_PIECE,
      LITERAL_BEGIN_PIECE,
      4,
      0},
    {"value ",
      ASSIGNMENT_LITERAL_BEGIN,
      ASSIGNMENT_LITERAL_BEGIN_PIECE,
      PARAMETER_LITERAL_BEGIN_PIECE,
      5,
      0},
    {"'x'",
      WORD_INITIAL_SQ_OPEN,
      WORD_INITIAL_SQ_OPEN_CHARACTER,
      SQ_CLOSE_CHARACTER,
      1,
      1},
  };
  for (unsigned recovery = 0; recovery < 2; recovery += 1) {
    for (
      size_t index = 0; index < sizeof(fixtures) / sizeof(*fixtures); index += 1
    ) {
      int32_t source[8];
      size_t length = strlen(fixtures[index].source);
      assert(length <= sizeof(source) / sizeof(*source));
      for (size_t offset = 0; offset < length; offset += 1) {
        source[offset] = (unsigned char)fixtures[index].source[offset];
      }
      struct Scanner scanner = {0};
      assert_source_step(&scanner, source, length, fixtures[index].begin, 0);
      assert(scanner.emission.symbol == fixtures[index].begin);
      char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
      unsigned state_length = snapshot_scanner(&scanner, state);
      bool valid[TOKEN_COUNT] = {false};
      valid[fixtures[index].other] = true;
      struct MockLexer lexer;
      init_mock_lexer(&lexer, source, length);
      assert(
        !tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid)
      );
      assert_scanner_matches_snapshot(&scanner, state, state_length);
      for (size_t token = 0; token < TOKEN_COUNT; token += 1) {
        valid[token] = recovery != 0;
      }
      valid[fixtures[index].piece] = true;
      init_mock_lexer(&lexer, source, length);
      assert(
        tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid)
      );
      assert(lexer.lexer.result_symbol == fixtures[index].piece);
      assert(lexer.mark == fixtures[index].width);
      assert(!scanner.emission.active);
      assert(scanner.context_count == fixtures[index].contexts);
      clear_scanner(&scanner);
    }
  }
}

static void test_physical_prefix_and_character_keep_the_same_begin(void) {
  const int32_t source[] = {'\\', '$', 'x'};
  struct Scanner scanner = {0};
  assert(scanner_source_ready(&scanner));
  assert(source_context_push(&scanner, BACKQUOTE_START));
  assert_source_step(
    &scanner,
    source,
    3,
    WORD_INITIAL_DOLLAR_EXPANSION_START,
    0
  );
  assert(scanner.emission.remaining == 2);
  const struct {
    enum TokenType source;
    enum TokenType other;
  } pieces[] = {
    {WORD_INITIAL_DOLLAR_EXPANSION_START_PREFIX,
      WORD_INITIAL_DOLLAR_SQ_DOLLAR_PREFIX},
    {WORD_INITIAL_DOLLAR_EXPANSION_START_CHARACTER,
      WORD_INITIAL_DOLLAR_SQ_DOLLAR_CHARACTER},
  };
  for (size_t index = 0; index < sizeof(pieces) / sizeof(*pieces); index += 1) {
    char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    unsigned state_length = snapshot_scanner(&scanner, state);
    bool valid[TOKEN_COUNT] = {false};
    valid[pieces[index].other] = true;
    struct MockLexer lexer;
    init_mock_lexer(&lexer, source + index, 3 - index);
    assert(
      !tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid)
    );
    assert_scanner_matches_snapshot(&scanner, state, state_length);
    assert_source_step(
      &scanner,
      source + index,
      3 - index,
      pieces[index].source,
      1
    );
    assert(scanner.context_count == 1);
    assert(scanner.contexts[0].opener == BACKQUOTE_START);
  }
  assert(!scanner.emission.active);
  clear_scanner(&scanner);
}

static void test_saved_source_requires_a_begin_token(void) {
  const int32_t source[] = {'e', 'c', 'h', 'o', ' '};
  const enum TokenType owners[] = {
    HERE_END_COMMIT,
    WORD_INITIAL_LITERAL_BEGIN_PIECE,
    PUNCT_RIGHT_PARENTHESIS_PREFIX,
  };
  struct Scanner scanner = {0};
  assert_source_step(&scanner, source, 5, WORD_INITIAL_LITERAL_BEGIN, 0);
  for (size_t index = 0; index < sizeof(owners) / sizeof(*owners); index += 1) {
    scanner.emission.symbol = owners[index];
    char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    unsigned state_length = snapshot_scanner(&scanner, state);
    struct Scanner restored = {0};
    tree_sitter_sh_external_scanner_deserialize(&restored, state, state_length);
    assert(!restored.emission.active);
    assert(restored.source.stage_count == 0);
    assert(restored.context_count == 0);
    clear_scanner(&restored);
  }
  clear_scanner(&scanner);
}

static void test_quote_context_commits_after_physical_punctuation(void) {
  const int32_t source[] = {'\'', 'a', '\\', '\n', 'b', '\'', ' '};
  struct Scanner scanner = {0};
  assert_source_step(&scanner, source, 7, WORD_INITIAL_SQ_OPEN, 0);
  assert(scanner.context_count == 0);
  assert_source_step(&scanner, source, 7, WORD_INITIAL_SQ_OPEN_CHARACTER, 1);
  assert(scanner.context_count == 1);
  assert(scanner.source.stages[0].disabled);
  assert_source_step(&scanner, source + 1, 6, SINGLE_QUOTE_CONTENT_BEGIN, 0);
  assert_source_step(
    &scanner,
    source + 1,
    6,
    SINGLE_QUOTE_CONTENT_BEGIN_PIECE,
    1
  );
  assert_source_step(
    &scanner,
    source + 2,
    5,
    SINGLE_QUOTE_CONTENT_BEGIN_PIECE,
    1
  );
  assert_source_step(
    &scanner,
    source + 3,
    4,
    SINGLE_QUOTE_CONTENT_BEGIN_PIECE,
    2
  );
  assert(!scanner.emission.active);
  assert_source_step(&scanner, source + 5, 2, SQ_CLOSE, 0);
  assert(scanner.context_count == 1);
  assert_source_step(&scanner, source + 5, 2, SQ_CLOSE_CHARACTER, 1);
  assert(scanner.context_count == 0);
  assert(!scanner.source.stages[0].disabled);
  clear_scanner(&scanner);
}

static void test_source_context_growth_at_capacity_preserves_state(void) {
  const int32_t source[] = {'`', 'x'};
  struct Scanner scanner = {0};
  assert_source_step(&scanner, source, 2, BACKQUOTE_START, 0);
  assert(append_pending_document(&scanner, make_repeated_document(512)));
  fill_pending_document_to_capacity(&scanner, 0);
  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned length = snapshot_scanner(&scanner, before);
  bool valid[TOKEN_COUNT] = {false};
  valid[BACKQUOTE_START_CHARACTER] = true;
  struct MockLexer lexer;
  init_mock_lexer(&lexer, source, 2);
  assert(!tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid));
  assert_scanner_matches_snapshot(&scanner, before, length);
  clear_scanner(&scanner);
}

#ifdef TREE_SITTER_REUSE_ALLOCATOR
static void test_embedded_here_document_read_failures_terminate(void) {
  assert(reuse_live_allocations == 0);
  size_t allocation_count = 0;
  for (size_t failure = 0; failure <= allocation_count; failure += 1) {
    struct SourceStage stage = source_stage(SOURCE_REMOVE_CONTINUATIONS);
    struct SourceFixture fixture;
    init_source_fixture(&fixture, "body\nEND\ntail", &stage, 1);
    struct EmbeddedSkip skip = {0};
    assert(embedded_append_pending(&skip, make_document("END", false, false)));
    size_t first_allocation = reuse_allocation_calls;
    if (failure > 0)
      reuse_fail_allocation_call = first_allocation + failure;
    enum ArithmeticValidation result =
      skip_embedded_here_document_bodies(NULL, &fixture.lookahead.lexer, &skip);
    reuse_fail_allocation_call = 0;
    if (failure == 0) {
      allocation_count = reuse_allocation_calls - first_allocation;
      assert(allocation_count > 0);
      assert(result == ARITHMETIC_VALIDATION_VALID);
      assert(fixture.lookahead.lexer.lookahead == 't');
    } else {
      assert(result == ARITHMETIC_VALIDATION_RESOURCE_FAILURE);
      assert(fixture.native.failed);
    }
    assert(skip.pending_count == 0);
    clear_embedded_skip(&skip);
    clear_source_fixture(&fixture);
    assert(reuse_live_allocations == 0);
  }
}

static void test_source_callback_allocation_failures_are_transactional(void) {
  assert(reuse_live_allocations == 0);
  const int32_t source[] = {'f', 'o', '\\', '\n', 'o', ' '};
  const enum TokenType symbols[] = {
    WORD_INITIAL_LITERAL_BEGIN,
    WORD_INITIAL_LITERAL_BEGIN_PIECE,
    LINE_CONTINUATION,
    REMOVED_NEWLINE,
  };
  const size_t offsets[] = {0, 0, 2, 3};
  const size_t marks[] = {0, 2, 1, 1};
  for (
    size_t phase = 0; phase < sizeof(symbols) / sizeof(symbols[0]); phase += 1
  ) {
    struct Scanner scanner = {0};
    for (size_t previous = 0; previous < phase; previous += 1) {
      assert_source_step(
        &scanner,
        source + offsets[previous],
        6 - offsets[previous],
        symbols[previous],
        marks[previous]
      );
    }
    char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    unsigned before_length = snapshot_scanner(&scanner, before);
    bool valid[TOKEN_COUNT] = {false};
    valid[symbols[phase]] = true;
    struct MockLexer lexer;
    init_mock_lexer(&lexer, source + offsets[phase], 6 - offsets[phase]);
    size_t first_allocation = reuse_allocation_calls;
    assert(tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid));
    size_t allocation_count = reuse_allocation_calls - first_allocation;
    assert(allocation_count > 0);
    clear_scanner(&scanner);
    for (size_t failure = 1; failure <= allocation_count; failure += 1) {
      tree_sitter_sh_external_scanner_deserialize(
        &scanner,
        before,
        before_length
      );
      reuse_fail_allocation_call = reuse_allocation_calls + failure;
      init_mock_lexer(&lexer, source + offsets[phase], 6 - offsets[phase]);
      assert(
        !tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid)
      );
      reuse_fail_allocation_call = 0;
      assert_scanner_matches_snapshot(&scanner, before, before_length);
      clear_scanner(&scanner);
      assert(reuse_live_allocations == 0);
    }
  }
}

static void
test_reserved_word_allocation_failures_do_not_fall_back_to_word(void) {
  const int32_t source[] = {'i', 'f', ' '};
  bool valid[TOKEN_COUNT] = {false};
  valid[IF_KEYWORD] = true;
  valid[WORD_INITIAL_LITERAL_BEGIN] = true;
  size_t allocation_count = 0;
  for (size_t failure = 0; failure <= allocation_count; failure += 1) {
    struct Scanner scanner = {0};
    char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    unsigned length = snapshot_scanner(&scanner, before);
    struct MockLexer lexer;
    init_mock_lexer(&lexer, source, 3);
    size_t calls = reuse_allocation_calls;
    if (failure > 0)
      reuse_fail_allocation_call = calls + failure;
    bool accepted =
      tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid);
    reuse_fail_allocation_call = 0;
    if (failure == 0) {
      allocation_count = reuse_allocation_calls - calls;
      assert(allocation_count > 0);
      assert(accepted);
      assert(lexer.lexer.result_symbol == IF_KEYWORD);
      assert(lexer.mark == 0);
    } else {
      assert(!accepted);
      assert_scanner_matches_snapshot(&scanner, before, length);
    }
    clear_scanner(&scanner);
    assert(reuse_live_allocations == 0);
  }
}

static void test_comment_boundary_allocation_failures_terminate(void) {
  const int32_t source[] = {' ', '#', ' ', '`', '\n'};
  bool valid[TOKEN_COUNT] = {false};
  valid[TERM_CONTINUATION] = true;
  valid[TRAILING_COMMENT_BOUNDARY] = true;
  struct Scanner scanner = {0};
  assert(scanner_source_ready(&scanner));
  assert(source_context_push(&scanner, BACKQUOTE_START));
  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(&scanner, before);
  struct MockLexer lexer;
  init_mock_lexer(&lexer, source, sizeof(source) / sizeof(source[0]));
  size_t first_allocation = reuse_allocation_calls;
  assert(tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid));
  size_t allocation_count = reuse_allocation_calls - first_allocation;
  assert(allocation_count > 0);
  assert(lexer.lexer.result_symbol == TRAILING_COMMENT_BOUNDARY);
  assert(lexer.mark == 0);
  clear_scanner(&scanner);
  assert(reuse_live_allocations == 0);
  for (size_t failure = 1; failure <= allocation_count; failure += 1) {
    tree_sitter_sh_external_scanner_deserialize(
      &scanner,
      before,
      before_length
    );
    init_mock_lexer(&lexer, source, sizeof(source) / sizeof(source[0]));
    reuse_fail_allocation_call = reuse_allocation_calls + failure;
    assert(
      !tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid)
    );
    assert(reuse_allocation_calls >= reuse_fail_allocation_call);
    reuse_fail_allocation_call = 0;
    assert_scanner_matches_snapshot(&scanner, before, before_length);
    clear_scanner(&scanner);
    assert(reuse_live_allocations == 0);
  }
}

static void
test_document_boundary_allocation_failures_do_not_close_the_term(void) {
  const int32_t source[] =
    {' ', '\n', 'w', 'o', 'r', 'd', '\n', 'E', 'N', 'D', '\n'};
  bool valid[TOKEN_COUNT] = {false};
  valid[TERM_CONTINUATION] = true;
  valid[TERM_BOUNDARY] = true;
  valid[PRE_NEWLINE_BLANK] = true;
  struct Scanner scanner = {0};
  assert(scanner_source_ready(&scanner));
  assert(append_pending_document(&scanner, make_document("END", false, false)));
  assert(activate_startable_pending_documents(&scanner));
  assert(source_context_push(&scanner, HERE_DOCUMENT_BODY_START));
  assert(source_context_push(&scanner, COMMAND_OPEN));
  scanner.substitution_depth = 1;
  char before[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
  unsigned before_length = snapshot_scanner(&scanner, before);
  struct MockLexer lexer;
  init_mock_lexer(&lexer, source, sizeof(source) / sizeof(source[0]));
  size_t first_allocation = reuse_allocation_calls;
  assert(tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid));
  size_t allocation_count = reuse_allocation_calls - first_allocation;
  assert(allocation_count > 0);
  assert(lexer.lexer.result_symbol == TERM_CONTINUATION);
  assert(lexer.mark == 0);
  assert_scanner_matches_snapshot(&scanner, before, before_length);
  clear_scanner(&scanner);
  for (size_t failure = 1; failure <= allocation_count; failure += 1) {
    tree_sitter_sh_external_scanner_deserialize(
      &scanner,
      before,
      before_length
    );
    reuse_fail_allocation_call = reuse_allocation_calls + failure;
    init_mock_lexer(&lexer, source, sizeof(source) / sizeof(source[0]));
    assert(
      !tree_sitter_sh_external_scanner_scan(&scanner, &lexer.lexer, valid)
    );
    assert(reuse_allocation_calls >= reuse_fail_allocation_call);
    reuse_fail_allocation_call = 0;
    assert_scanner_matches_snapshot(&scanner, before, before_length);
    clear_scanner(&scanner);
    assert(reuse_live_allocations == 0);
  }
}
#endif

int main(void) {
  test_lookahead_replays_raw_cuts_in_distinct_quote_views();
  test_delimiter_source_views_preserve_quotedness_and_bytes();
  test_embedded_readers_apply_source_views_before_comments_and_closers();
  test_here_document_readers_keep_the_owning_source_view();
  test_here_document_readers_stop_at_backquote_boundaries();
  test_disabled_and_rejected_recovery_scans_preserve_state();
  test_recovery_emits_newlines_without_reclassifying_continuations();
  test_recovery_preserves_reserved_and_punctuation_token_identity();
  test_parenthesis_recovery_defers_substitution_state_to_grammar();
  test_state_round_trip();
  test_equal_source_contexts_preserve_each_policy_restore();
  test_source_context_counts_round_trip_and_reject_overflow();
  test_here_document_line_layout_keeps_following_source_owners();
  test_here_document_line_start_only_emits_valid_tokens();
  test_newline_boundaries_require_a_valid_enclosing_terminator();
  test_layout_boundaries_do_not_turn_invalid_source_into_end();
  test_document_newline_boundaries_preserve_document_ownership();
  test_old_state_is_rejected();
  test_exact_fit_state_round_trip();
  test_pending_document_rejects_oversized_state();
  test_pending_activation_fits_after_depth_growth();
  test_pending_activation_fits_after_suspension();
  test_pending_activation_rejects_oversized_state();
  test_strict_scalar_encoding();
  test_control_escape_table();
  test_byte_delimiter_matching();
  test_double_quoted_parameter_delimiters();
  test_arithmetic_delimiters_preserve_nested_quote_context();
  test_substitution_hash_delimiter_words();
  test_dollar_single_quote_escape_units();
  test_dollar_single_quote_delimiter_bytes();
  test_delimiter_scan_resource_rollback();
  test_logical_lexical_emission_keeps_individual_physical_pairs();
  test_word_entry_classifies_and_owns_source_in_normal_and_recovery_scans();
  test_whole_source_requires_its_classification_and_nonempty_range();
  test_separators_preserve_operator_identity_and_pending_documents();
  test_tilde_word_entry_uses_the_grammar_owned_lexical_context();
  test_pending_emission_rejects_disabled_and_stale_source();
  test_lexical_source_rejects_another_begin_after_restore();
  test_physical_prefix_and_character_keep_the_same_begin();
  test_saved_source_requires_a_begin_token();
  test_quote_context_commits_after_physical_punctuation();
  test_source_context_growth_at_capacity_preserves_state();
#ifdef TREE_SITTER_REUSE_ALLOCATOR
  test_embedded_here_document_read_failures_terminate();
  test_delimiter_allocation_failures_preserve_state();
  test_pending_activation_allocation_failures_preserve_state();
  test_reuse_allocator_realloc_failure_rolls_back();
  test_source_callback_allocation_failures_are_transactional();
  test_reserved_word_allocation_failures_do_not_fall_back_to_word();
  test_comment_boundary_allocation_failures_terminate();
  test_document_boundary_allocation_failures_do_not_close_the_term();
  test_reuse_allocator_contract();
#endif
  return 0;
}
