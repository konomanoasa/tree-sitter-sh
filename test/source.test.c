#include <assert.h>
#include <stdlib.h>

static size_t source_allocations;
static size_t source_live_allocations;
static size_t source_allocation_failure;

static void *source_test_malloc(size_t size) {
  source_allocations += 1;
  if (source_allocations == source_allocation_failure) {
    return NULL;
  }
  void *result = malloc(size);
  source_live_allocations += result != NULL;
  return result;
}

static void *source_test_calloc(size_t count, size_t width) {
  source_allocations += 1;
  if (source_allocations == source_allocation_failure) {
    return NULL;
  }
  void *result = calloc(count, width);
  source_live_allocations += result != NULL;
  return result;
}

static void *source_test_realloc(void *pointer, size_t size) {
  source_allocations += 1;
  if (source_allocations == source_allocation_failure) {
    return NULL;
  }
  int fresh = pointer == NULL;
  void *result = realloc(pointer, size);
  source_live_allocations += fresh && result != NULL;
  return result;
}

static void source_test_free(void *pointer) {
  if (pointer != NULL) {
    assert(source_live_allocations > 0);
    source_live_allocations -= 1;
  }
  free(pointer);
}

#define ts_malloc source_test_malloc
#define ts_calloc source_test_calloc
#define ts_realloc source_test_realloc
#define ts_free source_test_free
#include "../src/input.h"

struct SourceTestInput {
  const int32_t *characters;
  size_t length;
  size_t position;
  size_t calls;
  bool fail;
};

static enum SourceResult source_test_read(void *payload, int32_t *value) {
  struct SourceTestInput *input = payload;
  input->calls += 1;
  if (input->fail) {
    return SOURCE_FAILURE;
  }
  if (input->position == input->length) {
    return SOURCE_END;
  }
  *value = input->characters[input->position++];
  return SOURCE_CHARACTER;
}

static void source_expect(
  struct SourceCursor *cursor,
  const int32_t *expected,
  size_t length
) {
  for (size_t index = 0; index < length; index += 1) {
    struct SourceCharacter character;
    assert(source_next(cursor, &character) == SOURCE_CHARACTER);
    assert(character.value == expected[index]);
  }
  struct SourceCharacter character;
  assert(source_next(cursor, &character) == SOURCE_END);
}

static void test_source_reading_preserves_scalars_and_nul(void) {
  const int32_t input[] = {'a', 0, 0x1f642, '\\', '\n', 'b'};
  struct SourceTestInput reader = {input, 6, 0, 0, false};
  struct SourceStage stage = source_stage(SOURCE_REMOVE_CONTINUATIONS);
  struct SourceCursor cursor;
  assert(source_cursor_init(&cursor, &stage, 1, source_test_read, &reader));
  const int32_t expected[] = {'a', 0, 0x1f642, 'b'};
  source_expect(&cursor, expected, 4);
  assert(cursor.logical[2].origin.start == 2);
  assert(cursor.logical[2].origin.end == 3);
  assert(cursor.logical[3].origin.start == 5);
  assert(cursor.removal_count == 1);
  assert(cursor.removals[0].slash == 3);
  assert(cursor.removals[0].origin.start == 4);
  assert(cursor.removals[0].origin.end == 5);
  source_cursor_clear(&cursor);

  reader = (struct SourceTestInput){input, 6, 0, 0, false};
  stage.disabled = true;
  assert(source_cursor_init(&cursor, &stage, 1, source_test_read, &reader));
  source_expect(&cursor, input, 6);
  assert(cursor.removal_count == 0);
  source_cursor_clear(&cursor);
}

static void test_ancestor_removal_survives_local_quoting(void) {
  struct SourceStage stages[] = {
    source_stage(SOURCE_REMOVE_CONTINUATIONS),
    source_stage(SOURCE_REMOVE_CONTINUATIONS),
  };
  stages[1].disabled = true;
  const int32_t input[] = {'x', '\\', '\n', 'y'};
  struct SourceTestInput reader = {input, 4, 0, 0, false};
  struct SourceCursor cursor;
  assert(source_cursor_init(&cursor, stages, 2, source_test_read, &reader));
  const int32_t expected[] = {'x', 'y'};
  source_expect(&cursor, expected, 2);
  assert(cursor.removal_count == 1);
  assert(cursor.removals[0].stage == 0);
  assert(cursor.removals[0].slash == 1);
  source_cursor_clear(&cursor);
}

static void test_backquote_source_records_units_and_boundaries(void) {
  const int32_t input[] = {'\\', '\\', '\\', '$', '\\', 96, 96};
  struct SourceTestInput reader = {input, 7, 0, 0, false};
  struct SourceStage stage = source_stage(SOURCE_BACKQUOTE_DECODE);
  struct SourceCursor cursor;
  assert(source_cursor_init(&cursor, &stage, 1, source_test_read, &reader));
  const int32_t expected[] = {'\\', '$', 96, 96};
  source_expect(&cursor, expected, 4);
  assert(cursor.logical[0].origin.start == 0);
  assert(cursor.logical[0].origin.end == 2);
  assert(cursor.logical[0].origin.character == 1);
  assert(cursor.logical[1].origin.start == 2);
  assert(cursor.logical[1].origin.character == 3);
  assert(cursor.logical[2].boundary_stage == SIZE_MAX);
  assert(cursor.logical[3].boundary_stage == 0);
  source_cursor_clear(&cursor);

  struct SourceStage stages[] = {stage, stage};
  const int32_t nested[] = {'\\', 96};
  reader = (struct SourceTestInput){nested, 2, 0, 0, false};
  assert(source_cursor_init(&cursor, stages, 2, source_test_read, &reader));
  const int32_t tick[] = {96};
  source_expect(&cursor, tick, 1);
  assert(cursor.logical[0].boundary_stage == 1);
  assert(cursor.logical[0].origin.start == 0);
  assert(cursor.logical[0].origin.character == 1);
  source_cursor_clear(&cursor);

  const int32_t nested_opener[] = {'\\', '\\', '\\', 96};
  reader = (struct SourceTestInput){nested_opener, 4, 0, 0, false};
  assert(source_cursor_init(&cursor, stages, 2, source_test_read, &reader));
  source_expect(&cursor, tick, 1);
  assert(cursor.logical[0].boundary_stage == SIZE_MAX);
  assert(cursor.logical[0].origin.start == 0);
  assert(cursor.logical[0].origin.end == 4);
  assert(cursor.logical[0].origin.character == 3);
  source_cursor_clear(&cursor);
}

static void test_source_pass_order_controls_removal(void) {
  const int32_t input[] = {'\\', '\\', '\n'};
  const int32_t literal[] = {'\\', '\n'};
  struct SourceStage stages[] = {
    source_stage(SOURCE_REMOVE_CONTINUATIONS),
    source_stage(SOURCE_BACKQUOTE_DECODE),
  };
  struct SourceCursor cursor;
  struct SourceTestInput reader = {input, 3, 0, 0, false};
  assert(source_cursor_init(&cursor, stages, 2, source_test_read, &reader));
  source_expect(&cursor, literal, 2);
  assert(cursor.removal_count == 0);
  source_cursor_clear(&cursor);

  stages[0] = source_stage(SOURCE_BACKQUOTE_DECODE);
  stages[1] = source_stage(SOURCE_REMOVE_CONTINUATIONS);
  reader = (struct SourceTestInput){input, 3, 0, 0, false};
  assert(source_cursor_init(&cursor, stages, 2, source_test_read, &reader));
  source_expect(&cursor, NULL, 0);
  assert(cursor.removal_count == 1);
  assert(cursor.removals[0].slash == 1);
  source_cursor_clear(&cursor);
}

static void test_adjacent_runs_and_bare_newlines_follow_each_pass(void) {
  struct SourceStage stages[] = {
    source_stage(SOURCE_REMOVE_CONTINUATIONS),
    source_stage(SOURCE_BACKQUOTE_DECODE),
    source_stage(SOURCE_REMOVE_CONTINUATIONS),
    source_stage(SOURCE_BACKQUOTE_DECODE),
  };
  stages[1].quoted = true;
  stages[3].quoted = true;
  const int32_t retained[] = {'\\', '\\', '\\', '\n', '\\', '\\', '\n', 'w'};
  const int32_t continued[] = {
    '\\',
    '\\',
    '\\',
    '\n',
    '\\',
    '\\',
    '\\',
    '\\',
    '\n',
    'w',
  };
  const int32_t bare[] = {'\\', '\\', '\\', '\n', '\n', 'w'};
  const struct {
    const int32_t *input;
    size_t length;
    int32_t output[3];
    size_t output_length;
    size_t removal_count;
    int64_t second_slash;
  } cases[] = {
    {retained, 8, {'\\', '\n', 'w'}, 3, 1, 0},
    {continued, 10, {'\\', 'w'}, 2, 2, 7},
    {bare, 6, {'w'}, 1, 2, INT64_MIN},
  };
  for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
    struct SourceTestInput reader =
      {cases[index].input, cases[index].length, 0, 0, false};
    struct SourceCursor cursor;
    assert(source_cursor_init(&cursor, stages, 4, source_test_read, &reader));
    source_expect(&cursor, cases[index].output, cases[index].output_length);
    assert(cursor.removal_count == cases[index].removal_count);
    assert(cursor.removals[0].slash == 2);
    if (cursor.removal_count == 2) {
      assert(cursor.removals[1].slash == cases[index].second_slash);
      assert(cursor.removals[1].stage == 2);
    }
    source_cursor_clear(&cursor);
  }
}

static void test_tab_stripping_follows_joined_line_starts(void) {
  struct SourceStage stages[] = {
    source_stage(SOURCE_REMOVE_CONTINUATIONS),
    source_stage(SOURCE_STRIP_LEADING_TABS),
  };
  const int32_t input[] = {'\t', '\\', '\n', '\t', 'I', 'N', '\n'};
  const int32_t expected[] = {'I', 'N', '\n'};
  struct SourceTestInput reader = {input, 7, 0, 0, false};
  struct SourceCursor cursor;
  assert(source_cursor_init(&cursor, stages, 2, source_test_read, &reader));
  source_expect(&cursor, expected, 3);
  assert(cursor.removal_count == 3);
  assert(cursor.removals[0].kind == SOURCE_REMOVED_TAB);
  assert(cursor.removals[1].kind == SOURCE_REMOVED_CONTINUATION);
  assert(cursor.removals[2].kind == SOURCE_REMOVED_TAB);
  source_cursor_clear(&cursor);

  const int32_t spaced[] = {' ', '\\', '\n', '\t', 'I', 'N', '\n'};
  const int32_t spaced_expected[] = {' ', '\t', 'I', 'N', '\n'};
  reader = (struct SourceTestInput){spaced, 7, 0, 0, false};
  assert(source_cursor_init(&cursor, stages, 2, source_test_read, &reader));
  source_expect(&cursor, spaced_expected, 5);
  assert(cursor.removal_count == 1);
  source_cursor_clear(&cursor);
}

static void test_every_raw_cut_restores_pending_source_without_rereading(void) {
  const int32_t input[] =
    {'a', '\\', '\\', '\\', '\n', '\\', '\\', '\n', 0x1f642, '\\'};
  struct SourceStage stages[] = {
    source_stage(SOURCE_REMOVE_CONTINUATIONS),
    source_stage(SOURCE_BACKQUOTE_DECODE),
    source_stage(SOURCE_REMOVE_CONTINUATIONS),
    source_stage(SOURCE_BACKQUOTE_DECODE),
  };
  struct SourceTestInput reader = {input, 10, 0, 0, false};
  struct SourceCursor cursor;
  assert(source_cursor_init(&cursor, stages, 4, source_test_read, &reader));
  struct SourceCharacter character;
  while (source_next(&cursor, &character) == SOURCE_CHARACTER) {
  }
  assert(!cursor.failed);
  size_t reads = reader.calls;
  for (size_t cut = 0; cut <= 10; cut += 1) {
    struct SourceSnapshot snapshot = {0};
    assert(source_snapshot_at(&cursor, cut, &snapshot));
    assert(reader.calls == reads);
    assert(snapshot.raw_position == (int64_t)cut);
    uint8_t encoded[SOURCE_STATE_CAPACITY];
    size_t length;
    assert(
      source_snapshot_serialize(&snapshot, encoded, sizeof(encoded), &length)
    );
    struct SourceSnapshot restored = {0};
    assert(source_snapshot_deserialize(&restored, encoded, length));
    struct SourceTestInput suffix = {input + cut, 10 - cut, 0, 0, false};
    struct SourceCursor resumed;
    assert(
      source_cursor_resume(&resumed, &restored, source_test_read, &suffix)
    );
    size_t index = snapshot.logical_count;
    while (source_next(&resumed, &character) == SOURCE_CHARACTER) {
      assert(index < cursor.logical_count);
      const struct SourceCharacter *expected = &cursor.logical[index++];
      assert(character.value == expected->value);
      assert(character.origin.start + (int64_t)cut == expected->origin.start);
      assert(character.origin.end + (int64_t)cut == expected->origin.end);
      assert(
        character.origin.character + (int64_t)cut == expected->origin.character
      );
      assert(character.boundary_stage == expected->boundary_stage);
    }
    assert(!resumed.failed);
    assert(index == cursor.logical_count);
    assert(
      snapshot.removal_count + resumed.removal_count == cursor.removal_count
    );
    for (size_t event = 0; event < resumed.removal_count; event += 1) {
      const struct SourceRemoval *actual = &resumed.removals[event];
      const struct SourceRemoval *expected =
        &cursor.removals[snapshot.removal_count + event];
      assert(actual->kind == expected->kind);
      assert(actual->origin.start + (int64_t)cut == expected->origin.start);
      assert(
        actual->slash == INT64_MIN
          ? expected->slash == INT64_MIN
          : actual->slash + (int64_t)cut == expected->slash
      );
    }
    source_cursor_clear(&resumed);
    source_snapshot_clear(&restored);
    source_snapshot_clear(&snapshot);
  }
  source_cursor_clear(&cursor);
}

static void test_snapshot_capacity_and_invalid_data_are_transactional(void) {
  struct SourceStage stage = source_stage(SOURCE_REMOVE_CONTINUATIONS);
  struct SourceCursor cursor;
  assert(source_cursor_init(&cursor, &stage, 1, NULL, NULL));
  assert(source_feed(&cursor, '\\'));
  struct SourceSnapshot snapshot = {0};
  assert(source_checkpoint(&cursor, &snapshot));
  uint8_t encoded[SOURCE_STATE_CAPACITY];
  size_t length;
  assert(
    source_snapshot_serialize(&snapshot, encoded, sizeof(encoded), &length)
  );
  size_t unchanged = length;
  uint8_t shortened[SOURCE_STATE_CAPACITY];
  assert(!source_snapshot_serialize(&snapshot, shortened, length - 1, &length));
  assert(length == 0);
  assert(snapshot.stages[0].has_pending);
  for (size_t prefix = 0; prefix < unchanged; prefix += 1) {
    assert(!source_snapshot_deserialize(&snapshot, encoded, prefix));
    assert(snapshot.stages[0].has_pending);
  }
  const uint8_t invalid[] = {1, 0, 9, 0};
  assert(!source_snapshot_deserialize(&snapshot, invalid, sizeof(invalid)));
  assert(snapshot.stages[0].has_pending);
  source_snapshot_clear(&snapshot);
  source_cursor_clear(&cursor);
}

static void test_source_failures_release_owned_memory(void) {
  struct SourceStage stages[] = {
    source_stage(SOURCE_REMOVE_CONTINUATIONS),
    source_stage(SOURCE_BACKQUOTE_DECODE),
  };
  const int32_t input[] = {'a', '\\', '\n', 'b'};
  for (size_t failure = 1; failure <= 12; failure += 1) {
    source_allocations = 0;
    source_allocation_failure = failure;
    struct SourceTestInput reader = {input, 4, 0, 0, false};
    struct SourceCursor cursor;
    if (source_cursor_init(&cursor, stages, 2, source_test_read, &reader)) {
      struct SourceCharacter character;
      while (source_next(&cursor, &character) == SOURCE_CHARACTER) {
      }
      struct SourceSnapshot snapshot = {0};
      (void)source_checkpoint(&cursor, &snapshot);
      source_snapshot_clear(&snapshot);
    }
    source_cursor_clear(&cursor);
    assert(source_live_allocations == 0);
  }
  source_allocation_failure = 0;
  struct SourceTestInput reader = {input, 4, 0, 0, true};
  struct SourceCursor cursor;
  assert(source_cursor_init(&cursor, stages, 2, source_test_read, &reader));
  struct SourceCharacter character;
  assert(source_next(&cursor, &character) == SOURCE_FAILURE);
  source_cursor_clear(&cursor);
}

struct SourceNativeMock {
  TSLexer lexer;
  const int32_t *input;
  size_t length;
  size_t position;
  size_t advances;
  size_t marks;
};

static void source_native_advance(TSLexer *lexer, bool skip) {
  struct SourceNativeMock *mock = (struct SourceNativeMock *)lexer;
  assert(!skip);
  assert(mock->position < mock->length);
  mock->position += 1;
  mock->advances += 1;
  lexer->lookahead =
    mock->position < mock->length ? mock->input[mock->position] : 0;
}

static void source_native_mark(TSLexer *lexer) {
  struct SourceNativeMock *mock = (struct SourceNativeMock *)lexer;
  mock->marks += 1;
}

static bool source_native_eof(const TSLexer *lexer) {
  const struct SourceNativeMock *mock = (const struct SourceNativeMock *)lexer;
  return mock->position == mock->length;
}

static void source_native_init(
  struct SourceNativeMock *mock,
  const int32_t *input,
  size_t length
) {
  *mock = (struct SourceNativeMock){
    .lexer =
      {
        .lookahead = length > 0 ? input[0] : 0,
        .advance = source_native_advance,
        .mark_end = source_native_mark,
        .eof = source_native_eof,
      },
    .input = input,
    .length = length,
  };
}

static void test_logical_context_changes_reclassify_cached_lookahead(void) {
  const int32_t input[] = {'\'', '\\', '\n', 'x', '\'', '\\', '\n', 'y'};
  struct SourceNativeMock mock;
  source_native_init(&mock, input, 8);
  struct NativeInput native = {.lexer = &mock.lexer};
  struct SourceStage local = source_stage(SOURCE_REMOVE_CONTINUATIONS);
  struct SourceSnapshot initial = {.stages = &local, .stage_count = 1};
  struct LogicalLexer logical;
  assert(logical_init(&logical, &native, &initial));
  assert(logical.lexer.lookahead == '\'');
  logical.lexer.mark_end(&logical.lexer);
  logical.lexer.advance(&logical.lexer, false);
  assert(logical.lexer.lookahead == 'x');
  assert(logical.consumed == 1);
  assert(mock.position == 4);

  struct SourceStage quoted = local;
  quoted.disabled = true;
  assert(logical_replace_view(&logical, 0, &quoted, 1));
  assert(logical.lexer.lookahead == '\\');
  assert(logical.base == 1);
  assert(logical.mark == 0);
  assert(mock.position == 4);
  logical.lexer.advance(&logical.lexer, false);
  assert(logical.lexer.lookahead == '\n');
  logical.lexer.advance(&logical.lexer, false);
  assert(logical.lexer.lookahead == 'x');
  logical.lexer.advance(&logical.lexer, false);
  assert(logical.lexer.lookahead == '\'');
  logical.lexer.advance(&logical.lexer, false);
  assert(logical.lexer.lookahead == '\\');
  assert(logical.consumed == 5);

  assert(logical_replace_view(&logical, 0, &local, 1));
  assert(logical.lexer.lookahead == 'y');
  assert(logical.base == 5);
  assert(logical.cursor.removal_count == 1);
  assert(logical.cursor.removals[0].slash == 0);
  logical.lexer.advance(&logical.lexer, false);
  assert(logical.lexer.eof(&logical.lexer));
  assert(mock.advances == 8);
  assert(mock.marks == 0);
  logical_clear(&logical);
  native_input_clear(&native);
}

static void
test_logical_backquote_closers_belong_to_the_innermost_active_view(void) {
  const struct {
    int32_t input[4];
    size_t length;
    size_t decoders;
    bool disable_inner;
    size_t boundary;
    bool closes;
  } fixtures[] = {
    {{'`'}, 1, 1, false, 0, true},
    {{'`'}, 1, 2, false, 0, false},
    {{'\\', '`'}, 2, 2, false, 2, true},
    {{'\\', '\\', '\\', '`'}, 4, 2, false, SIZE_MAX, false},
    {{'\\', '`'}, 2, 3, false, 2, false},
    {{'\\', '\\', '\\', '`'}, 4, 3, false, 4, true},
    {{'`'}, 1, 2, true, 0, true},
    {{'\\', '`'}, 2, 2, true, SIZE_MAX, false},
  };
  assert(!logical_backquote_end(NULL));
  for (
    size_t fixture = 0; fixture < sizeof(fixtures) / sizeof(fixtures[0]);
    fixture += 1
  ) {
    struct SourceStage stages[6];
    size_t count = 2 * fixtures[fixture].decoders;
    for (size_t index = 0; index < count; index += 2) {
      stages[index] = source_stage(SOURCE_BACKQUOTE_DECODE);
      stages[index + 1] = source_stage(SOURCE_REMOVE_CONTINUATIONS);
    }
    stages[count - 2].disabled = fixtures[fixture].disable_inner;
    struct SourceSnapshot initial = {.stages = stages, .stage_count = count};
    struct SourceNativeMock mock;
    source_native_init(
      &mock,
      fixtures[fixture].input,
      fixtures[fixture].length
    );
    struct NativeInput native = {.lexer = &mock.lexer};
    struct LogicalLexer logical;
    assert(logical_init(&logical, &native, &initial));
    assert(logical.character.value == '`');
    assert(logical.character.boundary_stage == fixtures[fixture].boundary);
    assert(logical_backquote_end(&logical) == fixtures[fixture].closes);
    assert(logical.character.origin.start == 0);
    assert(logical.character.origin.end == (int64_t)fixtures[fixture].length);
    for (size_t cut = 0; cut < fixtures[fixture].length; cut += 1) {
      struct SourceSnapshot snapshot = {0};
      assert(source_snapshot_at(&logical.cursor, cut, &snapshot));
      uint8_t bytes[SOURCE_STATE_CAPACITY];
      size_t length;
      assert(
        source_snapshot_serialize(&snapshot, bytes, sizeof(bytes), &length)
      );
      struct SourceSnapshot restored = {0};
      assert(source_snapshot_deserialize(&restored, bytes, length));
      struct SourceNativeMock resumed_mock;
      source_native_init(
        &resumed_mock,
        fixtures[fixture].input + cut,
        fixtures[fixture].length - cut
      );
      struct NativeInput resumed_native = {.lexer = &resumed_mock.lexer};
      struct LogicalLexer resumed;
      assert(logical_init(&resumed, &resumed_native, &restored));
      assert(resumed.character.boundary_stage == fixtures[fixture].boundary);
      assert(logical_backquote_end(&resumed) == fixtures[fixture].closes);
      assert(resumed.character.origin.start == -(int64_t)cut);
      assert(
        resumed.character.origin.end ==
        (int64_t)(fixtures[fixture].length - cut)
      );
      resumed.lexer.advance(&resumed.lexer, false);
      assert(resumed.lexer.eof(&resumed.lexer));
      assert(!logical_backquote_end(&resumed));
      assert(resumed_mock.advances == fixtures[fixture].length - cut);
      assert(resumed_mock.marks == 0);
      logical_clear(&resumed);
      native_input_clear(&resumed_native);
      source_snapshot_clear(&restored);
      source_snapshot_clear(&snapshot);
    }
    if (fixture == 1) {
      struct LogicalLexer outer;
      assert(logical_fork(&outer, &logical, 0, 2, NULL, 0));
      assert(logical_backquote_end(&outer));
      assert(!logical_backquote_end(&logical));
      logical_clear(&outer);
    }
    logical_clear(&logical);
    native_input_clear(&native);
  }
}

static void test_logical_forks_preserve_ancestor_pending_units(void) {
  const int32_t input[] = {'\\', '\n', 'x'};
  struct SourceNativeMock mock;
  source_native_init(&mock, input, 3);
  struct NativeInput native = {.lexer = &mock.lexer};
  struct SourceStage stages[] = {
    source_stage(SOURCE_REMOVE_CONTINUATIONS),
    source_stage(SOURCE_REMOVE_CONTINUATIONS),
  };
  struct SourceSnapshot initial = {.stages = stages, .stage_count = 2};
  struct LogicalLexer parent;
  assert(logical_init(&parent, &native, &initial));
  assert(parent.lexer.lookahead == 'x');
  struct SourceStage quoted = stages[1];
  quoted.disabled = true;
  struct LogicalLexer child;
  assert(logical_fork(&child, &parent, 0, 1, &quoted, 1));
  assert(child.lexer.lookahead == 'x');
  assert(child.cursor.removal_count == 1);
  assert(child.cursor.removals[0].stage == 0);
  assert(parent.base == 0);
  assert(parent.lexer.lookahead == 'x');
  logical_clear(&child);

  assert(logical_fork(&child, &parent, 1, 2, NULL, 0));
  assert(child.lexer.lookahead == 'x');
  assert(child.base == 1);
  assert(child.cursor.removals[0].slash == -1);
  assert(child.cursor.removals[0].origin.start == 0);
  logical_clear(&child);
  assert(!logical_fork(&child, &parent, 1, 0, &quoted, 1));
  logical_clear(&child);
  logical_rewind(&parent);
  assert(parent.lexer.lookahead == 'x');
  assert(mock.advances == 3);
  assert(mock.marks == 0);
  logical_clear(&parent);
  native_input_clear(&native);
}

static void test_logical_fork_allocation_failures_preserve_parent(void) {
  const int32_t input[] = {'a', '\\', '\n', 'b'};
  for (size_t failure = 1; failure <= 20; failure += 1) {
    struct SourceNativeMock mock;
    source_native_init(&mock, input, 4);
    struct NativeInput native = {.lexer = &mock.lexer};
    struct SourceStage stage = source_stage(SOURCE_REMOVE_CONTINUATIONS);
    struct SourceSnapshot initial = {.stages = &stage, .stage_count = 1};
    struct LogicalLexer parent;
    assert(logical_init(&parent, &native, &initial));
    assert(parent.lexer.lookahead == 'a');
    source_allocations = 0;
    source_allocation_failure = failure;
    struct LogicalLexer child;
    (void)logical_fork(&child, &parent, 0, 0, &stage, 1);
    assert(parent.lexer.lookahead == 'a');
    assert(parent.base == 0);
    logical_clear(&child);
    logical_clear(&parent);
    native_input_clear(&native);
    source_allocation_failure = 0;
    assert(source_live_allocations == 0);
  }
}

static void test_logical_resume_does_not_repeat_already_emitted_source(void) {
  const int32_t prefix[] = {'\\', '\\', '\\'};
  struct SourceStage stages[] = {
    source_stage(SOURCE_BACKQUOTE_DECODE),
    source_stage(SOURCE_REMOVE_CONTINUATIONS),
  };
  struct SourceCursor cursor;
  assert(source_cursor_init(&cursor, stages, 2, NULL, NULL));
  for (size_t index = 0; index < 3; index += 1) {
    assert(source_feed(&cursor, prefix[index]));
  }
  struct SourceSnapshot snapshot = {0};
  assert(source_checkpoint(&cursor, &snapshot));
  const int32_t suffix[] = {']'};
  struct SourceNativeMock mock;
  source_native_init(&mock, suffix, 1);
  struct NativeInput native = {.lexer = &mock.lexer};
  struct LogicalLexer input;
  assert(logical_init(&input, &native, &snapshot));
  assert(input.lexer.lookahead == ']');
  assert(input.character.origin.start == 0);
  assert(input.character.origin.end == 1);
  input.lexer.advance(&input.lexer, false);
  assert(input.lexer.eof(&input.lexer));
  assert(mock.advances == 1);
  logical_clear(&input);
  native_input_clear(&native);
  source_snapshot_clear(&snapshot);
  source_cursor_clear(&cursor);
}

int main(void) {
  test_source_reading_preserves_scalars_and_nul();
  test_ancestor_removal_survives_local_quoting();
  test_backquote_source_records_units_and_boundaries();
  test_source_pass_order_controls_removal();
  test_adjacent_runs_and_bare_newlines_follow_each_pass();
  test_tab_stripping_follows_joined_line_starts();
  test_every_raw_cut_restores_pending_source_without_rereading();
  test_snapshot_capacity_and_invalid_data_are_transactional();
  test_source_failures_release_owned_memory();
  test_logical_context_changes_reclassify_cached_lookahead();
  test_logical_backquote_closers_belong_to_the_innermost_active_view();
  test_logical_forks_preserve_ancestor_pending_units();
  test_logical_fork_allocation_failures_preserve_parent();
  test_logical_resume_does_not_repeat_already_emitted_source();
  assert(source_live_allocations == 0);
  return 0;
}
