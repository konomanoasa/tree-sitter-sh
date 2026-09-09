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
  QUOTED_HERE_DOCUMENT_END,
  HERE_DOCUMENT_END_BEGIN,
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
  HERE_DOCUMENT_BOUNDARY,
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
  CASE_ITEM_NS_BOUNDARY,
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
  NAME_EQUALS_BEGIN,
  FNAME_BEGIN,
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
  PIPELINE_NEGATION_BEGIN,
  BACKQUOTE_CONTINUATION_BEGIN,
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
  // The body starts at a newline no deeper than its declaring redirection.
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

static bool suspend_active_documents(struct Scanner *scanner) {
  if (scanner->active_count == 0) {
    return true;
  }

  if (
    scanner->suspended_frame_count >=
    SIZE_MAX /
    sizeof(struct HereDocumentFrame)
  ) {
    return false;
  }

  size_t frame_index = scanner->suspended_frame_count;
  size_t next_count = frame_index + 1;
  struct HereDocumentFrame *resized = ts_realloc(
    scanner->suspended_frames,
    next_count * sizeof(struct HereDocumentFrame)
  );
  if (resized == NULL) {
    return false;
  }

  struct HereDocumentFrame frame = {
    .documents = scanner->active_documents,
    .count = scanner->active_count,
    .body_backquote_depth = scanner->body_backquote_depth,
    .at_line_start = scanner->at_here_document_line_start,
  };
  resized[frame_index] = frame;
  scanner->suspended_frames = resized;
  scanner->suspended_frame_count = next_count;
  scanner->active_documents = NULL;
  scanner->active_count = 0;
  scanner->at_here_document_line_start = false;

  if (!scanner_state_fits(scanner)) {
    scanner->active_documents = frame.documents;
    scanner->active_count = frame.count;
    scanner->at_here_document_line_start = frame.at_line_start;
    scanner->suspended_frame_count = frame_index;
    scanner->suspended_frames[frame_index] = (struct HereDocumentFrame){0};
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

  if (!suspend_active_documents(scanner)) {
    ts_free(startable);
    ts_free(retained);
    return false;
  }

  if (retained_count == 0) {
    scanner->active_documents = scanner->pending_documents;
    scanner->active_count = scanner->pending_count;
    scanner->pending_documents = NULL;
    scanner->pending_count = 0;
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
    ts_free(scanner->pending_documents);
    scanner->pending_documents = retained;
    scanner->pending_count = retained_count;
    scanner->active_documents = startable;
    scanner->active_count = startable_count;
  }

  // Removing declaration depths covers any growth of the body's depth.
  scanner->body_backquote_depth = scanner->backquote_depth;
  return true;
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
    return false;
  }

  char *resized = ts_realloc(buffer->data, capacity);
  if (resized == NULL) {
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

static size_t fold_enclosed_quote_run(
  const struct Scanner *scanner,
  size_t run,
  size_t depth
);

// Backquote closers terminate comments too; escaped closers retain their
// prefix for the backquote-end token.
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
          // Leave an enclosing closer's escape run outside the comment's mark.
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
    is_active_backquote_boundary(scanner, lexer->lookahead) ||
    (scanner->active_count > 0 && lexer->lookahead == '\n')
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

enum ArithmeticValidation {
  ARITHMETIC_VALIDATION_INVALID,
  ARITHMETIC_VALIDATION_INCOMPLETE,
  ARITHMETIC_VALIDATION_VALID,
};
static enum ArithmeticValidation skip_embedded_construct(
  const struct Scanner *scanner,
  TSLexer *lexer,
  char initial_closer
);

// Skip nested constructs so their ']' cannot close the outer bracket.
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
  // An unescaped follower remains at lookahead for the bracket close scan.
  BRACKET_ESCAPE_MEMBER,
  BRACKET_ESCAPE_END_OF_INPUT,
};

// Fold backquote runs like the member parse before classifying the bracket.
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
  } else if (follower == '$') {
    size_t remainder = scanner->backquote_depth < sizeof(size_t) * CHAR_BIT
      ? run >> scanner->backquote_depth
      : 0;
    follower_is_escaped = (remainder & 1) != 0;
  } else if (follower == '"') {
    follower_is_escaped =
      (fold_enclosed_quote_run(scanner, run, scanner->backquote_depth) & 1) !=
      0;
  } else {
    follower_is_escaped =
      (fold_enclosed_plain_run(run, scanner->backquote_depth) & 1) != 0;
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

// The opening '[' is consumed; lookahead is the class marker.
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
  // Mark only the opener; the grammar must parse subsequent source parts.
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
  const struct Scanner *scanner,
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
  DELIMITER_DOLLAR_SINGLE_QUOTED,
};

enum DelimiterGroupKind {
  DELIMITER_GROUP_PARAMETER,
  DELIMITER_GROUP_COMMAND,
  DELIMITER_GROUP_ARITHMETIC,
  DELIMITER_GROUP_SUBSHELL,
  DELIMITER_GROUP_BACKQUOTE,
};

struct DelimiterWordTracker {
  char text[6];
  uint8_t length;
  bool active;
  bool candidate;
};

struct DelimiterGroupFrame {
  int32_t closing;
  enum DelimiterGroupKind kind;
  enum DelimiterQuote parent_quote;
  struct DelimiterWordTracker word;
  bool command_start;
};

struct DelimiterGroupBuffer {
  struct DelimiterGroupFrame *data;
  size_t length;
  size_t capacity;
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
  // Reserved prefixes whose following word is still at command start.
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

  static const char *const COMMAND_PREFIXES[] = {
    "!",
    "{",
    "if",
    "then",
    "elif",
    "else",
    "while",
    "until",
    "do",
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
  if (kind == CASE_WORD_COMMAND_PREFIX) {
    return CASE_TRACKER_NOTE_COMMAND_PREFIX;
  }
  if (kind == CASE_WORD_CASE) {
    return CASE_TRACKER_NOTE_BEGIN;
  }
  return CASE_TRACKER_NOTE_WORD;
}

static bool grow_element_buffer(
  void **data,
  size_t *capacity,
  size_t length,
  size_t element_size,
  size_t initial_capacity
) {
  if (length < *capacity) {
    return true;
  }

  size_t next_capacity = *capacity == 0 ? initial_capacity : *capacity * 2;
  if (next_capacity < *capacity || next_capacity > SIZE_MAX / element_size) {
    return false;
  }

  void *resized = ts_realloc(*data, next_capacity * element_size);
  if (resized == NULL) {
    return false;
  }
  *data = resized;
  *capacity = next_capacity;
  return true;
}

static bool append_case_tracker(struct CaseTrackerBuffer *cases, size_t depth) {
  if (!grow_element_buffer(
        (void **)&cases->data,
        &cases->capacity,
        cases->length,
        sizeof(struct CaseTracker),
        8
      )) {
    return false;
  }

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
  if (
    groups->length >=
    SCANNER_STATE_CAPACITY ||
    !grow_element_buffer(
      (void **)&groups->data,
      &groups->capacity,
      groups->length,
      sizeof(struct DelimiterGroupFrame),
      16
    )
  ) {
    return false;
  }

  groups->data[groups->length] = (struct DelimiterGroupFrame){
    .closing = closing,
    .kind = kind,
    .parent_quote = parent_quote,
    .word = {.candidate = true},
    .command_start = delimiter_group_holds_commands(kind),
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

static void reset_delimiter_word(struct DelimiterWordTracker *word) {
  word->length = 0;
  word->active = false;
  word->candidate = true;
}

static bool finish_delimiter_word(
  struct CaseTrackerBuffer *cases,
  struct DelimiterGroupBuffer *groups,
  size_t group_depth
) {
  struct DelimiterGroupFrame *group = &groups->data[group_depth - 1];
  struct DelimiterWordTracker *word = &group->word;
  if (!word->active) {
    reset_delimiter_word(word);
    return true;
  }

  char text[sizeof(word->text) + 1] = {0};
  if (word->candidate) {
    memcpy(text, word->text, word->length);
  }
  switch (case_tracker_note_word(
    active_case_tracker(cases, group_depth),
    word->candidate ? classify_case_word(text) : CASE_WORD_GENERIC,
    group->command_start
  )) {
  case CASE_TRACKER_NOTE_COMMAND_PREFIX:
    reset_delimiter_word(word);
    return true;
  case CASE_TRACKER_NOTE_END:
    cases->length -= 1;
    break;
  case CASE_TRACKER_NOTE_BEGIN:
    if (!append_case_tracker(cases, group_depth)) {
      return false;
    }
    break;
  default:
    break;
  }
  group->command_start = false;
  reset_delimiter_word(word);
  return true;
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
  if (count > SIZE_MAX - buffer->length) {
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
  *quote = groups->data[group_depth - 1].parent_quote;
  pop_case_trackers_at_depth(cases, group_depth);
  groups->length -= 1;
}

// Nested reopening requires an escape run and bypasses this toggle.
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
    while (escape_count > 0) {
      enum BackquoteTickPrefix prefix =
        classify_backquote_tick_prefix(*backquote_depth, escape_count);
      if (prefix != BACKQUOTE_TICK_PREFIX_NONE) {
        if (
          prefix ==
          BACKQUOTE_TICK_PREFIX_END &&
          (groups->length ==
            0 ||
            groups->data[groups->length - 1].kind != DELIMITER_GROUP_BACKQUOTE)
        ) {
          return DELIMITER_BACKSLASH_ERROR;
        }
        if (
          !append_repeated_byte(delimiter, '\\', escape_count / 2) ||
          !append_byte(delimiter, '`')
        ) {
          return DELIMITER_BACKSLASH_ERROR;
        }
        mark_delimiter_quoted(
          quoted,
          collecting_nested_delimiter,
          nested_delimiter_quoted
        );
        lexer->advance(lexer, false);

        if (prefix == BACKQUOTE_TICK_PREFIX_START) {
          if (
            *backquote_depth ==
            SIZE_MAX ||
            !push_delimiter_group(
              groups,
              '`',
              DELIMITER_GROUP_BACKQUOTE,
              *quote
            )
          ) {
            return DELIMITER_BACKSLASH_ERROR;
          }
          *backquote_depth += 1;
          *quote = DELIMITER_UNQUOTED;
        } else {
          pop_delimiter_group(groups, cases, quote);
          *backquote_depth -= 1;
        }
        return DELIMITER_BACKSLASH_OK;
      }

      if (escape_count == 1) {
        if (!append_byte(delimiter, '`')) {
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

      if (!append_byte(delimiter, '\\')) {
        return DELIMITER_BACKSLASH_ERROR;
      }
      mark_delimiter_quoted(
        quoted,
        collecting_nested_delimiter,
        nested_delimiter_quoted
      );
      escape_count -= 2;
    }

    if (!append_byte(delimiter, '`')) {
      return DELIMITER_BACKSLASH_ERROR;
    }
    lexer->advance(lexer, false);
    if (
      groups->length ==
      0 ||
      !toggle_delimiter_backquote_group(groups, cases, quote, backquote_depth)
    ) {
      return DELIMITER_BACKSLASH_ERROR;
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

  size_t folded = escape_count;
  if (lexer->lookahead == '"') {
    folded = fold_enclosed_quote_run(scanner, escape_count, *backquote_depth);
  } else if (lexer->lookahead == '$') {
    folded = *backquote_depth < sizeof(size_t) * CHAR_BIT
      ? folded >> *backquote_depth
      : 0;
  } else {
    for (size_t level = 0; level < *backquote_depth; level += 1) {
      folded -= folded >> 1;
    }
  }

  if (
    *quote ==
    DELIMITER_DOUBLE_QUOTED &&
    lexer->lookahead !=
    '$' &&
    lexer->lookahead !=
    '`' &&
    lexer->lookahead != '"'
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

static void track_delimiter_word_character(
  struct DelimiterWordTracker *word,
  int32_t character
) {
  if (!word->active) {
    reset_delimiter_word(word);
    word->active = true;
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

static void track_command_word_character(
  struct DelimiterWordTracker *command_word,
  bool at_nested_delimiter_base,
  int32_t character
) {
  if (command_word != NULL && !at_nested_delimiter_base) {
    track_delimiter_word_character(command_word, character);
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

static bool
scan_dollar_single_quote_escape(TSLexer *lexer, struct ByteBuffer *delimiter) {
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
      lexer->advance(lexer, false);
      if (lexer->lookahead != '\\') {
        return false;
      }
      lexer->advance(lexer, false);
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

// An odd remainder escapes the backquote into the next rescanning level.
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

// Body and delimiter scans must fold at the same enclosing depth; only the
// delimiter then undergoes its own quote removal.
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

// Mirrors delimiter folding; false means the run reaches an enclosing closer.
static bool
fold_enclosed_backquote_run(size_t run, size_t depth, size_t *surviving) {
  size_t remaining = run;
  size_t emitted = 0;
  while (remaining > 0) {
    enum BackquoteTickPrefix prefix =
      classify_backquote_tick_prefix(depth, remaining);
    if (prefix == BACKQUOTE_TICK_PREFIX_END) {
      return false;
    }
    if (prefix == BACKQUOTE_TICK_PREFIX_START) {
      *surviving = emitted + remaining / 2;
      return true;
    }
    if (remaining == 1) {
      *surviving = emitted;
      return true;
    }
    emitted += 1;
    remaining -= 2;
  }
  return false;
}

static bool append_nested_here_document(
  struct HereDocument **documents,
  size_t *count,
  const struct ByteBuffer *source,
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
    clear_document(&document);
    return false;
  }
  return true;
}

static bool scan_stripped_here_document_tabs(
  TSLexer *lexer,
  struct ByteBuffer *source,
  bool at_logical_line_start
) {
  while (at_logical_line_start && lexer->lookahead == '\t') {
    if (source != NULL && !append_byte(source, '\t')) {
      return false;
    }
    lexer->advance(lexer, false);
  }
  return true;
}

static bool scan_nested_here_document_line(
  TSLexer *lexer,
  struct ByteBuffer *source,
  const struct HereDocument *document,
  bool *is_end,
  bool *at_end_of_input
) {
  struct ByteBuffer candidate = {0};
  bool at_logical_line_start = document->strip_tabs;
  bool valid = true;

  while (valid) {
    if (!scan_stripped_here_document_tabs(
          lexer,
          source,
          at_logical_line_start
        )) {
      valid = false;
      break;
    }

    int32_t character = lexer->lookahead;
    if (lexer_at_eof(lexer)) {
      *at_end_of_input = true;
      break;
    }

    if (!document->quoted && character == '\\') {
      size_t backslash_count = 0;
      while (valid && lexer->lookahead == '\\') {
        valid = append_byte(source, '\\');
        backslash_count += 1;
        lexer->advance(lexer, false);
      }
      if (!valid) {
        break;
      }
      if ((backslash_count & 1) != 0 && lexer->lookahead == '\n') {
        valid = append_byte(source, '\n');
        if (!valid) {
          break;
        }
        lexer->advance(lexer, false);
        backslash_count -= 1;
      }
      if (backslash_count > 0) {
        valid = append_repeated_byte(&candidate, '\\', backslash_count);
        at_logical_line_start = false;
      }
      continue;
    }

    if (character == '\n') {
      valid = append_byte(source, '\n');
      if (valid) {
        lexer->advance(lexer, false);
      }
      break;
    }

    valid = append_codepoint(source, character) &&
      append_codepoint(&candidate, character);
    if (valid) {
      at_logical_line_start = false;
      lexer->advance(lexer, false);
    }
  }

  if (valid) {
    *is_end = candidate.length ==
      document->delimiter_length &&
      (candidate.length ==
        0 ||
        memcmp(candidate.data, document->delimiter, candidate.length) == 0);
  }
  ts_free(candidate.data);
  return valid;
}

static bool scan_nested_here_document_sequence(
  TSLexer *lexer,
  struct ByteBuffer *source,
  const struct HereDocument *documents,
  size_t count
) {
  for (size_t index = 0; index < count; index += 1) {
    while (true) {
      bool is_end = false;
      bool at_end_of_input = false;
      if (!scan_nested_here_document_line(
            lexer,
            source,
            &documents[index],
            &is_end,
            &at_end_of_input
          )) {
        return false;
      }
      if (is_end) {
        break;
      }
      if (at_end_of_input) {
        return false;
      }
    }
  }
  return true;
}

static bool push_dollar_delimiter_group(
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
    groups->data[groups->length - 1].kind = DELIMITER_GROUP_ARITHMETIC;
    groups->data[groups->length - 1].command_start = false;
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
      } else if (following == '$') {
        folded = fold_enclosed_special_run(run, backquote_depth);
      } else if (following == '"' || following == '\n') {
        folded = fold_enclosed_quote_run(scanner, run, backquote_depth);
      } else {
        folded = fold_enclosed_plain_run(run, backquote_depth);
      }
      if (!append_repeated_byte(
            delimiter,
            '\\',
            dollar ? folded / 2 : folded
          )) {
        return false;
      }
      if (dollar && (folded & 1) != 0) {
        if (!scan_dollar_single_quote_escape(lexer, delimiter)) {
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
      if (!scan_dollar_single_quote_escape(lexer, delimiter)) {
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

// Substitution openers reset *quote and return control to the group scanner.
static bool scan_delimiter_double_quoted_character(
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
      if (!push_dollar_delimiter_group(
            lexer,
            delimiter,
            groups,
            DELIMITER_DOUBLE_QUOTED
          )) {
        return false;
      }
      *quote = DELIMITER_UNQUOTED;
    }
    return true;
  }

  if (character == '`') {
    if (
      *backquote_depth ==
      SIZE_MAX ||
      !append_byte(delimiter, '`') ||
      !push_delimiter_group(
        groups,
        '`',
        DELIMITER_GROUP_BACKQUOTE,
        DELIMITER_DOUBLE_QUOTED
      )
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
      lexer->lookahead == '\\'
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
      quote ==
      DELIMITER_DOUBLE_QUOTED &&
      !(character == '\\' && delimiter_backquote_depth > 0)
    ) {
      valid = scan_delimiter_double_quoted_character(
        lexer,
        &delimiter,
        &groups,
        &quote,
        &delimiter_backquote_depth
      );
      continue;
    }

    size_t command_group_depth = delimiter_command_group_depth(&groups);
    struct DelimiterWordTracker *command_word = command_group_depth == 0
      ? NULL
      : &groups.data[command_group_depth - 1].word;
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
      character == '<'
    ) {
      valid = finish_delimiter_word(&cases, &groups, command_group_depth);
      if (!valid) {
        continue;
      }

      has_word_content = true;
      valid = append_byte(&delimiter, '<');
      if (!valid) {
        continue;
      }
      lexer->advance(lexer, false);
      if (lexer->lookahead != '<') {
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

      bool strip_tabs = lexer->lookahead == '-';
      if (strip_tabs) {
        valid = append_byte(&delimiter, '-');
        if (!valid) {
          continue;
        }
        lexer->advance(lexer, false);
      }
      expecting_nested_delimiter = true;
      nested_delimiter_strips_tabs = strip_tabs;
      reset_delimiter_word(command_word);
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
        groups.data[command_group_depth - 1].command_start = true;
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
      valid = finish_delimiter_word(&cases, &groups, command_group_depth);
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
      track_delimiter_word_character(command_word, character);
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
      track_command_word_character(
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
      track_command_word_character(
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
            track_delimiter_word_character(
              &groups.data[command_group_depth - 1].word,
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
        track_command_word_character(
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
      track_command_word_character(
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
        track_delimiter_word_character(command_word, character);
        groups.data[command_group_depth - 1].command_start = false;
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
        valid = push_dollar_delimiter_group(lexer, &delimiter, &groups, quote);
      }
      continue;
    }

    if (
      groups.length > 0 && character == groups.data[groups.length - 1].closing
    ) {
      struct CaseTracker *active_case =
        active_case_tracker(&cases, groups.length);
      if (
        character ==
        ')' &&
        active_case !=
        NULL &&
        case_tracker_in_pattern(active_case->state)
      ) {
        valid = append_codepoint(&delimiter, character);
        active_case->state = CASE_TRACKER_BODY;
        groups.data[groups.length - 1].command_start = true;
        lexer->advance(lexer, false);
        continue;
      }

      valid = append_codepoint(&delimiter, character);
      pop_delimiter_group(&groups, &cases, &quote);
      lexer->advance(lexer, false);
      continue;
    }

    if (
      groups.length >
      0 &&
      groups.data[groups.length - 1].closing ==
      ')' &&
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
                                                  : DELIMITER_GROUP_SUBSHELL;
      if (
        parent_kind !=
        DELIMITER_GROUP_PARAMETER &&
        !push_delimiter_group(&groups, ')', nested_kind, quote)
      ) {
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
        groups.data[active_command_depth - 1].command_start = true;
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
  if (!append_pending_document(scanner, document)) {
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

// The caller has verified that the lookahead is the token's character.
static bool scan_delimited_character_token(
  const struct Scanner *scanner,
  TSLexer *lexer,
  enum TokenType symbol
) {
  lexer->advance(lexer, false);
  lexer->mark_end(lexer);

  if (!skip_line_continuations(lexer)) {
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

static bool
read_reserved_word(const struct Scanner *scanner, TSLexer *lexer, char *word) {
  unsigned length = 0;

  while (is_lowercase_letter(lexer->lookahead)) {
    if (length == 5) {
      return false;
    }
    word[length] = (char)lexer->lookahead;
    length += 1;
    lexer->advance(lexer, false);
  }

  if (!skip_line_continuations(lexer)) {
    return false;
  }

  if (length == 0 || !is_token_delimiter(scanner, lexer)) {
    return false;
  }

  word[length] = '\0';
  return true;
}

static bool is_closing_reserved_word(const char *word) {
  if (strcmp(word, "in") == 0) {
    return true;
  }
  for (
    size_t index = 0; index < sizeof(CLOSING_WORDS) / sizeof(CLOSING_WORDS[0]);
    index += 1
  ) {
    if (strcmp(word, CLOSING_WORDS[index].text) == 0) {
      return true;
    }
  }
  return false;
}

static bool classify_case_item_ns_end(
  const char *word,
  const bool *valid_symbols,
  TSSymbol *symbol
) {
  if (!valid_symbols[CASE_ITEM_NS_BOUNDARY] || strcmp(word, "esac") != 0) {
    return false;
  }

  *symbol = CASE_ITEM_NS_BOUNDARY;
  return true;
}

static bool classify_reserved_word_or_case_end(
  const char *word,
  const bool *valid_symbols,
  TSSymbol *symbol
) {
  return classify_reserved_word(word, valid_symbols, symbol) ||
    classify_case_item_ns_end(word, valid_symbols, symbol);
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
  if (!read_reserved_word(scanner, lexer, word)) {
    return false;
  }

  TSSymbol symbol;
  if (classify_reserved_word_or_case_end(word, valid_symbols, &symbol)) {
    lexer->result_symbol = symbol;
    return true;
  }

  return false;
}

static bool scan_horizontal_blanks(TSLexer *lexer);

static bool scan_name_equals_begin_or_reserved_word(
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

  if (!skip_line_continuations(lexer)) {
    return false;
  }

  if (lexer->lookahead == '=') {
    if (!valid_symbols[NAME_EQUALS_BEGIN]) {
      return false;
    }
    lexer->result_symbol = NAME_EQUALS_BEGIN;
    return true;
  }

  if (
    is_name_character(lexer->lookahead) || !is_token_delimiter(scanner, lexer)
  ) {
    return false;
  }

  if (is_reserved_candidate && length > 0) {
    word[length] = '\0';
  } else {
    word[0] = '\0';
  }

  // Do not gate reserved-word recognition on parser acceptance: returning an
  // unavailable reserved token forces recovery instead of a command name.
  const struct ReservedWord *reserved_word = find_reserved_word(word);
  if (valid_symbols[FNAME_BEGIN] && reserved_word == NULL) {
    while (true) {
      if (!scan_horizontal_blanks(lexer) && !skip_line_continuations(lexer)) {
        return false;
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
      lexer->result_symbol = FNAME_BEGIN;
      return true;
    }
    return false;
  }

  if (reserved_word == NULL) {
    return false;
  }

  TSSymbol symbol;
  if (classify_reserved_word_or_case_end(word, valid_symbols, &symbol)) {
    lexer->result_symbol = symbol;
    return true;
  }
  if (valid_symbols[FNAME_BEGIN]) {
    lexer->result_symbol = (TSSymbol)reserved_word->symbol;
    return true;
  }
  return false;
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
  if (!skip_line_continuations(lexer)) {
    return false;
  }
  return is_token_delimiter(scanner, lexer);
}

// The caller has consumed the first backslash.
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
scan_case_item_ns_boundary(const struct Scanner *scanner, TSLexer *lexer) {
  if (!is_lowercase_letter(lexer->lookahead)) {
    return false;
  }

  char word[6];
  if (!read_reserved_word(scanner, lexer, word) || strcmp(word, "esac") != 0) {
    return false;
  }

  lexer->result_symbol = CASE_ITEM_NS_BOUNDARY;
  return true;
}

static bool
closing_reserved_word_ends_term(const char *word, const bool *valid_symbols) {
  for (
    size_t index = 0; index < sizeof(CLOSING_WORDS) / sizeof(CLOSING_WORDS[0]);
    index += 1
  ) {
    if (strcmp(word, CLOSING_WORDS[index].text) == 0) {
      return valid_symbols[CLOSING_WORDS[index].symbol] ||
        valid_symbols[FI_KEYWORD] ||
        valid_symbols[DONE_KEYWORD] ||
        valid_symbols[ESAC_KEYWORD] ||
        valid_symbols[CASE_ITEM_NS_BOUNDARY] ||
        valid_symbols[CASE_ITEM_END] ||
        valid_symbols[RIGHT_BRACE];
    }
  }
  return false;
}

static bool enclosing_structure_owns_stray(
  const struct Scanner *scanner,
  const bool *valid_symbols
) {
  if (
    scanner->substitution_depth >
    0 ||
    valid_symbols[COMMAND_SUBSTITUTION_CLOSE] ||
    valid_symbols[SUBSHELL_CLOSE] ||
    valid_symbols[PATTERN_END] ||
    valid_symbols[CASE_ITEM_END] ||
    valid_symbols[CASE_ITEM_NS_BOUNDARY] ||
    valid_symbols[RIGHT_BRACE]
  ) {
    return true;
  }
  for (
    size_t index = 0; index < sizeof(CLOSING_WORDS) / sizeof(CLOSING_WORDS[0]);
    index += 1
  ) {
    if (valid_symbols[CLOSING_WORDS[index].symbol]) {
      return true;
    }
  }
  return false;
}

enum HereDocumentLineKind {
  HERE_DOCUMENT_LINE_DELIMITER,
  HERE_DOCUMENT_LINE_LAYOUT,
  HERE_DOCUMENT_LINE_CONTENT,
  HERE_DOCUMENT_LINE_END_OF_INPUT,
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

// Advance lookahead without moving the mark; depth counts enclosing rescans.
static enum HereDocumentLineKind probe_here_document_line(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const struct HereDocument *document,
  size_t depth,
  struct HereDocumentLineStart *start
) {
  *start = (struct HereDocumentLineStart){0};

  if (lexer_at_eof(lexer)) {
    return HERE_DOCUMENT_LINE_END_OF_INPUT;
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
      lexer->advance(lexer, false);
      continue;
    }

    int32_t character = lexer->lookahead;
    if (lexer_at_eof(lexer)) {
      break;
    }
    if (character == '\n') {
      lexer->advance(lexer, false);
      break;
    }

    size_t pending_backslashes = 0;
    int32_t pending_character = character;
    bool has_pending_character = true;
    if (character == '\\' && (depth > 0 || !document->quoted)) {
      size_t run = 0;
      while (lexer->lookahead == '\\') {
        run += 1;
        lexer->advance(lexer, false);
      }
      has_pending_character = false;
      if (depth == 0) {
        pending_backslashes = run;
        if ((pending_backslashes & 1) != 0 && lexer->lookahead == '\n') {
          lexer->advance(lexer, false);
          pending_backslashes -= 1;
        }
      } else if (lexer->lookahead == '`') {
        lexer->advance(lexer, false);
        pending_character = '`';
        has_pending_character = true;
        if (!fold_enclosed_backquote_run(run, depth, &pending_backslashes)) {
          pending_backslashes = 0;
          matches = false;
        }
      } else if (lexer->lookahead == '$') {
        pending_backslashes = fold_enclosed_special_run(run, depth);
      } else if (lexer->lookahead == '"') {
        pending_backslashes = fold_enclosed_quote_run(scanner, run, depth);
      } else {
        pending_backslashes = fold_enclosed_plain_run(run, depth);
        if (
          !document->quoted &&
          (pending_backslashes & 1) !=
          0 &&
          lexer->lookahead == '\n'
        ) {
          lexer->advance(lexer, false);
          pending_backslashes -= 1;
        }
      }
      if (pending_backslashes == 0 && !has_pending_character) {
        continue;
      }
    } else if (depth > 0 && character == '`') {
      // Preserve the closer after a matched delimiter; otherwise advance to
      // guarantee progress.
      if (matches && delimiter_offset == document->delimiter_length) {
        break;
      }
      matches = false;
      lexer->advance(lexer, false);
    } else {
      lexer->advance(lexer, false);
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

  if (in_word || awaiting_delimiter) {
    start->first_is_delimited = true;
  }

  if (matches && delimiter_offset == document->delimiter_length) {
    return HERE_DOCUMENT_LINE_DELIMITER;
  }
  if (!has_content || start->first_character == '#') {
    return HERE_DOCUMENT_LINE_LAYOUT;
  }
  return HERE_DOCUMENT_LINE_CONTENT;
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
  return probe_here_document_line(
           scanner,
           lexer,
           &scanner->active_documents[0],
           scanner->body_backquote_depth,
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
    enum HereDocumentLineKind kind = probe_here_document_line(
      scanner,
      lexer,
      document,
      scanner->body_backquote_depth,
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

static bool is_function_body_reserved_word(const char *word) {
  return find_reserved_word(word) != NULL && !is_closing_reserved_word(word);
}

static bool finish_function_body_boundary(
  TSLexer *lexer,
  const bool *valid_symbols,
  bool has_function_body
) {
  if (
    !has_function_body || !valid_symbols[FUNCTION_BODY_CONTINUATION_BOUNDARY]
  ) {
    return false;
  }

  lexer->result_symbol = FUNCTION_BODY_CONTINUATION_BOUNDARY;
  return true;
}

static bool scan_function_body_boundary(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  lexer->mark_end(lexer);

  bool has_valid_layout = true;
  while (true) {
    scan_horizontal_blanks(lexer);

    if (lexer->lookahead == '\\') {
      if (!skip_line_continuations(lexer)) {
        has_valid_layout = false;
        break;
      }
      continue;
    }

    if (lexer->lookahead == '#') {
      advance_to_comment_end(scanner, lexer, false);
    }

    if (lexer->lookahead != '\n') {
      break;
    }
    lexer->advance(lexer, false);

    if (has_startable_pending_document(scanner)) {
      bool reached_input_end = false;
      for (
        size_t index = 0; !reached_input_end && index < scanner->pending_count;
        index += 1
      ) {
        if (!here_document_is_startable(
              scanner,
              &scanner->pending_documents[index]
            )) {
          continue;
        }
        while (true) {
          struct HereDocumentLineStart start;
          enum HereDocumentLineKind kind = probe_here_document_line(
            scanner,
            lexer,
            &scanner->pending_documents[index],
            scanner->backquote_depth,
            &start
          );
          if (kind == HERE_DOCUMENT_LINE_DELIMITER) {
            break;
          }
          if (kind == HERE_DOCUMENT_LINE_END_OF_INPUT) {
            reached_input_end = true;
            break;
          }
        }
      }
      if (reached_input_end) {
        return finish_function_body_boundary(lexer, valid_symbols, false);
      }
      continue;
    }

    if (scanner->active_count > 0) {
      const struct HereDocument *document = &scanner->active_documents[0];
      while (true) {
        struct HereDocumentLineStart start;
        enum HereDocumentLineKind kind = probe_here_document_line(
          scanner,
          lexer,
          document,
          scanner->body_backquote_depth,
          &start
        );
        if (kind == HERE_DOCUMENT_LINE_LAYOUT) {
          continue;
        }
        bool has_function_body = kind ==
          HERE_DOCUMENT_LINE_CONTENT &&
          (start.first_character ==
            '(' ||
            (start.first_character == '{' && start.first_is_delimited) ||
            (start.first_word_is_reserved_candidate &&
              start.first_is_delimited &&
              is_function_body_reserved_word(start.first_word)));
        return finish_function_body_boundary(
          lexer,
          valid_symbols,
          has_function_body
        );
      }
    }
  }

  bool has_function_body = has_valid_layout && lexer->lookahead == '(';
  if (has_valid_layout && lexer->lookahead == '{') {
    lexer->advance(lexer, false);
    has_function_body =
      skip_line_continuations(lexer) && is_token_delimiter(scanner, lexer);
  } else if (has_valid_layout && is_lowercase_letter(lexer->lookahead)) {
    char word[6];
    if (read_reserved_word(scanner, lexer, word)) {
      has_function_body = is_function_body_reserved_word(word);
    }
  }

  return finish_function_body_boundary(lexer, valid_symbols, has_function_body);
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

static bool missing_command_before_boundary(
  const struct Scanner *scanner,
  TSLexer *lexer,
  bool allow_newlines
) {
  while (true) {
    if (!scan_horizontal_layout(lexer)) {
      return false;
    }
    if (lexer->lookahead == '#') {
      advance_to_comment_end(scanner, lexer, false);
    }
    int32_t character = lexer->lookahead;
    if (character == '\n') {
      if (!allow_newlines) {
        return true;
      }
      if (has_startable_pending_document(scanner)) {
        return false;
      }
      lexer->advance(lexer, false);
      continue;
    }
    if (character == ';') {
      return true;
    }
    if (character == '|' || character == '&') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == character) {
        lexer->advance(lexer, false);
      } else if (character == '&') {
        return true;
      }
      allow_newlines = true;
      continue;
    }
    if (character == '!') {
      lexer->advance(lexer, false);
      if (
        !skip_line_continuations(lexer) || !is_token_delimiter(scanner, lexer)
      ) {
        return false;
      }
      allow_newlines = false;
      continue;
    }
    return false;
  }
}

// Keep a missing operand from claiming a command beyond its separator.
static bool finish_command_continuation_operator(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols,
  enum TokenType symbol
) {
  if (!valid_symbols[symbol]) {
    return false;
  }
  if (missing_command_before_boundary(scanner, lexer, true)) {
    if (!valid_symbols[TERM_CONTINUATION]) {
      return false;
    }
    symbol = TERM_CONTINUATION;
  }
  lexer->result_symbol = (TSSymbol)symbol;
  return true;
}

static bool scan_command_continuation_operator(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
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
  return finish_command_continuation_operator(
    scanner,
    lexer,
    valid_symbols,
    symbol
  );
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
  if (!read_reserved_word(scanner, lexer, word)) {
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
  bool blank_mark_committed = false;
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
      if (crossed_layout && !crossed_pairs && !blank_mark_committed) {
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
      if (crossed_layout && valid_symbols[WORD_SEPARATOR_BEGIN]) {
        // Reused subtrees can change token validity; classify by source only.
        if (!escape_run_begins_word(scanner, lexer)) {
          return false;
        }
        lexer->result_symbol = WORD_SEPARATOR_BEGIN;
        return true;
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
    return scan_command_continuation_operator(scanner, lexer, valid_symbols);
  }

  if (character == '&') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '&') {
      lexer->advance(lexer, false);
      return finish_command_continuation_operator(
        scanner,
        lexer,
        valid_symbols,
        AND_OR_CONTINUATION
      );
    }
    return scan_separator_operator_continuation(scanner, lexer, valid_symbols);
  }

  if (character == ';') {
    if (scan_case_item_terminator(lexer)) {
      if (valid_symbols[CASE_ITEM_END]) {
        lexer->result_symbol = CASE_ITEM_END;
        return true;
      }
      enum TokenType symbol =
        enclosing_structure_owns_stray(scanner, valid_symbols)
        ? TERM_BOUNDARY
        : TERM_CONTINUATION;
      if (valid_symbols[symbol]) {
        lexer->result_symbol = (TSSymbol)symbol;
        return true;
      }
      return false;
    }
    return scan_separator_operator_continuation(scanner, lexer, valid_symbols);
  }

  if (character == '\n') {
    // Attach the closer's lookahead to the term, not its following separator.
    if (
      crossed_layout &&
      !crossed_pairs &&
      !blank_mark_committed &&
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
    if (crossed_layout && !crossed_pairs && !blank_mark_committed) {
      lexer->mark_end(lexer);
      blank_mark_committed = true;
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
        (blank_mark_committed ||
          blank_before_continuation ||
          !valid_symbols[LINE_CONTINUATION])
      ) {
        if (here_document_delimiter_line_follows(scanner, lexer)) {
          return false;
        }
        // Expose closer lookahead before reducing the term. Pending documents
        // instead need PRE_NEWLINE_BLANK to own their declaration's newline.
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
    // Require a viable separator newline to avoid changing recovery states.
    int32_t after_separator = lexer->lookahead;
    if (finish_term_continuation(scanner, lexer, valid_symbols)) {
      lexer->result_symbol = SEPARATOR_NEWLINE;
      return true;
    }
    // EOF layout must invalidate the term when edited into a command.
    // Immediate EOF has no layout dependency and preserves command reuse.
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
    return classify_word_separator(
      lexer,
      valid_symbols,
      !crossed_pairs && !blank_mark_committed
    );
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
    !blank_mark_committed &&
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

  // Probe before choosing a symbol so follower edits invalidate the boundary.
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

// Pending here-documents prevent lookahead from crossing their body start.
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
      return !enclosing_structure_owns_stray(scanner, valid_symbols);
    case ';':
      lexer->advance(lexer, false);
      if (lexer->lookahead == ';' || lexer->lookahead == '&') {
        return !enclosing_structure_owns_stray(scanner, valid_symbols);
      }
      if (enclosing_structure_owns_stray(scanner, valid_symbols)) {
        return false;
      }
      continue;
    case '&':
    case '|':
      if (enclosing_structure_owns_stray(scanner, valid_symbols)) {
        return false;
      }
      lexer->advance(lexer, false);
      if (lexer->lookahead == character) {
        lexer->advance(lexer, false);
      }
      continue;
    case '}':
      return !right_brace_is_delimited(scanner, lexer) ||
        !enclosing_structure_owns_stray(scanner, valid_symbols);
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
      return scan_command_continuation_operator(scanner, lexer, valid_symbols);
    }
  }

  if (character == '&' && valid_symbols[AND_OR_CONTINUATION]) {
    return scan_command_continuation_operator(scanner, lexer, valid_symbols);
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
    valid_symbols[CASE_ITEM_NS_BOUNDARY] &&
    !crossed_layout &&
    !valid_symbols[LITERAL_HASH] &&
    scan_case_item_ns_boundary(scanner, lexer)
  ) {
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
    if (read_reserved_word(scanner, lexer, word)) {
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

struct ValidationToken {
  uint8_t kind;
  uint8_t category;
};

struct ValidationTokenBuffer {
  struct ValidationToken *data;
  size_t length;
  size_t capacity;
};

static bool append_validation_token(
  struct ValidationTokenBuffer *tokens,
  uint8_t kind,
  uint8_t category
) {
  if (!grow_element_buffer(
        (void **)&tokens->data,
        &tokens->capacity,
        tokens->length,
        sizeof(struct ValidationToken),
        64
      )) {
    return false;
  }

  tokens->data[tokens->length] =
    (struct ValidationToken){.kind = kind, .category = category};
  tokens->length += 1;
  return true;
}

struct EmbeddedFrame {
  char closer;
  bool command_context;
};

struct EmbeddedSkip {
  struct EmbeddedFrame *frames;
  size_t frame_count;
  size_t frame_capacity;
  struct CaseTrackerBuffer cases;
  struct HereDocument *pending;
  size_t pending_count;
  size_t pending_capacity;
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

static bool embedded_push_frame(
  struct EmbeddedSkip *skip,
  char closer,
  bool command_context
) {
  if (!grow_element_buffer(
        (void **)&skip->frames,
        &skip->frame_capacity,
        skip->frame_count,
        sizeof(struct EmbeddedFrame),
        16
      )) {
    return false;
  }
  skip->frames[skip->frame_count] = (struct EmbeddedFrame){
    .closer = closer,
    .command_context = command_context,
  };
  skip->frame_count += 1;
  return true;
}

static void embedded_note_word(struct EmbeddedSkip *skip, bool in_command) {
  if (!in_command) {
    return;
  }
  (void)case_tracker_note_word(
    active_case_tracker(&skip->cases, skip->frame_count),
    CASE_WORD_GENERIC,
    false
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

// The introducer's '(' or '{' is at lookahead.
static bool
embedded_push_dollar_group(struct EmbeddedSkip *skip, TSLexer *lexer) {
  bool command_context = lexer->lookahead == '(';
  if (!embedded_push_frame(
        skip,
        command_context ? ')' : '}',
        command_context
      )) {
    return false;
  }
  lexer->advance(lexer, false);

  if (command_context && lexer->lookahead == '(') {
    skip->frames[skip->frame_count - 1].command_context = false;
    if (!embedded_push_frame(skip, ')', false)) {
      return false;
    }
    lexer->advance(lexer, false);
  }
  return true;
}

static bool embedded_append_pending(
  struct EmbeddedSkip *skip,
  struct HereDocument document
) {
  if (!grow_element_buffer(
        (void **)&skip->pending,
        &skip->pending_capacity,
        skip->pending_count,
        sizeof(struct HereDocument),
        4
      )) {
    return false;
  }
  skip->pending[skip->pending_count] = document;
  skip->pending_count += 1;
  return true;
}

static enum ArithmeticValidation read_embedded_here_document_delimiter(
  TSLexer *lexer,
  bool strip_tabs,
  struct HereDocument *document
) {
  while (is_horizontal_blank(lexer->lookahead)) {
    lexer->advance(lexer, false);
  }

  enum DelimiterReadResult result =
    read_here_document_delimiter(lexer, NULL, 0, strip_tabs, false, document);
  if (result == DELIMITER_READ_WORD) {
    return ARITHMETIC_VALIDATION_VALID;
  }
  return result == DELIMITER_READ_INCOMPLETE ? ARITHMETIC_VALIDATION_INCOMPLETE
                                             : ARITHMETIC_VALIDATION_INVALID;
}

static enum ArithmeticValidation
skip_embedded_here_document_bodies(TSLexer *lexer, struct EmbeddedSkip *skip) {
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
      bool is_end = false;
      bool at_end_of_input = false;
      if (!scan_nested_here_document_line(
            lexer,
            NULL,
            document,
            &is_end,
            &at_end_of_input
          )) {
        result = ARITHMETIC_VALIDATION_INVALID;
        break;
      }
      if (is_end) {
        break;
      }
      if (at_end_of_input) {
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
  if (lexer->lookahead == '"') {
    *run = fold_enclosed_quote_run(scanner, *run, scanner->backquote_depth);
  } else if (lexer->lookahead == '$') {
    *run = fold_enclosed_special_run(*run, scanner->backquote_depth);
  } else if (lexer->lookahead == '`' && scanner->backquote_depth > 0) {
    if (
      fold_backquote_escape_run(scanner->backquote_depth, *run).acting_level <=
      scanner->backquote_depth
    ) {
      return false;
    }
    *run = fold_enclosed_special_run(*run, scanner->backquote_depth);
  } else {
    *run = fold_enclosed_plain_run(*run, scanner->backquote_depth);
  }
  return true;
}

// Command frames track case statements and here-documents because their
// parentheses and body lines cannot end the enclosing skip.
static enum ArithmeticValidation skip_embedded_construct(
  const struct Scanner *scanner,
  TSLexer *lexer,
  char initial_closer
) {
  struct EmbeddedSkip skip = {0};
  enum ArithmeticValidation result = ARITHMETIC_VALIDATION_VALID;
  bool at_word = false;
  bool at_command_position = true;

  if (!embedded_push_frame(&skip, initial_closer, initial_closer == ')')) {
    clear_embedded_skip(&skip);
    return ARITHMETIC_VALIDATION_INVALID;
  }
  if (initial_closer == ')' && lexer->lookahead == '(') {
    skip.frames[0].command_context = false;
    if (!embedded_push_frame(&skip, ')', false)) {
      clear_embedded_skip(&skip);
      return ARITHMETIC_VALIDATION_INVALID;
    }
    lexer->advance(lexer, false);
  }

  while (result == ARITHMETIC_VALIDATION_VALID && skip.frame_count > 0) {
    if (lexer_at_eof(lexer)) {
      result = ARITHMETIC_VALIDATION_INCOMPLETE;
      break;
    }

    struct EmbeddedFrame *frame = &skip.frames[skip.frame_count - 1];
    bool in_command = frame->closer == ')' && frame->command_context;
    struct CaseTracker *active_case =
      in_command ? active_case_tracker(&skip.cases, skip.frame_count) : NULL;
    int32_t character = lexer->lookahead;

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
          skip.frame_count -= 1;
        }
      }
      continue;
    }

    if (frame->closer == '"') {
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
      if (character == '"') {
        lexer->advance(lexer, false);
        skip.frame_count -= 1;
        continue;
      }
      if (character == '`') {
        lexer->advance(lexer, false);
        if (!embedded_push_frame(&skip, '`', false)) {
          result = ARITHMETIC_VALIDATION_INVALID;
          break;
        }
        continue;
      }
      if (character == '$') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '(' || lexer->lookahead == '{') {
          if (!embedded_push_dollar_group(&skip, lexer)) {
            result = ARITHMETIC_VALIDATION_INVALID;
            break;
          }
          at_command_position = true;
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
      if (run == 0 || continuation) {
        continue;
      }
      embedded_note_word(&skip, in_command);
      at_word = true;
      at_command_position = false;
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
      embedded_note_word(&skip, in_command);
      at_word = true;
      at_command_position = false;
      continue;
    }

    if (character == '"' || character == '`') {
      lexer->advance(lexer, false);
      embedded_note_word(&skip, in_command);
      if (!embedded_push_frame(&skip, (char)character, false)) {
        result = ARITHMETIC_VALIDATION_INVALID;
        break;
      }
      at_word = true;
      at_command_position = false;
      continue;
    }

    if (character == '$') {
      lexer->advance(lexer, false);
      embedded_note_word(&skip, in_command);
      at_word = true;
      at_command_position = false;
      if (lexer->lookahead == '(' || lexer->lookahead == '{') {
        if (!embedded_push_dollar_group(&skip, lexer)) {
          result = ARITHMETIC_VALIDATION_INVALID;
          break;
        }
        at_command_position = true;
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

    if (character == '#' && frame->closer == ')' && !at_word) {
      advance_to_line_end(lexer);
      continue;
    }

    if (
      in_command &&
      character ==
      '<' &&
      (active_case == NULL || active_case->state == CASE_TRACKER_BODY)
    ) {
      lexer->advance(lexer, false);
      if (lexer->lookahead != '<') {
        at_word = false;
        continue;
      }
      lexer->advance(lexer, false);
      bool strip_tabs = lexer->lookahead == '-';
      if (strip_tabs) {
        lexer->advance(lexer, false);
      }
      struct HereDocument document;
      result =
        read_embedded_here_document_delimiter(lexer, strip_tabs, &document);
      if (result != ARITHMETIC_VALIDATION_VALID) {
        break;
      }
      document.declaration_depth = skip.frame_count;
      if (!embedded_append_pending(&skip, document)) {
        clear_document(&document);
        result = ARITHMETIC_VALIDATION_INVALID;
        break;
      }
      at_word = false;
      continue;
    }

    if (character == '\n' && skip.pending_count > 0) {
      lexer->advance(lexer, false);
      result = skip_embedded_here_document_bodies(lexer, &skip);
      at_word = false;
      at_command_position = true;
      continue;
    }

    if (character == '(' && frame->closer == ')') {
      lexer->advance(lexer, false);
      if (active_case != NULL && case_tracker_in_pattern(active_case->state)) {
        active_case->state = CASE_TRACKER_IN_PATTERN;
        at_word = false;
        continue;
      }
      if (!embedded_push_frame(&skip, ')', in_command)) {
        result = ARITHMETIC_VALIDATION_INVALID;
        break;
      }
      at_word = false;
      at_command_position = true;
      continue;
    }

    if (character == ')' && frame->closer == ')') {
      if (active_case != NULL) {
        lexer->advance(lexer, false);
        if (case_tracker_in_pattern(active_case->state)) {
          active_case->state = CASE_TRACKER_BODY;
          at_word = false;
          at_command_position = true;
          continue;
        }
        at_word = true;
        at_command_position = false;
        continue;
      }
      lexer->advance(lexer, false);
      skip.frame_count -= 1;
      pop_case_trackers_at_depth(&skip.cases, skip.frame_count + 1);
      at_word = true;
      at_command_position = false;
      continue;
    }

    if (character == '}' && frame->closer == '}') {
      lexer->advance(lexer, false);
      skip.frame_count -= 1;
      at_word = true;
      at_command_position = false;
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
      at_word = false;
      at_command_position = true;
      continue;
    }

    if (!at_word && (character == '{' || character == '!')) {
      lexer->advance(lexer, false);
      if (
        in_command && at_command_position && embedded_word_is_delimited(lexer)
      ) {
        at_word = true;
        continue;
      }
      embedded_note_word(&skip, in_command);
      at_word = true;
      at_command_position = false;
      continue;
    }

    if (!at_word && is_lowercase_letter(character)) {
      char word[6];
      size_t length = 0;
      bool candidate = true;
      bool position = at_command_position;
      while (is_lowercase_letter(lexer->lookahead)) {
        if (length < sizeof(word) - 1) {
          word[length] = (char)lexer->lookahead;
          length += 1;
        } else {
          candidate = false;
        }
        lexer->advance(lexer, false);
      }
      word[length] = '\0';
      candidate = candidate && embedded_word_is_delimited(lexer);
      if (in_command) {
        switch (case_tracker_note_word(
          active_case,
          candidate ? classify_case_word(word) : CASE_WORD_GENERIC,
          position
        )) {
        case CASE_TRACKER_NOTE_COMMAND_PREFIX:
          at_word = true;
          continue;
        case CASE_TRACKER_NOTE_END:
          skip.cases.length -= 1;
          break;
        case CASE_TRACKER_NOTE_BEGIN:
          if (!append_case_tracker(&skip.cases, skip.frame_count)) {
            result = ARITHMETIC_VALIDATION_INVALID;
          }
          break;
        default:
          break;
        }
        if (result != ARITHMETIC_VALIDATION_VALID) {
          break;
        }
      }
      at_word = true;
      at_command_position = false;
      continue;
    }

    lexer->advance(lexer, false);
    if (is_horizontal_blank(character)) {
      at_word = false;
    } else if (
      character ==
      '\n' ||
      character ==
      ';' ||
      character ==
      '&' ||
      character == '|'
    ) {
      at_word = false;
      at_command_position = true;
    } else if (character == '<' || character == '>') {
      at_word = false;
    } else {
      embedded_note_word(&skip, in_command);
      at_word = true;
      at_command_position = false;
    }
  }

  clear_embedded_skip(&skip);
  return result;
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
validate_number_source(TSLexer *lexer, struct ValidationTokenBuffer *tokens) {
  bool single_token = true;

  if (lexer->lookahead == '0') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == 'x' || lexer->lookahead == 'X') {
      lexer->advance(lexer, false);
      size_t digits = 0;
      while (is_hexadecimal_digit(lexer->lookahead)) {
        digits += 1;
        lexer->advance(lexer, false);
      }
      single_token = digits > 0;
    } else {
      while (is_decimal_digit(lexer->lookahead)) {
        if (lexer->lookahead > '7') {
          single_token = false;
        }
        lexer->advance(lexer, false);
      }
    }
  } else {
    while (is_decimal_digit(lexer->lookahead)) {
      lexer->advance(lexer, false);
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
      lexer->advance(lexer, false);
    }
    return append_validation_token(tokens, VALIDATION_TOKEN_VARIABLE, 0);
  }
  return true;
}

static bool validate_operator_source(
  TSLexer *lexer,
  struct ValidationTokenBuffer *tokens,
  int32_t first
) {
  uint8_t category;
  int32_t second = lexer->lookahead;

  switch (first) {
  case '=':
    category = ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT;
    if (second == '=') {
      category = ARITHMETIC_OPERATOR_CATEGORY_EQUALITY;
      lexer->advance(lexer, false);
    }
    break;
  case '!':
    category = VALIDATION_OPERATOR_BANG;
    if (second == '=') {
      category = ARITHMETIC_OPERATOR_CATEGORY_EQUALITY;
      lexer->advance(lexer, false);
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
      lexer->advance(lexer, false);
    } else if (second == first && first != '^') {
      category = first == '|' ? ARITHMETIC_OPERATOR_CATEGORY_LOGICAL_OR
                              : ARITHMETIC_OPERATOR_CATEGORY_LOGICAL_AND;
      lexer->advance(lexer, false);
    }
    break;
  case '<':
  case '>':
    category = ARITHMETIC_OPERATOR_CATEGORY_RELATIONAL;
    if (second == first) {
      lexer->advance(lexer, false);
      category = ARITHMETIC_OPERATOR_CATEGORY_SHIFT;
      if (lexer->lookahead == '=') {
        category = ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT;
        lexer->advance(lexer, false);
      }
    } else if (second == '=') {
      lexer->advance(lexer, false);
    }
    break;
  case '+':
  case '-':
    category = ARITHMETIC_OPERATOR_CATEGORY_ADDITIVE;
    if (second == '=') {
      category = ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT;
      lexer->advance(lexer, false);
    } else if (second == first) {
      category = VALIDATION_OPERATOR_REPEATED_SIGN;
      lexer->advance(lexer, false);
    }
    break;
  case '*':
  case '/':
  case '%':
    category = ARITHMETIC_OPERATOR_CATEGORY_MULTIPLICATIVE;
    if (second == '=') {
      category = ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT;
      lexer->advance(lexer, false);
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
  struct ValidationTokenBuffer *tokens,
  bool *has_expansion
) {
  size_t group_depth = 0;

  while (true) {
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
          *has_expansion = true;
          if (!append_validation_token(tokens, VALIDATION_TOKEN_EXPANSION, 0)) {
            return ARITHMETIC_VALIDATION_INVALID;
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
      return ARITHMETIC_VALIDATION_INVALID;
    }

    if (lexer_at_eof(lexer)) {
      return ARITHMETIC_VALIDATION_INCOMPLETE;
    }

    if (character == ' ' || character == '\t' || character == '\n') {
      lexer->advance(lexer, false);
      continue;
    }

    if (character == '(') {
      if (group_depth == SIZE_MAX) {
        return ARITHMETIC_VALIDATION_INVALID;
      }
      group_depth += 1;
      lexer->advance(lexer, false);
      if (!append_validation_token(
            tokens,
            VALIDATION_TOKEN_LEFT_PARENTHESIS,
            0
          )) {
        return ARITHMETIC_VALIDATION_INVALID;
      }
      continue;
    }

    if (character == ')') {
      lexer->advance(lexer, false);
      if (group_depth > 0) {
        group_depth -= 1;
        if (!append_validation_token(
              tokens,
              VALIDATION_TOKEN_RIGHT_PARENTHESIS,
              0
            )) {
          return ARITHMETIC_VALIDATION_INVALID;
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
      if (!validate_number_source(lexer, tokens)) {
        return ARITHMETIC_VALIDATION_INVALID;
      }
      continue;
    }

    if (is_name_start_character(character)) {
      while (is_name_character(lexer->lookahead)) {
        lexer->advance(lexer, false);
      }
      if (!append_validation_token(tokens, VALIDATION_TOKEN_VARIABLE, 0)) {
        return ARITHMETIC_VALIDATION_INVALID;
      }
      continue;
    }

    if (character == '$') {
      lexer->advance(lexer, false);
      int32_t introducer = lexer->lookahead;
      if (introducer == '(' || introducer == '{') {
        lexer->advance(lexer, false);
        enum ArithmeticValidation nested =
          skip_embedded_construct(NULL, lexer, introducer == '(' ? ')' : '}');
        if (nested != ARITHMETIC_VALIDATION_VALID) {
          return nested;
        }
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
      *has_expansion = true;
      if (!append_validation_token(tokens, VALIDATION_TOKEN_EXPANSION, 0)) {
        return ARITHMETIC_VALIDATION_INVALID;
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
      *has_expansion = true;
      if (!append_validation_token(tokens, VALIDATION_TOKEN_EXPANSION, 0)) {
        return ARITHMETIC_VALIDATION_INVALID;
      }
      continue;
    }

    if (is_arithmetic_operator_start(character) || character == '~') {
      lexer->advance(lexer, false);
      if (!validate_operator_source(lexer, tokens, character)) {
        return ARITHMETIC_VALIDATION_INVALID;
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

// Keep nesting on the heap so source depth, not the C stack, is the limit.
static bool
structured_expression_is_valid(const struct StructuredValidation *validation) {
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
        if (!grow_element_buffer(
              (void **)&contexts,
              &context_capacity,
              context_count,
              sizeof(uint8_t),
              16
            )) {
          valid = false;
          break;
        }
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
      if (!grow_element_buffer(
            (void **)&contexts,
            &context_capacity,
            context_count,
            sizeof(uint8_t),
            16
          )) {
        valid = false;
        break;
      }
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
  return valid;
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

static bool scan_arithmetic_left_parenthesis(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  lexer->mark_end(lexer);
  lexer->advance(lexer, false);

  struct ValidationTokenBuffer tokens = {0};
  bool has_expansion = false;
  enum ArithmeticValidation result =
    validate_arithmetic_content(scanner, lexer, &tokens, &has_expansion);

  enum TokenType symbol = TOKEN_COUNT;
  if (result == ARITHMETIC_VALIDATION_VALID) {
    struct StructuredValidation validation = {
      .tokens = tokens.data,
      .count = tokens.length,
    };
    if (structured_expression_is_valid(&validation)) {
      symbol = ARITHMETIC_LEFT_PARENTHESIS;
    } else if (has_expansion) {
      symbol = ARITHMETIC_DYNAMIC_LEFT_PARENTHESIS;
    }
  } else if (result == ARITHMETIC_VALIDATION_INCOMPLETE) {
    symbol = valid_symbols[ARITHMETIC_LEFT_PARENTHESIS]
      ? ARITHMETIC_LEFT_PARENTHESIS
      : ARITHMETIC_DYNAMIC_LEFT_PARENTHESIS;
  }

  ts_free(tokens.data);
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
  // Nested substitutions close through BACKQUOTE_END_PREFIX, not a bare tick.
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

  size_t folded = lexer->lookahead == '"'
    ? fold_enclosed_quote_run(scanner, escape_count, scanner->backquote_depth)
    : fold_enclosed_plain_run(escape_count, scanner->backquote_depth);
  if (
    lexer->lookahead ==
    '"' &&
    folded ==
    0 &&
    valid_symbols[BACKQUOTE_QUOTE_PREFIX]
  ) {
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

static bool scan_backquote_prefix_after_first_backslash(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  size_t escape_count;
  if (!count_escape_run(lexer, 1, &escape_count)) {
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
    size_t remainder = scanner->backquote_depth < sizeof(size_t) * CHAR_BIT
      ? escape_count >> scanner->backquote_depth
      : 0;
    if ((remainder & 1) != 0) {
      if (valid_symbols[BACKQUOTE_CONTENT_RUN_BEGIN]) {
        lexer->result_symbol = BACKQUOTE_CONTENT_RUN_BEGIN;
        return true;
      }
      return false;
    }

    if (escape_count == 1 && valid_symbols[BACKQUOTE_DOLLAR_PREFIX]) {
      lexer->mark_end(lexer);
      lexer->advance(lexer, false);

      if (
        lexer->lookahead !=
        '{' &&
        lexer->lookahead !=
        '(' &&
        !is_parameter_start_character(lexer->lookahead)
      ) {
        return false;
      }

      lexer->result_symbol = BACKQUOTE_DOLLAR_PREFIX;
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
      // End the bracket before the run so the next scan can read the closer.
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

static bool scan_here_document_end_line(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const struct HereDocument *document,
  size_t depth
) {
  bool at_logical_line_start = document->strip_tabs;
  if (!scan_stripped_here_document_tabs(lexer, NULL, at_logical_line_start)) {
    return false;
  }
  size_t delimiter_offset = 0;
  while (delimiter_offset < document->delimiter_length) {
    if (lexer_at_eof(lexer) || lexer->lookahead == '\n') {
      return false;
    }

    int32_t source_character = lexer->lookahead;
    if (depth > 0 && source_character == '\\') {
      size_t run = 0;
      while (lexer->lookahead == '\\') {
        run += 1;
        lexer->advance(lexer, false);
      }
      size_t surviving;
      int32_t folded_character = 0;
      bool continues_line = false;
      if (lexer->lookahead == '`') {
        if (!fold_enclosed_backquote_run(run, depth, &surviving)) {
          return false;
        }
        folded_character = '`';
        lexer->advance(lexer, false);
      } else if (lexer->lookahead == '$') {
        surviving = fold_enclosed_special_run(run, depth);
      } else if (lexer->lookahead == '"') {
        surviving = fold_enclosed_quote_run(scanner, run, depth);
      } else {
        surviving = fold_enclosed_plain_run(run, depth);
        if (
          !document->quoted && (surviving & 1) != 0 && lexer->lookahead == '\n'
        ) {
          surviving -= 1;
          continues_line = true;
        }
      }
      for (; surviving > 0; surviving -= 1) {
        if (!match_here_document_delimiter_character(
              document,
              &delimiter_offset,
              '\\'
            )) {
          return false;
        }
        at_logical_line_start = false;
      }
      if (folded_character != 0) {
        if (!match_here_document_delimiter_character(
              document,
              &delimiter_offset,
              folded_character
            )) {
          return false;
        }
        at_logical_line_start = false;
      }
      if (continues_line) {
        lexer->advance(lexer, false);
        if (!scan_stripped_here_document_tabs(
              lexer,
              NULL,
              at_logical_line_start
            )) {
          return false;
        }
      }
      continue;
    }
    if (depth > 0 && source_character == '`') {
      return false;
    }
    if (!document->quoted && source_character == '\\') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '\n') {
        lexer->advance(lexer, false);
        if (!scan_stripped_here_document_tabs(
              lexer,
              NULL,
              at_logical_line_start
            )) {
          return false;
        }
        continue;
      }
    } else {
      lexer->advance(lexer, false);
    }
    at_logical_line_start = false;

    if (!match_here_document_delimiter_character(
          document,
          &delimiter_offset,
          source_character
        )) {
      return false;
    }
  }

  if (!document->quoted) {
    while (lexer->lookahead == '\\') {
      if (depth > 0) {
        size_t run = 0;
        while (lexer->lookahead == '\\') {
          run += 1;
          lexer->advance(lexer, false);
        }
        if (
          fold_enclosed_plain_run(run, depth) != 1 || lexer->lookahead != '\n'
        ) {
          return false;
        }
      } else {
        lexer->advance(lexer, false);
        if (lexer->lookahead != '\n') {
          return false;
        }
      }

      lexer->advance(lexer, false);
      if (!scan_stripped_here_document_tabs(
            lexer,
            NULL,
            at_logical_line_start
          )) {
        return false;
      }
    }
  }

  if (lexer->lookahead == '\n') {
    lexer->advance(lexer, false);
    return true;
  }

  return lexer_at_eof(lexer) || (depth > 0 && lexer->lookahead == '`');
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
    bool is_end = scan_here_document_end_line(
      scanner,
      lexer,
      document,
      scanner->body_backquote_depth
    );
    if (is_end && document->quoted && valid_symbols[QUOTED_HERE_DOCUMENT_END]) {
      lexer->mark_end(lexer);
      lexer->result_symbol = QUOTED_HERE_DOCUMENT_END;
      finish_active_document(scanner);
      scanner->at_here_document_line_start = false;
      return true;
    }

    if (is_end && !document->quoted && valid_symbols[HERE_DOCUMENT_END_BEGIN]) {
      lexer->result_symbol = HERE_DOCUMENT_END_BEGIN;
      scanner->at_here_document_line_start = false;
      return true;
    }

    if (is_end && valid_symbols[HERE_DOCUMENT_BOUNDARY]) {
      lexer->result_symbol = HERE_DOCUMENT_BOUNDARY;
      return true;
    }

    scanner->at_here_document_line_start = false;
    lexer->result_symbol = HERE_DOCUMENT_CONTENT_LINE_START;
    return true;
  }

  if (valid_symbols[HERE_DOCUMENT_BOUNDARY] && lexer->lookahead == '\n') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    if (
      scan_here_document_end_line(
        scanner,
        lexer,
        document,
        scanner->body_backquote_depth
      )
    ) {
      scanner->at_here_document_line_start = true;
      lexer->result_symbol = HERE_DOCUMENT_BOUNDARY;
      return true;
    }

    if (valid_symbols[NEWLINE]) {
      scanner->at_here_document_line_start = true;
      lexer->result_symbol = NEWLINE;
      return true;
    }
    return false;
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
  const struct HereDocument *document = &scanner->active_documents[0];
  lexer->mark_end(lexer);
  lexer->advance(lexer, false);

  struct HereDocumentLineStart start;
  enum HereDocumentLineKind kind = probe_here_document_line(
    scanner,
    lexer,
    document,
    scanner->body_backquote_depth,
    &start
  );
  if (kind == HERE_DOCUMENT_LINE_DELIMITER) {
    if (valid_symbols[HERE_DOCUMENT_BOUNDARY]) {
      lexer->result_symbol = HERE_DOCUMENT_BOUNDARY;
      return true;
    }
    return false;
  }
  if (
    !valid_symbols[SEPARATOR_NEWLINE] || has_startable_pending_document(scanner)
  ) {
    return false;
  }

  while (kind == HERE_DOCUMENT_LINE_LAYOUT) {
    kind = probe_here_document_line(
      scanner,
      lexer,
      document,
      scanner->body_backquote_depth,
      &start
    );
  }
  if (
    kind ==
    HERE_DOCUMENT_LINE_CONTENT &&
    here_document_line_continues_term(scanner, &start, valid_symbols)
  ) {
    lexer->result_symbol = SEPARATOR_NEWLINE;
    return true;
  }

  return false;
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

// Non-newline backslashes may start an enclosing backquote's escape token.
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
      // Leave special-character prefixes unread; ordinary followers consume
      // the final backslash during rescanning.
      if (
        lexer->lookahead !=
        '$' &&
        lexer->lookahead !=
        '`' &&
        lexer->lookahead !=
        '\n' &&
        !lexer_at_eof(lexer)
      ) {
        lexer->mark_end(lexer);
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
    (valid_symbols[NAME_EQUALS_BEGIN] || valid_symbols[FNAME_BEGIN]) &&
    is_name_start_character(lexer->lookahead)
  ) {
    return scan_name_equals_begin_or_reserved_word(
      scanner,
      lexer,
      valid_symbols
    );
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

  if (
    (valid_symbols[ARITHMETIC_LEFT_PARENTHESIS] ||
      valid_symbols[ARITHMETIC_DYNAMIC_LEFT_PARENTHESIS]) &&
    lexer->lookahead == '('
  ) {
    return scan_arithmetic_left_parenthesis(scanner, lexer, valid_symbols);
  }

  if (valid_symbols[FUNCTION_BODY_CONTINUATION_BOUNDARY]) {
    return scan_function_body_boundary(scanner, lexer, valid_symbols);
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
    valid_symbols[CASE_ITEM_NS_BOUNDARY] ||
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
      (valid_symbols[CASE_ITEM_NS_BOUNDARY] &&
        is_lowercase_letter(lexer->lookahead)) ||
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

  // Member scans must precede the reserved-character dispatch.
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
    // TERM_CONTINUATION alone does not glue a word to this brace; only a
    // possible word, assignment or redirect continuation can do that.
    bool closer_is_reachable = valid_symbols[RIGHT_BRACE] &&
      (!valid_symbols[TERM_CONTINUATION] ||
        !(valid_symbols[WORD_SEPARATOR_BEGIN] ||
          valid_symbols[ASSIGNMENT_SEPARATOR_BEGIN] ||
          valid_symbols[REDIRECT_SEPARATOR_BEGIN]));
    return (closer_is_reachable || valid_symbols[FNAME_BEGIN]) &&
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
    // Unavailable IO_NUMBER must not fall back to a filename.
    bool at_word_start =
      valid_symbols[WORD_BRACKET_LITERAL_START] && !valid_symbols[LITERAL_HASH];
    return (valid_symbols[FILE_DESCRIPTOR] || at_word_start) &&
      scan_file_descriptor(lexer);
  }

  if (lexer->lookahead == '!') {
    if (valid_symbols[PIPELINE_NEGATION_BEGIN]) {
      lexer->mark_end(lexer);
      lexer->advance(lexer, false);
      if (
        !skip_line_continuations(lexer) || !is_token_delimiter(scanner, lexer)
      ) {
        return false;
      }
      lexer->result_symbol =
        missing_command_before_boundary(scanner, lexer, false)
        ? PIPELINE_NEGATION
        : PIPELINE_NEGATION_BEGIN;
      return true;
    }
    return (valid_symbols[PIPELINE_NEGATION] || valid_symbols[FNAME_BEGIN]) &&
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

  return scan_dispatch(scanner, lexer, valid_symbols);
}
