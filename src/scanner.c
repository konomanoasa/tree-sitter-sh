#include "arithmetic.h"
#include "tree_sitter/alloc.h"
#include "tree_sitter/parser.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef TREE_SITTER_SERIALIZATION_BUFFER_SIZE
#define TREE_SITTER_SERIALIZATION_BUFFER_SIZE 1024
#endif

#define SCANNER_SERIALIZATION_VERSION 17
#define SCANNER_STATE_CAPACITY (TREE_SITTER_SERIALIZATION_BUFFER_SIZE - 1)

enum TokenType {
  LEFT_BRACE,
  RIGHT_BRACE,
  FILE_DESCRIPTOR,
  PIPELINE_NEGATION,
  IF_KEYWORD,
  THEN_KEYWORD,
  ELIF_KEYWORD,
  ELSE_KEYWORD,
  FI_KEYWORD,
  FOR_KEYWORD,
  IN_KEYWORD,
  DO_KEYWORD,
  DONE_KEYWORD,
  CASE_KEYWORD,
  ESAC_KEYWORD,
  WHILE_KEYWORD,
  UNTIL_KEYWORD,
  DLESS,
  DLESSDASH,
  HERE_END_BEGIN,
  HERE_END_COMMIT,
  HERE_DOCUMENT_LINE_END,
  HERE_DOCUMENT_BODY_START,
  QUOTED_HERE_DOCUMENT_BODY_START,
  QUOTED_HERE_DOCUMENT_END_BEGIN,
  QUOTED_HERE_DOCUMENT_END_TEXT,
  HERE_DOCUMENT_END_BEGIN,
  HERE_DOCUMENT_END_LEADING_TABS,
  HERE_DOCUMENT_END_COMMIT,
  HERE_DOCUMENT_SEQUENCE_END,
  HERE_DOCUMENT_CONTENT_LINE_START,
  NEWLINE,
  LINE_CONTINUATION,
  ARITHMETIC_ASSIGNMENT_OPERATOR_BOUNDARY,
  ARITHMETIC_QUESTION_OPERATOR_BOUNDARY,
  ARITHMETIC_COLON_OPERATOR_BOUNDARY,
  ARITHMETIC_LOGICAL_OR_OPERATOR_BOUNDARY,
  ARITHMETIC_LOGICAL_AND_OPERATOR_BOUNDARY,
  ARITHMETIC_BITWISE_OR_OPERATOR_BOUNDARY,
  ARITHMETIC_BITWISE_XOR_OPERATOR_BOUNDARY,
  ARITHMETIC_BITWISE_AND_OPERATOR_BOUNDARY,
  ARITHMETIC_EQUALITY_OPERATOR_BOUNDARY,
  ARITHMETIC_RELATIONAL_OPERATOR_BOUNDARY,
  ARITHMETIC_SHIFT_OPERATOR_BOUNDARY,
  ARITHMETIC_ADDITIVE_OPERATOR_BOUNDARY,
  ARITHMETIC_MULTIPLICATIVE_OPERATOR_BOUNDARY,
  ARITHMETIC_CLOSING_BOUNDARY,
  ARITHMETIC_LEFT_PARENTHESIS,
  ARITHMETIC_DYNAMIC_LEFT_PARENTHESIS,
  PATTERN_SPECIAL_LEFT_BRACKET,
  LITERAL_HASH,
  COMMENT_BOUNDARY,
  TRAILING_COMMENT_BOUNDARY,
  COMMENT,
  COMMENT_LINE_END,
  DOLLAR_EXPANSION_START,
  BRACED_PARAMETER_NUMBER_START,
  BRACED_POSITIONAL_PARAMETER_START,
  BACKQUOTE_START,
  BACKQUOTE_START_PREFIX,
  BACKQUOTE_DOLLAR_PREFIX,
  BACKQUOTE_END,
  BACKQUOTE_END_PREFIX,
  BACKQUOTE_CONTENT_RUN_BEGIN,
  BACKQUOTE_PAIR_RUN_BEGIN,
  BACKQUOTE_PAIR_RUN_END,
  PATTERN_CONTINUATION,
  PATTERN_END,
  PIPE_CONTINUATION,
  REDIRECT_LIST_BEGIN,
  CASE_ITEM_END,
  FUNCTION_BODY_CONTINUATION_BOUNDARY,
  COMMAND_SUBSTITUTION_BODY_BEGIN,
  SUBSHELL_CLOSE,
  WORD_BRACKET_LITERAL_START,
  PARAMETER_BRACKET_LITERAL_START,
  WORD_BRACKET_FALLBACK_END,
  PARAMETER_BRACKET_FALLBACK_END,
  PATTERN_BRACKET_CHARACTER,
  PARAMETER_PATTERN_BRACKET_CHARACTER,
  PATTERN_BRACKET_HYPHEN,
  WORD_TILDE_END,
  ASSIGNMENT_TILDE_END,
  TILDE_BRACKET_LITERAL_START,
  ASSIGNMENT_TILDE_BRACKET_LITERAL_START,
  ASSIGNMENT_NAME_TOKEN,
  FNAME_TOKEN,
  WORD_NAME_TOKEN,
  AND_OR_CONTINUATION,
  WORD_SEPARATOR_BEGIN,
  LIST_CONTINUATION,
  TERM_CONTINUATION,
  TERMINATOR_AHEAD,
  ASSIGNMENT_SEPARATOR_BEGIN,
  REDIRECT_SEPARATOR_BEGIN,
  PRE_NEWLINE_BLANK,
  TRAILING_CONTINUATION_BEGIN,
  COMMAND_SUBSTITUTION_CLOSE,
  SEPARATOR_NEWLINE,
  LAYOUT_BEGIN,
  TERM_BOUNDARY,
  WORD_PATTERN_BRACKET_OPEN,
  PARAMETER_PATTERN_BRACKET_OPEN,
  DOUBLE_QUOTED_BACKQUOTE_START,
  DOUBLE_QUOTED_BACKQUOTE_START_PREFIX,
  BACKQUOTE_QUOTE_PREFIX,
  BACKQUOTE_CONTINUATION_BEGIN,
  DOLLAR_SINGLE_QUOTE_ESCAPE,
  BACKQUOTE_DOLLAR_SINGLE_QUOTE_TEXT,
  BACKQUOTE_DOLLAR_SINGLE_QUOTE_PREFIX,
  BACKQUOTE_PATTERN_ESCAPE,
  PATTERN_CHARACTER_CLASS_END_COLON,
  TOKEN_COUNT,
};

enum ArithmeticOperatorCategory {
  ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT,
  ARITHMETIC_OPERATOR_CATEGORY_QUESTION,
  ARITHMETIC_OPERATOR_CATEGORY_COLON,
  ARITHMETIC_OPERATOR_CATEGORY_LOGICAL_OR,
  ARITHMETIC_OPERATOR_CATEGORY_LOGICAL_AND,
  ARITHMETIC_OPERATOR_CATEGORY_BITWISE_OR,
  ARITHMETIC_OPERATOR_CATEGORY_BITWISE_XOR,
  ARITHMETIC_OPERATOR_CATEGORY_BITWISE_AND,
  ARITHMETIC_OPERATOR_CATEGORY_EQUALITY,
  ARITHMETIC_OPERATOR_CATEGORY_RELATIONAL,
  ARITHMETIC_OPERATOR_CATEGORY_SHIFT,
  ARITHMETIC_OPERATOR_CATEGORY_ADDITIVE,
  ARITHMETIC_OPERATOR_CATEGORY_MULTIPLICATIVE,
  ARITHMETIC_OPERATOR_CATEGORY_COUNT,
};

static const enum TokenType
  ARITHMETIC_OPERATOR_BOUNDARIES[ARITHMETIC_OPERATOR_CATEGORY_COUNT] = {
    [ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT] =
      ARITHMETIC_ASSIGNMENT_OPERATOR_BOUNDARY,
    [ARITHMETIC_OPERATOR_CATEGORY_QUESTION] =
      ARITHMETIC_QUESTION_OPERATOR_BOUNDARY,
    [ARITHMETIC_OPERATOR_CATEGORY_COLON] = ARITHMETIC_COLON_OPERATOR_BOUNDARY,
    [ARITHMETIC_OPERATOR_CATEGORY_LOGICAL_OR] =
      ARITHMETIC_LOGICAL_OR_OPERATOR_BOUNDARY,
    [ARITHMETIC_OPERATOR_CATEGORY_LOGICAL_AND] =
      ARITHMETIC_LOGICAL_AND_OPERATOR_BOUNDARY,
    [ARITHMETIC_OPERATOR_CATEGORY_BITWISE_OR] =
      ARITHMETIC_BITWISE_OR_OPERATOR_BOUNDARY,
    [ARITHMETIC_OPERATOR_CATEGORY_BITWISE_XOR] =
      ARITHMETIC_BITWISE_XOR_OPERATOR_BOUNDARY,
    [ARITHMETIC_OPERATOR_CATEGORY_BITWISE_AND] =
      ARITHMETIC_BITWISE_AND_OPERATOR_BOUNDARY,
    [ARITHMETIC_OPERATOR_CATEGORY_EQUALITY] =
      ARITHMETIC_EQUALITY_OPERATOR_BOUNDARY,
    [ARITHMETIC_OPERATOR_CATEGORY_RELATIONAL] =
      ARITHMETIC_RELATIONAL_OPERATOR_BOUNDARY,
    [ARITHMETIC_OPERATOR_CATEGORY_SHIFT] = ARITHMETIC_SHIFT_OPERATOR_BOUNDARY,
    [ARITHMETIC_OPERATOR_CATEGORY_ADDITIVE] =
      ARITHMETIC_ADDITIVE_OPERATOR_BOUNDARY,
    [ARITHMETIC_OPERATOR_CATEGORY_MULTIPLICATIVE] =
      ARITHMETIC_MULTIPLICATIVE_OPERATOR_BOUNDARY,
};

struct HereDocument {
  uint8_t *delimiter;
  size_t delimiter_length;
  size_t declaration_depth;
  bool quoted;
  bool strip_tabs;
};

struct HereDocumentFrame {
  struct HereDocument *documents;
  size_t count;
  size_t body_backquote_depth;
  bool at_line_start;
};

struct Scanner {
  struct HereDocument *pending_documents;
  size_t pending_count;
  struct HereDocument *active_documents;
  size_t active_count;
  struct HereDocumentFrame *suspended_frames;
  size_t suspended_frame_count;
  bool expecting_delimiter;
  bool delimiter_strips_tabs;
  bool sequence_end_pending;
  bool at_here_document_line_start;
  size_t backquote_depth;
  size_t substitution_depth;
  size_t body_backquote_depth;
  size_t *quoted_backquote_depths;
  size_t quoted_backquote_count;
};

struct ByteBuffer {
  char *data;
  size_t length;
  size_t capacity;
  size_t limit;
  bool failed;
};

static bool scanner_state_fits(const struct Scanner *scanner);

struct ReservedWord {
  const char *text;
  enum TokenType symbol;
};

static const struct ReservedWord RESERVED_WORDS[] = {
  {"if", IF_KEYWORD},
  {"then", THEN_KEYWORD},
  {"elif", ELIF_KEYWORD},
  {"else", ELSE_KEYWORD},
  {"fi", FI_KEYWORD},
  {"for", FOR_KEYWORD},
  {"in", IN_KEYWORD},
  {"do", DO_KEYWORD},
  {"done", DONE_KEYWORD},
  {"case", CASE_KEYWORD},
  {"esac", ESAC_KEYWORD},
  {"while", WHILE_KEYWORD},
  {"until", UNTIL_KEYWORD},
};

static const struct ReservedWord CLOSING_WORDS[] = {
  {"then", THEN_KEYWORD},
  {"elif", ELIF_KEYWORD},
  {"else", ELSE_KEYWORD},
  {"fi", FI_KEYWORD},
  {"do", DO_KEYWORD},
  {"done", DONE_KEYWORD},
  {"esac", ESAC_KEYWORD},
};

static const struct ReservedWord *find_reserved_word(const char *word) {
  size_t word_count = sizeof(RESERVED_WORDS) / sizeof(RESERVED_WORDS[0]);
  for (size_t index = 0; index < word_count; index += 1) {
    if (strcmp(word, RESERVED_WORDS[index].text) == 0) {
      return &RESERVED_WORDS[index];
    }
  }
  return NULL;
}

static void clear_document(struct HereDocument *document) {
  ts_free(document->delimiter);
  document->delimiter = NULL;
  document->delimiter_length = 0;
  document->quoted = false;
  document->strip_tabs = false;
}

static void
clear_document_array(struct HereDocument **documents, size_t *count) {
  for (size_t index = 0; index < *count; index += 1) {
    clear_document(&(*documents)[index]);
  }

  ts_free(*documents);
  *documents = NULL;
  *count = 0;
}

static void reset_here_document_delimiter_scan(struct Scanner *scanner) {
  scanner->expecting_delimiter = false;
  scanner->delimiter_strips_tabs = false;
}

static void clear_scanner(struct Scanner *scanner) {
  reset_here_document_delimiter_scan(scanner);
  clear_document_array(&scanner->pending_documents, &scanner->pending_count);
  clear_document_array(&scanner->active_documents, &scanner->active_count);
  for (size_t index = 0; index < scanner->suspended_frame_count; index += 1) {
    clear_document_array(
      &scanner->suspended_frames[index].documents,
      &scanner->suspended_frames[index].count
    );
  }
  ts_free(scanner->suspended_frames);
  scanner->suspended_frames = NULL;
  scanner->suspended_frame_count = 0;
  scanner->sequence_end_pending = false;
  scanner->at_here_document_line_start = false;
  scanner->backquote_depth = 0;
  scanner->substitution_depth = 0;
  scanner->body_backquote_depth = 0;
  ts_free(scanner->quoted_backquote_depths);
  scanner->quoted_backquote_depths = NULL;
  scanner->quoted_backquote_count = 0;
}

static void trim_backquote_depth(struct Scanner *scanner, size_t depth) {
  scanner->backquote_depth = depth;
  while (
    scanner->quoted_backquote_count >
    0 &&
    scanner->quoted_backquote_depths[scanner->quoted_backquote_count - 1] >
    depth
  ) {
    scanner->quoted_backquote_count -= 1;
  }
}

static size_t enclosing_substitution_depth(const struct Scanner *scanner) {
  return scanner->substitution_depth + scanner->backquote_depth;
}

static bool here_document_is_startable(
  const struct Scanner *scanner,
  const struct HereDocument *document
) {
  return document->declaration_depth >= enclosing_substitution_depth(scanner);
}

static bool has_startable_pending_document(const struct Scanner *scanner) {
  for (size_t index = 0; index < scanner->pending_count; index += 1) {
    if (
      here_document_is_startable(scanner, &scanner->pending_documents[index])
    ) {
      return true;
    }
  }
  return false;
}

static bool append_document(
  struct HereDocument **documents,
  size_t *count,
  struct HereDocument document
) {
  if (*count >= SIZE_MAX / sizeof(struct HereDocument)) {
    return false;
  }

  size_t next_count = *count + 1;
  struct HereDocument *resized =
    ts_realloc(*documents, next_count * sizeof(struct HereDocument));
  if (resized == NULL) {
    return false;
  }

  resized[*count] = document;
  *documents = resized;
  *count = next_count;
  return true;
}

static bool
append_pending_document(struct Scanner *scanner, struct HereDocument document) {
  document.declaration_depth = enclosing_substitution_depth(scanner);

  if (!append_document(
        &scanner->pending_documents,
        &scanner->pending_count,
        document
      )) {
    return false;
  }

  if (!scanner_state_fits(scanner)) {
    scanner->pending_count -= 1;
    scanner->pending_documents[scanner->pending_count] =
      (struct HereDocument){0};
    return false;
  }
  return true;
}

static bool activate_startable_pending_documents(struct Scanner *scanner) {
  size_t startable_count = 0;
  for (size_t index = 0; index < scanner->pending_count; index += 1) {
    if (
      here_document_is_startable(scanner, &scanner->pending_documents[index])
    ) {
      startable_count += 1;
    }
  }
  if (startable_count == 0) {
    return false;
  }

  struct HereDocument *startable = NULL;
  struct HereDocument *retained = NULL;
  size_t retained_count = scanner->pending_count - startable_count;
  if (retained_count > 0) {
    startable = ts_calloc(startable_count, sizeof(struct HereDocument));
    retained = ts_calloc(retained_count, sizeof(struct HereDocument));
    if (startable == NULL || retained == NULL) {
      ts_free(startable);
      ts_free(retained);
      return false;
    }
  }

  struct Scanner next = *scanner;
  if (retained_count == 0) {
    next.active_documents = scanner->pending_documents;
    next.active_count = scanner->pending_count;
    next.pending_documents = NULL;
    next.pending_count = 0;
  } else {
    size_t startable_index = 0;
    size_t retained_index = 0;
    for (size_t index = 0; index < scanner->pending_count; index += 1) {
      struct HereDocument *document = &scanner->pending_documents[index];
      if (here_document_is_startable(scanner, document)) {
        startable[startable_index] = *document;
        startable_index += 1;
      } else {
        retained[retained_index] = *document;
        retained_index += 1;
      }
    }
    next.pending_documents = retained;
    next.pending_count = retained_count;
    next.active_documents = startable;
    next.active_count = startable_count;
  }

  if (scanner->active_count > 0) {
    if (
      scanner->suspended_frame_count >=
      SIZE_MAX /
      sizeof(struct HereDocumentFrame)
    ) {
      goto fail;
    }
    next.suspended_frame_count += 1;
    struct HereDocumentFrame *frames = ts_realloc(
      scanner->suspended_frames,
      next.suspended_frame_count * sizeof(struct HereDocumentFrame)
    );
    if (frames == NULL) {
      goto fail;
    }
    scanner->suspended_frames = frames;
    next.suspended_frames = frames;
    frames[scanner->suspended_frame_count] = (struct HereDocumentFrame){
      .documents = scanner->active_documents,
      .count = scanner->active_count,
      .body_backquote_depth = scanner->body_backquote_depth,
      .at_line_start = scanner->at_here_document_line_start,
    };
    next.at_here_document_line_start = false;
  }
  next.body_backquote_depth = scanner->backquote_depth;
  if (!scanner_state_fits(&next)) {
    if (scanner->active_count > 0) {
      scanner->suspended_frames[scanner->suspended_frame_count] =
        (struct HereDocumentFrame){0};
    }
    goto fail;
  }

  if (retained_count > 0) {
    ts_free(scanner->pending_documents);
  }
  *scanner = next;
  return true;

fail:
  ts_free(startable);
  ts_free(retained);
  return false;
}

static void restore_suspended_documents(struct Scanner *scanner) {
  if (scanner->suspended_frame_count == 0) {
    return;
  }

  scanner->suspended_frame_count -= 1;
  struct HereDocumentFrame frame =
    scanner->suspended_frames[scanner->suspended_frame_count];
  scanner->active_documents = frame.documents;
  scanner->active_count = frame.count;
  scanner->body_backquote_depth = frame.body_backquote_depth;
  scanner->at_here_document_line_start = frame.at_line_start;

  if (scanner->suspended_frame_count == 0) {
    ts_free(scanner->suspended_frames);
    scanner->suspended_frames = NULL;
  }
}

static void finish_active_document(struct Scanner *scanner) {
  trim_backquote_depth(scanner, scanner->body_backquote_depth);
  clear_document(&scanner->active_documents[0]);
  scanner->active_count -= 1;

  if (scanner->active_count > 0) {
    memmove(
      scanner->active_documents,
      scanner->active_documents + 1,
      scanner->active_count * sizeof(struct HereDocument)
    );
    return;
  }

  ts_free(scanner->active_documents);
  scanner->active_documents = NULL;
  scanner->sequence_end_pending = true;
}

static bool grow_byte_buffer(struct ByteBuffer *buffer, size_t minimum) {
  if (buffer->limit != 0 && minimum > buffer->limit) {
    buffer->failed = true;
    return false;
  }

  size_t capacity = buffer->capacity == 0 ? 32 : buffer->capacity;
  while (capacity < minimum) {
    if (capacity > SIZE_MAX / 2) {
      capacity = SIZE_MAX;
      break;
    }
    capacity *= 2;
  }

  if (buffer->limit != 0 && capacity > buffer->limit) {
    capacity = buffer->limit;
  }

  if (capacity < minimum) {
    buffer->failed = true;
    return false;
  }

  char *resized = ts_realloc(buffer->data, capacity);
  if (resized == NULL) {
    buffer->failed = true;
    return false;
  }

  buffer->data = resized;
  buffer->capacity = capacity;
  return true;
}

static bool append_byte(struct ByteBuffer *buffer, uint8_t byte) {
  if (buffer == NULL) {
    return true;
  }

  if (buffer->length == SIZE_MAX) {
    buffer->failed = true;
    return false;
  }

  size_t next_length = buffer->length + 1;
  if (
    next_length > buffer->capacity && !grow_byte_buffer(buffer, next_length)
  ) {
    return false;
  }

  buffer->data[buffer->length] = (char)byte;
  buffer->length = next_length;
  return true;
}

static bool
append_bytes(struct ByteBuffer *buffer, const uint8_t *bytes, size_t length) {
  if (buffer == NULL) {
    return true;
  }

  if (length > SIZE_MAX - buffer->length) {
    buffer->failed = true;
    return false;
  }

  size_t next_length = buffer->length + length;
  if (
    next_length > buffer->capacity && !grow_byte_buffer(buffer, next_length)
  ) {
    return false;
  }

  if (length > 0) {
    memcpy(buffer->data + buffer->length, bytes, length);
  }
  buffer->length = next_length;
  return true;
}

static bool
encode_utf8_scalar(int32_t character, uint8_t bytes[4], size_t *length) {
  if (character < 0 || character > 0x10ffff) {
    return false;
  }

  if (character <= 0x7f) {
    bytes[0] = (uint8_t)character;
    *length = 1;
    return true;
  }

  if (character <= 0x7ff) {
    bytes[0] = (uint8_t)(0xc0 | (character >> 6));
    bytes[1] = (uint8_t)(0x80 | (character & 0x3f));
    *length = 2;
    return true;
  }

  if (character >= 0xd800 && character <= 0xdfff) {
    return false;
  }

  if (character <= 0xffff) {
    bytes[0] = (uint8_t)(0xe0 | (character >> 12));
    bytes[1] = (uint8_t)(0x80 | ((character >> 6) & 0x3f));
    bytes[2] = (uint8_t)(0x80 | (character & 0x3f));
    *length = 3;
    return true;
  }

  bytes[0] = (uint8_t)(0xf0 | (character >> 18));
  bytes[1] = (uint8_t)(0x80 | ((character >> 12) & 0x3f));
  bytes[2] = (uint8_t)(0x80 | ((character >> 6) & 0x3f));
  bytes[3] = (uint8_t)(0x80 | (character & 0x3f));
  *length = 4;
  return true;
}

static bool append_codepoint(struct ByteBuffer *buffer, int32_t character) {
  uint8_t bytes[4];
  size_t length;
  return encode_utf8_scalar(character, bytes, &length) &&
    append_bytes(buffer, bytes, length);
}

static bool append_quoted_escape(struct ByteBuffer *buffer, int32_t character) {
  switch (character) {
  case 'a':
    return append_byte(buffer, '\a');
  case 'b':
    return append_byte(buffer, '\b');
  case 'e':
    return append_byte(buffer, 0x1b);
  case 'f':
    return append_byte(buffer, '\f');
  case 'n':
    return append_byte(buffer, '\n');
  case 'r':
    return append_byte(buffer, '\r');
  case 't':
    return append_byte(buffer, '\t');
  case 'v':
    return append_byte(buffer, '\v');
  default:
    return append_codepoint(buffer, character);
  }
}

static bool is_decimal_digit(int32_t character) {
  return character >= '0' && character <= '9';
}

static bool is_horizontal_blank(int32_t character) {
  return character == ' ' || character == '\t';
}

static bool is_name_start_character(int32_t character) {
  return (
    (character >= 'A' && character <= 'Z') ||
    (character >= 'a' && character <= 'z') ||
    character == '_'
  );
}

static bool is_name_character(int32_t character) {
  return is_name_start_character(character) || is_decimal_digit(character);
}

static bool is_special_parameter_character(int32_t character) {
  return (
    character ==
    '0' ||
    character ==
    '*' ||
    character ==
    '@' ||
    character ==
    '#' ||
    character ==
    '?' ||
    character ==
    '$' ||
    character ==
    '!' ||
    character == '-'
  );
}

static bool is_parameter_start_character(int32_t character) {
  return (
    is_name_start_character(character) ||
    is_decimal_digit(character) ||
    is_special_parameter_character(character)
  );
}

static bool is_lowercase_letter(int32_t character) {
  return character >= 'a' && character <= 'z';
}

static bool is_control_operator_start(int32_t character) {
  return (
    character ==
    '&' ||
    character ==
    '(' ||
    character ==
    ')' ||
    character ==
    ';' ||
    character ==
    '<' ||
    character ==
    '>' ||
    character == '|'
  );
}

static bool skip_line_continuations(TSLexer *lexer) {
  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return false;
    }
    lexer->advance(lexer, false);
  }
  return true;
}

static bool
count_escape_run(TSLexer *lexer, size_t initial, size_t *escape_count) {
  size_t count = initial;
  while (lexer->lookahead == '\\') {
    if (count == SIZE_MAX) {
      return false;
    }
    count += 1;
    lexer->advance(lexer, false);
  }
  *escape_count = count;
  return true;
}

static bool lexer_at_eof(const TSLexer *lexer) {
  return lexer->lookahead == 0 && lexer->eof(lexer);
}

static void advance_to_line_end(TSLexer *lexer) {
  while (!lexer_at_eof(lexer) && lexer->lookahead != '\n') {
    lexer->advance(lexer, false);
  }
}

static bool
is_active_backquote_boundary(const struct Scanner *scanner, int32_t character) {
  return scanner->backquote_depth > 0 && character == '`';
}

enum BackquoteTickPrefix {
  BACKQUOTE_TICK_PREFIX_NONE,
  BACKQUOTE_TICK_PREFIX_START,
  BACKQUOTE_TICK_PREFIX_END,
};

struct BackquoteEscapeRunFold {
  size_t acting_level;
  size_t leftover_count;
};

static struct BackquoteEscapeRunFold
fold_backquote_escape_run(size_t depth, size_t escape_count);

static enum BackquoteTickPrefix
classify_backquote_tick_prefix(size_t depth, size_t escape_count);

static size_t fold_enclosed_plain_run(size_t run, size_t depth);

static size_t fold_enclosed_special_run(size_t run, size_t depth);

static size_t fold_enclosed_escape_run(
  const struct Scanner *scanner,
  size_t run,
  size_t depth,
  int32_t following
);

static void advance_to_comment_end(
  const struct Scanner *scanner,
  TSLexer *lexer,
  bool mark
) {
  while (true) {
    if (mark) {
      lexer->mark_end(lexer);
    }
    if (lexer_at_eof(lexer) || lexer->lookahead == '\n') {
      return;
    }
    if (scanner->backquote_depth > 0) {
      if (lexer->lookahead == '`') {
        return;
      }
      if (lexer->lookahead == '\\') {
        size_t escape_count;
        if (!count_escape_run(lexer, 0, &escape_count)) {
          return;
        }
        if (lexer->lookahead == '`') {
          if (
            fold_backquote_escape_run(scanner->backquote_depth, escape_count)
              .acting_level > scanner->backquote_depth
          ) {
            lexer->advance(lexer, false);
            continue;
          }
          return;
        }
        continue;
      }
    }
    lexer->advance(lexer, false);
  }
}

static bool
comment_reaches_end(const struct Scanner *scanner, const TSLexer *lexer) {
  return lexer_at_eof(lexer) ||
    is_active_backquote_boundary(scanner, lexer->lookahead);
}

static bool
is_token_delimiter_character(size_t backquote_depth, int32_t character) {
  return (
    character ==
    ' ' ||
    character ==
    '\t' ||
    character ==
    '\n' ||
    is_control_operator_start(character) ||
    (backquote_depth > 0 && character == '`')
  );
}

static bool
is_token_delimiter_at_depth(const TSLexer *lexer, size_t backquote_depth) {
  return (
    lexer_at_eof(lexer) ||
    is_token_delimiter_character(backquote_depth, lexer->lookahead)
  );
}

static bool
is_token_delimiter(const struct Scanner *scanner, const TSLexer *lexer) {
  return is_token_delimiter_at_depth(lexer, scanner->backquote_depth);
}

static bool
skip_token_continuations(const struct Scanner *scanner, TSLexer *lexer) {
  while (lexer->lookahead == '\\') {
    size_t run;
    if (!count_escape_run(lexer, 0, &run)) {
      return false;
    }
    if (lexer->lookahead == '`') {
      return classify_backquote_tick_prefix(scanner->backquote_depth, run) ==
        BACKQUOTE_TICK_PREFIX_END;
    }
    if (
      lexer->lookahead !=
      '\n' ||
      fold_enclosed_plain_run(run, scanner->backquote_depth) != 1
    ) {
      return false;
    }
    lexer->advance(lexer, false);
  }
  return true;
}

static bool is_bracket_scan_boundary(
  const struct Scanner *scanner,
  const TSLexer *lexer,
  bool parameter_pattern
) {
  if (!parameter_pattern) {
    return is_token_delimiter(scanner, lexer);
  }

  return (
    lexer_at_eof(lexer) ||
    lexer->lookahead ==
    '}' ||
    is_active_backquote_boundary(scanner, lexer->lookahead)
  );
}

static bool is_quote_or_expansion_start(int32_t character) {
  return (
    character ==
    '\'' ||
    character ==
    '"' ||
    character ==
    '$' ||
    character == '`'
  );
}

struct ValidationToken {
  uint8_t kind;
  uint8_t category;
};

struct ValidationTokenBuffer {
  struct ValidationToken *data;
  size_t length;
  size_t capacity;
};

struct ArithmeticScan {
  struct ValidationTokenBuffer tokens;
  int32_t *source;
  size_t source_length;
  size_t source_capacity;
  struct EmbeddedSkip *embedded;
  char embedded_closer;
  size_t group_depth;
  bool has_expansion;
};

struct AmbiguousSubstitution {
  size_t start;
  size_t end;
  size_t parent;
  size_t next;
  struct Scanner context;
  struct ArithmeticScan scan;
  struct EmbeddedSkip *command;
  size_t command_resume;
  size_t resume;
  enum ArithmeticValidation result;
  bool scan_started;
  bool scan_complete;
  bool raw;
  bool arithmetic;
  bool complete;
};

struct LookaheadCharacter {
  int32_t value;
  size_t substitution;
};

struct LookaheadLexer {
  TSLexer lexer;
  TSLexer *source;
  struct LookaheadCharacter *characters;
  size_t length;
  size_t capacity;
  size_t position;
  struct AmbiguousSubstitution *substitutions;
  size_t substitution_count;
  size_t substitution_capacity;
  size_t requested;
  size_t active;
  bool failed;
};

static void *grow_element_buffer(
  void *data,
  size_t *capacity,
  size_t length,
  size_t element_size,
  size_t initial_capacity
);

static void lookahead_seek(struct LookaheadLexer *lookahead, size_t position) {
  lookahead->position = position;
  int32_t character = lookahead->characters[position].value;
  lookahead->lexer.lookahead = character < 0 ? 0 : character;
}

static bool lookahead_append(struct LookaheadLexer *lookahead) {
  struct LookaheadCharacter *characters = grow_element_buffer(
    lookahead->characters,
    &lookahead->capacity,
    lookahead->length,
    sizeof(struct LookaheadCharacter),
    64
  );
  if (characters == NULL) {
    lookahead->failed = true;
    lookahead->lexer.lookahead = 0;
    return false;
  }
  lookahead->characters = characters;
  lookahead->characters[lookahead->length++] = (struct LookaheadCharacter){
    .value =
      lexer_at_eof(lookahead->source) ? -1 : lookahead->source->lookahead,
    .substitution = SIZE_MAX,
  };
  return true;
}

static void lookahead_advance(TSLexer *lexer, bool skip) {
  struct LookaheadLexer *lookahead = (struct LookaheadLexer *)lexer;
  if (lookahead->failed) {
    return;
  }
  if (lookahead->length == 0) {
    lookahead->source->advance(lookahead->source, skip);
    lexer->lookahead = lookahead->source->lookahead;
    return;
  }
  if (lookahead->position + 1 == lookahead->length) {
    if (lexer_at_eof(lookahead->source)) {
      return;
    }
    lookahead->source->advance(lookahead->source, skip);
    if (!lookahead_append(lookahead)) {
      return;
    }
  }
  lookahead_seek(lookahead, lookahead->position + 1);
}

// Token marks precede speculative reads; replay keeps the physical mark.
static void lookahead_mark_end(TSLexer *lexer) {
  struct LookaheadLexer *lookahead = (struct LookaheadLexer *)lexer;
  if (
    !lookahead->failed &&
    (lookahead->length == 0 || lookahead->position + 1 == lookahead->length)
  ) {
    lookahead->source->mark_end(lookahead->source);
  }
}

static bool lookahead_eof(const TSLexer *lexer) {
  const struct LookaheadLexer *lookahead = (const struct LookaheadLexer *)lexer;
  return lookahead->failed ||
    (lookahead->length == 0
        ? lexer_at_eof(lookahead->source)
        : lookahead->characters[lookahead->position].value < 0);
}

static bool lookahead_is_pending(const TSLexer *lexer) {
  const struct LookaheadLexer *lookahead = (const struct LookaheadLexer *)lexer;
  return lookahead->failed || lookahead->requested != SIZE_MAX;
}

static void complete_ambiguous_substitutions(struct LookaheadLexer *lookahead);

static const struct AmbiguousSubstitution *
request_ambiguous_substitution(struct LookaheadLexer *lookahead, size_t index) {
  if (lookahead->active != SIZE_MAX) {
    lookahead->requested = index;
    return NULL;
  }
  size_t position = lookahead->position;
  lookahead->active = index;
  complete_ambiguous_substitutions(lookahead);
  lookahead_seek(lookahead, position);
  return lookahead->failed ? NULL : &lookahead->substitutions[index];
}

static const struct AmbiguousSubstitution *
lookup_ambiguous_substitution(const struct Scanner *scanner, TSLexer *lexer) {
  struct LookaheadLexer *lookahead = (struct LookaheadLexer *)lexer;
  if (lookahead->length == 0 && !lookahead_append(lookahead)) {
    return NULL;
  }
  for (
    size_t index = lookahead->characters[lookahead->position].substitution;
    index != SIZE_MAX;
    index = lookahead->substitutions[index].next
  ) {
    struct AmbiguousSubstitution *substitution =
      &lookahead->substitutions[index];
    if (
      substitution->start ==
      lookahead->position &&
      substitution->raw ==
      (scanner == NULL) &&
      (scanner ==
        NULL ||
        (substitution->context.backquote_depth ==
          scanner->backquote_depth &&
          substitution->context.quoted_backquote_count ==
          scanner->quoted_backquote_count))
    ) {
      if (!substitution->complete) {
        return request_ambiguous_substitution(lookahead, index);
      }
      return substitution;
    }
  }
  struct AmbiguousSubstitution *substitutions = grow_element_buffer(
    lookahead->substitutions,
    &lookahead->substitution_capacity,
    lookahead->substitution_count,
    sizeof(struct AmbiguousSubstitution),
    8
  );
  if (substitutions == NULL) {
    lookahead->failed = true;
    return NULL;
  }
  lookahead->substitutions = substitutions;
  size_t index = lookahead->substitution_count++;
  lookahead->substitutions[index] = (struct AmbiguousSubstitution){
    .start = lookahead->position,
    .parent = lookahead->active,
    .next = lookahead->characters[lookahead->position].substitution,
    .context = scanner == NULL ? (struct Scanner){0} : *scanner,
    .raw = scanner == NULL,
  };
  lookahead->characters[lookahead->position].substitution = index;
  return request_ambiguous_substitution(lookahead, index);
}

static enum ArithmeticValidation
skip_ambiguous_substitution(const struct Scanner *scanner, TSLexer *lexer) {
  const struct AmbiguousSubstitution *substitution =
    lookup_ambiguous_substitution(scanner, lexer);
  if (substitution == NULL) {
    return ARITHMETIC_VALIDATION_INVALID;
  }
  enum ArithmeticValidation result = substitution->result;
  lookahead_seek((struct LookaheadLexer *)lexer, substitution->end);
  return result;
}

static enum ArithmeticValidation skip_embedded_construct(
  const struct Scanner *scanner,
  TSLexer *lexer,
  char initial_closer
);
static bool scan_delimiter_single_quoted_segment(
  TSLexer *lexer,
  const struct Scanner *scanner,
  size_t backquote_depth,
  struct ByteBuffer *delimiter,
  bool dollar
);

static bool
skip_bracket_member_expansion(const struct Scanner *scanner, TSLexer *lexer) {
  int32_t character = lexer->lookahead;
  if (character == '\'') {
    lexer->advance(lexer, false);
    while (!lexer_at_eof(lexer) && lexer->lookahead != '\'') {
      lexer->advance(lexer, false);
    }
    if (lexer_at_eof(lexer)) {
      return false;
    }
    lexer->advance(lexer, false);
    return true;
  }
  if (character == '"' || character == '`') {
    lexer->advance(lexer, false);
    return skip_embedded_construct(scanner, lexer, (char)character) ==
      ARITHMETIC_VALIDATION_VALID;
  }
  if (character == '$') {
    lexer->advance(lexer, false);
    int32_t introducer = lexer->lookahead;
    if (introducer == '\'') {
      lexer->advance(lexer, false);
      return scan_delimiter_single_quoted_segment(
        lexer,
        scanner,
        scanner->backquote_depth,
        NULL,
        true
      );
    }
    if (introducer == '(' || introducer == '{') {
      lexer->advance(lexer, false);
      return skip_embedded_construct(
               scanner,
               lexer,
               introducer == '(' ? ')' : '}'
             ) == ARITHMETIC_VALIDATION_VALID;
    }
    if (is_name_start_character(introducer)) {
      while (is_name_character(lexer->lookahead)) {
        lexer->advance(lexer, false);
      }
      return true;
    }
    if (
      is_decimal_digit(introducer) || is_special_parameter_character(introducer)
    ) {
      lexer->advance(lexer, false);
      return true;
    }
    return true;
  }
  return true;
}

enum BracketEscape {
  BRACKET_ESCAPE_CONTINUATION,
  BRACKET_ESCAPE_MEMBER,
  BRACKET_ESCAPE_END_OF_INPUT,
};

static enum BracketEscape
skip_bracket_escape(const struct Scanner *scanner, TSLexer *lexer) {
  if (scanner->backquote_depth == 0) {
    if (skip_line_continuations(lexer)) {
      return BRACKET_ESCAPE_CONTINUATION;
    }
    if (lexer_at_eof(lexer)) {
      return BRACKET_ESCAPE_END_OF_INPUT;
    }
    lexer->advance(lexer, false);
    return BRACKET_ESCAPE_MEMBER;
  }
  size_t run;
  if (!count_escape_run(lexer, 0, &run)) {
    return BRACKET_ESCAPE_END_OF_INPUT;
  }
  if (lexer_at_eof(lexer)) {
    return BRACKET_ESCAPE_END_OF_INPUT;
  }
  int32_t follower = lexer->lookahead;
  if (follower == '\n') {
    if ((run & 1) != 0) {
      lexer->advance(lexer, false);
    }
    return run >= 2 ? BRACKET_ESCAPE_MEMBER : BRACKET_ESCAPE_CONTINUATION;
  }
  bool follower_is_escaped;
  if (follower == '`') {
    enum BackquoteTickPrefix prefix =
      classify_backquote_tick_prefix(scanner->backquote_depth, run);
    if (prefix == BACKQUOTE_TICK_PREFIX_START) {
      lexer->advance(lexer, false);
      return skip_embedded_construct(scanner, lexer, '`') ==
          ARITHMETIC_VALIDATION_VALID
        ? BRACKET_ESCAPE_MEMBER
        : BRACKET_ESCAPE_END_OF_INPUT;
    }
    follower_is_escaped = prefix ==
      BACKQUOTE_TICK_PREFIX_NONE &&
      fold_backquote_escape_run(scanner->backquote_depth, run).acting_level >
      scanner->backquote_depth +
      1;
  } else {
    follower_is_escaped = (fold_enclosed_escape_run(
                             scanner,
                             run,
                             scanner->backquote_depth,
                             follower
                           ) &
                            1) != 0;
  }
  if (follower_is_escaped) {
    lexer->advance(lexer, false);
  }
  return BRACKET_ESCAPE_MEMBER;
}

static bool is_bracket_literal_boundary(
  const struct Scanner *scanner,
  const TSLexer *lexer,
  bool parameter_pattern,
  enum TokenType symbol
) {
  return is_bracket_scan_boundary(scanner, lexer, parameter_pattern) ||
    (symbol ==
      ASSIGNMENT_TILDE_BRACKET_LITERAL_START &&
      lexer->lookahead == ':') ||
    ((symbol ==
       TILDE_BRACKET_LITERAL_START ||
       symbol == ASSIGNMENT_TILDE_BRACKET_LITERAL_START) &&
      lexer->lookahead == '/');
}

static bool skip_bracket_class_element(
  const struct Scanner *scanner,
  TSLexer *lexer,
  bool parameter_pattern,
  enum TokenType symbol
) {
  int32_t marker = lexer->lookahead;
  lexer->advance(lexer, false);
  bool has_content = false;
  while (
    !is_bracket_literal_boundary(scanner, lexer, parameter_pattern, symbol)
  ) {
    int32_t character = lexer->lookahead;
    if (character == '\\') {
      switch (skip_bracket_escape(scanner, lexer)) {
      case BRACKET_ESCAPE_CONTINUATION:
        continue;
      case BRACKET_ESCAPE_MEMBER:
        has_content = true;
        continue;
      case BRACKET_ESCAPE_END_OF_INPUT:
        return false;
      }
    }
    if (is_quote_or_expansion_start(character)) {
      if (!skip_bracket_member_expansion(scanner, lexer)) {
        return false;
      }
      has_content = true;
      continue;
    }
    if (character == ']' && marker != '.') {
      return false;
    }
    lexer->advance(lexer, false);
    if (character == marker && has_content) {
      if (lexer->lookahead == ']') {
        lexer->advance(lexer, false);
        return true;
      }
    }
    has_content = true;
  }
  return false;
}

static bool scan_bracket_literal_start(
  const struct Scanner *scanner,
  TSLexer *lexer,
  enum TokenType symbol,
  enum TokenType commit_symbol,
  bool parameter_pattern
) {
  if (lexer->lookahead != '[') {
    return false;
  }

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);

  bool has_member = false;
  bool may_be_negation = true;
  while (
    !is_bracket_literal_boundary(scanner, lexer, parameter_pattern, symbol)
  ) {
    int32_t character = lexer->lookahead;

    if (character == '\\') {
      switch (skip_bracket_escape(scanner, lexer)) {
      case BRACKET_ESCAPE_CONTINUATION:
        continue;
      case BRACKET_ESCAPE_MEMBER:
        has_member = true;
        may_be_negation = false;
        continue;
      case BRACKET_ESCAPE_END_OF_INPUT:
        lexer->result_symbol = (TSSymbol)symbol;
        return true;
      }
    }

    if (is_quote_or_expansion_start(character)) {
      if (!skip_bracket_member_expansion(scanner, lexer)) {
        lexer->result_symbol = (TSSymbol)symbol;
        return true;
      }
      has_member = true;
      may_be_negation = false;
      continue;
    }

    if (may_be_negation && character == '!') {
      may_be_negation = false;
      lexer->advance(lexer, false);
      continue;
    }
    may_be_negation = false;

    if (character == ']') {
      if (has_member) {
        lexer->result_symbol = (TSSymbol)commit_symbol;
        return true;
      }
      lexer->advance(lexer, false);
      has_member = true;
      continue;
    }

    if (character == '[') {
      lexer->advance(lexer, false);
      has_member = true;
      int32_t nested_marker = lexer->lookahead;
      if (
        (
          nested_marker == ':' || nested_marker == '.' || nested_marker == '='
        ) &&
        !skip_bracket_class_element(scanner, lexer, parameter_pattern, symbol)
      ) {
        lexer->result_symbol = (TSSymbol)symbol;
        return true;
      }
      continue;
    }

    lexer->advance(lexer, false);
    has_member = true;
  }

  lexer->result_symbol = (TSSymbol)symbol;
  return true;
}

static bool scan_pattern_bracket_character(
  struct Scanner *scanner,
  TSLexer *lexer,
  enum TokenType symbol,
  bool parameter_pattern
) {
  int32_t character = lexer->lookahead;
  if (
    lexer_at_eof(lexer) ||
    character ==
    '[' ||
    character ==
    ']' ||
    character ==
    '-' ||
    character ==
    '!' ||
    character ==
    '*' ||
    character ==
    '?' ||
    character ==
    ':' ||
    character ==
    '.' ||
    character ==
    '=' ||
    character ==
    '\\' ||
    is_quote_or_expansion_start(character) ||
    is_bracket_scan_boundary(scanner, lexer, parameter_pattern)
  ) {
    return false;
  }

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  if (character == '\n' && scanner->active_count > 0) {
    reset_here_document_delimiter_scan(scanner);
    scanner->at_here_document_line_start = true;
  }
  lexer->result_symbol = (TSSymbol)symbol;
  return true;
}

static bool scan_bracket_fallback_end(
  const struct Scanner *scanner,
  TSLexer *lexer,
  enum TokenType end_symbol,
  bool parameter_pattern
) {
  lexer->mark_end(lexer);
  if (!is_bracket_scan_boundary(scanner, lexer, parameter_pattern)) {
    return false;
  }

  lexer->result_symbol = (TSSymbol)end_symbol;
  return true;
}

enum DelimiterQuote {
  DELIMITER_UNQUOTED,
  DELIMITER_SINGLE_QUOTED,
  DELIMITER_DOUBLE_QUOTED,
  DELIMITER_PARAMETER_DOUBLE_QUOTED,
  DELIMITER_DOLLAR_SINGLE_QUOTED,
};

enum ParameterQuoteState {
  PARAMETER_QUOTE_START,
  PARAMETER_QUOTE_NAME,
  PARAMETER_QUOTE_POSITIONAL,
  PARAMETER_QUOTE_OPERATOR,
  PARAMETER_QUOTE_WORD,
  PARAMETER_QUOTE_PATTERN,
};

static enum ParameterQuoteState
parameter_quote_state_after(enum ParameterQuoteState state, int32_t character) {
  switch (state) {
  case PARAMETER_QUOTE_START:
    if (is_name_start_character(character)) {
      return PARAMETER_QUOTE_NAME;
    }
    if (is_decimal_digit(character)) {
      return PARAMETER_QUOTE_POSITIONAL;
    }
    return is_special_parameter_character(character) ? PARAMETER_QUOTE_OPERATOR
                                                     : PARAMETER_QUOTE_WORD;
  case PARAMETER_QUOTE_NAME:
    if (is_name_character(character)) {
      return state;
    }
    break;
  case PARAMETER_QUOTE_POSITIONAL:
    if (is_decimal_digit(character)) {
      return state;
    }
    break;
  case PARAMETER_QUOTE_OPERATOR:
    break;
  case PARAMETER_QUOTE_WORD:
  case PARAMETER_QUOTE_PATTERN:
    return state;
  }
  return character == '#' || character == '%' ? PARAMETER_QUOTE_PATTERN
                                              : PARAMETER_QUOTE_WORD;
}

enum DelimiterGroupKind {
  DELIMITER_GROUP_PARAMETER,
  DELIMITER_GROUP_COMMAND,
  DELIMITER_GROUP_ARITHMETIC,
  DELIMITER_GROUP_SUBSHELL,
  DELIMITER_GROUP_FUNCTION_HEADER,
  DELIMITER_GROUP_BACKQUOTE,
};

struct CommandWord {
  char text[6];
  uint8_t length;
  bool active;
  bool candidate;
  bool digits;
  bool name;
};

enum CommandPosition {
  COMMAND_POSITION_START,
  COMMAND_POSITION_WORD,
  COMMAND_POSITION_CLOSED,
  COMMAND_POSITION_FOR_NAME,
  COMMAND_POSITION_FOR_IN,
};

struct CommandTracker {
  struct CommandWord word;
  enum CommandPosition position;
  bool redirect_operand;
  bool function_name;
};

struct DelimiterGroupFrame {
  int32_t closing;
  enum DelimiterGroupKind kind;
  enum DelimiterQuote parent_quote;
  enum ParameterQuoteState parameter_quote_state;
  struct CommandTracker command;
};

struct DelimiterGroupBuffer {
  struct DelimiterGroupFrame *data;
  size_t length;
  size_t capacity;
  bool failed;
};

// Delimiter scans and embedded skips share case tracking. EXPECT_PATTERN
// distinguishes an esac closer from an ordinary word later in the pattern.
enum CaseTrackerState {
  CASE_TRACKER_EXPECT_WORD,
  CASE_TRACKER_EXPECT_IN,
  CASE_TRACKER_EXPECT_PATTERN,
  CASE_TRACKER_IN_PATTERN,
  CASE_TRACKER_BODY,
};

struct CaseTracker {
  size_t depth;
  uint8_t state;
};

static bool case_tracker_in_pattern(uint8_t state) {
  return (
    state == CASE_TRACKER_EXPECT_PATTERN || state == CASE_TRACKER_IN_PATTERN
  );
}

struct CaseTrackerBuffer {
  struct CaseTracker *data;
  size_t length;
  size_t capacity;
  bool failed;
};

static struct CaseTracker *
active_case_tracker(const struct CaseTrackerBuffer *cases, size_t depth) {
  if (cases->length == 0) {
    return NULL;
  }
  struct CaseTracker *tracker = &cases->data[cases->length - 1];
  return tracker->depth == depth ? tracker : NULL;
}

static void
pop_case_trackers_at_depth(struct CaseTrackerBuffer *cases, size_t depth) {
  while (cases->length > 0 && cases->data[cases->length - 1].depth >= depth) {
    cases->length -= 1;
  }
}

enum CaseWordKind {
  CASE_WORD_GENERIC,
  CASE_WORD_IN,
  CASE_WORD_ESAC,
  CASE_WORD_CASE,
  CASE_WORD_FOR,
  CASE_WORD_DO,
  CASE_WORD_COMMAND_CLOSE,
  CASE_WORD_COMMAND_PREFIX,
};

static enum CaseWordKind classify_case_word(const char *word) {
  if (strcmp(word, "in") == 0) {
    return CASE_WORD_IN;
  }
  if (strcmp(word, "esac") == 0) {
    return CASE_WORD_ESAC;
  }
  if (strcmp(word, "case") == 0) {
    return CASE_WORD_CASE;
  }
  if (strcmp(word, "for") == 0) {
    return CASE_WORD_FOR;
  }
  if (strcmp(word, "do") == 0) {
    return CASE_WORD_DO;
  }
  if (
    strcmp(word, "fi") ==
    0 ||
    strcmp(word, "done") ==
    0 ||
    strcmp(word, "}") == 0
  ) {
    return CASE_WORD_COMMAND_CLOSE;
  }

  static const char *const COMMAND_PREFIXES[] = {
    "!",
    "{",
    "if",
    "then",
    "elif",
    "else",
    "while",
    "until",
  };
  for (
    size_t index = 0;
    index < sizeof(COMMAND_PREFIXES) / sizeof(COMMAND_PREFIXES[0]);
    index += 1
  ) {
    if (strcmp(word, COMMAND_PREFIXES[index]) == 0) {
      return CASE_WORD_COMMAND_PREFIX;
    }
  }
  return CASE_WORD_GENERIC;
}

enum CaseTrackerNote {
  CASE_TRACKER_NOTE_WORD,
  CASE_TRACKER_NOTE_COMMAND_PREFIX,
  CASE_TRACKER_NOTE_END,
  CASE_TRACKER_NOTE_BEGIN,
};

static enum CaseTrackerNote case_tracker_note_word(
  struct CaseTracker *tracker,
  enum CaseWordKind kind,
  bool at_command_start
) {
  if (tracker != NULL && tracker->state != CASE_TRACKER_BODY) {
    switch (tracker->state) {
    case CASE_TRACKER_EXPECT_WORD:
      tracker->state = CASE_TRACKER_EXPECT_IN;
      break;
    case CASE_TRACKER_EXPECT_IN:
      if (kind == CASE_WORD_IN) {
        tracker->state = CASE_TRACKER_EXPECT_PATTERN;
      }
      break;
    case CASE_TRACKER_EXPECT_PATTERN:
      if (kind == CASE_WORD_ESAC) {
        return CASE_TRACKER_NOTE_END;
      }
      tracker->state = CASE_TRACKER_IN_PATTERN;
      break;
    default:
      break;
    }
    return CASE_TRACKER_NOTE_WORD;
  }

  if (!at_command_start) {
    return CASE_TRACKER_NOTE_WORD;
  }
  if (kind == CASE_WORD_ESAC && tracker != NULL) {
    return CASE_TRACKER_NOTE_END;
  }
  if (kind == CASE_WORD_COMMAND_PREFIX || kind == CASE_WORD_DO) {
    return CASE_TRACKER_NOTE_COMMAND_PREFIX;
  }
  if (kind == CASE_WORD_CASE) {
    return CASE_TRACKER_NOTE_BEGIN;
  }
  return CASE_TRACKER_NOTE_WORD;
}

static void *grow_element_buffer(
  void *data,
  size_t *capacity,
  size_t length,
  size_t element_size,
  size_t initial_capacity
) {
  if (length < *capacity) {
    return data;
  }

  size_t next_capacity = *capacity == 0 ? initial_capacity : *capacity * 2;
  if (next_capacity < *capacity || next_capacity > SIZE_MAX / element_size) {
    return NULL;
  }

  void *resized = ts_realloc(data, next_capacity * element_size);
  if (resized == NULL) {
    return NULL;
  }
  *capacity = next_capacity;
  return resized;
}

static bool append_case_tracker(struct CaseTrackerBuffer *cases, size_t depth) {
  struct CaseTracker *data = grow_element_buffer(
    cases->data,
    &cases->capacity,
    cases->length,
    sizeof(struct CaseTracker),
    8
  );
  if (data == NULL) {
    cases->failed = true;
    return false;
  }
  cases->data = data;
  cases->data[cases->length] = (struct CaseTracker){
    .depth = depth,
    .state = CASE_TRACKER_EXPECT_WORD,
  };
  cases->length += 1;
  return true;
}

static bool delimiter_group_holds_commands(enum DelimiterGroupKind kind) {
  return (
    kind ==
    DELIMITER_GROUP_COMMAND ||
    kind ==
    DELIMITER_GROUP_SUBSHELL ||
    kind == DELIMITER_GROUP_BACKQUOTE
  );
}

static bool push_delimiter_group(
  struct DelimiterGroupBuffer *groups,
  int32_t closing,
  enum DelimiterGroupKind kind,
  enum DelimiterQuote parent_quote
) {
  if (groups->length >= SCANNER_STATE_CAPACITY) {
    groups->failed = true;
    return false;
  }
  struct DelimiterGroupFrame *data = grow_element_buffer(
    groups->data,
    &groups->capacity,
    groups->length,
    sizeof(struct DelimiterGroupFrame),
    16
  );
  if (data == NULL) {
    groups->failed = true;
    return false;
  }
  groups->data = data;
  groups->data[groups->length] = (struct DelimiterGroupFrame){
    .closing = closing,
    .kind = kind,
    .parent_quote = parent_quote,
    .command = {.word = {.candidate = true, .digits = true, .name = true}},
  };
  groups->length += 1;
  return true;
}

static bool is_delimiter_word_character(int32_t character) {
  return (
    (character >= 'A' && character <= 'Z') ||
    (character >= 'a' && character <= 'z') ||
    (character >= '0' && character <= '9') ||
    character ==
    '_' ||
    character ==
    '!' ||
    character ==
    '{' ||
    character == '}'
  );
}

static void reset_command_word(struct CommandWord *word) {
  word->length = 0;
  word->active = false;
  word->candidate = true;
  word->digits = true;
  word->name = true;
}

static void reset_command_position(
  struct CommandTracker *command,
  enum CommandPosition position
) {
  reset_command_word(&command->word);
  command->position = position;
  command->redirect_operand = false;
  command->function_name = false;
}

static bool finish_command_word(
  struct CaseTrackerBuffer *cases,
  size_t depth,
  struct CommandTracker *command,
  bool before_redirect
) {
  struct CommandWord *word = &command->word;
  if (!word->active) {
    reset_command_word(word);
    return true;
  }
  if (word->digits && before_redirect) {
    reset_command_word(word);
    return true;
  }
  command->function_name = false;
  if (command->redirect_operand) {
    command->redirect_operand = false;
    reset_command_word(word);
    return true;
  }

  char text[sizeof(word->text) + 1] = {0};
  if (word->candidate) {
    memcpy(text, word->text, word->length);
  }
  enum CaseWordKind kind =
    word->candidate ? classify_case_word(text) : CASE_WORD_GENERIC;
  struct CaseTracker *tracker = active_case_tracker(cases, depth);
  bool in_case_header = tracker != NULL && tracker->state != CASE_TRACKER_BODY;
  enum CommandPosition position = command->position;
  bool at_command_start =
    position == COMMAND_POSITION_START || position == COMMAND_POSITION_CLOSED;
  command->position = COMMAND_POSITION_WORD;
  switch (case_tracker_note_word(tracker, kind, at_command_start)) {
  case CASE_TRACKER_NOTE_COMMAND_PREFIX:
    command->position = COMMAND_POSITION_START;
    break;
  case CASE_TRACKER_NOTE_END:
    cases->length -= 1;
    command->position = COMMAND_POSITION_CLOSED;
    break;
  case CASE_TRACKER_NOTE_BEGIN:
    if (!append_case_tracker(cases, depth)) {
      return false;
    }
    break;
  default:
    if (in_case_header) {
      break;
    }
    if (position == COMMAND_POSITION_FOR_NAME) {
      command->position = COMMAND_POSITION_FOR_IN;
    } else if (position == COMMAND_POSITION_FOR_IN) {
      if (kind == CASE_WORD_DO) {
        command->position = COMMAND_POSITION_START;
      }
    } else if (at_command_start) {
      if (kind == CASE_WORD_FOR) {
        command->position = COMMAND_POSITION_FOR_NAME;
      } else if (kind == CASE_WORD_COMMAND_CLOSE) {
        command->position = COMMAND_POSITION_CLOSED;
      } else {
        command->function_name = word->name;
      }
    }
    break;
  }
  reset_command_word(word);
  return true;
}

static void begin_command_redirect(struct CommandTracker *command) {
  if (command->position != COMMAND_POSITION_CLOSED) {
    command->position = COMMAND_POSITION_WORD;
  }
  command->redirect_operand = true;
  command->function_name = false;
}

static size_t
delimiter_command_group_depth(const struct DelimiterGroupBuffer *groups) {
  if (
    groups->length ==
    0 ||
    !delimiter_group_holds_commands(groups->data[groups->length - 1].kind)
  ) {
    return 0;
  }
  return groups->length;
}

static bool
append_repeated_byte(struct ByteBuffer *buffer, uint8_t byte, size_t count) {
  if (buffer == NULL) {
    return true;
  }

  if (count > SIZE_MAX - buffer->length) {
    buffer->failed = true;
    return false;
  }

  size_t next_length = buffer->length + count;
  if (
    next_length > buffer->capacity && !grow_byte_buffer(buffer, next_length)
  ) {
    return false;
  }
  if (count > 0) {
    memset(buffer->data + buffer->length, byte, count);
  }
  buffer->length = next_length;
  return true;
}

static void pop_delimiter_group(
  struct DelimiterGroupBuffer *groups,
  struct CaseTrackerBuffer *cases,
  enum DelimiterQuote *quote
) {
  size_t group_depth = groups->length;
  enum DelimiterGroupKind kind = groups->data[group_depth - 1].kind;
  *quote = groups->data[group_depth - 1].parent_quote;
  pop_case_trackers_at_depth(cases, group_depth);
  groups->length -= 1;
  size_t command_depth = delimiter_command_group_depth(groups);
  if (command_depth > 0) {
    struct CommandTracker *command = &groups->data[command_depth - 1].command;
    if (kind == DELIMITER_GROUP_FUNCTION_HEADER) {
      reset_command_position(command, COMMAND_POSITION_START);
    } else if (kind == DELIMITER_GROUP_SUBSHELL) {
      reset_command_position(command, COMMAND_POSITION_CLOSED);
    }
  }
}

static bool toggle_delimiter_backquote_group(
  struct DelimiterGroupBuffer *groups,
  struct CaseTrackerBuffer *cases,
  enum DelimiterQuote *quote,
  size_t *backquote_depth
) {
  if (
    groups->length >
    0 &&
    groups->data[groups->length - 1].kind == DELIMITER_GROUP_BACKQUOTE
  ) {
    pop_delimiter_group(groups, cases, quote);
    *backquote_depth -= 1;
    return true;
  }

  if (
    *backquote_depth ==
    SIZE_MAX ||
    !push_delimiter_group(groups, '`', DELIMITER_GROUP_BACKQUOTE, *quote)
  ) {
    return false;
  }
  *backquote_depth += 1;
  *quote = DELIMITER_UNQUOTED;
  return true;
}

enum DelimiterBackslashResult {
  DELIMITER_BACKSLASH_OK,
  DELIMITER_BACKSLASH_LEADING_CONTINUATION,
  DELIMITER_BACKSLASH_FOLDED_CONTINUATION,
  DELIMITER_BACKSLASH_ERROR,
};

static void mark_delimiter_quoted(
  bool *quoted,
  bool collecting_nested_delimiter,
  bool *nested_delimiter_quoted
) {
  *quoted = true;
  if (collecting_nested_delimiter) {
    *nested_delimiter_quoted = true;
  }
}

static enum DelimiterBackslashResult scan_delimiter_backslash_run(
  TSLexer *lexer,
  const struct Scanner *scanner,
  struct ByteBuffer *delimiter,
  struct DelimiterGroupBuffer *groups,
  struct CaseTrackerBuffer *cases,
  enum DelimiterQuote *quote,
  size_t *backquote_depth,
  size_t ambient_backquote_depth,
  bool at_delimiter_source_start,
  bool collecting_nested_delimiter,
  bool *quoted,
  bool *nested_delimiter_quoted
) {
  size_t escape_count;
  if (!count_escape_run(lexer, 0, &escape_count)) {
    return DELIMITER_BACKSLASH_ERROR;
  }

  if (lexer->lookahead == '`') {
    struct BackquoteEscapeRunFold fold =
      fold_backquote_escape_run(*backquote_depth, escape_count);
    bool ends_group = fold.acting_level == *backquote_depth;
    if (
      fold.acting_level <
      *backquote_depth ||
      (ends_group &&
        (groups->length ==
          0 ||
          groups->data[groups->length - 1].kind != DELIMITER_GROUP_BACKQUOTE))
    ) {
      return DELIMITER_BACKSLASH_ERROR;
    }
    size_t folded = ends_group
      ? fold.leftover_count
      : fold_enclosed_special_run(escape_count, *backquote_depth);
    if (
      !append_repeated_byte(delimiter, '\\', folded / 2) ||
      !append_byte(delimiter, '`')
    ) {
      return DELIMITER_BACKSLASH_ERROR;
    }
    if (fold_enclosed_special_run(escape_count, ambient_backquote_depth) > 0) {
      mark_delimiter_quoted(
        quoted,
        collecting_nested_delimiter,
        nested_delimiter_quoted
      );
    }
    lexer->advance(lexer, false);

    if (ends_group) {
      pop_delimiter_group(groups, cases, quote);
      *backquote_depth -= 1;
    } else if (fold.acting_level == *backquote_depth + 1) {
      if (
        *backquote_depth ==
        SIZE_MAX ||
        !push_delimiter_group(groups, '`', DELIMITER_GROUP_BACKQUOTE, *quote)
      ) {
        return DELIMITER_BACKSLASH_ERROR;
      }
      *backquote_depth += 1;
      *quote = DELIMITER_UNQUOTED;
    }
    return DELIMITER_BACKSLASH_OK;
  }

  if (lexer_at_eof(lexer) || lexer->lookahead == '\n') {
    size_t folded = fold_enclosed_plain_run(escape_count, *backquote_depth);
    size_t quoted_backslashes = folded / 2;
    if (!append_repeated_byte(delimiter, '\\', quoted_backslashes)) {
      return DELIMITER_BACKSLASH_ERROR;
    }
    if (quoted_backslashes > 0) {
      mark_delimiter_quoted(
        quoted,
        collecting_nested_delimiter,
        nested_delimiter_quoted
      );
    }
    if ((folded & 1) == 0) {
      return DELIMITER_BACKSLASH_OK;
    }
    if (lexer_at_eof(lexer)) {
      return DELIMITER_BACKSLASH_ERROR;
    }
    lexer->advance(lexer, false);
    if (at_delimiter_source_start && folded == 1) {
      return escape_count == 1 ? DELIMITER_BACKSLASH_LEADING_CONTINUATION
                               : DELIMITER_BACKSLASH_FOLDED_CONTINUATION;
    }
    return DELIMITER_BACKSLASH_OK;
  }

  size_t folded = fold_enclosed_escape_run(
    scanner,
    escape_count,
    *backquote_depth,
    lexer->lookahead
  );

  if (
    (*quote ==
      DELIMITER_DOUBLE_QUOTED ||
      *quote == DELIMITER_PARAMETER_DOUBLE_QUOTED) &&
    lexer->lookahead !=
    '$' &&
    lexer->lookahead !=
    '`' &&
    lexer->lookahead !=
    '"' &&
    !(*quote == DELIMITER_PARAMETER_DOUBLE_QUOTED && lexer->lookahead == '}')
  ) {
    return append_repeated_byte(delimiter, '\\', folded)
      ? DELIMITER_BACKSLASH_OK
      : DELIMITER_BACKSLASH_ERROR;
  }

  size_t literal_backslashes = folded >> 1;
  if (!append_repeated_byte(delimiter, '\\', literal_backslashes)) {
    return DELIMITER_BACKSLASH_ERROR;
  }
  if (literal_backslashes > 0) {
    mark_delimiter_quoted(
      quoted,
      collecting_nested_delimiter,
      nested_delimiter_quoted
    );
  }

  if ((folded & 1) == 0) {
    return DELIMITER_BACKSLASH_OK;
  }

  if (!append_codepoint(delimiter, lexer->lookahead)) {
    return DELIMITER_BACKSLASH_ERROR;
  }
  mark_delimiter_quoted(
    quoted,
    collecting_nested_delimiter,
    nested_delimiter_quoted
  );
  lexer->advance(lexer, false);
  return DELIMITER_BACKSLASH_OK;
}

static void
track_command_word_character(struct CommandWord *word, int32_t character) {
  if (!word->active) {
    reset_command_word(word);
    word->name = is_name_start_character(character);
    word->active = true;
  } else if (!is_name_character(character)) {
    word->name = false;
  }
  if (!is_decimal_digit(character)) {
    word->digits = false;
  }

  if (!is_delimiter_word_character(character)) {
    word->candidate = false;
    return;
  }

  if (word->length < sizeof(word->text)) {
    word->text[word->length] = (char)character;
    word->length += 1;
  } else {
    word->candidate = false;
  }
}

static void track_delimiter_command_character(
  struct CommandWord *command_word,
  bool at_nested_delimiter_base,
  int32_t character
) {
  if (command_word != NULL && !at_nested_delimiter_base) {
    track_command_word_character(command_word, character);
  }
}

static bool is_hexadecimal_digit(int32_t character) {
  return (
    (character >= '0' && character <= '9') ||
    (character >= 'A' && character <= 'F') ||
    (character >= 'a' && character <= 'f')
  );
}

static uint8_t hexadecimal_value(int32_t character) {
  if (character >= '0' && character <= '9') {
    return (uint8_t)(character - '0');
  }

  if (character >= 'A' && character <= 'F') {
    return (uint8_t)(character - 'A' + 10);
  }

  return (uint8_t)(character - 'a' + 10);
}

static bool defined_control_escape_byte(int32_t character, uint8_t *value) {
  if (character >= 'a' && character <= 'z') {
    *value = (uint8_t)(character - 'a' + 1);
    return true;
  }
  if (character >= 'A' && character <= 'Z') {
    *value = (uint8_t)(character - 'A' + 1);
    return true;
  }

  switch (character) {
  case '[':
    *value = 0x1b;
    return true;
  case '\\':
    *value = 0x1c;
    return true;
  case ']':
    *value = 0x1d;
    return true;
  case '^':
    *value = 0x1e;
    return true;
  case '_':
    *value = 0x1f;
    return true;
  case '?':
    *value = 0x7f;
    return true;
  default:
    return false;
  }
}

static bool control_escape_byte(int32_t character, uint8_t *value) {
  if (defined_control_escape_byte(character, value)) {
    return true;
  }
  if (character < 0) {
    return false;
  }

  *value = (uint8_t)((uint32_t)character & UINT8_C(0x1f));
  return true;
}

static bool scan_dollar_single_quote_backslashes(
  const struct Scanner *scanner,
  TSLexer *lexer,
  size_t depth,
  size_t *folded
);

static bool scan_dollar_single_quote_escape(
  const struct Scanner *scanner,
  TSLexer *lexer,
  size_t depth,
  struct ByteBuffer *delimiter
) {
  int32_t character = lexer->lookahead;
  if (lexer_at_eof(lexer)) {
    return false;
  }

  if (character == 'x') {
    lexer->advance(lexer, false);
    if (!is_hexadecimal_digit(lexer->lookahead)) {
      return append_byte(delimiter, 'x');
    }

    uint8_t value = 0;
    uint8_t digits = 0;
    while (digits < 2 && is_hexadecimal_digit(lexer->lookahead)) {
      value = (uint8_t)((value << 4) | hexadecimal_value(lexer->lookahead));
      digits += 1;
      lexer->advance(lexer, false);
    }
    return append_byte(delimiter, value);
  }

  if (character >= '0' && character <= '7') {
    uint8_t value = 0;
    uint8_t digits = 0;
    while (digits < 3 && lexer->lookahead >= '0' && lexer->lookahead <= '7') {
      value = (uint8_t)((value << 3) | (lexer->lookahead - '0'));
      digits += 1;
      lexer->advance(lexer, false);
    }
    return append_byte(delimiter, value);
  }

  if (character == 'c') {
    lexer->advance(lexer, false);
    if (lexer_at_eof(lexer)) {
      return false;
    }

    if (lexer->lookahead == '\\') {
      size_t folded;
      if (
        !scan_dollar_single_quote_backslashes(scanner, lexer, depth, &folded) ||
        folded != 2
      ) {
        return false;
      }
      uint8_t value;
      return control_escape_byte('\\', &value) && append_byte(delimiter, value);
    }

    int32_t control_character = lexer->lookahead;
    lexer->advance(lexer, false);
    uint8_t value;
    return control_escape_byte(control_character, &value) &&
      append_byte(delimiter, value);
  }

  lexer->advance(lexer, false);
  return append_quoted_escape(delimiter, character);
}

static struct BackquoteEscapeRunFold
fold_backquote_escape_run(size_t depth, size_t escape_count) {
  struct BackquoteEscapeRunFold fold = {
    .acting_level = 1,
    .leftover_count = escape_count,
  };
  while ((fold.leftover_count & 1) != 0 && fold.acting_level <= depth + 1) {
    fold.leftover_count >>= 1;
    fold.acting_level += 1;
  }
  return fold;
}

static enum BackquoteTickPrefix
classify_backquote_tick_prefix(size_t depth, size_t escape_count) {
  struct BackquoteEscapeRunFold fold =
    fold_backquote_escape_run(depth, escape_count);
  if (fold.leftover_count != 0) {
    return BACKQUOTE_TICK_PREFIX_NONE;
  }

  if (fold.acting_level == depth) {
    return BACKQUOTE_TICK_PREFIX_END;
  }

  if (fold.acting_level == depth + 1) {
    return BACKQUOTE_TICK_PREFIX_START;
  }

  return BACKQUOTE_TICK_PREFIX_NONE;
}

static size_t fold_enclosed_plain_run(size_t run, size_t depth) {
  size_t folded = run;
  for (size_t level = 0; level < depth && folded > 1; level += 1) {
    folded -= folded >> 1;
  }
  return folded;
}

static size_t fold_enclosed_special_run(size_t run, size_t depth) {
  return depth < sizeof(size_t) * CHAR_BIT ? run >> depth : 0;
}

static size_t fold_enclosed_quote_run(
  const struct Scanner *scanner,
  size_t run,
  size_t depth
) {
  if (scanner == NULL) {
    return fold_enclosed_plain_run(run, depth);
  }
  size_t level = 0;
  for (size_t index = 0; index < scanner->quoted_backquote_count; index += 1) {
    size_t quoted_depth = scanner->quoted_backquote_depths[index];
    if (quoted_depth > depth) {
      break;
    }
    run = fold_enclosed_plain_run(run, quoted_depth - level - 1) >> 1;
    level = quoted_depth;
  }
  return fold_enclosed_plain_run(run, depth - level);
}

static size_t fold_enclosed_escape_run(
  const struct Scanner *scanner,
  size_t run,
  size_t depth,
  int32_t following
) {
  switch (following) {
  case '$':
  case '`':
    return fold_enclosed_special_run(run, depth);
  case '"':
    return fold_enclosed_quote_run(scanner, run, depth);
  default:
    return fold_enclosed_plain_run(run, depth);
  }
}

static bool
fold_enclosed_backquote_run(size_t run, size_t depth, size_t *surviving) {
  struct BackquoteEscapeRunFold fold = fold_backquote_escape_run(depth, run);
  if (fold.acting_level <= depth) {
    return false;
  }
  *surviving = fold_enclosed_special_run(run, depth);
  return true;
}

static bool append_nested_here_document(
  struct HereDocument **documents,
  size_t *count,
  struct ByteBuffer *source,
  size_t delimiter_start,
  bool quoted,
  bool strip_tabs
) {
  if (delimiter_start > source->length) {
    return false;
  }

  size_t delimiter_length = source->length - delimiter_start;
  uint8_t *delimiter = NULL;
  if (delimiter_length > 0) {
    delimiter = ts_malloc(delimiter_length);
    if (delimiter == NULL) {
      source->failed = true;
      return false;
    }
    memcpy(delimiter, source->data + delimiter_start, delimiter_length);
  }

  struct HereDocument document = {
    .delimiter = delimiter,
    .delimiter_length = delimiter_length,
    .quoted = quoted,
    .strip_tabs = strip_tabs,
  };
  if (!append_document(documents, count, document)) {
    source->failed = true;
    clear_document(&document);
    return false;
  }
  return true;
}

enum HereDocumentLineKind {
  HERE_DOCUMENT_LINE_DELIMITER,
  HERE_DOCUMENT_LINE_LAYOUT,
  HERE_DOCUMENT_LINE_CONTENT,
  HERE_DOCUMENT_LINE_END_OF_INPUT,
  HERE_DOCUMENT_LINE_INVALID,
};

struct HereDocumentLineStart {
  int32_t first_character;
  char first_word[6];
  bool first_is_delimited;
  bool first_word_is_reserved_candidate;
};

static bool match_here_document_delimiter_character(
  const struct HereDocument *document,
  size_t *offset,
  int32_t character
) {
  uint8_t bytes[4];
  size_t length;
  if (
    !encode_utf8_scalar(character, bytes, &length) ||
    length >
    document->delimiter_length -
    *offset ||
    memcmp(document->delimiter + *offset, bytes, length) != 0
  ) {
    return false;
  }
  *offset += length;
  return true;
}

static bool
advance_here_document_source(TSLexer *lexer, struct ByteBuffer *source) {
  if (source != NULL && !append_codepoint(source, lexer->lookahead)) {
    return false;
  }
  lexer->advance(lexer, false);
  return true;
}

static enum HereDocumentLineKind read_here_document_line(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const struct HereDocument *document,
  size_t depth,
  struct ByteBuffer *source,
  struct HereDocumentLineStart *start
) {
  if (start != NULL) {
    *start = (struct HereDocumentLineStart){0};
    if (lexer_at_eof(lexer)) {
      return HERE_DOCUMENT_LINE_END_OF_INPUT;
    }
  }

  bool at_logical_line_start = document->strip_tabs;
  size_t delimiter_offset = 0;
  bool matches = true;
  bool has_content = false;
  size_t word_length = 0;
  bool in_word = false;
  bool awaiting_delimiter = false;

  while (true) {
    if (at_logical_line_start && lexer->lookahead == '\t') {
      if (!advance_here_document_source(lexer, source)) {
        return HERE_DOCUMENT_LINE_INVALID;
      }
      continue;
    }

    int32_t character = lexer->lookahead;
    if (lexer_at_eof(lexer)) {
      break;
    }
    if (character == '\n') {
      if (!advance_here_document_source(lexer, source)) {
        return HERE_DOCUMENT_LINE_INVALID;
      }
      break;
    }

    size_t pending_backslashes = 0;
    int32_t pending_character = character;
    bool has_pending_character = true;
    if (character == '\\' && (depth > 0 || !document->quoted)) {
      size_t run = 0;
      while (lexer->lookahead == '\\') {
        run += 1;
        if (!advance_here_document_source(lexer, source)) {
          return HERE_DOCUMENT_LINE_INVALID;
        }
      }
      has_pending_character = false;
      if (depth == 0) {
        pending_backslashes = run;
        if ((pending_backslashes & 1) != 0 && lexer->lookahead == '\n') {
          if (!advance_here_document_source(lexer, source)) {
            return HERE_DOCUMENT_LINE_INVALID;
          }
          pending_backslashes -= 1;
        }
      } else if (lexer->lookahead == '`') {
        if (!advance_here_document_source(lexer, source)) {
          return HERE_DOCUMENT_LINE_INVALID;
        }
        pending_character = '`';
        has_pending_character = true;
        if (!fold_enclosed_backquote_run(run, depth, &pending_backslashes)) {
          pending_backslashes = 0;
          matches = false;
        }
      } else {
        pending_backslashes =
          fold_enclosed_escape_run(scanner, run, depth, lexer->lookahead);
        if (
          !document->quoted &&
          (pending_backslashes & 1) !=
          0 &&
          lexer->lookahead == '\n'
        ) {
          if (!advance_here_document_source(lexer, source)) {
            return HERE_DOCUMENT_LINE_INVALID;
          }
          pending_backslashes -= 1;
        }
      }
      if (pending_backslashes == 0 && !has_pending_character) {
        continue;
      }
    } else if (depth > 0 && character == '`') {
      if (matches && delimiter_offset == document->delimiter_length) {
        break;
      }
      matches = false;
      if (!advance_here_document_source(lexer, source)) {
        return HERE_DOCUMENT_LINE_INVALID;
      }
    } else if (!advance_here_document_source(lexer, source)) {
      return HERE_DOCUMENT_LINE_INVALID;
    }
    at_logical_line_start = false;

    while (pending_backslashes > 0 || has_pending_character) {
      int32_t current;
      if (pending_backslashes > 0) {
        pending_backslashes -= 1;
        current = '\\';
      } else {
        current = pending_character;
        has_pending_character = false;
      }

      if (matches) {
        matches = match_here_document_delimiter_character(
          document,
          &delimiter_offset,
          current
        );
      }
      if (start == NULL) {
        if (!matches && source == NULL) {
          return HERE_DOCUMENT_LINE_CONTENT;
        }
        continue;
      }

      if (!has_content && !is_horizontal_blank(current)) {
        has_content = true;
        start->first_character = current;
        in_word = is_lowercase_letter(current);
        start->first_word_is_reserved_candidate = in_word;
        awaiting_delimiter = !in_word;
        if (in_word) {
          start->first_word[word_length] = (char)current;
          word_length += 1;
        }
      } else if (in_word) {
        if (is_lowercase_letter(current)) {
          if (word_length < sizeof(start->first_word) - 1) {
            start->first_word[word_length] = (char)current;
            word_length += 1;
          } else {
            start->first_word_is_reserved_candidate = false;
          }
        } else {
          in_word = false;
          start->first_is_delimited =
            is_token_delimiter_character(scanner->backquote_depth, current);
        }
      } else if (awaiting_delimiter) {
        awaiting_delimiter = false;
        start->first_is_delimited =
          is_token_delimiter_character(scanner->backquote_depth, current);
      }
    }
  }

  if (start != NULL && (in_word || awaiting_delimiter)) {
    start->first_is_delimited = true;
  }
  if (matches && delimiter_offset == document->delimiter_length) {
    return HERE_DOCUMENT_LINE_DELIMITER;
  }
  if (start != NULL && (!has_content || start->first_character == '#')) {
    return HERE_DOCUMENT_LINE_LAYOUT;
  }
  return HERE_DOCUMENT_LINE_CONTENT;
}

static bool scan_nested_here_document_sequence(
  TSLexer *lexer,
  struct ByteBuffer *source,
  const struct HereDocument *documents,
  size_t count
) {
  for (size_t index = 0; index < count; index += 1) {
    while (true) {
      enum HereDocumentLineKind kind = read_here_document_line(
        NULL,
        lexer,
        &documents[index],
        0,
        source,
        NULL
      );
      if (kind == HERE_DOCUMENT_LINE_DELIMITER) {
        break;
      }
      if (kind == HERE_DOCUMENT_LINE_INVALID || lexer_at_eof(lexer)) {
        return false;
      }
    }
  }
  return true;
}

static bool scan_dollar_single_quote_backslashes(
  const struct Scanner *scanner,
  TSLexer *lexer,
  size_t depth,
  size_t *folded
) {
  size_t pair_width =
    depth < sizeof(size_t) * CHAR_BIT - 1 ? (size_t)2 << depth : SIZE_MAX;
  size_t count = 0;
  while (lexer->lookahead == '\\' && count < pair_width) {
    lexer->advance(lexer, false);
    count += 1;
  }
  if (count == pair_width) {
    *folded = 2;
    return true;
  }
  switch (lexer->lookahead) {
  case '`':
    return fold_enclosed_backquote_run(count, depth, folded);
  case '\n':
    *folded = fold_enclosed_quote_run(scanner, count, depth);
    break;
  default:
    *folded = fold_enclosed_escape_run(scanner, count, depth, lexer->lookahead);
    break;
  }
  return true;
}

static bool
scan_dollar_single_quote_token(const struct Scanner *scanner, TSLexer *lexer) {
  size_t folded;
  if (!scan_dollar_single_quote_backslashes(
        scanner,
        lexer,
        scanner->backquote_depth,
        &folded
      )) {
    return false;
  }
  if (folded == 0) {
    if (lexer_at_eof(lexer)) {
      return false;
    }
    lexer->advance(lexer, false);
    lexer->result_symbol = BACKQUOTE_DOLLAR_SINGLE_QUOTE_TEXT;
  } else {
    if (
      folded ==
      1 &&
      !scan_dollar_single_quote_escape(
        scanner,
        lexer,
        scanner->backquote_depth,
        NULL
      )
    ) {
      return false;
    }
    lexer->result_symbol = DOLLAR_SINGLE_QUOTE_ESCAPE;
  }
  lexer->mark_end(lexer);
  return true;
}

static bool push_dollar_delimiter_group(
  const struct Scanner *scanner,
  size_t backquote_depth,
  TSLexer *lexer,
  struct ByteBuffer *delimiter,
  struct DelimiterGroupBuffer *groups,
  enum DelimiterQuote parent_quote
) {
  int32_t opening = lexer->lookahead;
  char closing = opening == '(' ? ')' : '}';
  enum DelimiterGroupKind kind =
    opening == '(' ? DELIMITER_GROUP_COMMAND : DELIMITER_GROUP_PARAMETER;
  if (
    !push_delimiter_group(groups, closing, kind, parent_quote) ||
    !append_codepoint(delimiter, opening)
  ) {
    return false;
  }
  lexer->advance(lexer, false);

  if (opening == '(' && lexer->lookahead == '(') {
    struct Scanner context = scanner == NULL ? (struct Scanner){0} : *scanner;
    context.backquote_depth = backquote_depth;
    const struct AmbiguousSubstitution *substitution =
      lookup_ambiguous_substitution(&context, lexer);
    if (substitution == NULL) {
      return false;
    }
    if (!substitution->arithmetic) {
      return true;
    }
    groups->data[groups->length - 1].kind = DELIMITER_GROUP_ARITHMETIC;
    reset_command_position(
      &groups->data[groups->length - 1].command,
      COMMAND_POSITION_WORD
    );
    if (
      !push_delimiter_group(
        groups,
        ')',
        DELIMITER_GROUP_ARITHMETIC,
        DELIMITER_UNQUOTED
      ) ||
      !append_byte(delimiter, '(')
    ) {
      return false;
    }
    lexer->advance(lexer, false);
  }
  return true;
}

static bool scan_delimiter_single_quoted_segment(
  TSLexer *lexer,
  const struct Scanner *scanner,
  size_t backquote_depth,
  struct ByteBuffer *delimiter,
  bool dollar
) {
  while (!lexer_at_eof(lexer)) {
    int32_t character = lexer->lookahead;
    if (character == '\'') {
      lexer->advance(lexer, false);
      return true;
    }
    if (character == '\\' && backquote_depth > 0) {
      size_t run;
      if (!count_escape_run(lexer, 0, &run)) {
        return false;
      }
      int32_t following = lexer->lookahead;
      size_t folded;
      if (following == '`') {
        if (!fold_enclosed_backquote_run(run, backquote_depth, &folded)) {
          return false;
        }
      } else if (following == '\n') {
        folded = fold_enclosed_quote_run(scanner, run, backquote_depth);
      } else {
        folded =
          fold_enclosed_escape_run(scanner, run, backquote_depth, following);
      }
      if (!append_repeated_byte(
            delimiter,
            '\\',
            dollar ? folded / 2 : folded
          )) {
        return false;
      }
      if (dollar && (folded & 1) != 0) {
        if (!scan_dollar_single_quote_escape(
              scanner,
              lexer,
              backquote_depth,
              delimiter
            )) {
          return false;
        }
      } else if (following == '\n' && folded == 0) {
        lexer->advance(lexer, false);
      } else if (following == '`') {
        if (!append_byte(delimiter, '`')) {
          return false;
        }
        lexer->advance(lexer, false);
      }
      continue;
    }
    if (dollar && character == '\\') {
      lexer->advance(lexer, false);
      if (!scan_dollar_single_quote_escape(
            scanner,
            lexer,
            backquote_depth,
            delimiter
          )) {
        return false;
      }
      continue;
    }
    if (!append_codepoint(delimiter, character)) {
      return false;
    }
    lexer->advance(lexer, false);
  }
  return false;
}

static bool scan_delimiter_double_quoted_character(
  const struct Scanner *scanner,
  TSLexer *lexer,
  struct ByteBuffer *delimiter,
  struct DelimiterGroupBuffer *groups,
  enum DelimiterQuote *quote,
  size_t *backquote_depth
) {
  int32_t character = lexer->lookahead;
  if (lexer_at_eof(lexer)) {
    return false;
  }

  if (character == '"') {
    *quote = DELIMITER_UNQUOTED;
    lexer->advance(lexer, false);
    return true;
  }

  if (character == '$') {
    lexer->advance(lexer, false);
    if (!append_byte(delimiter, '$')) {
      return false;
    }
    if (lexer->lookahead == '(' || lexer->lookahead == '{') {
      bool parameter = lexer->lookahead == '{';
      if (!push_dollar_delimiter_group(
            scanner,
            *backquote_depth,
            lexer,
            delimiter,
            groups,
            *quote
          )) {
        return false;
      }
      *quote =
        parameter ? DELIMITER_PARAMETER_DOUBLE_QUOTED : DELIMITER_UNQUOTED;
    }
    return true;
  }

  if (character == '`') {
    if (
      *backquote_depth ==
      SIZE_MAX ||
      !append_byte(delimiter, '`') ||
      !push_delimiter_group(groups, '`', DELIMITER_GROUP_BACKQUOTE, *quote)
    ) {
      return false;
    }
    *backquote_depth += 1;
    *quote = DELIMITER_UNQUOTED;
    lexer->advance(lexer, false);
    return true;
  }

  if (character == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '\n') {
      lexer->advance(lexer, false);
      return true;
    }
    if (
      lexer->lookahead ==
      '$' ||
      lexer->lookahead ==
      '`' ||
      lexer->lookahead ==
      '"' ||
      lexer->lookahead ==
      '\\' ||
      (*quote == DELIMITER_PARAMETER_DOUBLE_QUOTED && lexer->lookahead == '}')
    ) {
      if (!append_codepoint(delimiter, lexer->lookahead)) {
        return false;
      }
      lexer->advance(lexer, false);
      return true;
    }
    return append_byte(delimiter, '\\');
  }

  if (!append_codepoint(delimiter, character)) {
    return false;
  }
  lexer->advance(lexer, false);
  return true;
}

enum DelimiterReadResult {
  DELIMITER_READ_INVALID,
  DELIMITER_READ_RESOURCE_FAILURE,
  DELIMITER_READ_INCOMPLETE,
  DELIMITER_READ_CONTINUATION,
  DELIMITER_READ_FOLDED_CONTINUATION,
  DELIMITER_READ_WORD,
};

static enum DelimiterReadResult read_here_document_delimiter(
  TSLexer *lexer,
  const struct Scanner *scanner,
  size_t backquote_depth,
  bool strip_tabs,
  bool stop_at_continuation,
  struct HereDocument *document
) {
  enum DelimiterQuote quote = DELIMITER_UNQUOTED;
  struct ByteBuffer delimiter = {.limit = SCANNER_STATE_CAPACITY};
  struct DelimiterGroupBuffer groups = {0};
  struct CaseTrackerBuffer cases = {0};
  struct HereDocument *nested_documents = NULL;
  size_t nested_document_count = 0;
  size_t nested_delimiter_start = 0;
  size_t nested_delimiter_group_depth = 0;
  size_t delimiter_backquote_depth = backquote_depth;
  bool has_word_content = false;
  bool quoted = false;
  bool expecting_nested_delimiter = false;
  bool collecting_nested_delimiter = false;
  bool nested_delimiter_quoted = false;
  bool nested_delimiter_strips_tabs = false;
  bool valid = true;

  while (valid) {
    int32_t character = lexer->lookahead;

    if (quote == DELIMITER_PARAMETER_DOUBLE_QUOTED) {
      struct DelimiterGroupFrame *group = &groups.data[groups.length - 1];
      group->parameter_quote_state =
        parameter_quote_state_after(group->parameter_quote_state, character);
      if (group->parameter_quote_state == PARAMETER_QUOTE_PATTERN) {
        quote = DELIMITER_UNQUOTED;
      } else if (character == '}') {
        valid = append_byte(&delimiter, '}');
        pop_delimiter_group(&groups, &cases, &quote);
        lexer->advance(lexer, false);
        continue;
      }
    }

    if (
      quote ==
      DELIMITER_SINGLE_QUOTED ||
      quote == DELIMITER_DOLLAR_SINGLE_QUOTED
    ) {
      valid = scan_delimiter_single_quoted_segment(
        lexer,
        scanner,
        delimiter_backquote_depth,
        &delimiter,
        quote == DELIMITER_DOLLAR_SINGLE_QUOTED
      );
      if (valid) {
        quote = DELIMITER_UNQUOTED;
      }
      continue;
    }

    if (
      (quote ==
        DELIMITER_DOUBLE_QUOTED ||
        quote == DELIMITER_PARAMETER_DOUBLE_QUOTED) &&
      !(character == '\\' && delimiter_backquote_depth > 0)
    ) {
      valid = scan_delimiter_double_quoted_character(
        scanner,
        lexer,
        &delimiter,
        &groups,
        &quote,
        &delimiter_backquote_depth
      );
      continue;
    }

    size_t command_group_depth = delimiter_command_group_depth(&groups);
    struct CommandWord *command_word = command_group_depth == 0
      ? NULL
      : &groups.data[command_group_depth - 1].command.word;
    if (
      collecting_nested_delimiter &&
      groups.length ==
      nested_delimiter_group_depth &&
      is_token_delimiter_at_depth(lexer, backquote_depth)
    ) {
      valid = append_nested_here_document(
        &nested_documents,
        &nested_document_count,
        &delimiter,
        nested_delimiter_start,
        nested_delimiter_quoted,
        nested_delimiter_strips_tabs
      );
      collecting_nested_delimiter = false;
      if (command_group_depth > 0) {
        groups.data[command_group_depth - 1].command.redirect_operand = false;
      }
      nested_delimiter_quoted = false;
      nested_delimiter_strips_tabs = false;
      if (!valid) {
        continue;
      }
    }

    if (expecting_nested_delimiter) {
      if (character == ' ' || character == '\t') {
        valid = append_codepoint(&delimiter, character);
        if (valid) {
          lexer->advance(lexer, false);
        }
        continue;
      }

      expecting_nested_delimiter = false;
      if (
        character == '#' || is_token_delimiter_at_depth(lexer, backquote_depth)
      ) {
        nested_delimiter_strips_tabs = false;
      } else {
        collecting_nested_delimiter = true;
        nested_delimiter_start = delimiter.length;
        nested_delimiter_group_depth = groups.length;
      }
    }

    struct CaseTracker *operator_case =
      active_case_tracker(&cases, command_group_depth);
    bool in_case_pattern =
      operator_case != NULL && case_tracker_in_pattern(operator_case->state);
    if (
      command_group_depth >
      0 &&
      !collecting_nested_delimiter &&
      !in_case_pattern &&
      (character == '<' || character == '>')
    ) {
      struct CommandTracker *command =
        &groups.data[command_group_depth - 1].command;
      valid = finish_command_word(&cases, command_group_depth, command, true);
      if (!valid) {
        continue;
      }
      begin_command_redirect(command);

      has_word_content = true;
      valid = append_codepoint(&delimiter, character);
      if (!valid) {
        continue;
      }
      lexer->advance(lexer, false);
      if (character == '>') {
        if (
          lexer->lookahead ==
          '>' ||
          lexer->lookahead ==
          '|' ||
          lexer->lookahead == '&'
        ) {
          valid = append_codepoint(&delimiter, lexer->lookahead);
          lexer->advance(lexer, false);
        }
        continue;
      }
      if (lexer->lookahead != '<') {
        if (lexer->lookahead == '>' || lexer->lookahead == '&') {
          valid = append_codepoint(&delimiter, lexer->lookahead);
          lexer->advance(lexer, false);
        }
        continue;
      }

      valid = append_byte(&delimiter, '<');
      if (!valid) {
        continue;
      }
      lexer->advance(lexer, false);
      if (lexer->lookahead == '<') {
        continue;
      }

      bool nested_strip_tabs = lexer->lookahead == '-';
      if (nested_strip_tabs) {
        valid = append_byte(&delimiter, '-');
        if (!valid) {
          continue;
        }
        lexer->advance(lexer, false);
      }
      expecting_nested_delimiter = true;
      nested_delimiter_strips_tabs = nested_strip_tabs;
      reset_command_word(command_word);
      continue;
    }

    bool at_nested_delimiter_base = collecting_nested_delimiter &&
      command_group_depth == nested_delimiter_group_depth;
    if (
      command_group_depth >
      0 &&
      !at_nested_delimiter_base &&
      character ==
      '#' &&
      !command_word->active
    ) {
      do {
        has_word_content = true;
        valid = append_codepoint(&delimiter, lexer->lookahead);
        lexer->advance(lexer, false);
      } while (valid && !lexer_at_eof(lexer) && lexer->lookahead != '\n');

      if (valid && lexer->lookahead == '\n') {
        valid = append_byte(&delimiter, '\n');
        lexer->advance(lexer, false);
        struct CommandTracker *command =
          &groups.data[command_group_depth - 1].command;
        reset_command_position(
          command,
          command->position == COMMAND_POSITION_FOR_IN ? COMMAND_POSITION_FOR_IN
                                                       : COMMAND_POSITION_START
        );
        if (nested_document_count > 0) {
          valid = scan_nested_here_document_sequence(
            lexer,
            &delimiter,
            nested_documents,
            nested_document_count
          );
          clear_document_array(&nested_documents, &nested_document_count);
        }
      }
      continue;
    }

    if (
      command_group_depth >
      0 &&
      !at_nested_delimiter_base &&
      is_token_delimiter_at_depth(lexer, backquote_depth)
    ) {
      valid = finish_command_word(
        &cases,
        command_group_depth,
        &groups.data[command_group_depth - 1].command,
        false
      );
      if (!valid) {
        continue;
      }
    } else if (
      command_group_depth >
      0 &&
      !at_nested_delimiter_base &&
      character !=
      '\'' &&
      character !=
      '"' &&
      character !=
      '\\' &&
      character !=
      '$' &&
      character != '`'
    ) {
      track_command_word_character(command_word, character);
    }

    if (
      groups.length == 0 && is_token_delimiter_at_depth(lexer, backquote_depth)
    ) {
      break;
    }

    if (!has_word_content && groups.length == 0 && character == '#') {
      valid = false;
      break;
    }

    if (character == '\'') {
      track_delimiter_command_character(
        command_word,
        at_nested_delimiter_base,
        character
      );
      has_word_content = true;
      mark_delimiter_quoted(
        &quoted,
        collecting_nested_delimiter,
        &nested_delimiter_quoted
      );
      quote = DELIMITER_SINGLE_QUOTED;
      lexer->advance(lexer, false);
      continue;
    }

    if (character == '"') {
      track_delimiter_command_character(
        command_word,
        at_nested_delimiter_base,
        character
      );
      has_word_content = true;
      mark_delimiter_quoted(
        &quoted,
        collecting_nested_delimiter,
        &nested_delimiter_quoted
      );
      quote = DELIMITER_DOUBLE_QUOTED;
      lexer->advance(lexer, false);
      continue;
    }

    if (character == '\\') {
      bool at_delimiter_source_start =
        !has_word_content && delimiter.length == 0 && groups.length == 0;

      if (delimiter_backquote_depth > 0) {
        size_t original_delimiter_length = delimiter.length;
        enum DelimiterBackslashResult result = scan_delimiter_backslash_run(
          lexer,
          scanner,
          &delimiter,
          &groups,
          &cases,
          &quote,
          &delimiter_backquote_depth,
          backquote_depth,
          at_delimiter_source_start,
          collecting_nested_delimiter,
          &quoted,
          &nested_delimiter_quoted
        );
        if (result == DELIMITER_BACKSLASH_ERROR) {
          valid = false;
        } else if (
          result ==
          DELIMITER_BACKSLASH_LEADING_CONTINUATION ||
          result == DELIMITER_BACKSLASH_FOLDED_CONTINUATION
        ) {
          if (stop_at_continuation) {
            return result == DELIMITER_BACKSLASH_LEADING_CONTINUATION
              ? DELIMITER_READ_CONTINUATION
              : DELIMITER_READ_FOLDED_CONTINUATION;
          }
          valid = false;
        } else if (delimiter.length > original_delimiter_length) {
          has_word_content = true;
          if (
            command_group_depth >
            0 &&
            groups.length >=
            command_group_depth &&
            !at_nested_delimiter_base
          ) {
            track_command_word_character(
              &groups.data[command_group_depth - 1].command.word,
              character
            );
          }
        }
        continue;
      }

      lexer->advance(lexer, false);
      if (lexer_at_eof(lexer)) {
        valid = false;
      } else if (lexer->lookahead == '\n') {
        lexer->advance(lexer, false);
        if (at_delimiter_source_start && stop_at_continuation) {
          return DELIMITER_READ_CONTINUATION;
        }
      } else {
        has_word_content = true;
        mark_delimiter_quoted(
          &quoted,
          collecting_nested_delimiter,
          &nested_delimiter_quoted
        );
        track_delimiter_command_character(
          command_word,
          at_nested_delimiter_base,
          character
        );
        valid = append_codepoint(&delimiter, lexer->lookahead);
        lexer->advance(lexer, false);
      }
      continue;
    }

    if (character == '`') {
      track_delimiter_command_character(
        command_word,
        at_nested_delimiter_base,
        character
      );
      has_word_content = true;
      valid = append_byte(&delimiter, '`');
      if (!valid) {
        continue;
      }
      lexer->advance(lexer, false);
      valid = toggle_delimiter_backquote_group(
        &groups,
        &cases,
        &quote,
        &delimiter_backquote_depth
      );
      continue;
    }

    if (character == '$') {
      if (command_group_depth > 0 && !at_nested_delimiter_base) {
        track_command_word_character(command_word, character);
      }
      has_word_content = true;
      lexer->advance(lexer, false);

      if (lexer->lookahead == '\'') {
        mark_delimiter_quoted(
          &quoted,
          collecting_nested_delimiter,
          &nested_delimiter_quoted
        );
        quote = DELIMITER_DOLLAR_SINGLE_QUOTED;
        lexer->advance(lexer, false);
        continue;
      }

      valid = append_byte(&delimiter, '$');
      if (!valid) {
        continue;
      }

      if (lexer->lookahead == '(' || lexer->lookahead == '{') {
        valid = push_dollar_delimiter_group(
          scanner,
          delimiter_backquote_depth,
          lexer,
          &delimiter,
          &groups,
          quote
        );
      }
      continue;
    }

    if (command_group_depth > 0 && character == ')') {
      struct CaseTracker *active_case =
        active_case_tracker(&cases, command_group_depth);
      if (active_case != NULL && case_tracker_in_pattern(active_case->state)) {
        valid = append_codepoint(&delimiter, character);
        active_case->state = CASE_TRACKER_BODY;
        reset_command_position(
          &groups.data[groups.length - 1].command,
          COMMAND_POSITION_START
        );
        lexer->advance(lexer, false);
        continue;
      }
    }

    if (
      groups.length > 0 && character == groups.data[groups.length - 1].closing
    ) {
      valid = append_codepoint(&delimiter, character);
      pop_delimiter_group(&groups, &cases, &quote);
      lexer->advance(lexer, false);
      continue;
    }

    if (
      groups.length >
      0 &&
      (command_group_depth >
        0 ||
        groups.data[groups.length - 1].kind == DELIMITER_GROUP_ARITHMETIC) &&
      character == '('
    ) {
      struct CaseTracker *active_case =
        active_case_tracker(&cases, groups.length);
      if (active_case != NULL && case_tracker_in_pattern(active_case->state)) {
        has_word_content = true;
        valid = append_codepoint(&delimiter, character);
        active_case->state = CASE_TRACKER_IN_PATTERN;
        lexer->advance(lexer, false);
        continue;
      }

      enum DelimiterGroupKind parent_kind = groups.data[groups.length - 1].kind;
      enum DelimiterGroupKind nested_kind =
        parent_kind == DELIMITER_GROUP_ARITHMETIC ? DELIMITER_GROUP_ARITHMETIC
        : command_group_depth >
          0 &&
          groups.data[command_group_depth - 1].command.function_name
        ? DELIMITER_GROUP_FUNCTION_HEADER
        : DELIMITER_GROUP_SUBSHELL;
      if (!push_delimiter_group(&groups, ')', nested_kind, quote)) {
        valid = false;
        continue;
      }
    }

    if (lexer_at_eof(lexer)) {
      break;
    }

    has_word_content = true;
    valid = append_codepoint(&delimiter, character);
    lexer->advance(lexer, false);

    size_t active_command_depth = delimiter_command_group_depth(&groups);
    if (active_command_depth > 0) {
      struct CaseTracker *active_case =
        active_case_tracker(&cases, active_command_depth);
      if (
        character ==
        ';' &&
        active_case !=
        NULL &&
        active_case->state ==
        CASE_TRACKER_BODY &&
        (lexer->lookahead == ';' || lexer->lookahead == '&')
      ) {
        active_case->state = CASE_TRACKER_EXPECT_PATTERN;
      }

      if (
        character ==
        '\n' ||
        character ==
        ';' ||
        character ==
        '&' ||
        (character ==
          '|' &&
          !(active_case != NULL && case_tracker_in_pattern(active_case->state)))
      ) {
        struct CommandTracker *command =
          &groups.data[active_command_depth - 1].command;
        reset_command_position(
          command,
          character == '\n' && command->position == COMMAND_POSITION_FOR_IN
            ? COMMAND_POSITION_FOR_IN
            : COMMAND_POSITION_START
        );
      }

      if (character == '\n' && nested_document_count > 0) {
        valid = scan_nested_here_document_sequence(
          lexer,
          &delimiter,
          nested_documents,
          nested_document_count
        );
        clear_document_array(&nested_documents, &nested_document_count);
      }
    }
  }

  bool complete = valid &&
    has_word_content &&
    quote ==
    DELIMITER_UNQUOTED &&
    groups.length ==
    0 &&
    delimiter_backquote_depth ==
    backquote_depth &&
    !expecting_nested_delimiter &&
    !collecting_nested_delimiter &&
    nested_document_count == 0;
  ts_free(groups.data);
  ts_free(cases.data);
  clear_document_array(&nested_documents, &nested_document_count);
  if (!complete) {
    ts_free(delimiter.data);
    if (delimiter.failed || groups.failed || cases.failed) {
      return DELIMITER_READ_RESOURCE_FAILURE;
    }
    return lexer_at_eof(lexer) ? DELIMITER_READ_INCOMPLETE
                               : DELIMITER_READ_INVALID;
  }

  *document = (struct HereDocument){
    .delimiter = (uint8_t *)delimiter.data,
    .delimiter_length = delimiter.length,
    .quoted = quoted,
    .strip_tabs = strip_tabs,
  };
  return DELIMITER_READ_WORD;
}

static bool scan_here_document_delimiter(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  lexer->mark_end(lexer);
  struct HereDocument document;
  enum DelimiterReadResult result = read_here_document_delimiter(
    lexer,
    scanner,
    scanner->backquote_depth,
    scanner->delimiter_strips_tabs,
    valid_symbols[LINE_CONTINUATION] ||
      valid_symbols[BACKQUOTE_CONTINUATION_BEGIN],
    &document
  );
  if (result == DELIMITER_READ_CONTINUATION) {
    lexer->mark_end(lexer);
    lexer->result_symbol = LINE_CONTINUATION;
    return true;
  }
  if (result == DELIMITER_READ_FOLDED_CONTINUATION) {
    lexer->result_symbol = BACKQUOTE_CONTINUATION_BEGIN;
    return valid_symbols[BACKQUOTE_CONTINUATION_BEGIN];
  }
  if (result != DELIMITER_READ_WORD) {
    return false;
  }
  if (
    lookahead_is_pending(lexer) || !append_pending_document(scanner, document)
  ) {
    clear_document(&document);
    return false;
  }
  scanner->expecting_delimiter = false;
  scanner->delimiter_strips_tabs = false;
  lexer->result_symbol = HERE_END_BEGIN;
  return true;
}

static bool scan_here_end_commit(struct Scanner *scanner, TSLexer *lexer) {
  if (!is_token_delimiter(scanner, lexer)) {
    return false;
  }

  lexer->mark_end(lexer);
  lexer->result_symbol = HERE_END_COMMIT;
  return true;
}

static bool scan_delimited_character_token(
  const struct Scanner *scanner,
  TSLexer *lexer,
  enum TokenType symbol
) {
  lexer->advance(lexer, false);
  lexer->mark_end(lexer);

  if (!skip_token_continuations(scanner, lexer)) {
    return false;
  }

  if (!is_token_delimiter(scanner, lexer)) {
    return false;
  }

  lexer->result_symbol = (TSSymbol)symbol;
  return true;
}

static bool classify_reserved_word(
  const char *word,
  const bool *valid_symbols,
  TSSymbol *symbol
) {
  const struct ReservedWord *reserved_word = find_reserved_word(word);
  if (reserved_word == NULL || !valid_symbols[reserved_word->symbol]) {
    return false;
  }

  *symbol = (TSSymbol)reserved_word->symbol;
  return true;
}

static bool read_reserved_word(
  const struct Scanner *scanner,
  TSLexer *lexer,
  char *word,
  const bool *valid_symbols
) {
  unsigned length = 0;

  while (is_lowercase_letter(lexer->lookahead)) {
    if (length == 5) {
      return false;
    }
    word[length] = (char)lexer->lookahead;
    length += 1;
    lexer->advance(lexer, false);
  }

  word[length] = '\0';
  if (valid_symbols != NULL) {
    TSSymbol symbol;
    if (classify_reserved_word(word, valid_symbols, &symbol)) {
      lexer->mark_end(lexer);
    }
  }

  if (!skip_token_continuations(scanner, lexer)) {
    return false;
  }

  if (length == 0 || !is_token_delimiter(scanner, lexer)) {
    return false;
  }

  return true;
}

static bool scan_lowercase_dispatch(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  if (valid_symbols[LITERAL_HASH]) {
    return false;
  }

  char word[6];
  lexer->mark_end(lexer);
  if (!read_reserved_word(scanner, lexer, word, valid_symbols)) {
    return false;
  }

  TSSymbol symbol;
  if (classify_reserved_word(word, valid_symbols, &symbol)) {
    lexer->result_symbol = symbol;
    return true;
  }

  return false;
}

static bool scan_horizontal_blanks(TSLexer *lexer);

static bool emit_word_name(TSLexer *lexer, const bool *valid_symbols) {
  if (!valid_symbols[WORD_NAME_TOKEN]) {
    return false;
  }
  lexer->result_symbol = WORD_NAME_TOKEN;
  return true;
}

static bool scan_name_or_reserved_word(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  char word[6];
  size_t length = 0;
  bool is_reserved_candidate = true;

  lexer->mark_end(lexer);
  do {
    if (
      is_reserved_candidate &&
      lexer->lookahead >=
      'a' &&
      lexer->lookahead <=
      'z' &&
      length <
      sizeof(word) -
      1
    ) {
      word[length] = (char)lexer->lookahead;
      length += 1;
    } else {
      is_reserved_candidate = false;
    }
    lexer->advance(lexer, false);
  } while (is_name_character(lexer->lookahead));

  lexer->mark_end(lexer);
  if (valid_symbols[LITERAL_HASH]) {
    return emit_word_name(lexer, valid_symbols);
  }

  word[is_reserved_candidate ? length : 0] = '\0';
  const struct ReservedWord *reserved_word = find_reserved_word(word);
  // An unavailable reserved token forces recovery instead of a command name.
  TSSymbol symbol = TOKEN_COUNT;
  if (
    !classify_reserved_word(word, valid_symbols, &symbol) &&
    reserved_word !=
    NULL &&
    valid_symbols[FNAME_TOKEN]
  ) {
    symbol = (TSSymbol)reserved_word->symbol;
  }
  if (!skip_token_continuations(scanner, lexer)) {
    return emit_word_name(lexer, valid_symbols);
  }

  if (lexer->lookahead == '=') {
    if (!valid_symbols[ASSIGNMENT_NAME_TOKEN]) {
      return emit_word_name(lexer, valid_symbols);
    }
    lexer->result_symbol = ASSIGNMENT_NAME_TOKEN;
    return true;
  }

  if (
    is_name_character(lexer->lookahead) || !is_token_delimiter(scanner, lexer)
  ) {
    return emit_word_name(lexer, valid_symbols);
  }

  if (valid_symbols[FNAME_TOKEN] && reserved_word == NULL) {
    while (true) {
      if (
        !scan_horizontal_blanks(lexer) &&
        !skip_token_continuations(scanner, lexer)
      ) {
        return emit_word_name(lexer, valid_symbols);
      }
      if (
        lexer->lookahead !=
        ' ' &&
        lexer->lookahead !=
        '\t' &&
        lexer->lookahead != '\\'
      ) {
        break;
      }
    }
    if (lexer->lookahead == '(') {
      lexer->result_symbol = FNAME_TOKEN;
      return true;
    }
    return emit_word_name(lexer, valid_symbols);
  }

  if (symbol == TOKEN_COUNT) {
    return emit_word_name(lexer, valid_symbols);
  }
  lexer->result_symbol = symbol;
  return true;
}

static bool scan_case_item_terminator(TSLexer *lexer) {
  if (lexer->lookahead != ';') {
    return false;
  }
  lexer->advance(lexer, false);

  return lexer->lookahead == ';' || lexer->lookahead == '&';
}

static bool classify_comment_boundary(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
);

static bool classify_scanned_comment_boundary(
  TSLexer *lexer,
  const bool *valid_symbols,
  bool comment_reaches_input_end
);

static bool
right_brace_is_delimited(const struct Scanner *scanner, TSLexer *lexer) {
  lexer->advance(lexer, false);
  if (!skip_token_continuations(scanner, lexer)) {
    return false;
  }
  return is_token_delimiter(scanner, lexer);
}

static bool
escape_run_begins_word(const struct Scanner *scanner, TSLexer *lexer) {
  size_t escape_count;
  if (!count_escape_run(lexer, 1, &escape_count)) {
    return false;
  }

  return !(
    lexer->lookahead ==
    '`' &&
    scanner->backquote_depth >
    0 &&
    classify_backquote_tick_prefix(scanner->backquote_depth, escape_count) ==
    BACKQUOTE_TICK_PREFIX_END
  );
}

static bool
closing_reserved_word_ends_term(const char *word, const bool *valid_symbols) {
  for (
    size_t index = 0; index < sizeof(CLOSING_WORDS) / sizeof(CLOSING_WORDS[0]);
    index += 1
  ) {
    if (strcmp(word, CLOSING_WORDS[index].text) == 0) {
      return valid_symbols[CLOSING_WORDS[index].symbol];
    }
  }
  return false;
}

static bool here_document_line_continues_term(
  const struct Scanner *scanner,
  const struct HereDocumentLineStart *start,
  const bool *valid_symbols
) {
  int32_t character = start->first_character;
  if (is_active_backquote_boundary(scanner, character)) {
    return false;
  }
  switch (character) {
  case ')':
  case ';':
  case '&':
  case '|':
    return false;
  case '}':
    return !start->first_is_delimited;
  case '\\':
    return scanner->backquote_depth == 0;
  default:
    break;
  }
  return !(
    start->first_word_is_reserved_candidate &&
    start->first_is_delimited &&
    closing_reserved_word_ends_term(start->first_word, valid_symbols)
  );
}

static bool here_document_delimiter_line_follows(
  const struct Scanner *scanner,
  TSLexer *lexer
) {
  if (scanner->active_count == 0) {
    return false;
  }

  lexer->advance(lexer, false);
  struct HereDocumentLineStart start;
  return read_here_document_line(
           scanner,
           lexer,
           &scanner->active_documents[0],
           scanner->body_backquote_depth,
           NULL,
           &start
         ) == HERE_DOCUMENT_LINE_DELIMITER;
}

static bool probe_here_document_continuation(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  const struct HereDocument *document = &scanner->active_documents[0];
  while (true) {
    struct HereDocumentLineStart start;
    enum HereDocumentLineKind kind = read_here_document_line(
      scanner,
      lexer,
      document,
      scanner->body_backquote_depth,
      NULL,
      &start
    );
    if (kind == HERE_DOCUMENT_LINE_LAYOUT) {
      continue;
    }
    return kind ==
      HERE_DOCUMENT_LINE_CONTENT &&
      here_document_line_continues_term(scanner, &start, valid_symbols);
  }
}

static bool scan_horizontal_blanks(TSLexer *lexer) {
  bool found = false;
  while (is_horizontal_blank(lexer->lookahead)) {
    found = true;
    lexer->advance(lexer, false);
  }
  return found;
}

static bool scan_horizontal_layout(TSLexer *lexer) {
  while (true) {
    scan_horizontal_blanks(lexer);
    if (lexer->lookahead != '\\') {
      return true;
    }
    if (!skip_line_continuations(lexer)) {
      return false;
    }
  }
}

static bool
scan_command_continuation_operator(TSLexer *lexer, const bool *valid_symbols) {
  int32_t character = lexer->lookahead;
  if (character != '|' && character != '&') {
    return false;
  }
  lexer->advance(lexer, false);
  enum TokenType symbol = PIPE_CONTINUATION;
  if (lexer->lookahead == character) {
    lexer->advance(lexer, false);
    symbol = AND_OR_CONTINUATION;
  } else if (character == '&') {
    return false;
  }
  if (!valid_symbols[symbol]) {
    return false;
  }
  lexer->result_symbol = (TSSymbol)symbol;
  return true;
}

static bool
is_word_element_start(const struct Scanner *scanner, const TSLexer *lexer) {
  int32_t character = lexer->lookahead;
  if (lexer_at_eof(lexer) || is_active_backquote_boundary(scanner, character)) {
    return false;
  }
  switch (character) {
  case '\n':
  case '#':
  case ';':
  case '&':
  case '|':
  case '(':
  case ')':
    return false;
  default:
    return true;
  }
}

static bool is_closing_reserved_word_ahead(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  char word[6];
  if (!read_reserved_word(scanner, lexer, word, NULL)) {
    return false;
  }
  return closing_reserved_word_ends_term(word, valid_symbols);
}

static bool scan_separator_operator_continuation(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
);

static bool finish_term_continuation(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
);

static bool classify_shell_boundary(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols,
  bool crossed_layout
);

static bool classify_word_separator(
  TSLexer *lexer,
  const bool *valid_symbols,
  bool mark_blank_run
);

static bool backquote_prefix_token_is_valid(
  const struct Scanner *scanner,
  const bool *valid_symbols
);

static bool scan_backquote_prefix_after_first_backslash(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
);

// One forward scan gives every hierarchy level the same lookahead extent.
static bool scan_element_boundary_core(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols,
  bool leading_backslash_consumed
) {
  bool crossed_layout = false;
  bool crossed_pairs = false;
  bool blank_before_continuation = false;
  while (true) {
    if (leading_backslash_consumed) {
      leading_backslash_consumed = false;
    } else {
      if (scan_horizontal_blanks(lexer)) {
        crossed_layout = true;
      }
      if (lexer->lookahead != '\\') {
        break;
      }
      if (crossed_layout && !crossed_pairs) {
        // Keep the mark before blanks so edits beyond the continuation
        // invalidate the preceding term or command.
        blank_before_continuation = true;
      }
      lexer->advance(lexer, false);
    }
    if (lexer->lookahead != '\n') {
      if (
        !crossed_layout &&
        backquote_prefix_token_is_valid(scanner, valid_symbols)
      ) {
        return scan_backquote_prefix_after_first_backslash(
          scanner,
          lexer,
          valid_symbols
        );
      }
      if (crossed_layout) {
        size_t run;
        if (!count_escape_run(lexer, 1, &run)) {
          return false;
        }
        if (
          lexer->lookahead ==
          '\n' &&
          fold_enclosed_plain_run(run, scanner->backquote_depth) == 1
        ) {
          lexer->advance(lexer, false);
          crossed_pairs = true;
          continue;
        }
        if (
          lexer->lookahead ==
          '`' &&
          classify_backquote_tick_prefix(scanner->backquote_depth, run) ==
          BACKQUOTE_TICK_PREFIX_END
        ) {
          break;
        }
        if (valid_symbols[WORD_SEPARATOR_BEGIN]) {
          lexer->result_symbol = WORD_SEPARATOR_BEGIN;
          return true;
        }
      }
      return false;
    }
    if (!crossed_layout && valid_symbols[LINE_CONTINUATION]) {
      lexer->advance(lexer, false);
      lexer->mark_end(lexer);
      lexer->result_symbol = LINE_CONTINUATION;
      return true;
    }
    lexer->advance(lexer, false);
    crossed_layout = true;
    crossed_pairs = true;
  }

  int32_t character = lexer->lookahead;

  if (character == '|') {
    if (valid_symbols[PATTERN_CONTINUATION]) {
      lexer->result_symbol = PATTERN_CONTINUATION;
      return true;
    }
    return scan_command_continuation_operator(lexer, valid_symbols);
  }

  if (character == '&') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '&') {
      lexer->advance(lexer, false);
      if (!valid_symbols[AND_OR_CONTINUATION]) {
        return false;
      }
      lexer->result_symbol = AND_OR_CONTINUATION;
      return true;
    }
    return scan_separator_operator_continuation(scanner, lexer, valid_symbols);
  }

  if (character == ';') {
    if (scan_case_item_terminator(lexer)) {
      enum TokenType symbol =
        valid_symbols[CASE_ITEM_END] ? CASE_ITEM_END : TERM_BOUNDARY;
      if (!valid_symbols[symbol]) {
        return false;
      }
      lexer->result_symbol = (TSSymbol)symbol;
      return true;
    }
    return scan_separator_operator_continuation(scanner, lexer, valid_symbols);
  }

  if (character == '\n') {
    if (
      crossed_layout &&
      !crossed_pairs &&
      valid_symbols[TERM_BOUNDARY] &&
      valid_symbols[TERM_CONTINUATION] &&
      !has_startable_pending_document(scanner)
    ) {
      if (finish_term_continuation(scanner, lexer, valid_symbols)) {
        lexer->result_symbol = TERM_CONTINUATION;
        return true;
      }
      lexer->result_symbol = TERM_BOUNDARY;
      return true;
    }
    if (crossed_layout && !crossed_pairs) {
      lexer->mark_end(lexer);
    }
    if (crossed_layout) {
      if (
        valid_symbols[TERM_CONTINUATION] &&
        finish_term_continuation(scanner, lexer, valid_symbols)
      ) {
        lexer->result_symbol = TERM_CONTINUATION;
        return true;
      }
      // The preceding blank ends glued layout even while LINE_CONTINUATION
      // remains valid for a closed command or assignment.
      if (
        valid_symbols[PRE_NEWLINE_BLANK] &&
        (!crossed_pairs ||
          blank_before_continuation ||
          !valid_symbols[LINE_CONTINUATION])
      ) {
        if (here_document_delimiter_line_follows(scanner, lexer)) {
          return false;
        }
        if (
          valid_symbols[TERM_BOUNDARY] &&
          !has_startable_pending_document(scanner)
        ) {
          lexer->result_symbol = TERM_BOUNDARY;
          return true;
        }
        lexer->result_symbol = PRE_NEWLINE_BLANK;
        return true;
      }
      return false;
    }
    if (
      !valid_symbols[SEPARATOR_NEWLINE] ||
      has_startable_pending_document(scanner)
    ) {
      return false;
    }
    lexer->mark_end(lexer);
    lexer->advance(lexer, false);
    int32_t after_separator = lexer->lookahead;
    if (finish_term_continuation(scanner, lexer, valid_symbols)) {
      lexer->result_symbol = SEPARATOR_NEWLINE;
      return true;
    }
    bool trailing_layout_reaches_end = is_horizontal_blank(after_separator) ||
      after_separator ==
      '\\' ||
      after_separator ==
      '\n' ||
      after_separator == '#';
    if (
      valid_symbols[TERM_BOUNDARY] &&
      (!lexer_at_eof(lexer) || trailing_layout_reaches_end)
    ) {
      lexer->result_symbol = TERM_BOUNDARY;
      return true;
    }
    return false;
  }

  if (character == '#') {
    bool term_continuation_is_scannable = valid_symbols[TERM_CONTINUATION] &&
      !has_startable_pending_document(scanner);
    if (!term_continuation_is_scannable) {
      return classify_comment_boundary(scanner, lexer, valid_symbols);
    }
    advance_to_comment_end(scanner, lexer, false);
    bool comment_reaches_input_end = comment_reaches_end(scanner, lexer);
    if (
      !comment_reaches_input_end &&
      finish_term_continuation(scanner, lexer, valid_symbols)
    ) {
      lexer->result_symbol = TERM_CONTINUATION;
      return true;
    }
    return classify_scanned_comment_boundary(
      lexer,
      valid_symbols,
      comment_reaches_input_end
    );
  }

  if (
    (valid_symbols[WORD_SEPARATOR_BEGIN] ||
      valid_symbols[ASSIGNMENT_SEPARATOR_BEGIN] ||
      valid_symbols[REDIRECT_SEPARATOR_BEGIN]) &&
    is_word_element_start(scanner, lexer)
  ) {
    return classify_word_separator(lexer, valid_symbols, !crossed_pairs);
  }

  bool at_closer = lexer_at_eof(lexer) ||
    character ==
    ')' ||
    is_active_backquote_boundary(scanner, character);
  if (classify_shell_boundary(scanner, lexer, valid_symbols, crossed_layout)) {
    return true;
  }

  if (blank_before_continuation && valid_symbols[TERM_BOUNDARY] && at_closer) {
    lexer->result_symbol = TERM_BOUNDARY;
    return true;
  }

  if (
    crossed_pairs &&
    !blank_before_continuation &&
    valid_symbols[TRAILING_CONTINUATION_BEGIN] &&
    !valid_symbols[LINE_CONTINUATION] &&
    at_closer
  ) {
    lexer->result_symbol = TRAILING_CONTINUATION_BEGIN;
    return true;
  }

  return false;
}

static bool scan_element_boundary(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  lexer->mark_end(lexer);
  return scan_element_boundary_core(scanner, lexer, valid_symbols, false);
}

static bool classify_word_separator(
  TSLexer *lexer,
  const bool *valid_symbols,
  bool mark_blank_run
) {
  if (mark_blank_run) {
    lexer->mark_end(lexer);
  }
  int32_t character = lexer->lookahead;
  bool redirect_ahead = false;
  bool assignment_ahead = false;

  if (character == '<' || character == '>') {
    redirect_ahead = true;
  } else if (is_decimal_digit(character)) {
    while (is_decimal_digit(lexer->lookahead)) {
      lexer->advance(lexer, false);
    }
    if (skip_line_continuations(lexer)) {
      redirect_ahead = lexer->lookahead == '<' || lexer->lookahead == '>';
    }
  } else if (is_name_start_character(character)) {
    do {
      lexer->advance(lexer, false);
    } while (is_name_character(lexer->lookahead));
    assignment_ahead = lexer->lookahead == '=';
  }

  if (assignment_ahead && valid_symbols[ASSIGNMENT_SEPARATOR_BEGIN]) {
    lexer->result_symbol = ASSIGNMENT_SEPARATOR_BEGIN;
    return true;
  }
  if (redirect_ahead && valid_symbols[REDIRECT_SEPARATOR_BEGIN]) {
    lexer->result_symbol = REDIRECT_SEPARATOR_BEGIN;
    return true;
  }
  if (valid_symbols[WORD_SEPARATOR_BEGIN]) {
    lexer->result_symbol = WORD_SEPARATOR_BEGIN;
    return true;
  }
  return false;
}

static bool scan_separator_operator_continuation(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  if (!scan_horizontal_layout(lexer)) {
    if (!escape_run_begins_word(scanner, lexer)) {
      return false;
    }
    if (valid_symbols[LIST_CONTINUATION]) {
      lexer->result_symbol = LIST_CONTINUATION;
      return true;
    }
    if (valid_symbols[TERM_CONTINUATION]) {
      lexer->result_symbol = TERM_CONTINUATION;
      return true;
    }
    return false;
  }

  int32_t character = lexer->lookahead;
  bool at_input_end = lexer_at_eof(lexer);
  bool term_continues = finish_term_continuation(scanner, lexer, valid_symbols);

  if (valid_symbols[LIST_CONTINUATION]) {
    if (
      !at_input_end &&
      character !=
      '\n' &&
      character !=
      '#' &&
      !(character ==
        ')' &&
        (valid_symbols[COMMAND_SUBSTITUTION_CLOSE] ||
          valid_symbols[SUBSHELL_CLOSE])) &&
      !(is_active_backquote_boundary(scanner, character) &&
        valid_symbols[BACKQUOTE_END])
    ) {
      lexer->result_symbol = LIST_CONTINUATION;
      return true;
    }
  } else if (valid_symbols[TERM_CONTINUATION]) {
    if (term_continues) {
      lexer->result_symbol = TERM_CONTINUATION;
      return true;
    }
    if (
      has_startable_pending_document(scanner) &&
      (at_input_end || character == '\n' || character == '#')
    ) {
      return false;
    }
  }

  if (valid_symbols[TERMINATOR_AHEAD]) {
    lexer->result_symbol = TERMINATOR_AHEAD;
    return true;
  }

  return false;
}

static bool finish_term_continuation(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  bool cross_lines = !has_startable_pending_document(scanner);

  while (true) {
    if (!scan_horizontal_layout(lexer)) {
      return escape_run_begins_word(scanner, lexer);
    }
    if (lexer->lookahead == '#') {
      if (!cross_lines) {
        return false;
      }
      advance_to_comment_end(scanner, lexer, false);
      continue;
    }
    if (lexer->lookahead == '\n') {
      if (!cross_lines) {
        return false;
      }
      lexer->advance(lexer, false);
      if (scanner->active_count > 0) {
        return probe_here_document_continuation(scanner, lexer, valid_symbols);
      }
      continue;
    }

    int32_t character = lexer->lookahead;
    if (
      lexer_at_eof(lexer) || is_active_backquote_boundary(scanner, character)
    ) {
      return false;
    }
    switch (character) {
    case ')':
    case ';':
    case '&':
    case '|':
      return false;
    case '}':
      return !right_brace_is_delimited(scanner, lexer);
    default:
      break;
    }
    if (
      is_lowercase_letter(character) &&
      is_closing_reserved_word_ahead(scanner, lexer, valid_symbols)
    ) {
      return false;
    }

    return true;
  }
}

static bool scan_file_descriptor(TSLexer *lexer) {
  if (!is_decimal_digit(lexer->lookahead)) {
    return false;
  }

  lexer->mark_end(lexer);

  while (is_decimal_digit(lexer->lookahead)) {
    lexer->advance(lexer, false);
  }

  if (!skip_line_continuations(lexer)) {
    return false;
  }

  if (lexer->lookahead != '<' && lexer->lookahead != '>') {
    return false;
  }

  lexer->result_symbol = FILE_DESCRIPTOR;
  return true;
}

static bool scan_here_document_operator_commit(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  if (
    scanner->expecting_delimiter ||
    (!valid_symbols[DLESS] && !valid_symbols[DLESSDASH])
  ) {
    return false;
  }

  bool strip_tabs = valid_symbols[DLESSDASH];
  scanner->expecting_delimiter = true;
  scanner->delimiter_strips_tabs = strip_tabs;
  lexer->mark_end(lexer);
  lexer->result_symbol = strip_tabs ? DLESSDASH : DLESS;
  return true;
}

static bool classify_shell_boundary(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols,
  bool crossed_layout
) {
  int32_t character = lexer->lookahead;
  if (character == '|') {
    if (valid_symbols[PATTERN_CONTINUATION]) {
      lexer->result_symbol = PATTERN_CONTINUATION;
      return true;
    }
    if (
      valid_symbols[PIPE_CONTINUATION] || valid_symbols[AND_OR_CONTINUATION]
    ) {
      return scan_command_continuation_operator(lexer, valid_symbols);
    }
  }

  if (character == '&' && valid_symbols[AND_OR_CONTINUATION]) {
    return scan_command_continuation_operator(lexer, valid_symbols);
  }

  if (character == ')' && valid_symbols[PATTERN_END]) {
    lexer->result_symbol = PATTERN_END;
    return true;
  }

  if (character == ';' && valid_symbols[CASE_ITEM_END]) {
    if (!scan_case_item_terminator(lexer)) {
      return false;
    }
    lexer->result_symbol = CASE_ITEM_END;
    return true;
  }

  if (
    character ==
    '#' &&
    (crossed_layout || !valid_symbols[LITERAL_HASH]) &&
    classify_comment_boundary(scanner, lexer, valid_symbols)
  ) {
    return true;
  }

  if (
    valid_symbols[REDIRECT_LIST_BEGIN] &&
    (character == '<' || character == '>' || is_decimal_digit(character))
  ) {
    lexer->result_symbol = REDIRECT_LIST_BEGIN;
    return true;
  }

  if (is_lowercase_letter(character)) {
    if (crossed_layout) {
      return false;
    }

    char word[6];
    if (read_reserved_word(scanner, lexer, word, valid_symbols)) {
      TSSymbol reserved_symbol;
      if (classify_reserved_word(word, valid_symbols, &reserved_symbol)) {
        lexer->result_symbol = reserved_symbol;
        return true;
      }
    }
  }

  return false;
}

static bool scan_shell_boundary(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  lexer->mark_end(lexer);
  bool crossed_layout = is_horizontal_blank(lexer->lookahead) ||
    (lexer->lookahead == '\\' && valid_symbols[REDIRECT_LIST_BEGIN]);
  if (crossed_layout && !scan_horizontal_layout(lexer)) {
    return false;
  }

  return classify_shell_boundary(scanner, lexer, valid_symbols, crossed_layout);
}

static bool
scan_here_document_line_end(struct Scanner *scanner, TSLexer *lexer) {
  if (
    !has_startable_pending_document(scanner) || scanner->sequence_end_pending
  ) {
    return false;
  }

  lexer->mark_end(lexer);

  if (lexer->lookahead != '\n') {
    return false;
  }

  lexer->advance(lexer, false);
  if (!activate_startable_pending_documents(scanner)) {
    return false;
  }
  reset_here_document_delimiter_scan(scanner);
  scanner->at_here_document_line_start = true;
  lexer->mark_end(lexer);
  lexer->result_symbol = HERE_DOCUMENT_LINE_END;
  return true;
}

static bool scan_comment(const struct Scanner *scanner, TSLexer *lexer) {
  if (lexer->lookahead != '#') {
    return false;
  }

  advance_to_comment_end(scanner, lexer, true);
  lexer->result_symbol = COMMENT;
  return true;
}

static bool classify_scanned_comment_boundary(
  TSLexer *lexer,
  const bool *valid_symbols,
  bool comment_reaches_input_end
) {
  if (comment_reaches_input_end && valid_symbols[TRAILING_COMMENT_BOUNDARY]) {
    lexer->result_symbol = TRAILING_COMMENT_BOUNDARY;
    return true;
  }
  if (valid_symbols[COMMENT_BOUNDARY]) {
    lexer->result_symbol = COMMENT_BOUNDARY;
    return true;
  }
  return false;
}

static bool classify_comment_boundary(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  if (
    !valid_symbols[COMMENT_BOUNDARY] &&
    !valid_symbols[TRAILING_COMMENT_BOUNDARY]
  ) {
    return false;
  }
  bool comment_reaches_input_end = false;
  if (valid_symbols[TRAILING_COMMENT_BOUNDARY]) {
    advance_to_comment_end(scanner, lexer, false);
    comment_reaches_input_end = comment_reaches_end(scanner, lexer);
  }
  return classify_scanned_comment_boundary(
    lexer,
    valid_symbols,
    comment_reaches_input_end
  );
}

static bool finish_layout_begin(TSLexer *lexer, const bool *valid_symbols) {
  if (!valid_symbols[LAYOUT_BEGIN]) {
    return false;
  }
  lexer->result_symbol = LAYOUT_BEGIN;
  return true;
}

// Probe the follower to settle competing layout owners. Blank-led runs
// commit their blanks; continuation-led runs need a zero-width marker so
// the grammar can emit each continuation.
static bool classify_layout_run(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols,
  bool continuation_led
) {
  while (true) {
    scan_horizontal_blanks(lexer);
    if (lexer->lookahead != '\\') {
      break;
    }
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return continuation_led && finish_layout_begin(lexer, valid_symbols);
    }
    lexer->advance(lexer, false);
  }

  int32_t character = lexer->lookahead;
  if (
    character ==
    '#' &&
    (valid_symbols[COMMENT_BOUNDARY] ||
      valid_symbols[TRAILING_COMMENT_BOUNDARY])
  ) {
    return classify_comment_boundary(scanner, lexer, valid_symbols);
  }

  if (character == '\n' && valid_symbols[PRE_NEWLINE_BLANK]) {
    if (here_document_delimiter_line_follows(scanner, lexer)) {
      return false;
    }
    lexer->result_symbol = PRE_NEWLINE_BLANK;
    return true;
  }

  if (
    character ==
    ';' &&
    valid_symbols[CASE_ITEM_END] &&
    scan_case_item_terminator(lexer)
  ) {
    lexer->result_symbol = CASE_ITEM_END;
    return true;
  }

  if (!continuation_led) {
    return false;
  }
  if (finish_layout_begin(lexer, valid_symbols)) {
    return true;
  }
  if (valid_symbols[TRAILING_CONTINUATION_BEGIN]) {
    lexer->result_symbol = TRAILING_CONTINUATION_BEGIN;
    return true;
  }
  return false;
}

static bool continuation_led_layout_is_ambiguous(
  const struct Scanner *scanner,
  const bool *valid_symbols
) {
  return !scanner->expecting_delimiter &&
    !valid_symbols[FUNCTION_BODY_CONTINUATION_BOUNDARY] &&
    (valid_symbols[LAYOUT_BEGIN] ||
      valid_symbols[COMMENT_BOUNDARY] ||
      valid_symbols[PRE_NEWLINE_BLANK] ||
      (valid_symbols[TRAILING_CONTINUATION_BEGIN] &&
        !valid_symbols[LINE_CONTINUATION]));
}

static bool scan_line_continuation_after_backslash(
  TSLexer *lexer,
  const bool *valid_symbols
) {
  if (!valid_symbols[LINE_CONTINUATION] || lexer->lookahead != '\n') {
    return false;
  }

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  lexer->result_symbol = LINE_CONTINUATION;
  return true;
}

static bool scan_line_continuation(TSLexer *lexer, const bool *valid_symbols) {
  if (lexer->lookahead != '\\') {
    return false;
  }

  lexer->advance(lexer, false);
  return scan_line_continuation_after_backslash(lexer, valid_symbols);
}

static enum ArithmeticOperatorCategory
classify_arithmetic_operator(int32_t first, int32_t second, int32_t third) {
  switch (first) {
  case '=':
    return second == '=' ? ARITHMETIC_OPERATOR_CATEGORY_EQUALITY
                         : ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT;
  case '!':
    return second == '=' ? ARITHMETIC_OPERATOR_CATEGORY_EQUALITY
                         : ARITHMETIC_OPERATOR_CATEGORY_COUNT;
  case '|':
    if (second == '=') {
      return ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT;
    }
    return second == '|' ? ARITHMETIC_OPERATOR_CATEGORY_LOGICAL_OR
                         : ARITHMETIC_OPERATOR_CATEGORY_BITWISE_OR;
  case '&':
    if (second == '=') {
      return ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT;
    }
    return second == '&' ? ARITHMETIC_OPERATOR_CATEGORY_LOGICAL_AND
                         : ARITHMETIC_OPERATOR_CATEGORY_BITWISE_AND;
  case '^':
    return second == '=' ? ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT
                         : ARITHMETIC_OPERATOR_CATEGORY_BITWISE_XOR;
  case '<':
  case '>':
    if (second == first) {
      return third == '=' ? ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT
                          : ARITHMETIC_OPERATOR_CATEGORY_SHIFT;
    }
    return ARITHMETIC_OPERATOR_CATEGORY_RELATIONAL;
  case '+':
  case '-':
    if (second == '=') {
      return ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT;
    }
    // C token recognition reads an adjacent repeated sign as one increment or
    // decrement token, which POSIX arithmetic does not provide.
    return second == first ? ARITHMETIC_OPERATOR_CATEGORY_COUNT
                           : ARITHMETIC_OPERATOR_CATEGORY_ADDITIVE;
  case '*':
  case '/':
  case '%':
    return second == '=' ? ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT
                         : ARITHMETIC_OPERATOR_CATEGORY_MULTIPLICATIVE;
  case '?':
    return ARITHMETIC_OPERATOR_CATEGORY_QUESTION;
  case ':':
    return ARITHMETIC_OPERATOR_CATEGORY_COLON;
  default:
    return ARITHMETIC_OPERATOR_CATEGORY_COUNT;
  }
}

static bool arithmetic_operator_boundary_is_valid(const bool *valid_symbols) {
  for (
    size_t category = 0; category < ARITHMETIC_OPERATOR_CATEGORY_COUNT;
    category += 1
  ) {
    enum TokenType symbol = ARITHMETIC_OPERATOR_BOUNDARIES[category];
    if (valid_symbols[symbol]) {
      return true;
    }
  }
  return false;
}

static bool is_arithmetic_operator_start(int32_t character) {
  switch (character) {
  case '=':
  case '!':
  case '|':
  case '&':
  case '^':
  case '<':
  case '>':
  case '+':
  case '-':
  case '*':
  case '/':
  case '%':
  case '?':
  case ':':
    return true;
  default:
    return false;
  }
}

static bool is_arithmetic_operand_start(int32_t character) {
  return (
    is_name_start_character(character) ||
    is_decimal_digit(character) ||
    character ==
    '(' ||
    character ==
    '$' ||
    character ==
    '`' ||
    character ==
    '+' ||
    character ==
    '-' ||
    character ==
    '!' ||
    character == '~'
  );
}

static bool
scan_arithmetic_boundary(TSLexer *lexer, const bool *valid_symbols) {
  lexer->mark_end(lexer);
  while (
    lexer->lookahead ==
    ' ' ||
    lexer->lookahead ==
    '\t' ||
    lexer->lookahead == '\n'
  ) {
    lexer->advance(lexer, false);
  }

  if (valid_symbols[ARITHMETIC_CLOSING_BOUNDARY] && lexer->lookahead == ')') {
    lexer->result_symbol = ARITHMETIC_CLOSING_BOUNDARY;
    return true;
  }

  int32_t first = lexer->lookahead;
  lexer->advance(lexer, false);
  int32_t second = lexer->lookahead;
  lexer->advance(lexer, false);
  enum ArithmeticOperatorCategory category =
    classify_arithmetic_operator(first, second, lexer->lookahead);
  if (category != ARITHMETIC_OPERATOR_CATEGORY_COUNT) {
    enum TokenType operator_symbol = ARITHMETIC_OPERATOR_BOUNDARIES[category];
    if (valid_symbols[operator_symbol]) {
      lexer->result_symbol = (TSSymbol)operator_symbol;
      return true;
    }
  }

  return false;
}

// Resolve the arithmetic readings before parsing; racing them lets an edited
// tree reuse a flat subtree where a fresh parse selects the structured one.

enum ValidationTokenKind {
  VALIDATION_TOKEN_NUMBER,
  VALIDATION_TOKEN_VARIABLE,
  VALIDATION_TOKEN_EXPANSION,
  VALIDATION_TOKEN_OPERATOR,
  VALIDATION_TOKEN_LEFT_PARENTHESIS,
  VALIDATION_TOKEN_RIGHT_PARENTHESIS,
};

enum {
  VALIDATION_OPERATOR_BANG = ARITHMETIC_OPERATOR_CATEGORY_COUNT,
  VALIDATION_OPERATOR_TILDE,
  VALIDATION_OPERATOR_REPEATED_SIGN,
};

static bool append_validation_token(
  struct ValidationTokenBuffer *tokens,
  uint8_t kind,
  uint8_t category
) {
  struct ValidationToken *data = grow_element_buffer(
    tokens->data,
    &tokens->capacity,
    tokens->length,
    sizeof(struct ValidationToken),
    64
  );
  if (data == NULL) {
    return false;
  }
  tokens->data = data;
  tokens->data[tokens->length] =
    (struct ValidationToken){.kind = kind, .category = category};
  tokens->length += 1;
  return true;
}

static bool
append_arithmetic_source(struct ArithmeticScan *scan, int32_t character) {
  if (character == ' ' || character == '\t' || character == '\n') {
    character = ' ';
    if (
      scan->source_length > 0 && scan->source[scan->source_length - 1] == ' '
    ) {
      return true;
    }
  }
  if (character == -1 && scan->source_length > 0) {
    if (scan->source[scan->source_length - 1] == -1) {
      return true;
    }
    if (
      scan->source_length >=
      4 &&
      scan->source[scan->source_length - 1] ==
      ' ' &&
      scan->source[scan->source_length - 2] ==
      -1 &&
      scan->source[scan->source_length - 3] ==
      ' ' &&
      scan->source[scan->source_length - 4] == -1
    ) {
      // Two fragments retain the mandatory blank separating their tokens.
      scan->source_length -= 2;
    }
  }
  int32_t *source = grow_element_buffer(
    scan->source,
    &scan->source_capacity,
    scan->source_length,
    sizeof(int32_t),
    64
  );
  if (source == NULL) {
    return false;
  }
  scan->source = source;
  scan->source[scan->source_length++] = character;
  return true;
}

static bool
advance_arithmetic_source(TSLexer *lexer, struct ArithmeticScan *scan) {
  if (!append_arithmetic_source(scan, lexer->lookahead)) {
    return false;
  }
  lexer->advance(lexer, false);
  return true;
}

static bool append_arithmetic_expansion(struct ArithmeticScan *scan) {
  scan->has_expansion = true;
  return append_arithmetic_source(scan, -1) &&
    append_validation_token(&scan->tokens, VALIDATION_TOKEN_EXPANSION, 0);
}

enum EmbeddedFrameKind {
  EMBEDDED_SOURCE,
  EMBEDDED_COMMAND_SUBSTITUTION,
  EMBEDDED_SUBSHELL,
  EMBEDDED_FUNCTION_HEADER,
};

struct EmbeddedFrame {
  char closer;
  enum EmbeddedFrameKind kind;
  struct CommandTracker command;
  bool double_quoted_parameter;
  enum ParameterQuoteState parameter_quote;
};

struct EmbeddedSkip {
  struct EmbeddedFrame *frames;
  size_t frame_count;
  size_t frame_capacity;
  struct CaseTrackerBuffer cases;
  struct HereDocument *pending;
  size_t pending_count;
  size_t pending_capacity;
  bool started;
};

static void clear_embedded_skip(struct EmbeddedSkip *skip) {
  ts_free(skip->frames);
  ts_free(skip->cases.data);
  for (size_t index = 0; index < skip->pending_count; index += 1) {
    clear_document(&skip->pending[index]);
  }
  ts_free(skip->pending);
  *skip = (struct EmbeddedSkip){0};
}

static void clear_arithmetic_scan(struct ArithmeticScan *scan) {
  ts_free(scan->tokens.data);
  ts_free(scan->source);
  if (scan->embedded != NULL) {
    clear_embedded_skip(scan->embedded);
    ts_free(scan->embedded);
  }
  *scan = (struct ArithmeticScan){0};
}

static bool embedded_push_frame(
  struct EmbeddedSkip *skip,
  char closer,
  enum EmbeddedFrameKind kind,
  bool double_quoted
) {
  struct EmbeddedFrame *frames = grow_element_buffer(
    skip->frames,
    &skip->frame_capacity,
    skip->frame_count,
    sizeof(struct EmbeddedFrame),
    16
  );
  if (frames == NULL) {
    return false;
  }
  skip->frames = frames;
  skip->frames[skip->frame_count] = (struct EmbeddedFrame){
    .closer = closer,
    .kind = kind,
    .double_quoted_parameter = closer == '}' && double_quoted,
  };
  reset_command_position(
    &skip->frames[skip->frame_count].command,
    COMMAND_POSITION_START
  );
  skip->frame_count += 1;
  return true;
}

static void embedded_note_word(struct EmbeddedSkip *skip, bool in_command) {
  if (!in_command) {
    return;
  }
  track_command_word_character(
    &skip->frames[skip->frame_count - 1].command.word,
    '\\'
  );
}

static bool embedded_word_is_delimited(const TSLexer *lexer) {
  return (
    lexer_at_eof(lexer) ||
    is_horizontal_blank(lexer->lookahead) ||
    lexer->lookahead ==
    '\n' ||
    is_control_operator_start(lexer->lookahead)
  );
}

static enum ArithmeticValidation embedded_push_dollar_group(
  const struct Scanner *scanner,
  struct EmbeddedSkip *skip,
  TSLexer *lexer,
  bool double_quoted
) {
  bool command_context = lexer->lookahead == '(';
  lexer->advance(lexer, false);
  if (command_context && lexer->lookahead == '(') {
    return skip_ambiguous_substitution(scanner, lexer);
  }
  return embedded_push_frame(
           skip,
           command_context ? ')' : '}',
           command_context ? EMBEDDED_COMMAND_SUBSTITUTION : EMBEDDED_SOURCE,
           double_quoted
         )
    ? ARITHMETIC_VALIDATION_VALID
    : ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
}

static bool embedded_append_pending(
  struct EmbeddedSkip *skip,
  struct HereDocument document
) {
  struct HereDocument *pending = grow_element_buffer(
    skip->pending,
    &skip->pending_capacity,
    skip->pending_count,
    sizeof(struct HereDocument),
    4
  );
  if (pending == NULL) {
    return false;
  }
  skip->pending = pending;
  skip->pending[skip->pending_count] = document;
  skip->pending_count += 1;
  return true;
}

static enum ArithmeticValidation read_embedded_here_document_delimiter(
  const struct Scanner *scanner,
  TSLexer *lexer,
  bool strip_tabs,
  struct HereDocument *document
) {
  while (is_horizontal_blank(lexer->lookahead)) {
    lexer->advance(lexer, false);
  }

  enum DelimiterReadResult result = read_here_document_delimiter(
    lexer,
    scanner,
    scanner == NULL ? 0 : scanner->backquote_depth,
    strip_tabs,
    false,
    document
  );
  if (result == DELIMITER_READ_WORD) {
    return ARITHMETIC_VALIDATION_VALID;
  }
  if (result == DELIMITER_READ_RESOURCE_FAILURE) {
    return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
  }
  return result == DELIMITER_READ_INCOMPLETE ? ARITHMETIC_VALIDATION_INCOMPLETE
                                             : ARITHMETIC_VALIDATION_INVALID;
}

static enum ArithmeticValidation skip_embedded_here_document_bodies(
  const struct Scanner *scanner,
  TSLexer *lexer,
  struct EmbeddedSkip *skip
) {
  const struct Scanner unquoted_scanner = {0};
  if (scanner == NULL) {
    scanner = &unquoted_scanner;
  }
  size_t retained = 0;
  enum ArithmeticValidation result = ARITHMETIC_VALIDATION_VALID;
  for (size_t index = 0; index < skip->pending_count; index += 1) {
    struct HereDocument *document = &skip->pending[index];
    if (
      result !=
      ARITHMETIC_VALIDATION_VALID ||
      document->declaration_depth < skip->frame_count
    ) {
      skip->pending[retained] = *document;
      retained += 1;
      continue;
    }
    while (true) {
      struct HereDocumentLineStart start;
      enum HereDocumentLineKind kind = read_here_document_line(
        scanner,
        lexer,
        document,
        scanner->backquote_depth,
        NULL,
        &start
      );
      if (kind == HERE_DOCUMENT_LINE_DELIMITER) {
        break;
      }
      if (kind == HERE_DOCUMENT_LINE_END_OF_INPUT) {
        result = ARITHMETIC_VALIDATION_INCOMPLETE;
        break;
      }
    }
    clear_document(document);
  }
  skip->pending_count = retained;
  return result;
}

static bool read_embedded_escape_run(
  const struct Scanner *scanner,
  TSLexer *lexer,
  size_t *run
) {
  if (!count_escape_run(lexer, 0, run) || lexer_at_eof(lexer)) {
    return false;
  }
  if (scanner == NULL) {
    return true;
  }
  if (lexer->lookahead == '`') {
    return fold_enclosed_backquote_run(*run, scanner->backquote_depth, run);
  }
  *run = fold_enclosed_escape_run(
    scanner,
    *run,
    scanner->backquote_depth,
    lexer->lookahead
  );
  return true;
}

static enum ArithmeticValidation resume_embedded_construct(
  const struct Scanner *scanner,
  TSLexer *lexer,
  char initial_closer,
  bool double_quoted,
  struct EmbeddedSkip *skip
) {
  enum ArithmeticValidation result = ARITHMETIC_VALIDATION_VALID;
  struct LookaheadLexer *lookahead = (struct LookaheadLexer *)lexer;
  size_t resume = lookahead->position;
  size_t previous_depth = 0;
  struct EmbeddedFrame previous_frame = {0};
  size_t previous_case_count = 0;
  uint8_t previous_case_state = 0;

  if (!skip->started) {
    if (!embedded_push_frame(
          skip,
          initial_closer,
          initial_closer == ')' ? EMBEDDED_COMMAND_SUBSTITUTION
                                : EMBEDDED_SOURCE,
          double_quoted
        )) {
      lookahead->failed = true;
      return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
    }
    skip->started = true;
  }

  while (result == ARITHMETIC_VALIDATION_VALID && skip->frame_count > 0) {
    if (lexer_at_eof(lexer)) {
      result = ARITHMETIC_VALIDATION_INCOMPLETE;
      break;
    }

    struct EmbeddedFrame *frame = &skip->frames[skip->frame_count - 1];
    bool in_command = frame->kind != EMBEDDED_SOURCE;
    int32_t character = lexer->lookahead;
    resume = lookahead->position;
    previous_depth = skip->frame_count;
    previous_frame = *frame;
    previous_case_count = skip->cases.length;
    if (previous_case_count > 0) {
      previous_case_state = skip->cases.data[previous_case_count - 1].state;
    }

    if (
      in_command &&
      embedded_word_is_delimited(lexer) &&
      !finish_command_word(
        &skip->cases,
        skip->frame_count,
        &frame->command,
        character == '<' || character == '>'
      )
    ) {
      result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
      break;
    }
    struct CaseTracker *active_case =
      in_command ? active_case_tracker(&skip->cases, skip->frame_count) : NULL;

    if (frame->closer == '}') {
      frame->parameter_quote =
        parameter_quote_state_after(frame->parameter_quote, character);
    }
    bool quoted_parameter = frame->double_quoted_parameter &&
      frame->parameter_quote != PARAMETER_QUOTE_PATTERN;

    if (frame->closer == '`') {
      if (character == '\\') {
        size_t run;
        if (!read_embedded_escape_run(scanner, lexer, &run)) {
          result = ARITHMETIC_VALIDATION_INCOMPLETE;
          break;
        }
        if ((run & 1) != 0) {
          lexer->advance(lexer, false);
        }
      } else {
        lexer->advance(lexer, false);
        if (character == '`') {
          skip->frame_count -= 1;
        }
      }
      continue;
    }

    if (frame->closer == '"' || quoted_parameter) {
      if (character == '\\') {
        size_t run;
        if (!read_embedded_escape_run(scanner, lexer, &run)) {
          result = ARITHMETIC_VALIDATION_INCOMPLETE;
          break;
        }
        if ((run & 1) != 0) {
          lexer->advance(lexer, false);
        }
        continue;
      }
      if (character == frame->closer) {
        lexer->advance(lexer, false);
        skip->frame_count -= 1;
        continue;
      }
      if (character == '`' || character == '"') {
        lexer->advance(lexer, false);
        if (!embedded_push_frame(
              skip,
              (char)character,
              EMBEDDED_SOURCE,
              false
            )) {
          result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
          break;
        }
        continue;
      }
      if (character == '$') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '(' || lexer->lookahead == '{') {
          result = embedded_push_dollar_group(scanner, skip, lexer, true);
          if (result != ARITHMETIC_VALIDATION_VALID) {
            break;
          }
        }
        continue;
      }
      lexer->advance(lexer, false);
      continue;
    }

    if (character == '\\') {
      size_t run;
      if (!read_embedded_escape_run(scanner, lexer, &run)) {
        result = ARITHMETIC_VALIDATION_INCOMPLETE;
        break;
      }
      bool continuation = run == 1 && lexer->lookahead == '\n';
      if ((run & 1) != 0) {
        lexer->advance(lexer, false);
      }
      if (run != 0 && !continuation) {
        embedded_note_word(skip, in_command);
      }
      continue;
    }

    if (character == '\'') {
      lexer->advance(lexer, false);
      while (!lexer_at_eof(lexer) && lexer->lookahead != '\'') {
        lexer->advance(lexer, false);
      }
      if (lexer_at_eof(lexer)) {
        result = ARITHMETIC_VALIDATION_INCOMPLETE;
        break;
      }
      lexer->advance(lexer, false);
      embedded_note_word(skip, in_command);
      continue;
    }

    if (character == '"' || character == '`') {
      lexer->advance(lexer, false);
      embedded_note_word(skip, in_command);
      if (!embedded_push_frame(skip, (char)character, EMBEDDED_SOURCE, false)) {
        result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
        break;
      }
      continue;
    }

    if (character == '$') {
      lexer->advance(lexer, false);
      embedded_note_word(skip, in_command);
      if (lexer->lookahead == '(' || lexer->lookahead == '{') {
        result = embedded_push_dollar_group(scanner, skip, lexer, false);
        if (result != ARITHMETIC_VALIDATION_VALID) {
          break;
        }
        continue;
      }
      if (lexer->lookahead == '\'') {
        lexer->advance(lexer, false);
        while (true) {
          if (lexer_at_eof(lexer)) {
            result = ARITHMETIC_VALIDATION_INCOMPLETE;
            break;
          }
          if (lexer->lookahead == '\\') {
            size_t run;
            if (!read_embedded_escape_run(scanner, lexer, &run)) {
              result = ARITHMETIC_VALIDATION_INCOMPLETE;
              break;
            }
            if ((run & 1) != 0) {
              lexer->advance(lexer, false);
            }
            continue;
          }
          if (lexer->lookahead == '\'') {
            lexer->advance(lexer, false);
            break;
          }
          lexer->advance(lexer, false);
        }
      }
      continue;
    }

    if (character == '#' && in_command && !frame->command.word.active) {
      advance_to_line_end(lexer);
      continue;
    }

    if (
      in_command &&
      (character == '<' || character == '>') &&
      (active_case == NULL || active_case->state == CASE_TRACKER_BODY)
    ) {
      begin_command_redirect(&frame->command);
      lexer->advance(lexer, false);
      if (character != '<' || lexer->lookahead != '<') {
        if (
          lexer->lookahead ==
          '&' ||
          (character == '<' && lexer->lookahead == '>') ||
          (character ==
            '>' &&
            (lexer->lookahead == '>' || lexer->lookahead == '|'))
        ) {
          lexer->advance(lexer, false);
        }
        continue;
      }
      lexer->advance(lexer, false);
      bool strip_tabs = lexer->lookahead == '-';
      if (strip_tabs) {
        lexer->advance(lexer, false);
      }
      struct HereDocument document;
      result = read_embedded_here_document_delimiter(
        scanner,
        lexer,
        strip_tabs,
        &document
      );
      if (result != ARITHMETIC_VALIDATION_VALID) {
        break;
      }
      document.declaration_depth = skip->frame_count;
      if (!embedded_append_pending(skip, document)) {
        clear_document(&document);
        result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
        break;
      }
      frame->command.redirect_operand = false;
      continue;
    }

    if (character == '\n' && skip->pending_count > 0) {
      lexer->advance(lexer, false);
      result = skip_embedded_here_document_bodies(scanner, lexer, skip);
      reset_command_position(&frame->command, COMMAND_POSITION_START);
      continue;
    }

    if (character == '(' && in_command) {
      lexer->advance(lexer, false);
      if (active_case != NULL && case_tracker_in_pattern(active_case->state)) {
        active_case->state = CASE_TRACKER_IN_PATTERN;
        reset_command_position(&frame->command, COMMAND_POSITION_START);
        continue;
      }
      enum EmbeddedFrameKind kind = frame->command.function_name
        ? EMBEDDED_FUNCTION_HEADER
        : EMBEDDED_SUBSHELL;
      if (!embedded_push_frame(skip, ')', kind, false)) {
        result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
        break;
      }
      continue;
    }

    if (character == ')' && in_command) {
      if (active_case != NULL) {
        lexer->advance(lexer, false);
        if (case_tracker_in_pattern(active_case->state)) {
          active_case->state = CASE_TRACKER_BODY;
          reset_command_position(&frame->command, COMMAND_POSITION_START);
        }
        continue;
      }
      lexer->advance(lexer, false);
      enum EmbeddedFrameKind kind = frame->kind;
      skip->frame_count -= 1;
      pop_case_trackers_at_depth(&skip->cases, skip->frame_count + 1);
      if (
        skip->frame_count >
        0 &&
        (kind == EMBEDDED_FUNCTION_HEADER || kind == EMBEDDED_SUBSHELL)
      ) {
        reset_command_position(
          &skip->frames[skip->frame_count - 1].command,
          kind == EMBEDDED_FUNCTION_HEADER ? COMMAND_POSITION_START
                                           : COMMAND_POSITION_CLOSED
        );
      }
      continue;
    }

    if (character == '}' && frame->closer == '}') {
      lexer->advance(lexer, false);
      skip->frame_count -= 1;
      continue;
    }

    if (
      in_command &&
      active_case !=
      NULL &&
      active_case->state ==
      CASE_TRACKER_BODY &&
      character == ';'
    ) {
      lexer->advance(lexer, false);
      if (lexer->lookahead == ';' || lexer->lookahead == '&') {
        lexer->advance(lexer, false);
        active_case->state = CASE_TRACKER_EXPECT_PATTERN;
      }
      reset_command_position(&frame->command, COMMAND_POSITION_START);
      continue;
    }

    lexer->advance(lexer, false);
    if (!in_command || is_horizontal_blank(character)) {
      continue;
    }
    if (
      character ==
      '\n' ||
      character ==
      ';' ||
      character ==
      '&' ||
      character == '|'
    ) {
      enum CommandPosition position =
        character == '\n' && frame->command.position == COMMAND_POSITION_FOR_IN
        ? COMMAND_POSITION_FOR_IN
        : COMMAND_POSITION_START;
      reset_command_position(&frame->command, position);
    } else {
      track_command_word_character(&frame->command.word, character);
    }
  }

  if (!lookahead->failed && lookahead->requested != SIZE_MAX) {
    lookahead_seek(lookahead, resume);
    if (previous_depth > 0) {
      skip->frames[previous_depth - 1] = previous_frame;
    }
    skip->cases.length = previous_case_count;
    if (previous_case_count > 0) {
      skip->cases.data[previous_case_count - 1].state = previous_case_state;
    }
  }
  if (result == ARITHMETIC_VALIDATION_RESOURCE_FAILURE) {
    lookahead->failed = true;
  }
  return result;
}

static enum ArithmeticValidation skip_embedded_construct_core(
  const struct Scanner *scanner,
  TSLexer *lexer,
  char initial_closer
) {
  struct EmbeddedSkip skip = {0};
  enum ArithmeticValidation result =
    resume_embedded_construct(scanner, lexer, initial_closer, false, &skip);
  clear_embedded_skip(&skip);
  return result;
}

static enum ArithmeticValidation skip_embedded_construct(
  const struct Scanner *scanner,
  TSLexer *lexer,
  char initial_closer
) {
  if (initial_closer == ')' && lexer->lookahead == '(') {
    return skip_ambiguous_substitution(scanner, lexer);
  }
  return skip_embedded_construct_core(scanner, lexer, initial_closer);
}

static enum ArithmeticValidation
skip_backquote_substitution(TSLexer *lexer, size_t enclosing_depth) {
  size_t depth = enclosing_depth + 1;

  while (true) {
    if (lexer_at_eof(lexer)) {
      return ARITHMETIC_VALIDATION_INCOMPLETE;
    }

    size_t escape_count;
    if (!count_escape_run(lexer, 0, &escape_count)) {
      return ARITHMETIC_VALIDATION_INVALID;
    }

    if (lexer->lookahead == '`') {
      lexer->advance(lexer, false);
      if (escape_count == 0) {
        return ARITHMETIC_VALIDATION_VALID;
      }
      enum BackquoteTickPrefix prefix =
        classify_backquote_tick_prefix(depth, escape_count);
      if (prefix == BACKQUOTE_TICK_PREFIX_END) {
        depth -= 1;
        if (depth == enclosing_depth) {
          return ARITHMETIC_VALIDATION_VALID;
        }
      } else if (prefix == BACKQUOTE_TICK_PREFIX_START) {
        if (depth == SIZE_MAX) {
          return ARITHMETIC_VALIDATION_INVALID;
        }
        depth += 1;
      }
      continue;
    }

    if (escape_count > 0) {
      if (lexer_at_eof(lexer)) {
        return ARITHMETIC_VALIDATION_INCOMPLETE;
      }
      lexer->advance(lexer, false);
      continue;
    }

    lexer->advance(lexer, false);
  }
}

static bool
validate_number_source(TSLexer *lexer, struct ArithmeticScan *scan) {
  struct ValidationTokenBuffer *tokens = &scan->tokens;
  bool single_token = true;

  if (lexer->lookahead == '0') {
    if (!advance_arithmetic_source(lexer, scan)) {
      return false;
    }
    if (lexer->lookahead == 'x' || lexer->lookahead == 'X') {
      if (!advance_arithmetic_source(lexer, scan)) {
        return false;
      }
      size_t digits = 0;
      while (is_hexadecimal_digit(lexer->lookahead)) {
        digits += 1;
        if (!advance_arithmetic_source(lexer, scan)) {
          return false;
        }
      }
      single_token = digits > 0;
    } else {
      while (is_decimal_digit(lexer->lookahead)) {
        if (lexer->lookahead > '7') {
          single_token = false;
        }
        if (!advance_arithmetic_source(lexer, scan)) {
          return false;
        }
      }
    }
  } else {
    while (is_decimal_digit(lexer->lookahead)) {
      if (!advance_arithmetic_source(lexer, scan)) {
        return false;
      }
    }
  }

  if (!append_validation_token(tokens, VALIDATION_TOKEN_NUMBER, 0)) {
    return false;
  }
  if (
    !single_token &&
    !append_validation_token(tokens, VALIDATION_TOKEN_NUMBER, 0)
  ) {
    return false;
  }

  if (is_name_character(lexer->lookahead)) {
    while (is_name_character(lexer->lookahead)) {
      if (!advance_arithmetic_source(lexer, scan)) {
        return false;
      }
    }
    return append_validation_token(tokens, VALIDATION_TOKEN_VARIABLE, 0);
  }
  return true;
}

static bool validate_operator_source(
  TSLexer *lexer,
  struct ArithmeticScan *scan,
  int32_t first
) {
  struct ValidationTokenBuffer *tokens = &scan->tokens;
  if (!append_arithmetic_source(scan, first)) {
    return false;
  }
  uint8_t category;
  int32_t second = lexer->lookahead;

  switch (first) {
  case '=':
    category = ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT;
    if (second == '=') {
      category = ARITHMETIC_OPERATOR_CATEGORY_EQUALITY;
      if (!advance_arithmetic_source(lexer, scan)) {
        return false;
      }
    }
    break;
  case '!':
    category = VALIDATION_OPERATOR_BANG;
    if (second == '=') {
      category = ARITHMETIC_OPERATOR_CATEGORY_EQUALITY;
      if (!advance_arithmetic_source(lexer, scan)) {
        return false;
      }
    }
    break;
  case '|':
  case '&':
  case '^':
    category = first == '|'
      ? ARITHMETIC_OPERATOR_CATEGORY_BITWISE_OR
      : (first == '&' ? ARITHMETIC_OPERATOR_CATEGORY_BITWISE_AND
                      : ARITHMETIC_OPERATOR_CATEGORY_BITWISE_XOR);
    if (second == '=') {
      category = ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT;
      if (!advance_arithmetic_source(lexer, scan)) {
        return false;
      }
    } else if (second == first && first != '^') {
      category = first == '|' ? ARITHMETIC_OPERATOR_CATEGORY_LOGICAL_OR
                              : ARITHMETIC_OPERATOR_CATEGORY_LOGICAL_AND;
      if (!advance_arithmetic_source(lexer, scan)) {
        return false;
      }
    }
    break;
  case '<':
  case '>':
    category = ARITHMETIC_OPERATOR_CATEGORY_RELATIONAL;
    if (second == first) {
      if (!advance_arithmetic_source(lexer, scan)) {
        return false;
      }
      category = ARITHMETIC_OPERATOR_CATEGORY_SHIFT;
      if (lexer->lookahead == '=') {
        category = ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT;
        if (!advance_arithmetic_source(lexer, scan)) {
          return false;
        }
      }
    } else if (second == '=') {
      if (!advance_arithmetic_source(lexer, scan)) {
        return false;
      }
    }
    break;
  case '+':
  case '-':
    category = ARITHMETIC_OPERATOR_CATEGORY_ADDITIVE;
    if (second == '=') {
      category = ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT;
      if (!advance_arithmetic_source(lexer, scan)) {
        return false;
      }
    } else if (second == first) {
      category = VALIDATION_OPERATOR_REPEATED_SIGN;
      if (!advance_arithmetic_source(lexer, scan)) {
        return false;
      }
    }
    break;
  case '*':
  case '/':
  case '%':
    category = ARITHMETIC_OPERATOR_CATEGORY_MULTIPLICATIVE;
    if (second == '=') {
      category = ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT;
      if (!advance_arithmetic_source(lexer, scan)) {
        return false;
      }
    }
    break;
  case '?':
    category = ARITHMETIC_OPERATOR_CATEGORY_QUESTION;
    break;
  case ':':
    category = ARITHMETIC_OPERATOR_CATEGORY_COLON;
    break;
  default:
    category = VALIDATION_OPERATOR_TILDE;
    break;
  }

  return append_validation_token(tokens, VALIDATION_TOKEN_OPERATOR, category);
}

static enum ArithmeticValidation validate_arithmetic_content(
  const struct Scanner *scanner,
  TSLexer *lexer,
  struct ArithmeticScan *scan
) {
  struct ValidationTokenBuffer *tokens = &scan->tokens;

  while (true) {
    if (scan->embedded != NULL) {
      enum ArithmeticValidation nested = !scan->embedded->started &&
          scan->embedded_closer ==
          ')' &&
          lexer->lookahead == '('
        ? skip_ambiguous_substitution(scanner, lexer)
        : resume_embedded_construct(
            scanner,
            lexer,
            scan->embedded_closer,
            true,
            scan->embedded
          );
      if (lookahead_is_pending(lexer)) {
        return nested;
      }
      clear_embedded_skip(scan->embedded);
      ts_free(scan->embedded);
      scan->embedded = NULL;
      if (nested != ARITHMETIC_VALIDATION_VALID) {
        return nested;
      }
      if (!append_arithmetic_expansion(scan)) {
        return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
      }
      continue;
    }
    int32_t character = lexer->lookahead;

    if (character == '\\') {
      size_t escape_run = 0;
      while (lexer->lookahead == '\\' && escape_run < SIZE_MAX) {
        escape_run += 1;
        lexer->advance(lexer, false);
      }
      character = lexer->lookahead;
      if (character == '`') {
        enum BackquoteTickPrefix prefix =
          classify_backquote_tick_prefix(scanner->backquote_depth, escape_run);
        if (prefix == BACKQUOTE_TICK_PREFIX_START) {
          lexer->advance(lexer, false);
          enum ArithmeticValidation nested =
            skip_backquote_substitution(lexer, scanner->backquote_depth);
          if (nested != ARITHMETIC_VALIDATION_VALID) {
            return nested;
          }
          if (!append_arithmetic_expansion(scan)) {
            return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
          }
          continue;
        }
        if (prefix == BACKQUOTE_TICK_PREFIX_END) {
          return ARITHMETIC_VALIDATION_INCOMPLETE;
        }
        return ARITHMETIC_VALIDATION_INVALID;
      }
      if (lexer_at_eof(lexer)) {
        return ARITHMETIC_VALIDATION_INCOMPLETE;
      }
      if (
        fold_enclosed_escape_run(
          scanner,
          escape_run,
          scanner->backquote_depth,
          character
        ) != 0
      ) {
        return ARITHMETIC_VALIDATION_INVALID;
      }
    }

    if (lexer_at_eof(lexer)) {
      return ARITHMETIC_VALIDATION_INCOMPLETE;
    }

    if (character == ' ' || character == '\t' || character == '\n') {
      if (!append_arithmetic_source(scan, character)) {
        return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
      }
      lexer->advance(lexer, false);
      continue;
    }

    if (character == '(') {
      if (scan->group_depth == SIZE_MAX) {
        return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
      }
      scan->group_depth += 1;
      if (
        !advance_arithmetic_source(lexer, scan) ||
        !append_validation_token(tokens, VALIDATION_TOKEN_LEFT_PARENTHESIS, 0)
      ) {
        return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
      }
      continue;
    }

    if (character == ')') {
      lexer->advance(lexer, false);
      if (scan->group_depth > 0) {
        scan->group_depth -= 1;
        if (
          !append_arithmetic_source(scan, ')') ||
          !append_validation_token(
            tokens,
            VALIDATION_TOKEN_RIGHT_PARENTHESIS,
            0
          )
        ) {
          return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
        }
        continue;
      }

      if (lexer_at_eof(lexer)) {
        return ARITHMETIC_VALIDATION_INCOMPLETE;
      }
      return lexer->lookahead == ')' ? ARITHMETIC_VALIDATION_VALID
                                     : ARITHMETIC_VALIDATION_INVALID;
    }

    if (is_decimal_digit(character)) {
      if (!validate_number_source(lexer, scan)) {
        return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
      }
      continue;
    }

    if (is_name_start_character(character)) {
      while (is_name_character(lexer->lookahead)) {
        if (!advance_arithmetic_source(lexer, scan)) {
          return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
        }
      }
      if (!append_validation_token(tokens, VALIDATION_TOKEN_VARIABLE, 0)) {
        return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
      }
      continue;
    }

    if (character == '$') {
      lexer->advance(lexer, false);
      int32_t introducer = lexer->lookahead;
      if (introducer == '(' || introducer == '{') {
        lexer->advance(lexer, false);
        scan->embedded_closer = introducer == '(' ? ')' : '}';
        scan->embedded = ts_calloc(1, sizeof(struct EmbeddedSkip));
        if (scan->embedded == NULL) {
          return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
        }
        continue;
      } else if (is_name_start_character(introducer)) {
        while (is_name_character(lexer->lookahead)) {
          lexer->advance(lexer, false);
        }
      } else if (
        is_decimal_digit(introducer) ||
        is_special_parameter_character(introducer)
      ) {
        lexer->advance(lexer, false);
      } else if (lexer_at_eof(lexer)) {
        return ARITHMETIC_VALIDATION_INCOMPLETE;
      } else {
        return ARITHMETIC_VALIDATION_INVALID;
      }
      if (!append_arithmetic_expansion(scan)) {
        return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
      }
      continue;
    }

    if (character == '`') {
      if (scanner->backquote_depth > 0) {
        return ARITHMETIC_VALIDATION_INCOMPLETE;
      }
      lexer->advance(lexer, false);
      enum ArithmeticValidation nested = skip_backquote_substitution(lexer, 0);
      if (nested != ARITHMETIC_VALIDATION_VALID) {
        return nested;
      }
      if (!append_arithmetic_expansion(scan)) {
        return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
      }
      continue;
    }

    if (is_arithmetic_operator_start(character) || character == '~') {
      lexer->advance(lexer, false);
      if (!validate_operator_source(lexer, scan, character)) {
        return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
      }
      continue;
    }

    return ARITHMETIC_VALIDATION_INVALID;
  }
}

struct StructuredValidation {
  const struct ValidationToken *tokens;
  size_t count;
};

static bool validation_token_is_operator(
  const struct StructuredValidation *validation,
  size_t index,
  uint8_t category
) {
  return (
    index <
    validation->count &&
    validation->tokens[index].kind ==
    VALIDATION_TOKEN_OPERATOR &&
    validation->tokens[index].category == category
  );
}

enum StructuredContext {
  STRUCTURED_CONTEXT_GROUP,
  STRUCTURED_CONTEXT_TERNARY,
};

static size_t structured_assignment_head_end(
  const struct StructuredValidation *validation,
  size_t index
) {
  size_t opens = 0;
  while (
    index <
    validation->count &&
    validation->tokens[index].kind == VALIDATION_TOKEN_LEFT_PARENTHESIS
  ) {
    index += 1;
    opens += 1;
  }
  if (
    index >=
    validation->count ||
    validation->tokens[index].kind != VALIDATION_TOKEN_VARIABLE
  ) {
    return SIZE_MAX;
  }
  index += 1;
  while (
    opens >
    0 &&
    index <
    validation->count &&
    validation->tokens[index].kind == VALIDATION_TOKEN_RIGHT_PARENTHESIS
  ) {
    index += 1;
    opens -= 1;
  }
  if (
    opens >
    0 ||
    !validation_token_is_operator(
      validation,
      index,
      ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT
    )
  ) {
    return SIZE_MAX;
  }
  return index + 1;
}

static enum ArithmeticValidation
validate_structured_expression(const struct StructuredValidation *validation) {
  uint8_t *contexts = NULL;
  size_t context_count = 0;
  size_t context_capacity = 0;
  size_t index = 0;
  bool expecting_operand = true;
  bool at_expression_start = true;
  bool valid = true;

  while (valid && index < validation->count) {
    const struct ValidationToken *token = &validation->tokens[index];

    if (expecting_operand) {
      if (at_expression_start) {
        size_t after_head = structured_assignment_head_end(validation, index);
        if (after_head != SIZE_MAX) {
          index = after_head;
          continue;
        }
      }
      if (
        token->kind ==
        VALIDATION_TOKEN_NUMBER ||
        token->kind ==
        VALIDATION_TOKEN_VARIABLE ||
        token->kind == VALIDATION_TOKEN_EXPANSION
      ) {
        expecting_operand = false;
        index += 1;
        continue;
      }
      if (token->kind == VALIDATION_TOKEN_LEFT_PARENTHESIS) {
        uint8_t *grown = grow_element_buffer(
          contexts,
          &context_capacity,
          context_count,
          sizeof(uint8_t),
          16
        );
        if (grown == NULL) {
          ts_free(contexts);
          return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
        }
        contexts = grown;
        contexts[context_count] = STRUCTURED_CONTEXT_GROUP;
        context_count += 1;
        at_expression_start = true;
        index += 1;
        continue;
      }
      if (
        token->kind ==
        VALIDATION_TOKEN_OPERATOR &&
        (token->category ==
          ARITHMETIC_OPERATOR_CATEGORY_ADDITIVE ||
          token->category ==
          VALIDATION_OPERATOR_BANG ||
          token->category == VALIDATION_OPERATOR_TILDE)
      ) {
        at_expression_start = false;
        index += 1;
        continue;
      }
      valid = false;
      break;
    }

    if (token->kind == VALIDATION_TOKEN_RIGHT_PARENTHESIS) {
      if (
        context_count ==
        0 ||
        contexts[context_count - 1] != STRUCTURED_CONTEXT_GROUP
      ) {
        valid = false;
        break;
      }
      context_count -= 1;
      index += 1;
      continue;
    }
    if (token->kind != VALIDATION_TOKEN_OPERATOR) {
      valid = false;
      break;
    }
    if (
      token->category >=
      ARITHMETIC_OPERATOR_CATEGORY_LOGICAL_OR &&
      token->category <= ARITHMETIC_OPERATOR_CATEGORY_MULTIPLICATIVE
    ) {
      expecting_operand = true;
      at_expression_start = false;
      index += 1;
      continue;
    }
    if (token->category == ARITHMETIC_OPERATOR_CATEGORY_QUESTION) {
      uint8_t *grown = grow_element_buffer(
        contexts,
        &context_capacity,
        context_count,
        sizeof(uint8_t),
        16
      );
      if (grown == NULL) {
        ts_free(contexts);
        return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
      }
      contexts = grown;
      contexts[context_count] = STRUCTURED_CONTEXT_TERNARY;
      context_count += 1;
      expecting_operand = true;
      at_expression_start = true;
      index += 1;
      continue;
    }
    if (token->category == ARITHMETIC_OPERATOR_CATEGORY_COLON) {
      if (
        context_count ==
        0 ||
        contexts[context_count - 1] != STRUCTURED_CONTEXT_TERNARY
      ) {
        valid = false;
        break;
      }
      context_count -= 1;
      expecting_operand = true;
      at_expression_start = false;
      index += 1;
      continue;
    }
    valid = false;
    break;
  }

  valid = valid && !expecting_operand && context_count == 0;
  ts_free(contexts);
  return valid ? ARITHMETIC_VALIDATION_VALID : ARITHMETIC_VALIDATION_INVALID;
}

static bool increase_substitution_depth(struct Scanner *scanner) {
  if (scanner->substitution_depth == SIZE_MAX) {
    return false;
  }

  scanner->substitution_depth += 1;
  if (!scanner_state_fits(scanner)) {
    scanner->substitution_depth -= 1;
    return false;
  }
  return true;
}

static bool finish_command_substitution_body_begin(
  struct Scanner *scanner,
  TSLexer *lexer
) {
  if (!increase_substitution_depth(scanner)) {
    return false;
  }

  lexer->result_symbol = COMMAND_SUBSTITUTION_BODY_BEGIN;
  return true;
}

static enum TokenType classify_arithmetic_source(
  const struct Scanner *scanner,
  TSLexer *lexer,
  struct ArithmeticScan *scan,
  enum ArithmeticValidation *result
) {
  *result = validate_arithmetic_content(scanner, lexer, scan);

  enum TokenType symbol = TOKEN_COUNT;
  if (*result == ARITHMETIC_VALIDATION_VALID) {
    struct StructuredValidation validation = {
      .tokens = scan->tokens.data,
      .count = scan->tokens.length,
    };
    enum ArithmeticValidation structured =
      validate_structured_expression(&validation);
    if (structured == ARITHMETIC_VALIDATION_RESOURCE_FAILURE) {
      *result = structured;
    } else if (structured == ARITHMETIC_VALIDATION_VALID) {
      symbol = ARITHMETIC_LEFT_PARENTHESIS;
    } else if (scan->has_expansion) {
      enum ArithmeticValidation dynamic =
        validate_dynamic_arithmetic(scan->source, scan->source_length);
      if (dynamic == ARITHMETIC_VALIDATION_VALID) {
        symbol = ARITHMETIC_DYNAMIC_LEFT_PARENTHESIS;
      } else if (dynamic == ARITHMETIC_VALIDATION_RESOURCE_FAILURE) {
        *result = dynamic;
      }
    }
  } else if (*result == ARITHMETIC_VALIDATION_INCOMPLETE) {
    symbol = ARITHMETIC_LEFT_PARENTHESIS;
  }

  if (*result == ARITHMETIC_VALIDATION_RESOURCE_FAILURE) {
    ((struct LookaheadLexer *)lexer)->failed = true;
  }
  return symbol;
}

static bool scan_arithmetic_left_parenthesis(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  lexer->mark_end(lexer);
  lexer->advance(lexer, false);
  struct ArithmeticScan scan = {0};
  enum ArithmeticValidation result;
  enum TokenType symbol =
    classify_arithmetic_source(scanner, lexer, &scan, &result);
  clear_arithmetic_scan(&scan);
  if (lookahead_is_pending(lexer)) {
    return false;
  }
  if (result == ARITHMETIC_VALIDATION_INCOMPLETE) {
    symbol = valid_symbols[ARITHMETIC_LEFT_PARENTHESIS]
      ? ARITHMETIC_LEFT_PARENTHESIS
      : ARITHMETIC_DYNAMIC_LEFT_PARENTHESIS;
  }

  if (symbol == TOKEN_COUNT || !valid_symbols[symbol]) {
    if (valid_symbols[COMMAND_SUBSTITUTION_BODY_BEGIN]) {
      return finish_command_substitution_body_begin(scanner, lexer);
    }
    return false;
  }

  lexer->result_symbol = (TSSymbol)symbol;
  return true;
}

static bool scan_command_substitution_body_begin(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  lexer->mark_end(lexer);

  if (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '\n') {
      return scan_line_continuation_after_backslash(lexer, valid_symbols);
    }
    return finish_command_substitution_body_begin(scanner, lexer);
  }

  if (
    lexer->lookahead ==
    '(' &&
    (valid_symbols[ARITHMETIC_LEFT_PARENTHESIS] ||
      valid_symbols[ARITHMETIC_DYNAMIC_LEFT_PARENTHESIS])
  ) {
    return scan_arithmetic_left_parenthesis(scanner, lexer, valid_symbols);
  }

  return finish_command_substitution_body_begin(scanner, lexer);
}

static bool scan_pattern_special_left_bracket(TSLexer *lexer) {
  if (lexer->lookahead != '[') {
    return false;
  }

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  if (
    lexer->lookahead !=
    ':' &&
    lexer->lookahead !=
    '.' &&
    lexer->lookahead != '='
  ) {
    return false;
  }

  lexer->result_symbol = PATTERN_SPECIAL_LEFT_BRACKET;
  return true;
}

static bool scan_dollar_expansion_start(TSLexer *lexer) {
  if (lexer->lookahead != '$') {
    return false;
  }

  lexer->advance(lexer, false);
  if (
    is_parameter_start_character(lexer->lookahead) ||
    lexer->lookahead ==
    '{' ||
    lexer->lookahead == '('
  ) {
    lexer->mark_end(lexer);
    lexer->result_symbol = DOLLAR_EXPANSION_START;
    return true;
  }

  return false;
}

static bool
scan_braced_numeric_parameter_start(TSLexer *lexer, const bool *valid_symbols) {
  if (!is_decimal_digit(lexer->lookahead)) {
    return false;
  }

  lexer->mark_end(lexer);
  bool has_digit = false;
  bool has_multiple_digits = false;
  bool has_nonzero_digit = false;

  while (is_decimal_digit(lexer->lookahead)) {
    if (has_digit) {
      has_multiple_digits = true;
    }
    has_digit = true;
    if (lexer->lookahead != '0') {
      has_nonzero_digit = true;
    }
    lexer->advance(lexer, false);
  }

  if (has_nonzero_digit && valid_symbols[BRACED_POSITIONAL_PARAMETER_START]) {
    lexer->result_symbol = BRACED_POSITIONAL_PARAMETER_START;
    return true;
  }

  if (
    !has_nonzero_digit &&
    has_multiple_digits &&
    valid_symbols[BRACED_PARAMETER_NUMBER_START]
  ) {
    lexer->result_symbol = BRACED_PARAMETER_NUMBER_START;
    return true;
  }

  return false;
}

static bool increase_backquote_depth(struct Scanner *scanner) {
  if (scanner->backquote_depth == SIZE_MAX) {
    return false;
  }

  scanner->backquote_depth += 1;
  if (!scanner_state_fits(scanner)) {
    scanner->backquote_depth -= 1;
    return false;
  }
  return true;
}

static bool increase_quoted_backquote_depth(struct Scanner *scanner) {
  if (!increase_backquote_depth(scanner)) {
    return false;
  }
  size_t count = scanner->quoted_backquote_count;
  if (count >= SIZE_MAX / sizeof(size_t)) {
    scanner->backquote_depth -= 1;
    return false;
  }
  size_t *depths =
    ts_realloc(scanner->quoted_backquote_depths, (count + 1) * sizeof(size_t));
  if (depths == NULL) {
    scanner->backquote_depth -= 1;
    return false;
  }
  scanner->quoted_backquote_depths = depths;
  depths[count] = scanner->backquote_depth;
  scanner->quoted_backquote_count += 1;
  if (!scanner_state_fits(scanner)) {
    scanner->quoted_backquote_count -= 1;
    scanner->backquote_depth -= 1;
    return false;
  }
  return true;
}

static bool
scan_backquote_start(struct Scanner *scanner, TSLexer *lexer, bool quoted) {
  if (scanner->backquote_depth > 0) {
    return false;
  }

  if (
    lexer->lookahead !=
    '`' ||
    !(quoted ? increase_quoted_backquote_depth(scanner)
             : increase_backquote_depth(scanner))
  ) {
    return false;
  }

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  lexer->result_symbol =
    quoted ? DOUBLE_QUOTED_BACKQUOTE_START : BACKQUOTE_START;
  return true;
}

static bool scan_backquote_end(struct Scanner *scanner, TSLexer *lexer) {
  if (scanner->backquote_depth != 1) {
    return false;
  }

  if (lexer->lookahead == '`') {
    lexer->advance(lexer, false);
  } else {
    return false;
  }

  trim_backquote_depth(scanner, scanner->backquote_depth - 1);
  lexer->mark_end(lexer);
  lexer->result_symbol = BACKQUOTE_END;
  return true;
}

static bool backquote_prefix_token_is_valid(
  const struct Scanner *scanner,
  const bool *valid_symbols
) {
  return scanner->backquote_depth >
    0 &&
    (valid_symbols[BACKQUOTE_DOLLAR_PREFIX] ||
      valid_symbols[BACKQUOTE_DOLLAR_SINGLE_QUOTE_PREFIX] ||
      valid_symbols[BACKQUOTE_START_PREFIX] ||
      valid_symbols[DOUBLE_QUOTED_BACKQUOTE_START_PREFIX] ||
      valid_symbols[BACKQUOTE_QUOTE_PREFIX] ||
      valid_symbols[BACKQUOTE_CONTINUATION_BEGIN] ||
      valid_symbols[BACKQUOTE_END_PREFIX]);
}

static bool element_boundary_symbols_are_valid(const bool *valid_symbols) {
  return (
    valid_symbols[WORD_SEPARATOR_BEGIN] ||
    valid_symbols[ASSIGNMENT_SEPARATOR_BEGIN] ||
    valid_symbols[REDIRECT_SEPARATOR_BEGIN] ||
    valid_symbols[PIPE_CONTINUATION] ||
    valid_symbols[AND_OR_CONTINUATION] ||
    valid_symbols[LIST_CONTINUATION] ||
    valid_symbols[TERM_CONTINUATION] ||
    valid_symbols[TERMINATOR_AHEAD]
  );
}

// Classify the whole run: consuming a pair first changes the remainder's fold.
static bool scan_backquote_ordinary_escape_run(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols,
  size_t escape_count
) {
  if (lexer_at_eof(lexer) || lexer->lookahead == '\n') {
    return false;
  }

  size_t folded = fold_enclosed_escape_run(
    scanner,
    escape_count,
    scanner->backquote_depth,
    lexer->lookahead
  );
  if (lexer->lookahead == '"' && folded == 0) {
    if (!valid_symbols[BACKQUOTE_QUOTE_PREFIX]) {
      return false;
    }
    lexer->mark_end(lexer);
    lexer->result_symbol = BACKQUOTE_QUOTE_PREFIX;
    return true;
  }
  enum TokenType symbol =
    (folded & 1) != 0 ? BACKQUOTE_CONTENT_RUN_BEGIN : BACKQUOTE_PAIR_RUN_BEGIN;
  if (!valid_symbols[symbol]) {
    return false;
  }
  lexer->result_symbol = (TSSymbol)symbol;
  return true;
}

static bool scan_backquote_pattern_escape(
  const struct Scanner *scanner,
  TSLexer *lexer,
  size_t *escape_count
) {
  size_t depth = scanner->backquote_depth;
  // A complete raw block yields two backslashes without changing the tail's
  // fold.
  size_t pair_width =
    depth < sizeof(size_t) * CHAR_BIT - 1 ? (size_t)1 << (depth + 1) : 0;
  while (lexer->lookahead == '\\') {
    if (*escape_count == SIZE_MAX) {
      return false;
    }
    *escape_count += 1;
    lexer->advance(lexer, false);
    if (*escape_count == pair_width) {
      lexer->mark_end(lexer);
      lexer->result_symbol = BACKQUOTE_PATTERN_ESCAPE;
      return true;
    }
  }

  if (lexer_at_eof(lexer) || lexer->lookahead == '\n') {
    return false;
  }
  if (
    lexer->lookahead ==
    '`' &&
    fold_backquote_escape_run(depth, *escape_count).acting_level <=
    depth +
    1
  ) {
    return false;
  }
  size_t folded =
    fold_enclosed_escape_run(scanner, *escape_count, depth, lexer->lookahead);
  if (folded == 0) {
    return false;
  }
  if (folded == 1) {
    lexer->advance(lexer, false);
  }
  lexer->mark_end(lexer);
  lexer->result_symbol = BACKQUOTE_PATTERN_ESCAPE;
  return true;
}

static bool scan_backquote_prefix_after_first_backslash(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  size_t escape_count = 1;
  if (
    valid_symbols[BACKQUOTE_PATTERN_ESCAPE] &&
    scan_backquote_pattern_escape(scanner, lexer, &escape_count)
  ) {
    return true;
  }
  if (!count_escape_run(lexer, escape_count, &escape_count)) {
    return false;
  }

  if (
    escape_count >
    1 &&
    lexer->lookahead ==
    '\n' &&
    fold_enclosed_plain_run(escape_count, scanner->backquote_depth) ==
    1 &&
    valid_symbols[BACKQUOTE_CONTINUATION_BEGIN]
  ) {
    lexer->result_symbol = BACKQUOTE_CONTINUATION_BEGIN;
    return true;
  }

  if (lexer->lookahead == '\n' && escape_count > 1) {
    size_t folded =
      fold_enclosed_plain_run(escape_count, scanner->backquote_depth);
    enum TokenType symbol = (folded & 1) != 0 ? BACKQUOTE_CONTENT_RUN_BEGIN
                                              : BACKQUOTE_PAIR_RUN_BEGIN;
    if (valid_symbols[symbol]) {
      lexer->result_symbol = (TSSymbol)symbol;
      return true;
    }
  }

  if (escape_count == 1 && lexer->lookahead == '\n') {
    if (element_boundary_symbols_are_valid(valid_symbols)) {
      return scan_element_boundary_core(scanner, lexer, valid_symbols, true);
    }
    if (continuation_led_layout_is_ambiguous(scanner, valid_symbols)) {
      lexer->advance(lexer, false);
      return classify_layout_run(scanner, lexer, valid_symbols, true);
    }
    return scan_line_continuation_after_backslash(lexer, valid_symbols);
  }

  if (lexer->lookahead == '$') {
    size_t remainder =
      fold_enclosed_special_run(escape_count, scanner->backquote_depth);
    if ((remainder & 1) != 0) {
      if (valid_symbols[BACKQUOTE_CONTENT_RUN_BEGIN]) {
        lexer->result_symbol = BACKQUOTE_CONTENT_RUN_BEGIN;
        return true;
      }
      return false;
    }

    if (
      remainder ==
      0 &&
      (valid_symbols[BACKQUOTE_DOLLAR_PREFIX] ||
        valid_symbols[BACKQUOTE_DOLLAR_SINGLE_QUOTE_PREFIX])
    ) {
      lexer->mark_end(lexer);
      lexer->advance(lexer, false);

      if (
        lexer->lookahead !=
        '{' &&
        lexer->lookahead !=
        '(' &&
        !(lexer->lookahead ==
          '\'' &&
          valid_symbols[BACKQUOTE_DOLLAR_SINGLE_QUOTE_PREFIX]) &&
        !is_parameter_start_character(lexer->lookahead)
      ) {
        return false;
      }

      enum TokenType symbol = lexer->lookahead == '\''
        ? BACKQUOTE_DOLLAR_SINGLE_QUOTE_PREFIX
        : BACKQUOTE_DOLLAR_PREFIX;
      if (!valid_symbols[symbol]) {
        return false;
      }
      lexer->result_symbol = (TSSymbol)symbol;
      return true;
    }

    if (escape_count >= 2 && valid_symbols[BACKQUOTE_PAIR_RUN_BEGIN]) {
      lexer->result_symbol = BACKQUOTE_PAIR_RUN_BEGIN;
      return true;
    }
    return false;
  }

  if (lexer->lookahead != '`') {
    return scan_backquote_ordinary_escape_run(
      scanner,
      lexer,
      valid_symbols,
      escape_count
    );
  }

  struct BackquoteEscapeRunFold fold =
    fold_backquote_escape_run(scanner->backquote_depth, escape_count);

  if (
    fold.leftover_count == 0 && fold.acting_level == scanner->backquote_depth
  ) {
    if (!valid_symbols[BACKQUOTE_END_PREFIX]) {
      bool word_end = valid_symbols[WORD_BRACKET_FALLBACK_END];
      bool parameter_end = valid_symbols[PARAMETER_BRACKET_FALLBACK_END];
      if (word_end != parameter_end) {
        lexer->result_symbol =
          word_end ? WORD_BRACKET_FALLBACK_END : PARAMETER_BRACKET_FALLBACK_END;
        return true;
      }
      return false;
    }
    lexer->mark_end(lexer);
    trim_backquote_depth(scanner, scanner->backquote_depth - 1);
    lexer->result_symbol = BACKQUOTE_END_PREFIX;
    return true;
  }

  if (
    fold.leftover_count ==
    0 &&
    fold.acting_level ==
    scanner->backquote_depth +
    1
  ) {
    if (
      !(valid_symbols[BACKQUOTE_START_PREFIX] ||
        valid_symbols[DOUBLE_QUOTED_BACKQUOTE_START_PREFIX]) ||
      !(valid_symbols[DOUBLE_QUOTED_BACKQUOTE_START_PREFIX]
          ? increase_quoted_backquote_depth(scanner)
          : increase_backquote_depth(scanner))
    ) {
      return false;
    }
    lexer->mark_end(lexer);
    lexer->result_symbol = valid_symbols[DOUBLE_QUOTED_BACKQUOTE_START_PREFIX]
      ? DOUBLE_QUOTED_BACKQUOTE_START_PREFIX
      : BACKQUOTE_START_PREFIX;
    return true;
  }

  if (fold.acting_level > scanner->backquote_depth + 1) {
    if (valid_symbols[BACKQUOTE_CONTENT_RUN_BEGIN]) {
      lexer->result_symbol = BACKQUOTE_CONTENT_RUN_BEGIN;
      return true;
    }
    return false;
  }

  if (
    fold.acting_level >=
    scanner->backquote_depth &&
    valid_symbols[BACKQUOTE_PAIR_RUN_BEGIN]
  ) {
    lexer->result_symbol = BACKQUOTE_PAIR_RUN_BEGIN;
    return true;
  }

  return false;
}

static bool scan_backquote_prefix(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  if (lexer->lookahead != '\\' || scanner->backquote_depth == 0) {
    return false;
  }

  lexer->mark_end(lexer);
  lexer->advance(lexer, false);
  return scan_backquote_prefix_after_first_backslash(
    scanner,
    lexer,
    valid_symbols
  );
}

static bool
scan_here_document_end_commit(struct Scanner *scanner, TSLexer *lexer) {
  if (scanner->active_count == 0 || scanner->at_here_document_line_start) {
    return false;
  }

  if (lexer->lookahead == '\n') {
    lexer->advance(lexer, false);
  } else if (
    !lexer_at_eof(lexer) &&
    !(scanner->body_backquote_depth > 0 && lexer->lookahead == '`')
  ) {
    return false;
  }

  lexer->mark_end(lexer);
  lexer->result_symbol = HERE_DOCUMENT_END_COMMIT;
  finish_active_document(scanner);
  return true;
}

static bool scan_quoted_here_document_end_text(
  const struct Scanner *scanner,
  TSLexer *lexer
) {
  bool consumed = false;
  while (
    !lexer_at_eof(lexer) &&
    lexer->lookahead !=
    '\n' &&
    !(scanner->body_backquote_depth > 0 && lexer->lookahead == '`')
  ) {
    if (scanner->body_backquote_depth > 0 && lexer->lookahead == '\\') {
      do {
        lexer->advance(lexer, false);
      } while (lexer->lookahead == '\\');
      if (lexer->lookahead == '`') {
        lexer->advance(lexer, false);
      }
    } else {
      lexer->advance(lexer, false);
    }
    consumed = true;
  }
  lexer->mark_end(lexer);
  lexer->result_symbol = QUOTED_HERE_DOCUMENT_END_TEXT;
  return consumed;
}

static bool scan_active_here_document(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  const struct HereDocument *document = &scanner->active_documents[0];
  enum TokenType body_start_symbol = document->quoted
    ? QUOTED_HERE_DOCUMENT_BODY_START
    : HERE_DOCUMENT_BODY_START;
  if (valid_symbols[body_start_symbol]) {
    scanner->at_here_document_line_start = true;
    lexer->mark_end(lexer);
    lexer->result_symbol = (TSSymbol)body_start_symbol;
    return true;
  }

  if (scanner->at_here_document_line_start) {
    lexer->mark_end(lexer);
    bool is_end = read_here_document_line(
                    scanner,
                    lexer,
                    document,
                    scanner->body_backquote_depth,
                    NULL,
                    NULL
                  ) == HERE_DOCUMENT_LINE_DELIMITER;
    enum TokenType end_begin = document->quoted ? QUOTED_HERE_DOCUMENT_END_BEGIN
                                                : HERE_DOCUMENT_END_BEGIN;
    if (is_end && valid_symbols[end_begin]) {
      lexer->result_symbol = (TSSymbol)end_begin;
      scanner->at_here_document_line_start = false;
      return true;
    }

    scanner->at_here_document_line_start = false;
    lexer->result_symbol = HERE_DOCUMENT_CONTENT_LINE_START;
    return true;
  }

  if (valid_symbols[LINE_CONTINUATION] && lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return false;
    }
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    lexer->result_symbol = LINE_CONTINUATION;
    return true;
  }

  if (valid_symbols[NEWLINE] && lexer->lookahead == '\n') {
    lexer->advance(lexer, false);
    reset_here_document_delimiter_scan(scanner);
    scanner->at_here_document_line_start = true;
    lexer->mark_end(lexer);
    lexer->result_symbol = NEWLINE;
    return true;
  }

  return false;
}

static bool scan_here_document_body_newline(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  if (has_startable_pending_document(scanner)) {
    return false;
  }

  lexer->mark_end(lexer);
  lexer->advance(lexer, false);
  if (!probe_here_document_continuation(scanner, lexer, valid_symbols)) {
    return false;
  }
  lexer->result_symbol = SEPARATOR_NEWLINE;
  return true;
}

static bool
scan_here_document_sequence_end(struct Scanner *scanner, TSLexer *lexer) {
  if (!scanner->sequence_end_pending) {
    return false;
  }

  scanner->sequence_end_pending = false;
  restore_suspended_documents(scanner);
  lexer->mark_end(lexer);
  lexer->result_symbol = HERE_DOCUMENT_SEQUENCE_END;
  return true;
}

void *tree_sitter_sh_external_scanner_create(void) {
  return ts_calloc(1, sizeof(struct Scanner));
}

void tree_sitter_sh_external_scanner_destroy(void *payload) {
  struct Scanner *scanner = payload;
  if (scanner == NULL) {
    return;
  }

  clear_scanner(scanner);
  ts_free(scanner);
}

struct StateWriter {
  char *data;
  size_t length;
  size_t capacity;
};

static bool write_state_bytes(
  struct StateWriter *writer,
  const char *bytes,
  size_t length
) {
  if (length > writer->capacity - writer->length) {
    return false;
  }

  if (length > 0) {
    memcpy(writer->data + writer->length, bytes, length);
  }
  writer->length += length;
  return true;
}

static bool write_state_byte(struct StateWriter *writer, uint8_t byte) {
  char data = (char)byte;
  return write_state_bytes(writer, &data, 1);
}

static bool write_state_size(struct StateWriter *writer, size_t value) {
  do {
    uint8_t byte = (uint8_t)(value & 0x7f);
    value >>= 7;
    if (value != 0) {
      byte |= 0x80;
    }
    if (!write_state_byte(writer, byte)) {
      return false;
    }
  } while (value != 0);

  return true;
}

static uint8_t document_flags(const struct HereDocument *document) {
  return (uint8_t)((document->quoted ? 1 : 0) | (document->strip_tabs ? 2 : 0));
}

static bool write_document_delimiter(
  struct StateWriter *writer,
  const struct HereDocument *document
) {
  return (
    write_state_size(writer, document->delimiter_length) &&
    write_state_bytes(
      writer,
      (const char *)document->delimiter,
      document->delimiter_length
    )
  );
}

static bool serialize_document(
  struct StateWriter *writer,
  const struct HereDocument *document,
  bool with_declaration_depth
) {
  if (!write_state_byte(writer, document_flags(document))) {
    return false;
  }
  if (
    with_declaration_depth &&
    !write_state_size(writer, document->declaration_depth)
  ) {
    return false;
  }
  return write_document_delimiter(writer, document);
}

static bool serialize_document_run(
  struct StateWriter *writer,
  const struct HereDocument *documents,
  size_t count,
  bool with_declaration_depth
) {
  for (size_t index = 0; index < count; index += 1) {
    if (!serialize_document(
          writer,
          &documents[index],
          with_declaration_depth
        )) {
      return false;
    }
  }
  return true;
}

static bool serialize_document_array(
  struct StateWriter *writer,
  const struct HereDocument *documents,
  size_t count,
  bool with_declaration_depth
) {
  return (
    write_state_size(writer, count) &&
    serialize_document_run(writer, documents, count, with_declaration_depth)
  );
}

static bool serialize_scanner_state(
  const struct Scanner *scanner,
  struct StateWriter *writer
) {
  if (
    !write_state_byte(
      writer,
      (scanner->expecting_delimiter ? 1 : 0) |
        (scanner->delimiter_strips_tabs ? 2 : 0) |
        (scanner->sequence_end_pending ? 4 : 0) |
        (scanner->at_here_document_line_start ? 8 : 0) |
        (scanner->quoted_backquote_count > 0 ? 16 : 0)
    ) ||
    !write_state_size(writer, scanner->backquote_depth) ||
    !write_state_size(writer, scanner->substitution_depth) ||
    !write_state_size(writer, scanner->body_backquote_depth) ||
    !serialize_document_array(
      writer,
      scanner->pending_documents,
      scanner->pending_count,
      true
    ) ||
    !serialize_document_array(
      writer,
      scanner->active_documents,
      scanner->active_count,
      false
    ) ||
    !write_state_size(writer, scanner->suspended_frame_count)
  ) {
    return false;
  }

  for (
    size_t frame_index = 0; frame_index < scanner->suspended_frame_count;
    frame_index += 1
  ) {
    const struct HereDocumentFrame *frame =
      &scanner->suspended_frames[frame_index];
    if (frame->count > SIZE_MAX >> 1) {
      return false;
    }
    if (
      !write_state_size(
        writer,
        (frame->count << 1) | (frame->at_line_start ? 1 : 0)
      ) ||
      !write_state_size(writer, frame->body_backquote_depth) ||
      !serialize_document_run(writer, frame->documents, frame->count, false)
    ) {
      return false;
    }
  }

  if (scanner->quoted_backquote_count > 0) {
    if (!write_state_size(writer, scanner->quoted_backquote_count)) {
      return false;
    }
    for (
      size_t index = 0; index < scanner->quoted_backquote_count; index += 1
    ) {
      if (!write_state_size(writer, scanner->quoted_backquote_depths[index])) {
        return false;
      }
    }
  }
  return true;
}

static bool scanner_state_fits(const struct Scanner *scanner) {
  char data[SCANNER_STATE_CAPACITY];
  struct StateWriter writer = {
    .data = data,
    .capacity = sizeof(data),
  };
  return serialize_scanner_state(scanner, &writer);
}

struct SerializedScannerState {
  const char *data;
  size_t length;
  size_t offset;
};

static bool read_byte(struct SerializedScannerState *state, uint8_t *byte) {
  if (state->offset >= state->length) {
    return false;
  }
  *byte = (uint8_t)state->data[state->offset];
  state->offset += 1;
  return true;
}

static bool read_size(struct SerializedScannerState *state, size_t *value) {
  size_t result = 0;
  size_t factor = 1;

  while (true) {
    uint8_t byte;
    if (!read_byte(state, &byte)) {
      return false;
    }

    size_t digit = byte & 0x7f;
    if (digit > (SIZE_MAX - result) / factor) {
      return false;
    }
    result += digit * factor;
    if ((byte & 0x80) == 0) {
      *value = result;
      return true;
    }
    if (factor > SIZE_MAX / 128) {
      return false;
    }
    factor *= 128;
  }
}

static bool read_document_body(
  struct SerializedScannerState *state,
  uint8_t flags,
  struct HereDocument *document,
  bool with_declaration_depth
) {
  size_t declaration_depth = 0;
  size_t delimiter_length;
  if (
    (flags & ~UINT8_C(3)) !=
    0 ||
    (with_declaration_depth && !read_size(state, &declaration_depth)) ||
    !read_size(state, &delimiter_length) ||
    delimiter_length >
    state->length -
    state->offset
  ) {
    return false;
  }

  uint8_t *delimiter = NULL;
  if (delimiter_length > 0) {
    delimiter = ts_malloc(delimiter_length);
    if (delimiter == NULL) {
      return false;
    }
    memcpy(delimiter, state->data + state->offset, delimiter_length);
  }
  state->offset += delimiter_length;

  *document = (struct HereDocument){
    .delimiter = delimiter,
    .delimiter_length = delimiter_length,
    .declaration_depth = declaration_depth,
    .quoted = (flags & 1) != 0,
    .strip_tabs = (flags & 2) != 0,
  };
  return true;
}

static bool deserialize_document(
  struct SerializedScannerState *state,
  struct HereDocument *document,
  bool with_declaration_depth
) {
  uint8_t flags;
  return read_byte(state, &flags) &&
    read_document_body(state, flags, document, with_declaration_depth);
}

static bool deserialize_document_run(
  struct SerializedScannerState *state,
  struct HereDocument **documents,
  size_t *actual_count,
  size_t count,
  bool with_declaration_depth
) {
  if (
    count >
    SIZE_MAX /
    sizeof(struct HereDocument) ||
    count >
    state->length -
    state->offset
  ) {
    return false;
  }

  if (count == 0) {
    return true;
  }

  *documents = ts_calloc(count, sizeof(struct HereDocument));
  if (*documents == NULL) {
    return false;
  }

  for (size_t index = 0; index < count; index += 1) {
    if (!deserialize_document(
          state,
          &(*documents)[index],
          with_declaration_depth
        )) {
      return false;
    }
    *actual_count += 1;
  }
  return true;
}

static bool deserialize_document_array(
  struct SerializedScannerState *state,
  struct HereDocument **documents,
  size_t *actual_count,
  bool with_declaration_depth
) {
  size_t count;
  return read_size(state, &count) &&
    deserialize_document_run(
      state,
      documents,
      actual_count,
      count,
      with_declaration_depth
    );
}

static bool deserialize_scanner_state(
  struct Scanner *scanner,
  const char *data,
  size_t length
) {
  struct SerializedScannerState state = {
    .data = data,
    .length = length,
  };
  uint8_t flags;
  if (
    !read_byte(&state, &flags) ||
    (flags & ~UINT8_C(31)) !=
    0 ||
    !read_size(&state, &scanner->backquote_depth) ||
    !read_size(&state, &scanner->substitution_depth) ||
    !read_size(&state, &scanner->body_backquote_depth) ||
    !deserialize_document_array(
      &state,
      &scanner->pending_documents,
      &scanner->pending_count,
      true
    ) ||
    !deserialize_document_array(
      &state,
      &scanner->active_documents,
      &scanner->active_count,
      false
    )
  ) {
    return false;
  }

  size_t suspended_frame_count;
  if (
    !read_size(&state, &suspended_frame_count) ||
    suspended_frame_count >
    SIZE_MAX /
    sizeof(struct HereDocumentFrame) ||
    suspended_frame_count >
    state.length -
    state.offset
  ) {
    return false;
  }

  if (suspended_frame_count > 0) {
    scanner->suspended_frames =
      ts_calloc(suspended_frame_count, sizeof(struct HereDocumentFrame));
    if (scanner->suspended_frames == NULL) {
      return false;
    }
  }
  scanner->suspended_frame_count = suspended_frame_count;

  for (
    size_t frame_index = 0; frame_index < suspended_frame_count;
    frame_index += 1
  ) {
    size_t packed_count;
    struct HereDocumentFrame *frame = &scanner->suspended_frames[frame_index];
    if (
      !read_size(&state, &packed_count) ||
      !read_size(&state, &frame->body_backquote_depth) ||
      !deserialize_document_run(
        &state,
        &frame->documents,
        &frame->count,
        packed_count >> 1,
        false
      )
    ) {
      return false;
    }
    frame->at_line_start = (packed_count & 1) != 0;
  }

  if ((flags & 16) != 0) {
    size_t count;
    if (
      !read_size(&state, &count) ||
      count ==
      0 ||
      count >
      state.length -
      state.offset ||
      count >
      SIZE_MAX /
      sizeof(size_t)
    ) {
      return false;
    }
    scanner->quoted_backquote_depths = ts_calloc(count, sizeof(size_t));
    if (scanner->quoted_backquote_depths == NULL) {
      return false;
    }
    size_t previous = 0;
    for (size_t index = 0; index < count; index += 1) {
      size_t depth;
      if (
        !read_size(&state, &depth) ||
        depth <=
        previous ||
        depth > scanner->backquote_depth
      ) {
        return false;
      }
      scanner->quoted_backquote_depths[index] = depth;
      previous = depth;
    }
    scanner->quoted_backquote_count = count;
  }
  if (state.offset != state.length) {
    return false;
  }

  scanner->expecting_delimiter = (flags & 1) != 0;
  scanner->delimiter_strips_tabs = (flags & 2) != 0;
  scanner->sequence_end_pending = (flags & 4) != 0;
  scanner->at_here_document_line_start = (flags & 8) != 0;
  return true;
}

unsigned
tree_sitter_sh_external_scanner_serialize(void *payload, char *buffer) {
  const struct Scanner *scanner = payload;
  if (scanner == NULL) {
    return 0;
  }

  struct StateWriter writer = {
    .data = buffer + 1,
    .capacity = SCANNER_STATE_CAPACITY,
  };
  if (!serialize_scanner_state(scanner, &writer)) {
    return 0;
  }

  buffer[0] = SCANNER_SERIALIZATION_VERSION;
  return (unsigned)writer.length + 1;
}

void tree_sitter_sh_external_scanner_deserialize(
  void *payload,
  const char *buffer,
  unsigned length
) {
  struct Scanner *scanner = payload;
  if (scanner == NULL) {
    return;
  }

  clear_scanner(scanner);
  if (
    length <
    1 ||
    length >
    TREE_SITTER_SERIALIZATION_BUFFER_SIZE ||
    (uint8_t)buffer[0] != SCANNER_SERIALIZATION_VERSION
  ) {
    return;
  }

  if (!deserialize_scanner_state(scanner, buffer + 1, length - 1)) {
    clear_scanner(scanner);
  }
}

static bool scan_tilde_end(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  enum TokenType symbol =
    valid_symbols[ASSIGNMENT_TILDE_END] ? ASSIGNMENT_TILDE_END : WORD_TILDE_END;
  lexer->mark_end(lexer);
  if (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return !(
               scanner->active_count > 0 && scanner->at_here_document_line_start
             ) &&
        backquote_prefix_token_is_valid(scanner, valid_symbols) &&
        scan_backquote_prefix_after_first_backslash(
          scanner,
          lexer,
          valid_symbols
        );
    }
    lexer->advance(lexer, false);
    if (!skip_line_continuations(lexer)) {
      return false;
    }
  }

  if (
    lexer->lookahead ==
    '/' ||
    (symbol == ASSIGNMENT_TILDE_END && lexer->lookahead == ':') ||
    is_token_delimiter(scanner, lexer)
  ) {
    lexer->result_symbol = (TSSymbol)symbol;
    return true;
  }
  return false;
}

static bool scan_dispatch(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  if (valid_symbols[BACKQUOTE_PAIR_RUN_END]) {
    lexer->mark_end(lexer);
    if (lexer->lookahead == '\\') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '\\') {
        return false;
      }
      if (
        lexer->lookahead !=
        '$' &&
        lexer->lookahead !=
        '`' &&
        !lexer_at_eof(lexer)
      ) {
        return false;
      }
    }
    lexer->result_symbol = BACKQUOTE_PAIR_RUN_END;
    return true;
  }

  if (valid_symbols[COMMAND_SUBSTITUTION_BODY_BEGIN]) {
    return scan_command_substitution_body_begin(scanner, lexer, valid_symbols);
  }

  if (
    lexer->lookahead ==
    '\\' &&
    continuation_led_layout_is_ambiguous(scanner, valid_symbols) &&
    !element_boundary_symbols_are_valid(valid_symbols) &&
    !backquote_prefix_token_is_valid(scanner, valid_symbols)
  ) {
    lexer->mark_end(lexer);
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return false;
    }
    lexer->advance(lexer, false);
    return classify_layout_run(scanner, lexer, valid_symbols, true);
  }

  if (
    (valid_symbols[ASSIGNMENT_NAME_TOKEN] ||
      valid_symbols[FNAME_TOKEN] ||
      valid_symbols[WORD_NAME_TOKEN]) &&
    is_name_start_character(lexer->lookahead)
  ) {
    return scan_name_or_reserved_word(scanner, lexer, valid_symbols);
  }

  if (
    (valid_symbols[ASSIGNMENT_TILDE_END] || valid_symbols[WORD_TILDE_END]) &&
    (lexer->lookahead ==
      '\\' ||
      lexer->lookahead ==
      ':' ||
      lexer->lookahead ==
      '/' ||
      is_token_delimiter(scanner, lexer))
  ) {
    return scan_tilde_end(scanner, lexer, valid_symbols);
  }

  bool parameter_bracket_boundary_is_valid =
    valid_symbols[PARAMETER_BRACKET_FALLBACK_END];
  bool word_bracket_boundary_is_valid =
    valid_symbols[WORD_BRACKET_FALLBACK_END];
  if (parameter_bracket_boundary_is_valid != word_bracket_boundary_is_valid) {
    bool parameter_pattern = parameter_bracket_boundary_is_valid;
    if (
      scan_bracket_fallback_end(
        scanner,
        lexer,
        parameter_pattern ? PARAMETER_BRACKET_FALLBACK_END
                          : WORD_BRACKET_FALLBACK_END,
        parameter_pattern
      )
    ) {
      return true;
    }
  }

  if (
    valid_symbols[HERE_DOCUMENT_SEQUENCE_END] &&
    scan_here_document_sequence_end(scanner, lexer)
  ) {
    return true;
  }

  if (
    valid_symbols[BACKQUOTE_END] &&
    is_active_backquote_boundary(scanner, lexer->lookahead)
  ) {
    return scan_backquote_end(scanner, lexer);
  }

  // At a body line start the end-line comparison decides first: an escape
  // run there may fold into the delimiter rather than begin body content.
  if (
    lexer->lookahead ==
    '\\' &&
    !(scanner->active_count > 0 && scanner->at_here_document_line_start) &&
    !(scanner->expecting_delimiter && valid_symbols[HERE_END_BEGIN]) &&
    backquote_prefix_token_is_valid(scanner, valid_symbols)
  ) {
    return scan_backquote_prefix(scanner, lexer, valid_symbols);
  }

  if (
    valid_symbols[HERE_DOCUMENT_LINE_END] &&
    has_startable_pending_document(scanner) &&
    lexer->lookahead == '\n'
  ) {
    return scan_here_document_line_end(scanner, lexer);
  }

  if (valid_symbols[COMMENT_LINE_END] && lexer->lookahead == '\n') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    reset_here_document_delimiter_scan(scanner);
    if (scanner->active_count > 0) {
      scanner->at_here_document_line_start = true;
    }
    lexer->result_symbol = COMMENT_LINE_END;
    return true;
  }

  if (
    valid_symbols[HERE_DOCUMENT_END_COMMIT] &&
    scanner->active_count >
    0 &&
    scan_here_document_end_commit(scanner, lexer)
  ) {
    return true;
  }

  if (scanner->active_count > 0) {
    if (
      valid_symbols[HERE_DOCUMENT_END_LEADING_TABS] &&
      scanner->active_documents[0].strip_tabs &&
      lexer->lookahead == '\t'
    ) {
      do {
        lexer->advance(lexer, false);
      } while (lexer->lookahead == '\t');
      lexer->mark_end(lexer);
      lexer->result_symbol = HERE_DOCUMENT_END_LEADING_TABS;
      return true;
    }
    if (valid_symbols[QUOTED_HERE_DOCUMENT_END_TEXT]) {
      return scan_quoted_here_document_end_text(scanner, lexer);
    }
  }

  if (
    scanner->active_count >
    0 &&
    (scanner->at_here_document_line_start ||
      valid_symbols[HERE_DOCUMENT_BODY_START] ||
      valid_symbols[QUOTED_HERE_DOCUMENT_BODY_START] ||
      (valid_symbols[LINE_CONTINUATION] && lexer->lookahead == '\\') ||
      (valid_symbols[NEWLINE] && lexer->lookahead == '\n'))
  ) {
    return scan_active_here_document(scanner, lexer, valid_symbols);
  }

  bool arithmetic_boundary_is_valid =
    arithmetic_operator_boundary_is_valid(valid_symbols) ||
    valid_symbols[ARITHMETIC_CLOSING_BOUNDARY];
  bool arithmetic_boundary_matches_lookahead = lexer->lookahead !=
    '$' &&
    lexer->lookahead !=
    '`' &&
    (is_arithmetic_operand_start(lexer->lookahead) ||
      is_arithmetic_operator_start(lexer->lookahead) ||
      lexer->lookahead ==
      ')' ||
      lexer->lookahead ==
      ' ' ||
      lexer->lookahead ==
      '\t' ||
      lexer->lookahead ==
      '\n' ||
      lexer->lookahead == '\\');
  if (arithmetic_boundary_is_valid && arithmetic_boundary_matches_lookahead) {
    return scan_arithmetic_boundary(lexer, valid_symbols);
  }

  if (
    scanner->active_count >
    0 &&
    lexer->lookahead ==
    '\n' &&
    valid_symbols[SEPARATOR_NEWLINE] &&
    !scanner->expecting_delimiter &&
    !valid_symbols[HERE_END_COMMIT] &&
    !valid_symbols[FUNCTION_BODY_CONTINUATION_BOUNDARY]
  ) {
    return scan_here_document_body_newline(scanner, lexer, valid_symbols);
  }

  if (scanner->expecting_delimiter && valid_symbols[HERE_END_BEGIN]) {
    return scan_here_document_delimiter(scanner, lexer, valid_symbols);
  }

  if (valid_symbols[HERE_END_COMMIT] && scan_here_end_commit(scanner, lexer)) {
    return true;
  }

  if (valid_symbols[DLESS] || valid_symbols[DLESSDASH]) {
    return scan_here_document_operator_commit(scanner, lexer, valid_symbols);
  }

  if (valid_symbols[DOLLAR_SINGLE_QUOTE_ESCAPE] && lexer->lookahead == '\\') {
    return scan_dollar_single_quote_token(scanner, lexer);
  }

  if (
    (valid_symbols[ARITHMETIC_LEFT_PARENTHESIS] ||
      valid_symbols[ARITHMETIC_DYNAMIC_LEFT_PARENTHESIS]) &&
    lexer->lookahead == '('
  ) {
    return scan_arithmetic_left_parenthesis(scanner, lexer, valid_symbols);
  }

  if (valid_symbols[FUNCTION_BODY_CONTINUATION_BOUNDARY]) {
    lexer->mark_end(lexer);
    lexer->result_symbol = FUNCTION_BODY_CONTINUATION_BOUNDARY;
    return true;
  }

  bool element_boundary_is_valid =
    element_boundary_symbols_are_valid(valid_symbols);
  if (element_boundary_is_valid) {
    int32_t boundary_character = lexer->lookahead;
    bool element_boundary_matches = is_horizontal_blank(boundary_character) ||
      boundary_character ==
      '\\' ||
      boundary_character ==
      ';' ||
      boundary_character ==
      '&' ||
      boundary_character ==
      '|' ||
      boundary_character ==
      '\n' ||
      (boundary_character == '#' && !valid_symbols[LITERAL_HASH]);
    if (element_boundary_matches) {
      return scan_element_boundary(scanner, lexer, valid_symbols);
    }
  }

  if (
    valid_symbols[PRE_NEWLINE_BLANK] && is_horizontal_blank(lexer->lookahead)
  ) {
    scan_horizontal_blanks(lexer);
    lexer->mark_end(lexer);
    return classify_layout_run(scanner, lexer, valid_symbols, false);
  }

  bool comment_boundary_is_valid =
    valid_symbols[COMMENT_BOUNDARY] || valid_symbols[TRAILING_COMMENT_BOUNDARY];
  bool shell_boundary_is_valid = valid_symbols[PATTERN_CONTINUATION] ||
    valid_symbols[PATTERN_END] ||
    valid_symbols[PIPE_CONTINUATION] ||
    valid_symbols[AND_OR_CONTINUATION] ||
    valid_symbols[REDIRECT_LIST_BEGIN] ||
    valid_symbols[CASE_ITEM_END] ||
    comment_boundary_is_valid;
  bool direct_hash_boundary_is_valid = lexer->lookahead ==
    '#' &&
    !valid_symbols[LITERAL_HASH] &&
    comment_boundary_is_valid;
  if (
    shell_boundary_is_valid &&
    (is_horizontal_blank(lexer->lookahead) ||
      (lexer->lookahead ==
        '|' &&
        (valid_symbols[PATTERN_CONTINUATION] ||
          valid_symbols[PIPE_CONTINUATION] ||
          valid_symbols[AND_OR_CONTINUATION])) ||
      (lexer->lookahead == '&' && valid_symbols[AND_OR_CONTINUATION]) ||
      (lexer->lookahead == ')' && valid_symbols[PATTERN_END]) ||
      (lexer->lookahead == ';' && valid_symbols[CASE_ITEM_END]) ||
      direct_hash_boundary_is_valid ||
      ((lexer->lookahead ==
         '<' ||
         lexer->lookahead ==
         '>' ||
         is_decimal_digit(lexer->lookahead)) &&
        valid_symbols[REDIRECT_LIST_BEGIN]) ||
      (lexer->lookahead == '\\' && valid_symbols[REDIRECT_LIST_BEGIN]))
  ) {
    return scan_shell_boundary(scanner, lexer, valid_symbols);
  }

  if (
    (valid_symbols[COMMAND_SUBSTITUTION_CLOSE] ||
      valid_symbols[SUBSHELL_CLOSE]) &&
    lexer->lookahead == ')'
  ) {
    if (valid_symbols[COMMAND_SUBSTITUTION_CLOSE]) {
      lexer->advance(lexer, false);
    }
    lexer->mark_end(lexer);
    if (valid_symbols[COMMAND_SUBSTITUTION_CLOSE]) {
      if (scanner->substitution_depth > 0) {
        scanner->substitution_depth -= 1;
      }
      lexer->result_symbol = COMMAND_SUBSTITUTION_CLOSE;
    } else {
      lexer->result_symbol = SUBSHELL_CLOSE;
    }
    return true;
  }

  if (lexer->lookahead == '$' && valid_symbols[DOLLAR_EXPANSION_START]) {
    return scan_dollar_expansion_start(lexer);
  }

  if (
    lexer->lookahead == ':' && valid_symbols[PATTERN_CHARACTER_CLASS_END_COLON]
  ) {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    lexer->result_symbol = PATTERN_CHARACTER_CLASS_END_COLON;
    return lexer->lookahead == ']';
  }

  if (lexer->lookahead == '[' && valid_symbols[PATTERN_SPECIAL_LEFT_BRACKET]) {
    return scan_pattern_special_left_bracket(lexer);
  }

  if (
    (valid_symbols[BACKQUOTE_START] ||
      valid_symbols[DOUBLE_QUOTED_BACKQUOTE_START]) &&
    (lexer->lookahead == '`')
  ) {
    return scan_backquote_start(
      scanner,
      lexer,
      valid_symbols[DOUBLE_QUOTED_BACKQUOTE_START]
    );
  }

  if (valid_symbols[LINE_CONTINUATION] && lexer->lookahead == '\\') {
    return scan_line_continuation(lexer, valid_symbols);
  }

  if (valid_symbols[NEWLINE] && lexer->lookahead == '\n') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    reset_here_document_delimiter_scan(scanner);
    lexer->result_symbol = NEWLINE;
    return true;
  }

  if (valid_symbols[PATTERN_BRACKET_HYPHEN] && lexer->lookahead == '-') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    lexer->result_symbol = PATTERN_BRACKET_HYPHEN;
    return true;
  }

  if (
    valid_symbols[PARAMETER_PATTERN_BRACKET_CHARACTER] &&
    scan_pattern_bracket_character(
      scanner,
      lexer,
      PARAMETER_PATTERN_BRACKET_CHARACTER,
      true
    )
  ) {
    return true;
  }

  if (
    valid_symbols[PATTERN_BRACKET_CHARACTER] &&
    scan_pattern_bracket_character(
      scanner,
      lexer,
      PATTERN_BRACKET_CHARACTER,
      false
    )
  ) {
    return true;
  }

  if (lexer->lookahead == '{') {
    return valid_symbols[LEFT_BRACE] &&
      scan_delimited_character_token(scanner, lexer, LEFT_BRACE);
  }

  if (lexer->lookahead == '}') {
    bool closer_is_reachable = valid_symbols[RIGHT_BRACE] &&
      (!valid_symbols[TERM_CONTINUATION] ||
        !(valid_symbols[WORD_SEPARATOR_BEGIN] ||
          valid_symbols[ASSIGNMENT_SEPARATOR_BEGIN] ||
          valid_symbols[REDIRECT_SEPARATOR_BEGIN]));
    return (closer_is_reachable || valid_symbols[FNAME_TOKEN]) &&
      scan_delimited_character_token(scanner, lexer, RIGHT_BRACE);
  }

  if (
    lexer->lookahead ==
    '[' &&
    (valid_symbols[WORD_BRACKET_LITERAL_START] ||
      valid_symbols[PARAMETER_BRACKET_LITERAL_START] ||
      valid_symbols[TILDE_BRACKET_LITERAL_START] ||
      valid_symbols[ASSIGNMENT_TILDE_BRACKET_LITERAL_START])
  ) {
    bool parameter_pattern = valid_symbols[PARAMETER_PATTERN_BRACKET_OPEN] ||
      valid_symbols[PARAMETER_BRACKET_LITERAL_START];
    enum TokenType commit_symbol = parameter_pattern
      ? PARAMETER_PATTERN_BRACKET_OPEN
      : WORD_PATTERN_BRACKET_OPEN;
    enum TokenType literal_symbol = parameter_pattern
      ? PARAMETER_BRACKET_LITERAL_START
      : WORD_BRACKET_LITERAL_START;
    if (valid_symbols[ASSIGNMENT_TILDE_BRACKET_LITERAL_START]) {
      literal_symbol = ASSIGNMENT_TILDE_BRACKET_LITERAL_START;
    } else if (valid_symbols[TILDE_BRACKET_LITERAL_START]) {
      literal_symbol = TILDE_BRACKET_LITERAL_START;
    }
    return scan_bracket_literal_start(
      scanner,
      lexer,
      literal_symbol,
      commit_symbol,
      parameter_pattern
    );
  }

  if (
    is_decimal_digit(lexer->lookahead) &&
    (valid_symbols[BRACED_PARAMETER_NUMBER_START] ||
      valid_symbols[BRACED_POSITIONAL_PARAMETER_START])
  ) {
    return scan_braced_numeric_parameter_start(lexer, valid_symbols);
  }

  if (is_decimal_digit(lexer->lookahead)) {
    bool at_word_start =
      valid_symbols[WORD_BRACKET_LITERAL_START] && !valid_symbols[LITERAL_HASH];
    return (valid_symbols[FILE_DESCRIPTOR] || at_word_start) &&
      scan_file_descriptor(lexer);
  }

  if (lexer->lookahead == '!') {
    return (valid_symbols[PIPELINE_NEGATION] || valid_symbols[FNAME_TOKEN]) &&
      scan_delimited_character_token(scanner, lexer, PIPELINE_NEGATION);
  }

  if (is_lowercase_letter(lexer->lookahead)) {
    return scan_lowercase_dispatch(scanner, lexer, valid_symbols);
  }

  if (valid_symbols[LITERAL_HASH] && lexer->lookahead == '#') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    lexer->result_symbol = LITERAL_HASH;
    return true;
  }

  if (valid_symbols[COMMENT] && lexer->lookahead == '#') {
    return scan_comment(scanner, lexer);
  }

  return false;
}

static void complete_ambiguous_substitutions(struct LookaheadLexer *lookahead) {
  while (!lookahead->failed && lookahead->active != SIZE_MAX) {
    lookahead->requested = SIZE_MAX;
    size_t index = lookahead->active;
    struct AmbiguousSubstitution substitution = lookahead->substitutions[index];
    if (!substitution.scan_complete) {
      if (substitution.scan_started) {
        lookahead_seek(lookahead, substitution.resume);
      } else {
        lookahead_seek(lookahead, substitution.start);
        lookahead->lexer.advance(&lookahead->lexer, false);
        substitution.scan_started = true;
      }
      enum TokenType symbol = classify_arithmetic_source(
        &substitution.context,
        &lookahead->lexer,
        &substitution.scan,
        &substitution.result
      );
      substitution.resume = lookahead->position;
      if (!lookahead_is_pending(&lookahead->lexer)) {
        substitution.arithmetic = symbol != TOKEN_COUNT;
        substitution.scan_complete = true;
        clear_arithmetic_scan(&substitution.scan);
      }
    }
    if (!lookahead_is_pending(&lookahead->lexer)) {
      if (substitution.arithmetic) {
        if (substitution.result == ARITHMETIC_VALIDATION_VALID) {
          lookahead->lexer.advance(&lookahead->lexer, false);
        }
      } else {
        if (substitution.command == NULL) {
          substitution.command = ts_calloc(1, sizeof(struct EmbeddedSkip));
          substitution.command_resume = substitution.start;
        }
        if (substitution.command == NULL) {
          lookahead->failed = true;
        } else {
          lookahead_seek(lookahead, substitution.command_resume);
          substitution.result = resume_embedded_construct(
            substitution.raw ? NULL : &substitution.context,
            &lookahead->lexer,
            ')',
            false,
            substitution.command
          );
          substitution.command_resume = lookahead->position;
        }
      }
      if (!lookahead_is_pending(&lookahead->lexer)) {
        substitution.end = lookahead->position;
        substitution.complete = true;
        if (substitution.command != NULL) {
          clear_embedded_skip(substitution.command);
          ts_free(substitution.command);
          substitution.command = NULL;
        }
      }
    }
    lookahead->substitutions[index] = substitution;
    lookahead->active =
      substitution.complete ? substitution.parent : lookahead->requested;
  }
  lookahead->requested = SIZE_MAX;
}

static bool scan_with_lookahead(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  struct LookaheadLexer lookahead = {
    .lexer = *lexer,
    .source = lexer,
    .requested = SIZE_MAX,
    .active = SIZE_MAX,
  };
  lookahead.lexer.advance = lookahead_advance;
  lookahead.lexer.mark_end = lookahead_mark_end;
  lookahead.lexer.eof = lookahead_eof;
  bool accepted = scan_dispatch(scanner, &lookahead.lexer, valid_symbols);
  lexer->result_symbol = lookahead.lexer.result_symbol;
  ts_free(lookahead.characters);
  for (size_t index = 0; index < lookahead.substitution_count; index += 1) {
    clear_arithmetic_scan(&lookahead.substitutions[index].scan);
    if (lookahead.substitutions[index].command != NULL) {
      clear_embedded_skip(lookahead.substitutions[index].command);
      ts_free(lookahead.substitutions[index].command);
    }
  }
  ts_free(lookahead.substitutions);
  return accepted && !lookahead.failed;
}

bool tree_sitter_sh_external_scanner_scan(
  void *payload,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  struct Scanner *scanner = payload;
  if (scanner == NULL) {
    return false;
  }

  bool all_symbols_are_valid = true;
  for (size_t symbol = 0; symbol < TOKEN_COUNT; symbol += 1) {
    if (!valid_symbols[symbol]) {
      all_symbols_are_valid = false;
      break;
    }
  }
  if (all_symbols_are_valid) {
    return false;
  }

  if (
    valid_symbols[COMMAND_SUBSTITUTION_BODY_BEGIN] ||
    valid_symbols[ARITHMETIC_LEFT_PARENTHESIS] ||
    valid_symbols[ARITHMETIC_DYNAMIC_LEFT_PARENTHESIS] ||
    (scanner->expecting_delimiter && valid_symbols[HERE_END_BEGIN]) ||
    (lexer->lookahead ==
      '[' &&
      (valid_symbols[WORD_BRACKET_LITERAL_START] ||
        valid_symbols[PARAMETER_BRACKET_LITERAL_START] ||
        valid_symbols[TILDE_BRACKET_LITERAL_START] ||
        valid_symbols[ASSIGNMENT_TILDE_BRACKET_LITERAL_START]))
  ) {
    return scan_with_lookahead(scanner, lexer, valid_symbols);
  }
  return scan_dispatch(scanner, lexer, valid_symbols);
}
