#include "tree_sitter/alloc.h"
#include "tree_sitter/parser.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef TREE_SITTER_SERIALIZATION_BUFFER_SIZE
#define TREE_SITTER_SERIALIZATION_BUFFER_SIZE 1024
#endif

#define SCANNER_SERIALIZATION_VERSION 13
#define SCANNER_STATE_CAPACITY (TREE_SITTER_SERIALIZATION_BUFFER_SIZE - 1)

enum TokenType {
  LEFT_BRACE,
  RIGHT_BRACE,
  FILE_DESCRIPTOR,
  IO_LOCATION,
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
  MISSING_HERE_DOCUMENT_DELIMITER,
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
  ARITHMETIC_PLUS_OPERAND_BOUNDARY,
  ARITHMETIC_MINUS_OPERAND_BOUNDARY,
  ARITHMETIC_OPERAND_BOUNDARY,
  ARITHMETIC_CLOSING_BOUNDARY,
  ARITHMETIC_LEFT_PARENTHESIS,
  ARITHMETIC_DYNAMIC_LEFT_PARENTHESIS,
  ARITHMETIC_INCOMPLETE_LEFT_PARENTHESIS,
  PATTERN_SPECIAL_LEFT_BRACKET,
  LITERAL_HASH,
  COMMENT_BOUNDARY,
  TRAILING_COMMENT_BOUNDARY,
  COMMENT,
  COMMENT_LINE_END,
  HERE_DOCUMENT_BOUNDARY,
  UNBRACED_PARAMETER_START,
  BRACED_PARAMETER_NUMBER_START,
  BRACED_POSITIONAL_PARAMETER_START,
  BACKQUOTE_START,
  BACKQUOTE_START_PREFIX,
  BACKQUOTE_DOLLAR_PREFIX,
  BACKQUOTE_END,
  BACKQUOTE_END_PREFIX,
  PATTERN_CONTINUATION,
  PATTERN_END,
  PIPE_CONTINUATION,
  REDIRECT_LIST_BEGIN,
  CASE_ITEM_END,
  CASE_ITEM_NS_BOUNDARY,
  COMPOUND_COMMAND_RECOVERY_BOUNDARY,
  SUBSHELL_RECOVERY_BOUNDARY,
  DIRECT_RECOVERY_BOUNDARY,
  HEADER_RECOVERY_BOUNDARY,
  FOR_TAIL_RECOVERY_BOUNDARY,
  CASE_ITEMS_RECOVERY_BOUNDARY,
  COMPOUND_LIST_BOUNDARY,
  FUNCTION_BODY_CONTINUATION_BOUNDARY,
  FUNCTION_BODY_RECOVERY_BOUNDARY,
  PARAMETER_MISSING_RECOVERY_BOUNDARY,
  PARAMETER_OPERATOR_RECOVERY_BOUNDARY,
  PARAMETER_TAIL_RECOVERY_BOUNDARY,
  DOUBLE_QUOTED_PARAMETER_TAIL_RECOVERY_BOUNDARY,
  INPUT_END_RECOVERY,
  MISSING_COMMAND_RECOVERY_BOUNDARY,
  INVALID_RESERVED_COMMAND_START,
  INVALID_CASE_TERMINATOR_START,
  INVALID_COMMAND_CHARACTER_SOURCE,
  SEPARATOR_RECOVERY,
  REDIRECTION_TARGET_RECOVERY,
  HERE_DOCUMENT_END_RECOVERY,
  BACKQUOTE_END_RECOVERY,
  QUOTE_INPUT_END_RECOVERY,
  COMMAND_SUBSTITUTION_INPUT_END_RECOVERY,
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
  TOKEN_COUNT,
};

_Static_assert(
  TOKEN_COUNT == 122,
  "external token enum diverges from the grammar externals"
);

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
  // The combined substitution depth at the declaring redirection. The body
  // starts at the first newline whose enclosing depth does not exceed it:
  // newlines inside a deeper substitution belong to that substitution's own
  // program, while a substitution that closes before any newline releases
  // its documents to the enclosing program.
  size_t declaration_depth;
  bool quoted;
  bool strip_tabs;
};

struct CapturedHereDocument {
  struct HereDocument document;
  uint32_t source_end_column;
};

struct HereDocumentFrame {
  struct HereDocument *documents;
  size_t count;
  size_t body_substitution_depth;
  size_t body_backquote_depth;
  bool at_line_start;
};

struct Scanner {
  struct CapturedHereDocument *captured_documents;
  size_t captured_count;
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
  // A redirection-target recovery that classified a pure digit run as the
  // next redirection's IO_NUMBER; the following scan owes that run its
  // redirect separator.
  bool redirect_follows_target_recovery;
  size_t backquote_depth;
  size_t substitution_depth;
  // The depths at the active here-document sequence's own newline. Ending a
  // document restores them, so constructs a body line left open cannot leak
  // depth into the source after the document.
  size_t body_substitution_depth;
  size_t body_backquote_depth;
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

static void clear_document(struct HereDocument *document) {
  ts_free(document->delimiter);
  document->delimiter = NULL;
  document->delimiter_length = 0;
  document->quoted = false;
  document->strip_tabs = false;
}

static void clear_captured_document(struct CapturedHereDocument *captured) {
  clear_document(&captured->document);
  captured->source_end_column = 0;
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

static void clear_captured_document_array(
  struct CapturedHereDocument **documents,
  size_t *count
) {
  for (size_t index = 0; index < *count; index += 1) {
    clear_captured_document(&(*documents)[index]);
  }

  ts_free(*documents);
  *documents = NULL;
  *count = 0;
}

static void clear_scanner(struct Scanner *scanner) {
  clear_captured_document_array(
    &scanner->captured_documents,
    &scanner->captured_count
  );
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
  scanner->expecting_delimiter = false;
  scanner->delimiter_strips_tabs = false;
  scanner->sequence_end_pending = false;
  scanner->at_here_document_line_start = false;
  scanner->redirect_follows_target_recovery = false;
  scanner->backquote_depth = 0;
  scanner->substitution_depth = 0;
  scanner->body_substitution_depth = 0;
  scanner->body_backquote_depth = 0;
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
  return append_document(
    &scanner->pending_documents,
    &scanner->pending_count,
    document
  );
}

static bool append_captured_document(
  struct Scanner *scanner,
  struct HereDocument document,
  uint32_t source_end_column
) {
  if (
    scanner->captured_count >= SIZE_MAX / sizeof(struct CapturedHereDocument)
  ) {
    return false;
  }

  document.declaration_depth = enclosing_substitution_depth(scanner);

  size_t next_count = scanner->captured_count + 1;
  struct CapturedHereDocument *resized = ts_realloc(
    scanner->captured_documents,
    next_count * sizeof(struct CapturedHereDocument)
  );
  if (resized == NULL) {
    return false;
  }

  resized[scanner->captured_count] = (struct CapturedHereDocument){
    .document = document,
    .source_end_column = source_end_column,
  };
  scanner->captured_documents = resized;
  scanner->captured_count = next_count;

  if (!scanner_state_fits(scanner)) {
    scanner->captured_count -= 1;
    scanner->captured_documents[scanner->captured_count] =
      (struct CapturedHereDocument){0};
    return false;
  }
  return true;
}

static bool move_captured_document_to_pending(struct Scanner *scanner) {
  if (scanner->captured_count == 0) {
    return false;
  }

  size_t captured_index = scanner->captured_count - 1;
  struct HereDocument document =
    scanner->captured_documents[captured_index].document;
  if (!append_pending_document(scanner, document)) {
    return false;
  }

  scanner->captured_count -= 1;
  scanner->captured_documents[captured_index] =
    (struct CapturedHereDocument){0};

  if (scanner->captured_count == 0) {
    ts_free(scanner->captured_documents);
    scanner->captured_documents = NULL;
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
    .body_substitution_depth = scanner->body_substitution_depth,
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

  scanner->body_substitution_depth = scanner->substitution_depth;
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
  scanner->body_substitution_depth = frame.body_substitution_depth;
  scanner->body_backquote_depth = frame.body_backquote_depth;
  scanner->at_here_document_line_start = frame.at_line_start;

  if (scanner->suspended_frame_count == 0) {
    ts_free(scanner->suspended_frames);
    scanner->suspended_frames = NULL;
  }
}

static void finish_active_document(struct Scanner *scanner) {
  scanner->substitution_depth = scanner->body_substitution_depth;
  scanner->backquote_depth = scanner->body_backquote_depth;
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

static bool lexer_at_eof(const TSLexer *lexer) {
  return lexer->lookahead == 0 && lexer->eof(lexer);
}

static void advance_to_line_end(TSLexer *lexer) {
  while (!lexer_at_eof(lexer) && lexer->lookahead != '\n') {
    lexer->advance(lexer, false);
  }
}

// POSIX satisfies the search for the matching backquote with the first
// unquoted, non-escaped backquote, so inside an open backquote substitution
// that character ends whatever token is being recognized.
static bool
is_active_backquote_boundary(const struct Scanner *scanner, int32_t character) {
  return scanner->backquote_depth > 0 && character == '`';
}

static bool
is_token_delimiter_character(const struct Scanner *scanner, int32_t character) {
  return (
    character ==
    ' ' ||
    character ==
    '\t' ||
    character ==
    '\n' ||
    is_control_operator_start(character) ||
    is_active_backquote_boundary(scanner, character)
  );
}

static bool
is_token_delimiter(const struct Scanner *scanner, const TSLexer *lexer) {
  return (
    lexer_at_eof(lexer) ||
    is_token_delimiter_character(scanner, lexer->lookahead)
  );
}

static bool is_missing_here_document_delimiter_boundary(
  const struct Scanner *scanner,
  const TSLexer *lexer
) {
  int32_t character = lexer->lookahead;
  return (
    lexer_at_eof(lexer) ||
    character ==
    '\n' ||
    character ==
    '#' ||
    is_control_operator_start(character) ||
    is_active_backquote_boundary(scanner, character)
  );
}

static bool is_bracket_scan_boundary(
  const struct Scanner *scanner,
  const TSLexer *lexer,
  bool parameter_pattern
) {
  if (!parameter_pattern) {
    return is_token_delimiter(scanner, lexer);
  }

  /*
   * The closing backquote of an enclosing substitution and, inside an active
   * here-document body, the line start that may hold the delimiter are
   * synchronization boundaries the bracket source never crosses.
   */
  return (
    lexer_at_eof(lexer) ||
    lexer->lookahead ==
    '}' ||
    is_active_backquote_boundary(scanner, lexer->lookahead) ||
    (scanner->active_count > 0 && lexer->lookahead == '\n')
  );
}

// A quote or expansion introducer defers the bracket classification to the
// grammar's structured readings, so the incompleteness scans stop at one.
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

static bool scan_bracket_literal_start(
  const struct Scanner *scanner,
  TSLexer *lexer,
  enum TokenType symbol,
  bool parameter_pattern
) {
  if (lexer->lookahead != '[') {
    return false;
  }

  lexer->advance(lexer, false);
  /*
   * Scan ahead only to prove that the bracket expression is incomplete.
   * Keep the token at the opener so the grammar owns every following source
   * part and can independently recognize later bracket expressions.
   */
  lexer->mark_end(lexer);

  bool has_member = false;
  bool may_be_negation = true;
  while (!is_bracket_scan_boundary(scanner, lexer, parameter_pattern)) {
    int32_t character = lexer->lookahead;

    if (character == '\\') {
      if (!skip_line_continuations(lexer)) {
        return false;
      }
      continue;
    }

    if (is_quote_or_expansion_start(character)) {
      return false;
    }

    if (may_be_negation && character == '!') {
      may_be_negation = false;
      lexer->advance(lexer, false);
      continue;
    }
    may_be_negation = false;

    if (character == ']') {
      if (has_member) {
        return false;
      }
      lexer->advance(lexer, false);
      has_member = true;
      continue;
    }

    if (character == '[') {
      lexer->advance(lexer, false);
      if (!skip_line_continuations(lexer)) {
        return false;
      }
      int32_t nested_marker = lexer->lookahead;
      bool is_nested_special =
        nested_marker == ':' || nested_marker == '.' || nested_marker == '=';
      if (is_nested_special) {
        lexer->advance(lexer, false);
        size_t content_length = 0;
        bool closed = false;
        while (!is_bracket_scan_boundary(scanner, lexer, parameter_pattern)) {
          int32_t nested_character = lexer->lookahead;
          if (nested_character == '\\') {
            if (!skip_line_continuations(lexer)) {
              return false;
            }
            continue;
          }
          if (is_quote_or_expansion_start(nested_character)) {
            return false;
          }
          if (nested_character == ']' && nested_marker != '.') {
            lexer->result_symbol = (TSSymbol)symbol;
            return true;
          }

          lexer->advance(lexer, false);
          if (nested_character == nested_marker && content_length > 0) {
            if (!skip_line_continuations(lexer)) {
              return false;
            }
            if (lexer->lookahead == ']') {
              lexer->advance(lexer, false);
              closed = true;
              break;
            }
          }
          content_length += 1;
        }
        if (!closed) {
          lexer->result_symbol = (TSSymbol)symbol;
          return true;
        }
      }
      has_member = true;
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

enum DelimiterCaseState {
  DELIMITER_CASE_EXPECT_WORD,
  DELIMITER_CASE_EXPECT_IN,
  DELIMITER_CASE_EXPECT_PATTERN,
  DELIMITER_CASE_BODY,
};

struct DelimiterCaseFrame {
  size_t group_depth;
  enum DelimiterCaseState state;
};

struct DelimiterCaseBuffer {
  struct DelimiterCaseFrame *data;
  size_t length;
  size_t capacity;
};

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

static bool
append_delimiter_case(struct DelimiterCaseBuffer *cases, size_t group_depth) {
  if (!grow_element_buffer(
        (void **)&cases->data,
        &cases->capacity,
        cases->length,
        sizeof(struct DelimiterCaseFrame),
        8
      )) {
    return false;
  }

  cases->data[cases->length] = (struct DelimiterCaseFrame){
    .group_depth = group_depth,
    .state = DELIMITER_CASE_EXPECT_WORD,
  };
  cases->length += 1;
  return true;
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
    .command_start = kind ==
      DELIMITER_GROUP_COMMAND ||
      kind ==
      DELIMITER_GROUP_SUBSHELL ||
      kind == DELIMITER_GROUP_BACKQUOTE,
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

static bool delimiter_word_equals(
  const struct DelimiterWordTracker *word,
  const char *text
) {
  size_t length = strlen(text);
  return (
    word->candidate &&
    word->length ==
    length &&
    memcmp(word->text, text, length) == 0
  );
}

static void reset_delimiter_word(struct DelimiterWordTracker *word) {
  word->length = 0;
  word->active = false;
  word->candidate = true;
}

static bool finish_delimiter_word(
  struct DelimiterCaseBuffer *cases,
  struct DelimiterGroupBuffer *groups,
  size_t group_depth
) {
  struct DelimiterGroupFrame *group = &groups->data[group_depth - 1];
  struct DelimiterWordTracker *word = &group->word;
  if (!word->active) {
    reset_delimiter_word(word);
    return true;
  }

  bool at_command_start = group->command_start;
  struct DelimiterCaseFrame *active_case =
    cases->length == 0 ? NULL : &cases->data[cases->length - 1];
  if (active_case != NULL && active_case->group_depth == group_depth) {
    if (active_case->state == DELIMITER_CASE_EXPECT_WORD) {
      active_case->state = DELIMITER_CASE_EXPECT_IN;
      group->command_start = false;
      reset_delimiter_word(word);
      return true;
    }

    if (active_case->state == DELIMITER_CASE_EXPECT_IN) {
      if (delimiter_word_equals(word, "in")) {
        active_case->state = DELIMITER_CASE_EXPECT_PATTERN;
      }
      group->command_start = false;
      reset_delimiter_word(word);
      return true;
    }

    if (
      (active_case->state ==
        DELIMITER_CASE_EXPECT_PATTERN ||
        active_case->state == DELIMITER_CASE_BODY) &&
      (active_case->state ==
        DELIMITER_CASE_EXPECT_PATTERN ||
        at_command_start) &&
      delimiter_word_equals(word, "esac")
    ) {
      cases->length -= 1;
      group->command_start = false;
      reset_delimiter_word(word);
      return true;
    }
  }

  if (
    at_command_start &&
    (delimiter_word_equals(word, "!") ||
      delimiter_word_equals(word, "{") ||
      delimiter_word_equals(word, "if") ||
      delimiter_word_equals(word, "then") ||
      delimiter_word_equals(word, "elif") ||
      delimiter_word_equals(word, "else") ||
      delimiter_word_equals(word, "while") ||
      delimiter_word_equals(word, "until") ||
      delimiter_word_equals(word, "do"))
  ) {
    group->command_start = true;
    reset_delimiter_word(word);
    return true;
  }

  if (at_command_start && delimiter_word_equals(word, "case")) {
    if (!append_delimiter_case(cases, group_depth)) {
      return false;
    }
  }
  group->command_start = false;
  reset_delimiter_word(word);
  return true;
}

static size_t
delimiter_command_group_depth(const struct DelimiterGroupBuffer *groups) {
  if (groups->length == 0) {
    return 0;
  }

  enum DelimiterGroupKind kind = groups->data[groups->length - 1].kind;
  return (kind ==
           DELIMITER_GROUP_COMMAND ||
           kind ==
           DELIMITER_GROUP_SUBSHELL ||
           kind == DELIMITER_GROUP_BACKQUOTE)
    ? groups->length
    : 0;
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
  struct DelimiterCaseBuffer *cases,
  enum DelimiterQuote *quote
) {
  size_t group_depth = groups->length;
  *quote = groups->data[group_depth - 1].parent_quote;
  while (
    cases->length >
    0 &&
    cases->data[cases->length - 1].group_depth >= group_depth
  ) {
    cases->length -= 1;
  }
  groups->length -= 1;
}

enum BackquoteTickPrefix {
  BACKQUOTE_TICK_PREFIX_NONE,
  BACKQUOTE_TICK_PREFIX_START,
  BACKQUOTE_TICK_PREFIX_END,
};

static enum BackquoteTickPrefix classify_backquote_tick_prefix(
  size_t depth,
  size_t escape_count,
  bool allow_start,
  bool allow_end
);

enum DelimiterBackslashResult {
  DELIMITER_BACKSLASH_OK,
  DELIMITER_BACKSLASH_LEADING_CONTINUATION,
  DELIMITER_BACKSLASH_ERROR,
};

static void mark_delimiter_escape(
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
  struct ByteBuffer *delimiter,
  struct DelimiterGroupBuffer *groups,
  struct DelimiterCaseBuffer *cases,
  enum DelimiterQuote *quote,
  size_t *backquote_depth,
  bool at_delimiter_source_start,
  bool collecting_nested_delimiter,
  bool *quoted,
  bool *nested_delimiter_quoted
) {
  size_t escape_count = 0;
  while (lexer->lookahead == '\\') {
    if (escape_count == SIZE_MAX) {
      return DELIMITER_BACKSLASH_ERROR;
    }
    escape_count += 1;
    lexer->advance(lexer, false);
  }

  if (lexer->lookahead == '`') {
    while (escape_count > 0) {
      enum BackquoteTickPrefix prefix = classify_backquote_tick_prefix(
        *backquote_depth,
        escape_count,
        true,
        true
      );
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
        mark_delimiter_escape(
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
        mark_delimiter_escape(
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
      mark_delimiter_escape(
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
      groups->length >
      0 &&
      groups->data[groups->length - 1].kind == DELIMITER_GROUP_BACKQUOTE
    ) {
      pop_delimiter_group(groups, cases, quote);
      *backquote_depth -= 1;
      return DELIMITER_BACKSLASH_OK;
    }
    if (groups->length == 0) {
      return DELIMITER_BACKSLASH_ERROR;
    }
    if (
      *backquote_depth ==
      SIZE_MAX ||
      !push_delimiter_group(groups, '`', DELIMITER_GROUP_BACKQUOTE, *quote)
    ) {
      return DELIMITER_BACKSLASH_ERROR;
    }
    *backquote_depth += 1;
    *quote = DELIMITER_UNQUOTED;
    return DELIMITER_BACKSLASH_OK;
  }

  if (lexer_at_eof(lexer) || lexer->lookahead == '\n') {
    size_t quoted_backslashes = escape_count / 2;
    if (!append_repeated_byte(delimiter, '\\', quoted_backslashes)) {
      return DELIMITER_BACKSLASH_ERROR;
    }
    if (quoted_backslashes > 0) {
      mark_delimiter_escape(
        quoted,
        collecting_nested_delimiter,
        nested_delimiter_quoted
      );
    }
    if ((escape_count & 1) == 0) {
      return DELIMITER_BACKSLASH_OK;
    }
    if (lexer_at_eof(lexer)) {
      return DELIMITER_BACKSLASH_ERROR;
    }
    lexer->advance(lexer, false);
    return at_delimiter_source_start && escape_count == 1
      ? DELIMITER_BACKSLASH_LEADING_CONTINUATION
      : DELIMITER_BACKSLASH_OK;
  }

  /*
   * Fold the run through the enclosing backquote levels down to the level
   * that owns the here-document. Each level halves a pair into one backslash.
   * A backslash before a dollar sign is consumed by the level that removes
   * it, so the dollar's count floors; before any other character the odd
   * backslash keeps its literal meaning and the count ceils. At the owning
   * level a remaining pair is a quoted literal backslash and a remaining odd
   * backslash quotes the character; an unquoted dollar sign stays with the
   * caller's expansion handling.
   */
  size_t folded = escape_count;
  if (lexer->lookahead == '$') {
    folded = *backquote_depth < sizeof(size_t) * CHAR_BIT
      ? folded >> *backquote_depth
      : 0;
  } else {
    for (size_t level = 0; level < *backquote_depth; level += 1) {
      folded -= folded >> 1;
    }
  }

  size_t literal_backslashes = folded >> 1;
  if (!append_repeated_byte(delimiter, '\\', literal_backslashes)) {
    return DELIMITER_BACKSLASH_ERROR;
  }
  if (literal_backslashes > 0) {
    mark_delimiter_escape(
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
  mark_delimiter_escape(
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

static bool backquote_escape_count(
  size_t depth,
  bool opens_nested_substitution,
  size_t *count
) {
  if (!opens_nested_substitution && depth == 0) {
    return false;
  }

  size_t exponent = opens_nested_substitution ? depth : depth - 1;
  if (exponent > sizeof(size_t) * CHAR_BIT) {
    return false;
  }

  size_t result = 0;
  for (size_t index = 0; index < exponent; index += 1) {
    result = result * 2 + 1;
  }
  *count = result;
  return true;
}

static enum BackquoteTickPrefix classify_backquote_tick_prefix(
  size_t depth,
  size_t escape_count,
  bool allow_start,
  bool allow_end
) {
  size_t expected_escape_count;
  if (
    allow_end &&
    backquote_escape_count(depth, false, &expected_escape_count) &&
    escape_count == expected_escape_count
  ) {
    return BACKQUOTE_TICK_PREFIX_END;
  }

  if (
    allow_start &&
    backquote_escape_count(depth, true, &expected_escape_count) &&
    escape_count == expected_escape_count
  ) {
    return BACKQUOTE_TICK_PREFIX_START;
  }

  return BACKQUOTE_TICK_PREFIX_NONE;
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
      valid = append_byte(source, '\\');
      if (!valid) {
        break;
      }
      lexer->advance(lexer, false);
      if (lexer->lookahead == '\n') {
        valid = append_byte(source, '\n');
        if (!valid) {
          break;
        }
        lexer->advance(lexer, false);
        continue;
      }
      valid = append_byte(&candidate, '\\');
      at_logical_line_start = false;
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

/*
 * Read the "(" or "{" that follows a dollar sign in a here-document
 * delimiter, pushing the group frame and upgrading "$((" to its arithmetic
 * pair. The caller has already consumed the dollar sign.
 */
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

static bool scan_here_document_delimiter(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  lexer->mark_end(lexer);
  enum DelimiterQuote quote = DELIMITER_UNQUOTED;
  struct ByteBuffer delimiter = {.limit = SCANNER_STATE_CAPACITY};
  struct DelimiterGroupBuffer groups = {0};
  struct DelimiterCaseBuffer cases = {0};
  struct HereDocument *nested_documents = NULL;
  size_t nested_document_count = 0;
  size_t nested_delimiter_start = 0;
  size_t nested_delimiter_group_depth = 0;
  size_t delimiter_backquote_depth = scanner->backquote_depth;
  bool has_word_content = false;
  bool quoted = false;
  bool expecting_nested_delimiter = false;
  bool collecting_nested_delimiter = false;
  bool nested_delimiter_quoted = false;
  bool nested_delimiter_strips_tabs = false;
  bool valid = true;

  while (valid) {
    int32_t character = lexer->lookahead;

    if (quote == DELIMITER_SINGLE_QUOTED) {
      if (lexer_at_eof(lexer)) {
        valid = false;
      } else if (character == '\'') {
        quote = DELIMITER_UNQUOTED;
        lexer->advance(lexer, false);
      } else {
        valid = append_codepoint(&delimiter, character);
        lexer->advance(lexer, false);
      }
      continue;
    }

    if (quote == DELIMITER_DOLLAR_SINGLE_QUOTED) {
      if (lexer_at_eof(lexer)) {
        valid = false;
      } else if (character == '\'') {
        quote = DELIMITER_UNQUOTED;
        lexer->advance(lexer, false);
      } else if (character == '\\') {
        lexer->advance(lexer, false);
        valid = scan_dollar_single_quote_escape(lexer, &delimiter);
      } else {
        valid = append_codepoint(&delimiter, character);
        lexer->advance(lexer, false);
      }
      continue;
    }

    if (quote == DELIMITER_DOUBLE_QUOTED) {
      if (lexer_at_eof(lexer)) {
        valid = false;
      } else if (character == '"') {
        quote = DELIMITER_UNQUOTED;
        lexer->advance(lexer, false);
      } else if (character == '$') {
        lexer->advance(lexer, false);
        valid = append_byte(&delimiter, '$');
        if (valid && (lexer->lookahead == '(' || lexer->lookahead == '{')) {
          valid = push_dollar_delimiter_group(
            lexer,
            &delimiter,
            &groups,
            DELIMITER_DOUBLE_QUOTED
          );
          if (valid) {
            quote = DELIMITER_UNQUOTED;
          }
        }
      } else if (character == '`') {
        valid = delimiter_backquote_depth <
          SIZE_MAX &&
          append_byte(&delimiter, '`') &&
          push_delimiter_group(
            &groups,
            '`',
            DELIMITER_GROUP_BACKQUOTE,
            DELIMITER_DOUBLE_QUOTED
          );
        if (valid) {
          delimiter_backquote_depth += 1;
          quote = DELIMITER_UNQUOTED;
          lexer->advance(lexer, false);
        }
      } else if (character == '\\') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '\n') {
          lexer->advance(lexer, false);
        } else if (
          lexer->lookahead ==
          '$' ||
          lexer->lookahead ==
          '`' ||
          lexer->lookahead ==
          '"' ||
          lexer->lookahead == '\\'
        ) {
          valid = append_codepoint(&delimiter, lexer->lookahead);
          lexer->advance(lexer, false);
        } else {
          valid = append_byte(&delimiter, '\\');
        }
      } else {
        valid = append_codepoint(&delimiter, character);
        lexer->advance(lexer, false);
      }
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
      is_token_delimiter(scanner, lexer)
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
      if (character == '#' || is_token_delimiter(scanner, lexer)) {
        nested_delimiter_strips_tabs = false;
      } else {
        collecting_nested_delimiter = true;
        nested_delimiter_start = delimiter.length;
        nested_delimiter_group_depth = groups.length;
      }
    }

    struct DelimiterCaseFrame *operator_case =
      cases.length == 0 ? NULL : &cases.data[cases.length - 1];
    bool in_case_pattern = operator_case !=
      NULL &&
      operator_case->group_depth ==
      command_group_depth &&
      operator_case->state == DELIMITER_CASE_EXPECT_PATTERN;
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
      is_token_delimiter(scanner, lexer)
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

    if (groups.length == 0 && is_token_delimiter(scanner, lexer)) {
      break;
    }

    if (!has_word_content && groups.length == 0 && character == '#') {
      valid = false;
      break;
    }

    if (character == '\'') {
      if (command_group_depth > 0 && !at_nested_delimiter_base) {
        track_delimiter_word_character(command_word, character);
      }
      has_word_content = true;
      quoted = true;
      if (collecting_nested_delimiter) {
        nested_delimiter_quoted = true;
      }
      quote = DELIMITER_SINGLE_QUOTED;
      lexer->advance(lexer, false);
      continue;
    }

    if (character == '"') {
      if (command_group_depth > 0 && !at_nested_delimiter_base) {
        track_delimiter_word_character(command_word, character);
      }
      has_word_content = true;
      quoted = true;
      if (collecting_nested_delimiter) {
        nested_delimiter_quoted = true;
      }
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
        } else if (result == DELIMITER_BACKSLASH_LEADING_CONTINUATION) {
          if (valid_symbols[LINE_CONTINUATION]) {
            lexer->mark_end(lexer);
            lexer->result_symbol = LINE_CONTINUATION;
            return true;
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
        if (at_delimiter_source_start && valid_symbols[LINE_CONTINUATION]) {
          lexer->mark_end(lexer);
          lexer->result_symbol = LINE_CONTINUATION;
          return true;
        }
      } else {
        has_word_content = true;
        quoted = true;
        if (command_group_depth > 0 && !at_nested_delimiter_base) {
          track_delimiter_word_character(command_word, character);
        }
        if (collecting_nested_delimiter) {
          nested_delimiter_quoted = true;
        }
        valid = append_codepoint(&delimiter, lexer->lookahead);
        lexer->advance(lexer, false);
      }
      continue;
    }

    if (character == '`') {
      if (command_group_depth > 0 && !at_nested_delimiter_base) {
        track_delimiter_word_character(command_word, character);
      }
      has_word_content = true;
      valid = append_byte(&delimiter, '`');
      if (!valid) {
        continue;
      }
      lexer->advance(lexer, false);

      if (
        groups.length >
        0 &&
        groups.data[groups.length - 1].kind == DELIMITER_GROUP_BACKQUOTE
      ) {
        pop_delimiter_group(&groups, &cases, &quote);
        delimiter_backquote_depth -= 1;
        continue;
      }

      // At group depth zero an open backquote already ended the delimiter word
      // through is_token_delimiter, so this backquote can only open a group.
      valid = delimiter_backquote_depth <
        SIZE_MAX &&
        push_delimiter_group(&groups, '`', DELIMITER_GROUP_BACKQUOTE, quote);
      if (valid) {
        delimiter_backquote_depth += 1;
        quote = DELIMITER_UNQUOTED;
      }
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
        quoted = true;
        if (collecting_nested_delimiter) {
          nested_delimiter_quoted = true;
        }
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
      struct DelimiterCaseFrame *active_case =
        cases.length == 0 ? NULL : &cases.data[cases.length - 1];
      if (
        character ==
        ')' &&
        active_case !=
        NULL &&
        active_case->group_depth ==
        groups.length &&
        active_case->state == DELIMITER_CASE_EXPECT_PATTERN
      ) {
        valid = append_codepoint(&delimiter, character);
        active_case->state = DELIMITER_CASE_BODY;
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
      struct DelimiterCaseFrame *active_case =
        cases.length == 0 ? NULL : &cases.data[cases.length - 1];
      if (
        active_case !=
        NULL &&
        active_case->group_depth ==
        groups.length &&
        active_case->state == DELIMITER_CASE_EXPECT_PATTERN
      ) {
        has_word_content = true;
        valid = append_codepoint(&delimiter, character);
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
      struct DelimiterCaseFrame *active_case =
        cases.length == 0 ? NULL : &cases.data[cases.length - 1];
      if (
        character ==
        ';' &&
        active_case !=
        NULL &&
        active_case->group_depth ==
        active_command_depth &&
        active_case->state ==
        DELIMITER_CASE_BODY &&
        (lexer->lookahead == ';' || lexer->lookahead == '&')
      ) {
        active_case->state = DELIMITER_CASE_EXPECT_PATTERN;
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
          !(active_case !=
            NULL &&
            active_case->group_depth ==
            active_command_depth &&
            active_case->state == DELIMITER_CASE_EXPECT_PATTERN))
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

  if (
    !valid ||
    !has_word_content ||
    quote !=
    DELIMITER_UNQUOTED ||
    groups.length !=
    0 ||
    delimiter_backquote_depth !=
    scanner->backquote_depth ||
    expecting_nested_delimiter ||
    collecting_nested_delimiter ||
    nested_document_count != 0
  ) {
    ts_free(delimiter.data);
    ts_free(groups.data);
    ts_free(cases.data);
    clear_document_array(&nested_documents, &nested_document_count);
    return false;
  }

  struct HereDocument document = {
    .delimiter = (uint8_t *)delimiter.data,
    .delimiter_length = delimiter.length,
    .quoted = quoted,
    .strip_tabs = scanner->delimiter_strips_tabs,
  };
  if (!append_captured_document(scanner, document, lexer->get_column(lexer))) {
    clear_document(&document);
    ts_free(groups.data);
    ts_free(cases.data);
    clear_document_array(&nested_documents, &nested_document_count);
    return false;
  }

  ts_free(groups.data);
  ts_free(cases.data);
  clear_document_array(&nested_documents, &nested_document_count);
  scanner->expecting_delimiter = false;
  scanner->delimiter_strips_tabs = false;
  lexer->result_symbol = HERE_END_BEGIN;
  return true;
}

static bool scan_here_end_commit(struct Scanner *scanner, TSLexer *lexer) {
  if (
    scanner->captured_count ==
    0 ||
    !is_token_delimiter(scanner, lexer) ||
    lexer->get_column(lexer) !=
    scanner->captured_documents[scanner->captured_count - 1].source_end_column
  ) {
    return false;
  }

  if (!move_captured_document_to_pending(scanner)) {
    return false;
  }

  lexer->mark_end(lexer);
  lexer->result_symbol = HERE_END_COMMIT;
  return true;
}

static bool read_reserved_character(
  const struct Scanner *scanner,
  TSLexer *lexer,
  int32_t character
) {
  if (lexer->lookahead != character) {
    return false;
  }

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);

  if (!skip_line_continuations(lexer)) {
    return false;
  }

  if (!is_token_delimiter(scanner, lexer)) {
    return false;
  }

  return true;
}

static bool scan_reserved_character(
  const struct Scanner *scanner,
  TSLexer *lexer,
  int32_t character,
  enum TokenType symbol
) {
  if (!read_reserved_character(scanner, lexer, character)) {
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
  size_t word_count = sizeof(RESERVED_WORDS) / sizeof(RESERVED_WORDS[0]);
  for (size_t index = 0; index < word_count; index += 1) {
    const struct ReservedWord *reserved_word = &RESERVED_WORDS[index];
    if (
      valid_symbols[reserved_word->symbol] &&
      strcmp(word, reserved_word->text) == 0
    ) {
      *symbol = (TSSymbol)reserved_word->symbol;
      return true;
    }
  }

  return false;
}

static bool reserved_word_symbol_is_valid(const bool *valid_symbols) {
  size_t word_count = sizeof(RESERVED_WORDS) / sizeof(RESERVED_WORDS[0]);
  for (size_t index = 0; index < word_count; index += 1) {
    if (valid_symbols[RESERVED_WORDS[index].symbol]) {
      return true;
    }
  }

  return false;
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

static bool is_recovery_reserved_word(const char *word) {
  return (
    strcmp(word, "then") ==
    0 ||
    strcmp(word, "elif") ==
    0 ||
    strcmp(word, "else") ==
    0 ||
    strcmp(word, "fi") ==
    0 ||
    strcmp(word, "in") ==
    0 ||
    strcmp(word, "do") ==
    0 ||
    strcmp(word, "done") ==
    0 ||
    strcmp(word, "esac") == 0
  );
}

// POSIX case-statement termination ends the open case item at a real "esac"
// even where the item body could still begin its first command, so the closer
// reaches its formal owner instead of an in-body recovery reading.
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

static bool classify_lowercase_recovery(
  const char *word,
  const bool *valid_symbols,
  TSSymbol *symbol
) {
  if (!is_recovery_reserved_word(word)) {
    return false;
  }

  if (valid_symbols[SUBSHELL_RECOVERY_BOUNDARY]) {
    *symbol = SUBSHELL_RECOVERY_BOUNDARY;
  } else if (valid_symbols[SEPARATOR_RECOVERY]) {
    *symbol = SEPARATOR_RECOVERY;
  } else if (valid_symbols[COMPOUND_COMMAND_RECOVERY_BOUNDARY]) {
    *symbol = COMPOUND_COMMAND_RECOVERY_BOUNDARY;
  } else if (valid_symbols[MISSING_COMMAND_RECOVERY_BOUNDARY]) {
    *symbol = MISSING_COMMAND_RECOVERY_BOUNDARY;
  } else {
    return false;
  }

  return true;
}

static bool scan_lowercase_dispatch(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols,
  bool allow_recovery
) {
  // Reserved word recognition applies only where a token can begin. Where a
  // number sign would be word source rather than a comment, the position
  // continues a word or assignment value, so a reserved spelling there is
  // ordinary word source and never classifies as a keyword or a recovery.
  if (valid_symbols[LITERAL_HASH]) {
    return false;
  }

  char word[6];
  lexer->mark_end(lexer);
  if (!read_reserved_word(scanner, lexer, word)) {
    return false;
  }

  TSSymbol symbol;
  if (classify_reserved_word(word, valid_symbols, &symbol)) {
    lexer->result_symbol = symbol;
    return true;
  }

  if (classify_case_item_ns_end(word, valid_symbols, &symbol)) {
    lexer->result_symbol = symbol;
    return true;
  }

  if (
    allow_recovery && classify_lowercase_recovery(word, valid_symbols, &symbol)
  ) {
    lexer->result_symbol = symbol;
    return true;
  }

  if (
    valid_symbols[INVALID_RESERVED_COMMAND_START] &&
    is_recovery_reserved_word(word)
  ) {
    lexer->result_symbol = INVALID_RESERVED_COMMAND_START;
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

  if (valid_symbols[FNAME_BEGIN] && !is_recovery_reserved_word(word)) {
    TSSymbol reserved_symbol;
    bool is_reserved =
      classify_reserved_word(word, valid_symbols, &reserved_symbol);
    if (!is_reserved) {
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
  }

  if (word[0] == '\0') {
    return false;
  }

  TSSymbol symbol;
  if (classify_reserved_word(word, valid_symbols, &symbol)) {
    lexer->result_symbol = symbol;
    return true;
  }
  if (classify_case_item_ns_end(word, valid_symbols, &symbol)) {
    lexer->result_symbol = symbol;
    return true;
  }
  if (
    valid_symbols[COMPOUND_LIST_BOUNDARY] && is_recovery_reserved_word(word)
  ) {
    lexer->result_symbol = COMPOUND_LIST_BOUNDARY;
    return true;
  }
  if (classify_lowercase_recovery(word, valid_symbols, &symbol)) {
    lexer->result_symbol = symbol;
    return true;
  }
  if (
    valid_symbols[INVALID_RESERVED_COMMAND_START] &&
    is_recovery_reserved_word(word)
  ) {
    lexer->result_symbol = INVALID_RESERVED_COMMAND_START;
    return true;
  }

  return false;
}

static bool scan_case_item_terminator(TSLexer *lexer);

static bool scan_horizontal_layout(TSLexer *lexer);

static bool
classify_comment_boundary(TSLexer *lexer, const bool *valid_symbols);

static bool classify_scanned_comment_boundary(
  TSLexer *lexer,
  const bool *valid_symbols,
  bool comment_reaches_input_end
);

/*
 * POSIX recognizes a right brace as the Rbrace reserved word only when it is
 * the whole token, so a closer reading needs a token delimiter after the
 * brace across line continuations. Consumes lookahead past the brace.
 */
static bool
right_brace_is_delimited(const struct Scanner *scanner, TSLexer *lexer) {
  lexer->advance(lexer, false);
  if (!skip_line_continuations(lexer)) {
    return false;
  }
  return is_token_delimiter(scanner, lexer);
}

/*
 * After a layout scan stopped at a backslash that does not begin a
 * continuation pair, with that first backslash consumed, reports whether the
 * escape run begins word source of another command. Inside backquotes the
 * run that spells the enclosing substitution's closer is a closer instead.
 * Consumes the remaining backslashes of the run.
 */
static bool
escape_run_begins_word(const struct Scanner *scanner, TSLexer *lexer) {
  size_t escape_count = 1;
  while (lexer->lookahead == '\\') {
    if (escape_count == SIZE_MAX) {
      return false;
    }
    escape_count += 1;
    lexer->advance(lexer, false);
  }

  return !(
    lexer->lookahead ==
    '`' &&
    scanner->backquote_depth >
    0 &&
    classify_backquote_tick_prefix(
      scanner->backquote_depth,
      escape_count,
      false,
      true
    ) == BACKQUOTE_TICK_PREFIX_END
  );
}

static bool
classify_compound_list_boundary(const struct Scanner *scanner, TSLexer *lexer) {
  int32_t character = lexer->lookahead;
  if (character == ')' || is_active_backquote_boundary(scanner, character)) {
    lexer->result_symbol = COMPOUND_LIST_BOUNDARY;
    return true;
  }

  if (character == '}') {
    if (!right_brace_is_delimited(scanner, lexer)) {
      return false;
    }
    lexer->result_symbol = COMPOUND_LIST_BOUNDARY;
    return true;
  }

  if (character == ';') {
    if (!scan_case_item_terminator(lexer)) {
      return false;
    }
    lexer->result_symbol = COMPOUND_LIST_BOUNDARY;
    return true;
  }

  if (!is_lowercase_letter(character)) {
    return false;
  }

  char word[6];
  if (!read_reserved_word(scanner, lexer, word)) {
    return false;
  }

  if (is_recovery_reserved_word(word)) {
    lexer->result_symbol = COMPOUND_LIST_BOUNDARY;
    return true;
  }

  return false;
}

static bool
scan_compound_list_boundary(const struct Scanner *scanner, TSLexer *lexer) {
  lexer->mark_end(lexer);

  bool crossed_layout = is_horizontal_blank(lexer->lookahead);
  if (crossed_layout && !scan_horizontal_layout(lexer)) {
    return false;
  }

  if (!crossed_layout && lexer_at_eof(lexer)) {
    lexer->result_symbol = COMPOUND_LIST_BOUNDARY;
    return true;
  }

  return classify_compound_list_boundary(scanner, lexer);
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

static bool scan_direct_recovery_boundary(
  const struct Scanner *scanner,
  TSLexer *lexer,
  enum TokenType symbol,
  const bool *valid_symbols
) {
  lexer->mark_end(lexer);

  /*
   * These recoveries synchronize from word positions, where a right brace is
   * ordinary word source rather than a closer, so they never stop at one.
   * A raw newline or comment marks a boundary only where no rule accepts the
   * newline or comment itself; anywhere a linebreak or trailing comment may
   * follow, that source must remain available to the grammar.
   */
  bool is_boundary = lexer_at_eof(lexer) ||
    lexer->lookahead ==
    ')' ||
    is_active_backquote_boundary(scanner, lexer->lookahead);
  if (symbol == HEADER_RECOVERY_BOUNDARY) {
    is_boundary = is_boundary ||
      lexer->lookahead ==
      '\n' ||
      lexer->lookahead ==
      '#' ||
      lexer->lookahead ==
      ';' ||
      lexer->lookahead == '&';
  }
  if (symbol == DIRECT_RECOVERY_BOUNDARY && valid_symbols[PATTERN_END]) {
    // Inside an unclosed pattern a raw newline or comment cannot follow, so
    // the item recovers there; after the pattern's closer they stay layout.
    // A number sign the pattern word can still absorb is word source, not a
    // comment, so it never marks the boundary.
    is_boundary = is_boundary ||
      lexer->lookahead ==
      '\n' ||
      (lexer->lookahead == '#' && !valid_symbols[LITERAL_HASH]);
  }

  if (!is_boundary) {
    return false;
  }

  lexer->result_symbol = (TSSymbol)symbol;
  return true;
}

static const struct ReservedWord CLOSING_WORDS[] = {
  {"then", THEN_KEYWORD},
  {"elif", ELIF_KEYWORD},
  {"else", ELSE_KEYWORD},
  {"fi", FI_KEYWORD},
  {"do", DO_KEYWORD},
  {"done", DONE_KEYWORD},
  {"esac", ESAC_KEYWORD},
};

// A reserved word ends the enclosing term only where the grammar can accept
// it as a closer; elsewhere it is an invalid command that list recovery owns.
static bool
closing_reserved_word_ends_term(const char *word, const bool *valid_symbols) {
  for (
    size_t index = 0; index < sizeof(CLOSING_WORDS) / sizeof(CLOSING_WORDS[0]);
    index += 1
  ) {
    if (strcmp(word, CLOSING_WORDS[index].text) == 0) {
      // The word may also close an enclosing construct through a recovery
      // cascade; any pending final closer marks that possibility.
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

/*
 * Classifies the next logical line of an active here-document body without
 * committing any token extent. The delimiter comparison of
 * scan_here_document_end_line runs in parallel with the line classification
 * the shell-side continuation probes need, because the lexer cannot rewind
 * to reclassify a line once the comparison has consumed it. The lexer is
 * only advanced, never marked, so callers read ahead past their own extent
 * and stand at the following line start when the classified line is layout.
 */
static enum HereDocumentLineKind probe_here_document_line(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const struct HereDocument *document,
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

    if (!document->quoted && character == '\\') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '\n') {
        lexer->advance(lexer, false);
        continue;
      }
    } else {
      lexer->advance(lexer, false);
    }
    at_logical_line_start = false;

    if (matches) {
      uint8_t bytes[4];
      size_t length;
      if (
        !encode_utf8_scalar(character, bytes, &length) ||
        length >
        document->delimiter_length -
        delimiter_offset ||
        memcmp(document->delimiter + delimiter_offset, bytes, length) != 0
      ) {
        matches = false;
      } else {
        delimiter_offset += length;
      }
    }

    if (!has_content && !is_horizontal_blank(character)) {
      has_content = true;
      start->first_character = character;
      in_word = is_lowercase_letter(character);
      start->first_word_is_reserved_candidate = in_word;
      awaiting_delimiter = !in_word;
      if (in_word) {
        start->first_word[word_length] = (char)character;
        word_length += 1;
      }
    } else if (in_word) {
      if (is_lowercase_letter(character)) {
        if (word_length < sizeof(start->first_word) - 1) {
          start->first_word[word_length] = (char)character;
          word_length += 1;
        } else {
          start->first_word_is_reserved_candidate = false;
        }
      } else {
        in_word = false;
        start->first_is_delimited =
          is_token_delimiter_character(scanner, character);
      }
    } else if (awaiting_delimiter) {
      awaiting_delimiter = false;
      start->first_is_delimited =
        is_token_delimiter_character(scanner, character);
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

// Decides whether a probed content line begins another command of the
// enclosing term, mirroring the decision finish_term_continuation makes
// from a live lookahead position.
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
    // The escape begins word source of another command; inside backquotes
    // the run may instead spell the enclosing substitution's closer, which
    // the probe cannot count, so the line stays with the closer side there.
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

/*
 * At a raw newline, reports whether the following line ends the innermost
 * active here-document. The lookahead is consumed past the caller's marked
 * extent, so the caller must return directly after a negative answer too.
 */
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
           &start
         ) == HERE_DOCUMENT_LINE_DELIMITER;
}

// From a here-document body line start, reports whether a further command of
// the enclosing term begins before the document's delimiter line.
static bool probe_here_document_continuation(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  const struct HereDocument *document = &scanner->active_documents[0];
  while (true) {
    struct HereDocumentLineStart start;
    enum HereDocumentLineKind kind =
      probe_here_document_line(scanner, lexer, document, &start);
    if (kind == HERE_DOCUMENT_LINE_LAYOUT) {
      continue;
    }
    return kind ==
      HERE_DOCUMENT_LINE_CONTENT &&
      here_document_line_continues_term(scanner, &start, valid_symbols);
  }
}

static bool is_function_body_reserved_word(const char *word) {
  return strcmp(word, "if") ==
    0 ||
    strcmp(word, "for") ==
    0 ||
    strcmp(word, "case") ==
    0 ||
    strcmp(word, "while") ==
    0 ||
    strcmp(word, "until") == 0;
}

static bool finish_function_body_boundary(
  TSLexer *lexer,
  const bool *valid_symbols,
  bool has_function_body
) {
  enum TokenType symbol = has_function_body
    ? FUNCTION_BODY_CONTINUATION_BOUNDARY
    : FUNCTION_BODY_RECOVERY_BOUNDARY;
  if (!valid_symbols[symbol]) {
    return false;
  }

  lexer->result_symbol = (TSSymbol)symbol;
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
    while (is_horizontal_blank(lexer->lookahead)) {
      lexer->advance(lexer, false);
    }

    if (lexer->lookahead == '\\') {
      if (!skip_line_continuations(lexer)) {
        has_valid_layout = false;
        break;
      }
      continue;
    }

    if (lexer->lookahead == '#') {
      advance_to_line_end(lexer);
    }

    if (lexer->lookahead != '\n') {
      break;
    }
    lexer->advance(lexer, false);

    if (has_startable_pending_document(scanner)) {
      // The newline activates the startable pending here-documents, so the
      // following lines are their bodies. The shell source that may hold the
      // function body resumes only after the last document's delimiter line.
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
      // Function-body layout crosses body lines only up to the delimiter
      // line of the active here-document.
      const struct HereDocument *document = &scanner->active_documents[0];
      while (true) {
        struct HereDocumentLineStart start;
        enum HereDocumentLineKind kind =
          probe_here_document_line(scanner, lexer, document, &start);
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

static bool scan_recovery_boundary(
  const struct Scanner *scanner,
  TSLexer *lexer,
  enum TokenType symbol,
  const bool *valid_symbols
) {
  lexer->mark_end(lexer);

  bool is_compound_recovery = symbol ==
    COMPOUND_COMMAND_RECOVERY_BOUNDARY ||
    symbol ==
    SUBSHELL_RECOVERY_BOUNDARY ||
    symbol == CASE_ITEMS_RECOVERY_BOUNDARY;

  if (
    (is_compound_recovery || symbol == SEPARATOR_RECOVERY) &&
    lexer->lookahead == ';'
  ) {
    if (!scan_case_item_terminator(lexer)) {
      return false;
    }
    lexer->result_symbol = (TSSymbol)symbol;
    return true;
  }

  /*
   * A right brace closes a group only where POSIX recognizes it as a reserved
   * word. The recoveries below synchronize at command positions, where that
   * holds; every other recovery reaches a right brace only from within a
   * command, where the brace is ordinary word source that recovery must not
   * consume.
   */
  bool is_direct_boundary = lexer->lookahead ==
    '}' &&
    (symbol ==
      COMPOUND_COMMAND_RECOVERY_BOUNDARY ||
      symbol ==
      SUBSHELL_RECOVERY_BOUNDARY ||
      symbol == MISSING_COMMAND_RECOVERY_BOUNDARY);
  if (symbol == SEPARATOR_RECOVERY) {
    // Inside an open command substitution every right parenthesis reaches a
    // real closer, so the stray-parenthesis reading exists only outside one;
    // refusing it here keeps the closer's version from forking at each
    // nesting level.
    is_direct_boundary = is_direct_boundary ||
      (lexer->lookahead ==
        ')' &&
        !valid_symbols[COMMAND_SUBSTITUTION_CLOSE] &&
        !valid_symbols[SUBSHELL_CLOSE] &&
        scanner->substitution_depth == 0) ||
      (is_active_backquote_boundary(scanner, lexer->lookahead) &&
        !valid_symbols[BACKQUOTE_END]);
  }
  if (is_compound_recovery) {
    is_direct_boundary =
      (is_direct_boundary ||
        lexer_at_eof(lexer) ||
        ((symbol ==
           COMPOUND_COMMAND_RECOVERY_BOUNDARY ||
           symbol == CASE_ITEMS_RECOVERY_BOUNDARY) &&
          lexer->lookahead == ')') ||
        is_active_backquote_boundary(scanner, lexer->lookahead));
  } else if (symbol != SEPARATOR_RECOVERY) {
    is_direct_boundary =
      (is_direct_boundary ||
        lexer_at_eof(lexer) ||
        (lexer->lookahead ==
          '\n' &&
          (symbol ==
            REDIRECTION_TARGET_RECOVERY ||
            symbol == MISSING_COMMAND_RECOVERY_BOUNDARY)) ||
        (lexer->lookahead ==
          '|' &&
          symbol == MISSING_COMMAND_RECOVERY_BOUNDARY) ||
        // A redirection or pipe operator can never begin the missing target
        // word, so the following redirection keeps its own operator.
        (symbol ==
          REDIRECTION_TARGET_RECOVERY &&
          (lexer->lookahead ==
            '<' ||
            lexer->lookahead ==
            '>' ||
            lexer->lookahead == '|')) ||
        lexer->lookahead ==
        ')' ||
        lexer->lookahead ==
        ';' ||
        lexer->lookahead ==
        '&' ||
        is_active_backquote_boundary(scanner, lexer->lookahead) ||
        lexer->lookahead == '#');
  }

  if (is_direct_boundary) {
    lexer->result_symbol = (TSSymbol)symbol;
    return true;
  }

  if (symbol == REDIRECTION_TARGET_RECOVERY) {
    return false;
  }

  if (!is_lowercase_letter(lexer->lookahead)) {
    return false;
  }

  char word[6];
  if (!read_reserved_word(scanner, lexer, word)) {
    return false;
  }

  if (symbol == CASE_ITEMS_RECOVERY_BOUNDARY) {
    if (valid_symbols[ESAC_KEYWORD] && strcmp(word, "esac") == 0) {
      lexer->result_symbol = ESAC_KEYWORD;
      return true;
    }

    if (!scan_horizontal_layout(lexer)) {
      return false;
    }
    if (lexer->lookahead == ')' || lexer->lookahead == '|') {
      return false;
    }
  }

  TSSymbol reserved_symbol;
  if (classify_reserved_word(word, valid_symbols, &reserved_symbol)) {
    lexer->result_symbol = reserved_symbol;
    return true;
  }

  if (symbol == CASE_ITEMS_RECOVERY_BOUNDARY) {
    return false;
  }

  if (!is_recovery_reserved_word(word)) {
    return false;
  }

  lexer->result_symbol = (TSSymbol)symbol;
  return true;
}

static bool scan_parameter_expansion_recovery_boundary(
  const struct Scanner *scanner,
  TSLexer *lexer,
  enum TokenType symbol,
  const bool *valid_symbols
) {
  lexer->mark_end(lexer);
  int32_t character = lexer->lookahead;

  /*
   * POSIX reads a braced expansion up to the matching right brace, so behind
   * an operator every character is ordinary word content and only real
   * terminators end the expansion there: the end of input, the closing
   * backquote of an enclosing substitution, and inside double quotes an
   * unterminated inner double quote. The parameter, operator, and
   * pre-operator positions additionally synchronize at layout and control
   * characters, which can never continue a valid expansion.
   */
  bool at_word_position = symbol ==
    INPUT_END_RECOVERY ||
    symbol == DOUBLE_QUOTED_PARAMETER_TAIL_RECOVERY_BOUNDARY;
  bool is_outer_boundary =
    lexer_at_eof(lexer) || is_active_backquote_boundary(scanner, character);
  if (symbol == PARAMETER_TAIL_RECOVERY_BOUNDARY) {
    // Directly after the parameter only the closing brace, an operator
    // spelling, a continuation pair, the quoted-region probe below, or —
    // while the racing string-length reading still expects its parameter —
    // a parameter spelling may continue the expansion, so any other
    // character proves it invalid and the recovery synchronizes without
    // blanket error recovery, whose artifacts an edit could reuse.
    bool length_parameter_may_follow =
      valid_symbols[BRACED_POSITIONAL_PARAMETER_START];
    is_outer_boundary = is_outer_boundary ||
      (character !=
        '}' &&
        character !=
        ':' &&
        character !=
        '-' &&
        character !=
        '=' &&
        character !=
        '?' &&
        character !=
        '+' &&
        character !=
        '%' &&
        character !=
        '#' &&
        character !=
        '\\' &&
        character !=
        '"' &&
        !(length_parameter_may_follow &&
          (is_name_character(character) ||
            is_special_parameter_character(character))));
  } else if (!at_word_position) {
    is_outer_boundary = is_outer_boundary ||
      character ==
      '\n' ||
      character ==
      ' ' ||
      character ==
      '\t' ||
      character ==
      ')' ||
      character ==
      ';' ||
      character == '&';
  }
  bool includes_closing_brace = symbol ==
    PARAMETER_MISSING_RECOVERY_BOUNDARY ||
    symbol == PARAMETER_OPERATOR_RECOVERY_BOUNDARY;
  // An expansion tail operator that can never begin a parameter proves the
  // parameter position empty, so the zero-width recovery holds it and the
  // real operator, word, and closer stay with the ordinary tail.
  bool operator_leads_missing_parameter = symbol ==
    PARAMETER_MISSING_RECOVERY_BOUNDARY &&
    (character ==
      ':' ||
      character ==
      '%' ||
      character ==
      '+' ||
      character == '=');
  if (
    is_outer_boundary ||
    operator_leads_missing_parameter ||
    (includes_closing_brace && character == '}')
  ) {
    lexer->result_symbol = (TSSymbol)symbol;
    return true;
  }

  /*
   * An inner double quote opens an ordinary quoted string, so it ends the
   * expansion for recovery only when that quoted string never terminates.
   */
  bool includes_double_quote = symbol != INPUT_END_RECOVERY;
  if (includes_double_quote && character == '"') {
    lexer->advance(lexer, false);
    while (!lexer_at_eof(lexer) && lexer->lookahead != '"') {
      if (lexer->lookahead == '\\') {
        lexer->advance(lexer, false);
        if (lexer_at_eof(lexer)) {
          break;
        }
      }
      lexer->advance(lexer, false);
    }
    if (lexer_at_eof(lexer)) {
      lexer->result_symbol = (TSSymbol)symbol;
      return true;
    }
  }
  return false;
}

static bool scan_horizontal_blanks(TSLexer *lexer) {
  bool found = false;
  while (is_horizontal_blank(lexer->lookahead)) {
    found = true;
    lexer->advance(lexer, false);
  }
  return found;
}

// Skips horizontal blanks and line-continuation pairs. Returns false only
// when a backslash is not part of a continuation pair.
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

// The command-continuation markers settle which hierarchy level owns the
// layout before an operator: a single pipe continues the pipe sequence
// without reducing it, while a double operator reduces the pipe sequence and
// continues the and-or. Both are zero-width and precede the layout run.
static bool
scan_command_continuation_operator(TSLexer *lexer, const bool *valid_symbols) {
  if (lexer->lookahead == '|') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '|') {
      if (!valid_symbols[AND_OR_CONTINUATION]) {
        return false;
      }
      lexer->result_symbol = AND_OR_CONTINUATION;
      return true;
    }
    if (!valid_symbols[PIPE_CONTINUATION]) {
      return false;
    }
    lexer->result_symbol = PIPE_CONTINUATION;
    return true;
  }

  if (lexer->lookahead != '&') {
    return false;
  }

  lexer->advance(lexer, false);
  if (lexer->lookahead != '&' || !valid_symbols[AND_OR_CONTINUATION]) {
    return false;
  }

  lexer->result_symbol = AND_OR_CONTINUATION;
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
  const struct Scanner *scanner,
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

// Classifies the boundary after a completed command-level element in one
// forward scan, so every hierarchy level reduces deterministically:
//
//   WORD_SEPARATOR_BEGIN   another word, assignment, or redirection follows
//                          inside the same simple command
//   PIPE_CONTINUATION      a single pipe continues the pipe sequence
//   AND_OR_CONTINUATION    a double operator continues the and-or
//   LIST_CONTINUATION      a separator operator continues the list on the
//                          same logical line
//   TERM_CONTINUATION      a separator and layout run is followed by another
//                          command in the same term or complete_commands
//
// Every marker precedes the layout run it classifies and is zero width,
// except that a marker settled behind a leading blank run keeps that
// completed run, so the lines after the marker own their own layout.
// Backslash-newline pairs are ordinary units of the run: the scan crosses
// them like blanks and the classified owner's layout holds them, so a pair
// alone never begins a formal element that the logical source lacks.
static bool scan_element_boundary_core(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols,
  bool leading_backslash_consumed
) {
  bool crossed_layout = false;
  bool crossed_pairs = false;
  bool blank_mark_committed = false;
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
      // A blank run before a continuation pair is committed as the token
      // range now: whichever marker this scan emits stays a hidden token,
      // and the pairs after it lex as individual line continuations.
      if (crossed_layout && !crossed_pairs && !blank_mark_committed) {
        lexer->mark_end(lexer);
        blank_mark_committed = true;
      }
      lexer->advance(lexer, false);
    }
    if (lexer->lookahead != '\n') {
      // A backslash that does not begin a continuation pair starts an
      // escaped character, which is ordinary word source of the next
      // element. Inside backquotes the prefix machinery owns escape runs,
      // and an escape run that closes the active substitution is a closer,
      // not an element.
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
        // The escape arithmetic alone decides whether this run closes the
        // enclosing substitution: the current token validities differ
        // between a fresh parse and an incremental parse resuming at a
        // reused node, and the classification must not.
        if (!escape_run_begins_word(scanner, lexer)) {
          return false;
        }
        lexer->result_symbol = WORD_SEPARATOR_BEGIN;
        return true;
      }
      return false;
    }
    if (!crossed_layout && valid_symbols[LINE_CONTINUATION]) {
      // A valid line continuation directly after the element means a
      // completed structure owns a trailing run here, so the pair joins
      // that run instead of opening a classified boundary.
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
      if (valid_symbols[AND_OR_CONTINUATION]) {
        lexer->result_symbol = AND_OR_CONTINUATION;
        return true;
      }
      if (valid_symbols[HEADER_RECOVERY_BOUNDARY]) {
        lexer->result_symbol = HEADER_RECOVERY_BOUNDARY;
        return true;
      }
      return false;
    }
    return scan_separator_operator_continuation(scanner, lexer, valid_symbols);
  }

  if (character == ';') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == ';' || lexer->lookahead == '&') {
      if (valid_symbols[CASE_ITEM_END]) {
        lexer->result_symbol = CASE_ITEM_END;
        return true;
      }
      // A separator or stray-terminator recovery begins zero-width ahead of
      // the terminator; a crossed blank run is shifted first, and a purely
      // continuation-led run follows the marker instead.
      if (
        (!crossed_layout || (crossed_pairs && !blank_mark_committed)) &&
        valid_symbols[INVALID_CASE_TERMINATOR_START]
      ) {
        lexer->result_symbol = INVALID_CASE_TERMINATOR_START;
        return true;
      }
      if (
        (!crossed_layout || (crossed_pairs && !blank_mark_committed)) &&
        valid_symbols[SEPARATOR_RECOVERY]
      ) {
        lexer->result_symbol = SEPARATOR_RECOVERY;
        return true;
      }
      if (valid_symbols[HEADER_RECOVERY_BOUNDARY]) {
        lexer->result_symbol = HEADER_RECOVERY_BOUNDARY;
        return true;
      }
      return false;
    }
    return scan_separator_operator_continuation(scanner, lexer, valid_symbols);
  }

  if (character == '\n') {
    // The blank run before the newline is the extent of whichever symbol
    // wins below, so the continuation probe may read past the newline.
    if (crossed_layout && !crossed_pairs && !blank_mark_committed) {
      lexer->mark_end(lexer);
      blank_mark_committed = true;
    }
    if (crossed_layout) {
      // A layout run that reaches another command in the same sequence is a
      // separating newline run: the continuation marker keeps the leading
      // blank run, and the run's own elements own the crossed pairs and
      // every following line. Any other run leaves the blank to the
      // pre-newline reading of the trailing and here-document structures.
      if (
        valid_symbols[TERM_CONTINUATION] &&
        finish_term_continuation(scanner, lexer, valid_symbols)
      ) {
        lexer->result_symbol = TERM_CONTINUATION;
        return true;
      }
      if (blank_mark_committed && valid_symbols[PRE_NEWLINE_BLANK]) {
        if (here_document_delimiter_line_follows(scanner, lexer)) {
          // The blank run may not bridge into the delimiter line; the
          // boundary that closes the innermost open structure owns the
          // newline position instead.
          return false;
        }
        lexer->result_symbol = PRE_NEWLINE_BLANK;
        return true;
      }
      if (
        crossed_pairs &&
        !blank_mark_committed &&
        valid_symbols[TRAILING_CONTINUATION_BEGIN] &&
        !valid_symbols[LINE_CONTINUATION]
      ) {
        // Only layout continues the input and no completed element offers
        // the pairs a run of its own, so the continuation-led trailing run
        // begins at this boundary and owns them.
        lexer->result_symbol = TRAILING_CONTINUATION_BEGIN;
        return true;
      }
      if (valid_symbols[HEADER_RECOVERY_BOUNDARY]) {
        return scan_direct_recovery_boundary(
          scanner,
          lexer,
          HEADER_RECOVERY_BOUNDARY,
          valid_symbols
        );
      }
      return false;
    }
    if (valid_symbols[HEADER_RECOVERY_BOUNDARY]) {
      return scan_direct_recovery_boundary(
        scanner,
        lexer,
        HEADER_RECOVERY_BOUNDARY,
        valid_symbols
      );
    }
    if (
      !valid_symbols[SEPARATOR_NEWLINE] ||
      has_startable_pending_document(scanner)
    ) {
      return false;
    }
    // A newline followed by another command begins a separating newline
    // run. The marker stays zero width before the newline it classifies,
    // so the continuation probe reads past the marked end.
    lexer->mark_end(lexer);
    lexer->advance(lexer, false);
    if (finish_term_continuation(scanner, lexer, valid_symbols)) {
      lexer->result_symbol = SEPARATOR_NEWLINE;
      return true;
    }
    return false;
  }

  if (character == '#') {
    bool term_continuation_is_scannable = valid_symbols[TERM_CONTINUATION] &&
      !has_startable_pending_document(scanner);
    if (!term_continuation_is_scannable) {
      if (valid_symbols[HEADER_RECOVERY_BOUNDARY]) {
        return scan_direct_recovery_boundary(
          scanner,
          lexer,
          HEADER_RECOVERY_BOUNDARY,
          valid_symbols
        );
      }
      return classify_comment_boundary(lexer, valid_symbols);
    }
    // One probe serves both decisions: the comment's own line settles the
    // trailing classification, and the run after its newline settles the
    // term continuation.
    advance_to_line_end(lexer);
    bool comment_reaches_input_end = lexer_at_eof(lexer);
    if (
      !comment_reaches_input_end &&
      finish_term_continuation(scanner, lexer, valid_symbols)
    ) {
      lexer->result_symbol = TERM_CONTINUATION;
      return true;
    }
    if (valid_symbols[HEADER_RECOVERY_BOUNDARY]) {
      return scan_direct_recovery_boundary(
        scanner,
        lexer,
        HEADER_RECOVERY_BOUNDARY,
        valid_symbols
      );
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
      scanner,
      lexer,
      valid_symbols,
      !crossed_pairs && !blank_mark_committed
    );
  }

  bool at_input_end = lexer_at_eof(lexer);
  if (
    character ==
    ')' &&
    crossed_pairs &&
    !blank_mark_committed &&
    scanner->substitution_depth ==
    0 &&
    !valid_symbols[COMMAND_SUBSTITUTION_CLOSE] &&
    !valid_symbols[SUBSHELL_CLOSE] &&
    !valid_symbols[PATTERN_END] &&
    valid_symbols[SEPARATOR_RECOVERY]
  ) {
    // The purely continuation-led run reaches a stray right parenthesis, so
    // the list recovery begins zero-width here and its own layout owns the
    // run.
    lexer->result_symbol = SEPARATOR_RECOVERY;
    return true;
  }

  if (classify_shell_boundary(scanner, lexer, valid_symbols, crossed_layout)) {
    return true;
  }

  if (
    crossed_pairs &&
    !blank_mark_committed &&
    valid_symbols[TRAILING_CONTINUATION_BEGIN] &&
    !valid_symbols[LINE_CONTINUATION] &&
    (at_input_end ||
      character ==
      ')' ||
      is_active_backquote_boundary(scanner, character))
  ) {
    // The crossed continuation pairs reach the end of input or the closer
    // of the enclosing structure, and no completed element offers them a
    // run of its own, so the continuation-led trailing run begins here.
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

/*
 * Reads the "{location}" body after a consumed left brace and reports whether
 * a redirection operator follows: at least one plain character, a closing
 * right brace, then '<' or '>' across line continuations. Quotes, escapes,
 * and token delimiters inside the braces disprove the location. When
 * commit_location is set the token end tracks the final right brace.
 */
static bool scan_io_location_body(
  const struct Scanner *scanner,
  TSLexer *lexer,
  bool commit_location
) {
  unsigned character_count = 0;
  bool ends_with_right_brace = false;

  while (
    lexer->lookahead !=
    '<' &&
    lexer->lookahead !=
    '>' &&
    lexer->lookahead != '\\'
  ) {
    if (is_token_delimiter(scanner, lexer)) {
      return false;
    }

    if (
      lexer->lookahead ==
      '\'' ||
      lexer->lookahead ==
      '"' ||
      lexer->lookahead == '`'
    ) {
      return false;
    }

    ends_with_right_brace = lexer->lookahead == '}';
    character_count += 1;
    lexer->advance(lexer, false);
    if (commit_location && ends_with_right_brace) {
      lexer->mark_end(lexer);
    }
  }

  if (character_count < 2 || !ends_with_right_brace) {
    return false;
  }

  if (!skip_line_continuations(lexer)) {
    return false;
  }

  return lexer->lookahead == '<' || lexer->lookahead == '>';
}

// Decides which kind of simple-command element follows a word separator: an
// assignment word, a redirection, or an ordinary word. The decision repeats
// the adjacency checks that commit those elements, so the parser can pick
// the prefix continuation or the command word without racing.
static bool classify_word_separator(
  const struct Scanner *scanner,
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
    if (skip_line_continuations(lexer)) {
      assignment_ahead = lexer->lookahead == '=';
    }
  } else if (character == '{' && valid_symbols[REDIRECT_SEPARATOR_BEGIN]) {
    lexer->advance(lexer, false);
    redirect_ahead = scan_io_location_body(scanner, lexer, false);
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

// After a consumed separator operator, decides whether the enclosing
// sequence continues with another command. Where a list is open the operator
// belongs to the list or to the complete_command terminator, and only the
// same logical line continues it; a term also continues across the newline
// run that its separator owns.
static bool scan_separator_operator_continuation(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  if (!scan_horizontal_layout(lexer)) {
    // The backslash begins an escaped character: word source of another
    // command on the same logical line, unless the run closes the enclosing
    // backquote substitution.
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

  /*
   * Every decision input is read before any symbol is chosen, so the token's
   * lookahead extent is a function of the source alone. An edit history that
   * resumes in a state where only a terminator or recovery symbol is valid
   * then records the same extent as a fresh parse, and a later edit
   * invalidates the same nodes in both.
   */
  int32_t character = lexer->lookahead;
  // A lone separator operator can neither begin a command nor a recovery
  // command, so the list ends and its terminator keeps its formal owner.
  bool lone_separator_ahead = false;
  if (character == ';') {
    lexer->advance(lexer, false);
    lone_separator_ahead = lexer->lookahead != ';' && lexer->lookahead != '&';
  }
  bool at_input_end = lexer_at_eof(lexer);
  bool term_continues = !lone_separator_ahead &&
    finish_term_continuation(scanner, lexer, valid_symbols);

  if (valid_symbols[LIST_CONTINUATION]) {
    // A closer belongs to the enclosing substitution where one is open;
    // elsewhere it begins a recovery command that continues the list.
    if (
      !at_input_end &&
      character !=
      '\n' &&
      character !=
      '#' &&
      !lone_separator_ahead &&
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

  if (valid_symbols[MISSING_COMMAND_RECOVERY_BOUNDARY]) {
    lexer->result_symbol = MISSING_COMMAND_RECOVERY_BOUNDARY;
    return true;
  }

  if (valid_symbols[HEADER_RECOVERY_BOUNDARY]) {
    lexer->result_symbol = HEADER_RECOVERY_BOUNDARY;
    return true;
  }

  return false;
}

// Crosses the newline, blank-line, and comment-line run that a separating
// newline_list may own, then reports whether another command begins. Pending
// startable here-documents make any newline run's extent depend on the
// document bodies, so the scan then classifies the current line only and
// leaves every run that reaches a newline or comment to the grammar's raced
// here-document variant. Inside an active here-document body every further
// line is probed against the delimiter line, which ends the continuation
// before any later command.
static bool finish_term_continuation(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  bool cross_lines = !has_startable_pending_document(scanner);

  while (true) {
    if (!scan_horizontal_layout(lexer)) {
      // The backslash begins an escaped character: word source of another
      // command, unless the run closes the enclosing backquote substitution.
      return escape_run_begins_word(scanner, lexer);
    }
    if (lexer->lookahead == '#') {
      if (!cross_lines) {
        return false;
      }
      advance_to_line_end(lexer);
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

static bool
scan_pipeline_negation(const struct Scanner *scanner, TSLexer *lexer) {
  if (lexer->lookahead != '!') {
    return false;
  }

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);

  if (!skip_line_continuations(lexer)) {
    return false;
  }

  if (!is_token_delimiter(scanner, lexer)) {
    return false;
  }

  lexer->result_symbol = PIPELINE_NEGATION;
  return true;
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

static bool scan_left_brace_or_io_location(
  const struct Scanner *scanner,
  TSLexer *lexer,
  bool left_brace_is_valid,
  bool io_location_is_valid,
  bool recovery_source_is_valid
) {
  if (lexer->lookahead != '{') {
    return false;
  }

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);

  if (!skip_line_continuations(lexer)) {
    return false;
  }

  if (is_token_delimiter(scanner, lexer)) {
    if (left_brace_is_valid) {
      lexer->result_symbol = LEFT_BRACE;
      return true;
    }

    if (recovery_source_is_valid) {
      lexer->result_symbol = INVALID_COMMAND_CHARACTER_SOURCE;
      return true;
    }

    return false;
  }

  if (!io_location_is_valid) {
    return false;
  }

  if (!scan_io_location_body(scanner, lexer, true)) {
    return false;
  }

  lexer->result_symbol = IO_LOCATION;
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

static bool
scan_missing_here_document_delimiter(struct Scanner *scanner, TSLexer *lexer) {
  if (
    !scanner->expecting_delimiter ||
    !is_missing_here_document_delimiter_boundary(scanner, lexer)
  ) {
    return false;
  }

  scanner->expecting_delimiter = false;
  scanner->delimiter_strips_tabs = false;
  lexer->mark_end(lexer);
  lexer->result_symbol = MISSING_HERE_DOCUMENT_DELIMITER;
  return true;
}

static bool scan_case_item_terminator(TSLexer *lexer) {
  if (lexer->lookahead != ';') {
    return false;
  }
  lexer->advance(lexer, false);

  return lexer->lookahead == ';' || lexer->lookahead == '&';
}

static bool classify_shell_boundary(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols,
  bool crossed_layout
) {
  int32_t character = lexer->lookahead;
  if (
    crossed_layout &&
    valid_symbols[COMPOUND_LIST_BOUNDARY] &&
    (character ==
      ')' ||
      character ==
      '}' ||
      character ==
      ';' ||
      is_active_backquote_boundary(scanner, character) ||
      is_lowercase_letter(character))
  ) {
    return classify_compound_list_boundary(scanner, lexer);
  }

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
    valid_symbols[CASE_ITEM_NS_BOUNDARY] &&
    (crossed_layout || !valid_symbols[LITERAL_HASH]) &&
    scan_case_item_ns_boundary(scanner, lexer)
  ) {
    return true;
  }

  if (
    character ==
    '#' &&
    (crossed_layout || !valid_symbols[LITERAL_HASH]) &&
    classify_comment_boundary(lexer, valid_symbols)
  ) {
    return true;
  }

  if (
    valid_symbols[REDIRECT_LIST_BEGIN] &&
    (character ==
      '<' ||
      character ==
      '>' ||
      character ==
      '{' ||
      is_decimal_digit(character))
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
  scanner->at_here_document_line_start = true;
  lexer->mark_end(lexer);
  lexer->result_symbol = HERE_DOCUMENT_LINE_END;
  return true;
}

static bool scan_comment(TSLexer *lexer) {
  if (lexer->lookahead != '#') {
    return false;
  }

  advance_to_line_end(lexer);

  lexer->mark_end(lexer);
  lexer->result_symbol = COMMENT;
  return true;
}

/*
 * Classifies the boundary before a comment. A comment that reaches the end of
 * input has no terminating newline, so only trailing layout may own it; a
 * comment a newline terminates is a comment line of the enclosing newline
 * run. Where only the comment-line boundary is valid the trailing comment
 * degrades to that reading.
 */
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

/*
 * Classifies the boundary at a comment's first character. The lookahead past
 * the committed token end reads the comment without consuming it, so the
 * caller must not rely on the lookahead position afterward.
 */
static bool
classify_comment_boundary(TSLexer *lexer, const bool *valid_symbols) {
  if (
    !valid_symbols[COMMENT_BOUNDARY] &&
    !valid_symbols[TRAILING_COMMENT_BOUNDARY]
  ) {
    return false;
  }
  bool comment_reaches_input_end = false;
  if (valid_symbols[TRAILING_COMMENT_BOUNDARY]) {
    advance_to_line_end(lexer);
    comment_reaches_input_end = lexer_at_eof(lexer);
  }
  return classify_scanned_comment_boundary(
    lexer,
    valid_symbols,
    comment_reaches_input_end
  );
}

static enum ArithmeticOperatorCategory
classify_arithmetic_operator(int32_t first, int32_t second, int32_t third);
static bool is_arithmetic_operator_start(int32_t character);
static bool is_arithmetic_operand_start(int32_t character);

static bool arithmetic_operand_boundary_is_valid(const bool *valid_symbols) {
  return (
    valid_symbols[ARITHMETIC_PLUS_OPERAND_BOUNDARY] ||
    valid_symbols[ARITHMETIC_MINUS_OPERAND_BOUNDARY] ||
    valid_symbols[ARITHMETIC_OPERAND_BOUNDARY]
  );
}

static bool finish_line_continuation(TSLexer *lexer, enum TokenType symbol) {
  if (lexer->lookahead != '\n') {
    return false;
  }

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  lexer->result_symbol = symbol;
  return true;
}

static bool scan_line_continuation_after_backslash(
  TSLexer *lexer,
  const bool *valid_symbols
) {
  if (!valid_symbols[LINE_CONTINUATION]) {
    return false;
  }

  return finish_line_continuation(lexer, LINE_CONTINUATION);
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

static bool scan_arithmetic_layout(TSLexer *lexer) {
  while (true) {
    while (
      lexer->lookahead ==
      ' ' ||
      lexer->lookahead ==
      '\t' ||
      lexer->lookahead == '\n'
    ) {
      lexer->advance(lexer, false);
    }

    if (lexer->lookahead != '\\') {
      return true;
    }
    if (!skip_line_continuations(lexer)) {
      return false;
    }
  }
}

static bool
scan_arithmetic_boundary(TSLexer *lexer, const bool *valid_symbols) {
  lexer->mark_end(lexer);
  if (!scan_arithmetic_layout(lexer)) {
    return false;
  }

  if (valid_symbols[ARITHMETIC_CLOSING_BOUNDARY] && lexer->lookahead == ')') {
    lexer->result_symbol = ARITHMETIC_CLOSING_BOUNDARY;
    return true;
  }

  if (
    arithmetic_operand_boundary_is_valid(valid_symbols) &&
    is_arithmetic_operand_start(lexer->lookahead)
  ) {
    lexer->result_symbol = valid_symbols[ARITHMETIC_PLUS_OPERAND_BOUNDARY]
      ? ARITHMETIC_PLUS_OPERAND_BOUNDARY
      : (valid_symbols[ARITHMETIC_MINUS_OPERAND_BOUNDARY]
            ? ARITHMETIC_MINUS_OPERAND_BOUNDARY
            : ARITHMETIC_OPERAND_BOUNDARY);
    return true;
  }

  /*
   * Operators are classified on the logical source, so removed newlines
   * between their characters do not change the classification.
   */
  int32_t first = lexer->lookahead;
  lexer->advance(lexer, false);
  if (!skip_line_continuations(lexer)) {
    return false;
  }
  int32_t second = lexer->lookahead;
  lexer->advance(lexer, false);
  if (!skip_line_continuations(lexer)) {
    return false;
  }
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

/*
 * POSIX gives arithmetic expansion precedence over the command substitution
 * that starts with a subshell: source beginning with "$((" is an arithmetic
 * expansion exactly when it can be parsed as one, falls back to a command
 * substitution once that parse is determined impossible, and remains an
 * incomplete arithmetic expansion when the input ends before the
 * determination. This scan settles that choice at the second left
 * parenthesis and additionally distinguishes the three arithmetic readings —
 * a structured expression, a flat run around runtime fragments, and an
 * incomplete expansion — so the parser only ever pursues one interpretation.
 * Racing those readings instead would let an edited tree keep a flat-reading
 * subtree that an incremental reparse of the restored source reuses, losing
 * the structured reading that a parse from scratch produces. Nested
 * expansions are skipped structurally rather than parsed in full, as POSIX
 * permits for this determination, so a nested body that changes token
 * boundaries (for example an unbalanced parenthesis in a case pattern or
 * here-document line) can misdirect only the fallback choice, never the
 * committed parse.
 */

enum ArithmeticValidation {
  ARITHMETIC_VALIDATION_INVALID,
  ARITHMETIC_VALIDATION_INCOMPLETE,
  ARITHMETIC_VALIDATION_VALID,
};

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
  // A ')' frame whose interior is shell command source, where case
  // statements and here-documents decide what a right parenthesis or a line
  // closes. The tentatively arithmetic "$((" interior stays outside.
  bool command_context;
};

enum EmbeddedCaseState {
  EMBEDDED_CASE_EXPECT_WORD,
  EMBEDDED_CASE_EXPECT_IN,
  EMBEDDED_CASE_PATTERN,
  EMBEDDED_CASE_BODY,
};

struct EmbeddedCaseFrame {
  size_t frame_count;
  uint8_t state;
};

struct EmbeddedSkip {
  struct EmbeddedFrame *frames;
  size_t frame_count;
  size_t frame_capacity;
  struct EmbeddedCaseFrame *cases;
  size_t case_count;
  size_t case_capacity;
  struct HereDocument *pending;
  size_t pending_count;
  size_t pending_capacity;
};

static void clear_embedded_skip(struct EmbeddedSkip *skip) {
  ts_free(skip->frames);
  ts_free(skip->cases);
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

static bool embedded_push_case(struct EmbeddedSkip *skip) {
  if (!grow_element_buffer(
        (void **)&skip->cases,
        &skip->case_capacity,
        skip->case_count,
        sizeof(struct EmbeddedCaseFrame),
        8
      )) {
    return false;
  }
  skip->cases[skip->case_count] = (struct EmbeddedCaseFrame){
    .frame_count = skip->frame_count,
    .state = EMBEDDED_CASE_EXPECT_WORD,
  };
  skip->case_count += 1;
  return true;
}

static struct EmbeddedCaseFrame *
embedded_active_case(struct EmbeddedSkip *skip) {
  if (skip->case_count == 0) {
    return NULL;
  }
  struct EmbeddedCaseFrame *frame = &skip->cases[skip->case_count - 1];
  return frame->frame_count == skip->frame_count ? frame : NULL;
}

// Any completed word satisfies the selector position of an open case
// statement, whatever source it is built from.
static void embedded_note_word(struct EmbeddedSkip *skip, bool in_command) {
  if (!in_command) {
    return;
  }
  struct EmbeddedCaseFrame *active_case = embedded_active_case(skip);
  if (active_case != NULL && active_case->state == EMBEDDED_CASE_EXPECT_WORD) {
    active_case->state = EMBEDDED_CASE_EXPECT_IN;
  }
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

/*
 * Read the delimiter word of a here-document operator inside a skipped
 * construct, producing the quote-removed spelling the body lines are matched
 * against. Nested expansion spellings stay raw source bytes: they cannot
 * change where a matching delimiter line begins.
 */
static enum ArithmeticValidation read_embedded_here_document_delimiter(
  TSLexer *lexer,
  bool strip_tabs,
  struct HereDocument *document
) {
  struct ByteBuffer delimiter = {0};
  bool quoted = false;
  enum ArithmeticValidation result = ARITHMETIC_VALIDATION_VALID;

  while (is_horizontal_blank(lexer->lookahead)) {
    lexer->advance(lexer, false);
  }

  while (result == ARITHMETIC_VALIDATION_VALID) {
    int32_t character = lexer->lookahead;
    if (lexer_at_eof(lexer)) {
      break;
    }
    if (
      is_horizontal_blank(character) ||
      character ==
      '\n' ||
      is_control_operator_start(character) ||
      character ==
      '<' ||
      character == '>'
    ) {
      break;
    }
    if (character == '\\') {
      lexer->advance(lexer, false);
      if (lexer_at_eof(lexer)) {
        result = ARITHMETIC_VALIDATION_INCOMPLETE;
        break;
      }
      if (lexer->lookahead == '\n') {
        lexer->advance(lexer, false);
        continue;
      }
      quoted = true;
      if (!append_codepoint(&delimiter, lexer->lookahead)) {
        result = ARITHMETIC_VALIDATION_INVALID;
        break;
      }
      lexer->advance(lexer, false);
      continue;
    }
    if (character == '\'') {
      quoted = true;
      lexer->advance(lexer, false);
      while (!lexer_at_eof(lexer) && lexer->lookahead != '\'') {
        if (!append_codepoint(&delimiter, lexer->lookahead)) {
          result = ARITHMETIC_VALIDATION_INVALID;
          break;
        }
        lexer->advance(lexer, false);
      }
      if (result != ARITHMETIC_VALIDATION_VALID) {
        break;
      }
      if (lexer_at_eof(lexer)) {
        result = ARITHMETIC_VALIDATION_INCOMPLETE;
        break;
      }
      lexer->advance(lexer, false);
      continue;
    }
    if (character == '"') {
      quoted = true;
      lexer->advance(lexer, false);
      while (!lexer_at_eof(lexer) && lexer->lookahead != '"') {
        int32_t content = lexer->lookahead;
        if (content == '\\') {
          lexer->advance(lexer, false);
          if (lexer_at_eof(lexer)) {
            break;
          }
          content = lexer->lookahead;
          if (
            content !=
            '$' &&
            content !=
            '`' &&
            content !=
            '"' &&
            content != '\\'
          ) {
            if (!append_byte(&delimiter, '\\')) {
              result = ARITHMETIC_VALIDATION_INVALID;
              break;
            }
          }
        }
        if (!append_codepoint(&delimiter, content)) {
          result = ARITHMETIC_VALIDATION_INVALID;
          break;
        }
        lexer->advance(lexer, false);
      }
      if (result != ARITHMETIC_VALIDATION_VALID) {
        break;
      }
      if (lexer_at_eof(lexer)) {
        result = ARITHMETIC_VALIDATION_INCOMPLETE;
        break;
      }
      lexer->advance(lexer, false);
      continue;
    }
    if (!append_codepoint(&delimiter, character)) {
      result = ARITHMETIC_VALIDATION_INVALID;
      break;
    }
    lexer->advance(lexer, false);
  }

  if (result == ARITHMETIC_VALIDATION_VALID && delimiter.length == 0) {
    result = ARITHMETIC_VALIDATION_INVALID;
  }
  if (result != ARITHMETIC_VALIDATION_VALID) {
    ts_free(delimiter.data);
    return result;
  }

  *document = (struct HereDocument){
    .delimiter = (uint8_t *)delimiter.data,
    .delimiter_length = delimiter.length,
    .quoted = quoted,
    .strip_tabs = strip_tabs,
  };
  return ARITHMETIC_VALIDATION_VALID;
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

/*
 * Skip one embedded construct whose interior POSIX excludes from the
 * enclosing scan: a "$(...)" or "${...}" body, and the quoted regions and
 * plain backquote substitutions that appear inside them. The frame stack
 * mirrors which delimiter is currently open, and inside command frames the
 * case-statement states and pending here-documents decide which right
 * parentheses and lines belong to the construct, so a pattern's parenthesis
 * or a body line cannot end the skip early.
 */
static enum ArithmeticValidation
skip_embedded_construct(TSLexer *lexer, char initial_closer) {
  struct EmbeddedSkip skip = {0};
  enum ArithmeticValidation result = ARITHMETIC_VALIDATION_VALID;
  bool at_word = false;
  bool at_command_position = true;

  if (!embedded_push_frame(&skip, initial_closer, initial_closer == ')')) {
    clear_embedded_skip(&skip);
    return ARITHMETIC_VALIDATION_INVALID;
  }
  if (initial_closer == ')' && lexer->lookahead == '(') {
    // "$((" tentatively reads as an arithmetic pair, exactly as the
    // here-document delimiter scan treats it.
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
    struct EmbeddedCaseFrame *active_case =
      in_command ? embedded_active_case(&skip) : NULL;
    int32_t character = lexer->lookahead;

    if (frame->closer == '`') {
      lexer->advance(lexer, false);
      if (character == '\\') {
        if (lexer_at_eof(lexer)) {
          result = ARITHMETIC_VALIDATION_INCOMPLETE;
          break;
        }
        lexer->advance(lexer, false);
      } else if (character == '`') {
        skip.frame_count -= 1;
      }
      continue;
    }

    if (frame->closer == '"') {
      if (character == '\\') {
        lexer->advance(lexer, false);
        if (lexer_at_eof(lexer)) {
          result = ARITHMETIC_VALIDATION_INCOMPLETE;
          break;
        }
        lexer->advance(lexer, false);
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
          bool command_context = lexer->lookahead == '(';
          if (!embedded_push_frame(
                &skip,
                command_context ? ')' : '}',
                command_context
              )) {
            result = ARITHMETIC_VALIDATION_INVALID;
            break;
          }
          lexer->advance(lexer, false);
          if (command_context && lexer->lookahead == '(') {
            skip.frames[skip.frame_count - 1].command_context = false;
            if (!embedded_push_frame(&skip, ')', false)) {
              result = ARITHMETIC_VALIDATION_INVALID;
              break;
            }
            lexer->advance(lexer, false);
          }
          at_command_position = true;
        }
        continue;
      }
      lexer->advance(lexer, false);
      continue;
    }

    if (character == '\\') {
      lexer->advance(lexer, false);
      if (lexer_at_eof(lexer)) {
        result = ARITHMETIC_VALIDATION_INCOMPLETE;
        break;
      }
      if (lexer->lookahead == '\n') {
        // A line continuation joins the logical line without ending words
        // or starting here-document bodies.
        lexer->advance(lexer, false);
        continue;
      }
      lexer->advance(lexer, false);
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
        bool command_context = lexer->lookahead == '(';
        if (!embedded_push_frame(
              &skip,
              command_context ? ')' : '}',
              command_context
            )) {
          result = ARITHMETIC_VALIDATION_INVALID;
          break;
        }
        lexer->advance(lexer, false);
        if (command_context && lexer->lookahead == '(') {
          skip.frames[skip.frame_count - 1].command_context = false;
          if (!embedded_push_frame(&skip, ')', false)) {
            result = ARITHMETIC_VALIDATION_INVALID;
            break;
          }
          lexer->advance(lexer, false);
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
            lexer->advance(lexer, false);
            if (lexer_at_eof(lexer)) {
              result = ARITHMETIC_VALIDATION_INCOMPLETE;
              break;
            }
            lexer->advance(lexer, false);
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
      (active_case == NULL || active_case->state == EMBEDDED_CASE_BODY)
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
      if (active_case != NULL && active_case->state == EMBEDDED_CASE_PATTERN) {
        // The optional pattern-introducing parenthesis owns no frame; its
        // partner ends the pattern instead of a group.
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
        if (active_case->state == EMBEDDED_CASE_PATTERN) {
          active_case->state = EMBEDDED_CASE_BODY;
          at_word = false;
          at_command_position = true;
          continue;
        }
        // An open case statement owns every further right parenthesis on
        // this frame, so an unbalanced one cannot end the construct.
        at_word = true;
        at_command_position = false;
        continue;
      }
      lexer->advance(lexer, false);
      skip.frame_count -= 1;
      while (
        skip.case_count >
        0 &&
        skip.cases[skip.case_count - 1].frame_count > skip.frame_count
      ) {
        skip.case_count -= 1;
      }
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
      EMBEDDED_CASE_BODY &&
      character == ';'
    ) {
      lexer->advance(lexer, false);
      if (lexer->lookahead == ';' || lexer->lookahead == '&') {
        lexer->advance(lexer, false);
        active_case->state = EMBEDDED_CASE_PATTERN;
      }
      at_word = false;
      at_command_position = true;
      continue;
    }

    if (!at_word && is_lowercase_letter(character)) {
      // A fresh unquoted lowercase run may be the reserved word that drives
      // the case-statement states; whatever it is, the run is consumed as
      // one word here.
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
      int32_t next = lexer->lookahead;
      bool delimited = lexer_at_eof(lexer) ||
        is_horizontal_blank(next) ||
        next ==
        '\n' ||
        is_control_operator_start(next) ||
        next ==
        '<' ||
        next ==
        '>' ||
        next ==
        '(' ||
        next == ')';
      if (in_command) {
        if (active_case != NULL) {
          if (active_case->state == EMBEDDED_CASE_EXPECT_WORD) {
            active_case->state = EMBEDDED_CASE_EXPECT_IN;
          } else if (
            active_case->state ==
            EMBEDDED_CASE_EXPECT_IN &&
            candidate &&
            delimited &&
            strcmp(word, "in") == 0
          ) {
            active_case->state = EMBEDDED_CASE_PATTERN;
          } else if (
            candidate &&
            delimited &&
            strcmp(word, "esac") ==
            0 &&
            (active_case->state ==
              EMBEDDED_CASE_PATTERN ||
              (active_case->state == EMBEDDED_CASE_BODY && position))
          ) {
            skip.case_count -= 1;
          } else if (
            candidate &&
            delimited &&
            strcmp(word, "case") ==
            0 &&
            position &&
            active_case->state == EMBEDDED_CASE_BODY
          ) {
            if (!embedded_push_case(&skip)) {
              result = ARITHMETIC_VALIDATION_INVALID;
              break;
            }
          }
        } else if (
          candidate && delimited && position && strcmp(word, "case") == 0
        ) {
          if (!embedded_push_case(&skip)) {
            result = ARITHMETIC_VALIDATION_INVALID;
            break;
          }
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
      at_word = true;
      at_command_position = false;
    }
  }

  // A construct that closes before the newline that would start its pending
  // bodies releases them to the enclosing program, whose own line scan is
  // outside this skip.
  clear_embedded_skip(&skip);
  return result;
}

/*
 * Skip a backquote substitution opened at the given enclosing depth, using
 * the escaped-backquote spelling POSIX requires for nested substitutions.
 */
static enum ArithmeticValidation
skip_backquote_substitution(TSLexer *lexer, size_t enclosing_depth) {
  size_t depth = enclosing_depth + 1;

  while (true) {
    if (lexer_at_eof(lexer)) {
      return ARITHMETIC_VALIDATION_INCOMPLETE;
    }

    size_t escape_count = 0;
    while (lexer->lookahead == '\\') {
      if (escape_count == SIZE_MAX) {
        return ARITHMETIC_VALIDATION_INVALID;
      }
      escape_count += 1;
      lexer->advance(lexer, false);
    }

    if (lexer->lookahead == '`') {
      lexer->advance(lexer, false);
      if (escape_count == 0) {
        /*
         * POSIX satisfies the search for a matching backquote with the first
         * non-escaped backquote, so a plain backquote ends the whole
         * substitution even when nested spellings are still open.
         */
        return ARITHMETIC_VALIDATION_VALID;
      }
      enum BackquoteTickPrefix prefix =
        classify_backquote_tick_prefix(depth, escape_count, true, true);
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

/*
 * Read the lookahead that follows an operator character on the logical
 * source: fold single-backslash line continuations and stop at the first
 * character that is not part of one, reporting how many backslashes remain
 * unconsumed before it.
 */
static void read_logical_lookahead(TSLexer *lexer, size_t *escape_run) {
  *escape_run = 0;
  while (lexer->lookahead == '\\') {
    size_t run = 0;
    while (lexer->lookahead == '\\' && run < SIZE_MAX) {
      run += 1;
      lexer->advance(lexer, false);
    }
    if (run == 1 && lexer->lookahead == '\n') {
      lexer->advance(lexer, false);
      continue;
    }
    *escape_run = run;
    return;
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
  int32_t first,
  size_t *pending_escapes
) {
  uint8_t category;
  size_t escape_run = 0;
  read_logical_lookahead(lexer, &escape_run);
  int32_t second = escape_run > 0 ? 0 : lexer->lookahead;

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
      read_logical_lookahead(lexer, &escape_run);
      category = ARITHMETIC_OPERATOR_CATEGORY_SHIFT;
      if (escape_run == 0 && lexer->lookahead == '=') {
        category = ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT;
        lexer->advance(lexer, false);
        read_logical_lookahead(lexer, &escape_run);
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

  *pending_escapes = escape_run;
  return append_validation_token(tokens, VALIDATION_TOKEN_OPERATOR, category);
}

static enum ArithmeticValidation validate_arithmetic_content(
  const struct Scanner *scanner,
  TSLexer *lexer,
  struct ValidationTokenBuffer *tokens,
  bool *has_expansion
) {
  size_t group_depth = 0;
  size_t pending_escapes = 0;

  while (true) {
    int32_t character = lexer->lookahead;

    if (pending_escapes == 0 && character == '\\') {
      read_logical_lookahead(lexer, &pending_escapes);
      if (pending_escapes == 0) {
        continue;
      }
      character = lexer->lookahead;
    }

    if (pending_escapes > 0) {
      if (character == '`') {
        enum BackquoteTickPrefix prefix = classify_backquote_tick_prefix(
          scanner->backquote_depth,
          pending_escapes,
          true,
          true
        );
        pending_escapes = 0;
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

      if (!skip_line_continuations(lexer)) {
        return ARITHMETIC_VALIDATION_INVALID;
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
          skip_embedded_construct(lexer, introducer == '(' ? ')' : '}');
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
      if (!validate_operator_source(
            lexer,
            tokens,
            character,
            &pending_escapes
          )) {
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

// Recognizes an lvalue directly followed by an assignment operator: an
// optional run of left parentheses, a variable, the matching right
// parentheses, and the operator. Reports the index directly after the
// operator, or SIZE_MAX.
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

// Validates the token run as one structured POSIX arithmetic expression
// without recursion. Open groups and pending ternaries live on a heap-grown
// stack, so the accepted nesting is bounded by the token run alone, and an
// assignment head is recognized exactly where a whole expression starts:
// at the input, behind an assignment, inside parentheses, and as a ternary
// consequence.
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

// Settles the reading at the second left parenthesis of "$((" as a zero-width
// marker before the parenthesis: one of the arithmetic readings, or the
// command-substitution fallback whose first command begins with the
// parenthesis.
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
  if (result == ARITHMETIC_VALIDATION_INCOMPLETE) {
    symbol = ARITHMETIC_INCOMPLETE_LEFT_PARENTHESIS;
  } else if (result == ARITHMETIC_VALIDATION_VALID) {
    struct StructuredValidation validation = {
      .tokens = tokens.data,
      .count = tokens.length,
    };
    if (structured_expression_is_valid(&validation)) {
      symbol = ARITHMETIC_LEFT_PARENTHESIS;
    } else if (has_expansion) {
      symbol = ARITHMETIC_DYNAMIC_LEFT_PARENTHESIS;
    }
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

// Settles the reading directly after "$(" and its leading continuation
// pairs: a further pair stays ahead of the marker, a left parenthesis defers
// to the arithmetic race, and everywhere else the command-substitution body
// begins at a zero-width marker that opens its nesting level.
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
      valid_symbols[ARITHMETIC_DYNAMIC_LEFT_PARENTHESIS] ||
      valid_symbols[ARITHMETIC_INCOMPLETE_LEFT_PARENTHESIS])
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

static bool scan_unbraced_parameter_start(TSLexer *lexer) {
  if (lexer->lookahead != '$') {
    return false;
  }

  lexer->mark_end(lexer);
  lexer->advance(lexer, false);
  if (
    (is_name_start_character(lexer->lookahead) ||
      is_decimal_digit(lexer->lookahead) ||
      is_special_parameter_character(lexer->lookahead))
  ) {
    lexer->result_symbol = UNBRACED_PARAMETER_START;
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

  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      break;
    }
    lexer->advance(lexer, false);
  }

  if (is_decimal_digit(lexer->lookahead)) {
    return false;
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

static bool scan_backquote_start(struct Scanner *scanner, TSLexer *lexer) {
  if (scanner->backquote_depth > 0) {
    return false;
  }

  if (lexer->lookahead != '`' || !increase_backquote_depth(scanner)) {
    return false;
  }

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  lexer->result_symbol = BACKQUOTE_START;
  return true;
}

static bool scan_backquote_end(struct Scanner *scanner, TSLexer *lexer) {
  if (scanner->backquote_depth == 0) {
    return false;
  }

  if (lexer->lookahead == '`') {
    lexer->advance(lexer, false);
  } else {
    return false;
  }

  scanner->backquote_depth -= 1;
  lexer->mark_end(lexer);
  lexer->result_symbol = BACKQUOTE_END;
  return true;
}

static bool
scan_backquote_end_recovery(struct Scanner *scanner, TSLexer *lexer) {
  if (scanner->backquote_depth == 0 || !lexer_at_eof(lexer)) {
    return false;
  }

  scanner->backquote_depth -= 1;
  lexer->mark_end(lexer);
  lexer->result_symbol = BACKQUOTE_END_RECOVERY;
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

static bool scan_backquote_prefix_after_first_backslash(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  size_t escape_count = 1;
  while (lexer->lookahead == '\\') {
    if (escape_count == SIZE_MAX) {
      return false;
    }
    escape_count += 1;
    lexer->advance(lexer, false);
  }

  if (escape_count == 1 && lexer->lookahead == '\n') {
    if (element_boundary_symbols_are_valid(valid_symbols)) {
      // The pair follows a completed element inside the substitution, so
      // the element boundary classifies the layout run it leads before any
      // pair lexes as a line continuation.
      return scan_element_boundary_core(scanner, lexer, valid_symbols, true);
    }
    if (
      valid_symbols[TRAILING_CONTINUATION_BEGIN] &&
      !valid_symbols[LINE_CONTINUATION]
    ) {
      // Only trailing layout may follow the substitution's completed
      // commands, so the pair begins the continuation-led run.
      lexer->result_symbol = TRAILING_CONTINUATION_BEGIN;
      return true;
    }
    return scan_line_continuation_after_backslash(lexer, valid_symbols);
  }

  if (
    escape_count ==
    1 &&
    lexer->lookahead ==
    '$' &&
    valid_symbols[BACKQUOTE_DOLLAR_PREFIX]
  ) {
    lexer->mark_end(lexer);
    lexer->advance(lexer, false);

    if (
      lexer->lookahead !=
      '{' &&
      lexer->lookahead !=
      '(' &&
      !is_name_start_character(lexer->lookahead) &&
      !is_decimal_digit(lexer->lookahead) &&
      !is_special_parameter_character(lexer->lookahead)
    ) {
      return false;
    }

    lexer->result_symbol = BACKQUOTE_DOLLAR_PREFIX;
    return true;
  }

  if (lexer->lookahead != '`') {
    return false;
  }

  enum BackquoteTickPrefix prefix = classify_backquote_tick_prefix(
    scanner->backquote_depth,
    escape_count,
    valid_symbols[BACKQUOTE_START_PREFIX],
    valid_symbols[BACKQUOTE_END_PREFIX]
  );
  if (prefix == BACKQUOTE_TICK_PREFIX_END) {
    lexer->mark_end(lexer);
    scanner->backquote_depth -= 1;
    lexer->result_symbol = BACKQUOTE_END_PREFIX;
    return true;
  }

  if (
    prefix == BACKQUOTE_TICK_PREFIX_START && increase_backquote_depth(scanner)
  ) {
    lexer->mark_end(lexer);
    lexer->result_symbol = BACKQUOTE_START_PREFIX;
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
  TSLexer *lexer,
  const struct HereDocument *document
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

    uint8_t source_bytes[4];
    size_t source_length;
    if (
      !encode_utf8_scalar(source_character, source_bytes, &source_length) ||
      source_length >
      document->delimiter_length -
      delimiter_offset ||
      memcmp(
        document->delimiter + delimiter_offset,
        source_bytes,
        source_length
      ) != 0
    ) {
      return false;
    }
    delimiter_offset += source_length;
  }

  if (!document->quoted) {
    while (lexer->lookahead == '\\') {
      lexer->advance(lexer, false);
      if (lexer->lookahead != '\n') {
        return false;
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

  if (lexer_at_eof(lexer)) {
    return true;
  }

  return false;
}

static bool
scan_here_document_end_commit(struct Scanner *scanner, TSLexer *lexer) {
  if (scanner->active_count == 0 || scanner->at_here_document_line_start) {
    return false;
  }

  if (lexer->lookahead == '\n') {
    lexer->advance(lexer, false);
  } else if (!lexer_at_eof(lexer)) {
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
    bool at_end_of_input = lexer_at_eof(lexer);
    lexer->mark_end(lexer);
    bool is_end = scan_here_document_end_line(lexer, document);
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

    if (
      (is_end || at_end_of_input) &&
      valid_symbols[BACKQUOTE_END_RECOVERY] &&
      scanner->backquote_depth > 0
    ) {
      scanner->backquote_depth -= 1;
      lexer->result_symbol = BACKQUOTE_END_RECOVERY;
      return true;
    }

    if (is_end && valid_symbols[HERE_DOCUMENT_BOUNDARY]) {
      lexer->result_symbol = HERE_DOCUMENT_BOUNDARY;
      return true;
    }

    if (at_end_of_input && valid_symbols[HERE_DOCUMENT_END_RECOVERY]) {
      lexer->result_symbol = HERE_DOCUMENT_END_RECOVERY;
      finish_active_document(scanner);
      scanner->at_here_document_line_start = false;
      return true;
    }

    if (valid_symbols[HERE_DOCUMENT_CONTENT_LINE_START]) {
      scanner->at_here_document_line_start = false;
      lexer->result_symbol = HERE_DOCUMENT_CONTENT_LINE_START;
      return true;
    }

    return false;
  }

  if (
    valid_symbols[BACKQUOTE_END_RECOVERY] &&
    scan_backquote_end_recovery(scanner, lexer)
  ) {
    return true;
  }

  if (valid_symbols[HERE_DOCUMENT_END_RECOVERY] && lexer_at_eof(lexer)) {
    lexer->mark_end(lexer);
    lexer->result_symbol = HERE_DOCUMENT_END_RECOVERY;
    finish_active_document(scanner);
    return true;
  }

  /*
   * A newline where the enclosing construct could also close at the
   * document's delimiter line decides against that line now: at the
   * following line start ordinary tokens would claim the delimiter source
   * before any boundary reading. Elsewhere the newline is consumed plainly
   * and the next line start classifies itself.
   */
  if (valid_symbols[HERE_DOCUMENT_BOUNDARY] && lexer->lookahead == '\n') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    if (scan_here_document_end_line(lexer, document)) {
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
    scanner->at_here_document_line_start = true;
    lexer->mark_end(lexer);
    lexer->result_symbol = NEWLINE;
    return true;
  }

  return false;
}

/*
 * A raw newline inside an active here-document body that no newline token
 * may consume. The line that follows decides between the enclosing term's
 * separator, the boundary that synchronizes the innermost open construct at
 * the delimiter line, and the plain newline-position recoveries. Every
 * token this scan emits is zero width before the newline, so the newline
 * itself stays with the formal layout structure that owns it.
 */
static bool scan_here_document_body_newline(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  const struct HereDocument *document = &scanner->active_documents[0];
  lexer->mark_end(lexer);
  lexer->advance(lexer, false);

  struct HereDocumentLineStart start;
  enum HereDocumentLineKind kind =
    probe_here_document_line(scanner, lexer, document, &start);

  if (kind != HERE_DOCUMENT_LINE_DELIMITER) {
    if (
      valid_symbols[SEPARATOR_NEWLINE] &&
      !has_startable_pending_document(scanner)
    ) {
      while (kind == HERE_DOCUMENT_LINE_LAYOUT) {
        kind = probe_here_document_line(scanner, lexer, document, &start);
      }
      if (
        kind ==
        HERE_DOCUMENT_LINE_CONTENT &&
        here_document_line_continues_term(scanner, &start, valid_symbols)
      ) {
        lexer->result_symbol = SEPARATOR_NEWLINE;
        return true;
      }
    }
    if (valid_symbols[REDIRECTION_TARGET_RECOVERY]) {
      lexer->result_symbol = REDIRECTION_TARGET_RECOVERY;
      return true;
    }
    if (
      valid_symbols[MISSING_COMMAND_RECOVERY_BOUNDARY] &&
      !valid_symbols[PRE_NEWLINE_BLANK] &&
      !valid_symbols[SEPARATOR_RECOVERY] &&
      !valid_symbols[COMPOUND_COMMAND_RECOVERY_BOUNDARY] &&
      !valid_symbols[CASE_ITEMS_RECOVERY_BOUNDARY]
    ) {
      lexer->result_symbol = MISSING_COMMAND_RECOVERY_BOUNDARY;
      return true;
    }
    return false;
  }

  static const enum TokenType CLOSING_RECOVERIES[] = {
    HERE_DOCUMENT_BOUNDARY,
    HEADER_RECOVERY_BOUNDARY,
    DIRECT_RECOVERY_BOUNDARY,
    FOR_TAIL_RECOVERY_BOUNDARY,
    SUBSHELL_RECOVERY_BOUNDARY,
    COMPOUND_LIST_BOUNDARY,
    REDIRECTION_TARGET_RECOVERY,
    COMPOUND_COMMAND_RECOVERY_BOUNDARY,
    CASE_ITEMS_RECOVERY_BOUNDARY,
    MISSING_COMMAND_RECOVERY_BOUNDARY,
  };
  for (
    size_t index = 0;
    index < sizeof(CLOSING_RECOVERIES) / sizeof(CLOSING_RECOVERIES[0]);
    index += 1
  ) {
    if (valid_symbols[CLOSING_RECOVERIES[index]]) {
      lexer->result_symbol = (TSSymbol)CLOSING_RECOVERIES[index];
      return true;
    }
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

void *tree_sitter_posix_sh_external_scanner_create(void) {
  return ts_calloc(1, sizeof(struct Scanner));
}

void tree_sitter_posix_sh_external_scanner_destroy(void *payload) {
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

// The declaration depth only steers pending documents toward the newline
// that starts their bodies, so activated documents serialize without it.
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

static bool serialize_captured_document(
  struct StateWriter *writer,
  const struct CapturedHereDocument *captured
) {
  return (
    write_state_byte(writer, document_flags(&captured->document)) &&
    write_state_size(writer, captured->source_end_column) &&
    write_state_size(writer, captured->document.declaration_depth) &&
    write_document_delimiter(writer, &captured->document)
  );
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

static bool serialize_captured_document_array(
  struct StateWriter *writer,
  const struct CapturedHereDocument *documents,
  size_t count
) {
  if (!write_state_size(writer, count)) {
    return false;
  }

  for (size_t index = 0; index < count; index += 1) {
    if (!serialize_captured_document(writer, &documents[index])) {
      return false;
    }
  }
  return true;
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
        (scanner->redirect_follows_target_recovery ? 16 : 0)
    ) ||
    !write_state_size(writer, scanner->backquote_depth) ||
    !write_state_size(writer, scanner->substitution_depth) ||
    !write_state_size(writer, scanner->body_substitution_depth) ||
    !write_state_size(writer, scanner->body_backquote_depth) ||
    !serialize_captured_document_array(
      writer,
      scanner->captured_documents,
      scanner->captured_count
    ) ||
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
      !write_state_size(writer, frame->body_substitution_depth) ||
      !write_state_size(writer, frame->body_backquote_depth) ||
      !serialize_document_run(writer, frame->documents, frame->count, false)
    ) {
      return false;
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
  // Activating pending documents rewrites the body base depths to the
  // current depths without its own capacity guard, so the probe sizes them
  // at those prospective values: every guarded growth point then reserves
  // the activation's serialized growth, and the limit stays a deterministic
  // refusal instead of a silent overflow at serialization time.
  struct Scanner probe = *scanner;
  if (probe.body_substitution_depth < probe.substitution_depth) {
    probe.body_substitution_depth = probe.substitution_depth;
  }
  if (probe.body_backquote_depth < probe.backquote_depth) {
    probe.body_backquote_depth = probe.backquote_depth;
  }
  return serialize_scanner_state(&probe, &writer);
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

static bool deserialize_captured_document(
  struct SerializedScannerState *state,
  struct CapturedHereDocument *captured
) {
  uint8_t flags;
  size_t source_end_column;
  if (
    !read_byte(state, &flags) ||
    !read_size(state, &source_end_column) ||
    source_end_column >
    UINT32_MAX ||
    !read_document_body(state, flags, &captured->document, true)
  ) {
    return false;
  }

  captured->source_end_column = (uint32_t)source_end_column;
  return true;
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

static bool deserialize_captured_document_array(
  struct SerializedScannerState *state,
  struct CapturedHereDocument **documents,
  size_t *actual_count
) {
  size_t count;
  if (
    !read_size(state, &count) ||
    count >
    SIZE_MAX /
    sizeof(struct CapturedHereDocument) ||
    count >
    state->length -
    state->offset
  ) {
    return false;
  }

  if (count == 0) {
    return true;
  }

  *documents = ts_calloc(count, sizeof(struct CapturedHereDocument));
  if (*documents == NULL) {
    return false;
  }

  for (size_t index = 0; index < count; index += 1) {
    if (!deserialize_captured_document(state, &(*documents)[index])) {
      return false;
    }
    *actual_count += 1;
  }
  return true;
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
    !read_size(&state, &scanner->body_substitution_depth) ||
    !read_size(&state, &scanner->body_backquote_depth) ||
    !deserialize_captured_document_array(
      &state,
      &scanner->captured_documents,
      &scanner->captured_count
    ) ||
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
      !read_size(&state, &frame->body_substitution_depth) ||
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

  if (state.offset != state.length) {
    return false;
  }

  scanner->expecting_delimiter = (flags & 1) != 0;
  scanner->delimiter_strips_tabs = (flags & 2) != 0;
  scanner->sequence_end_pending = (flags & 4) != 0;
  scanner->at_here_document_line_start = (flags & 8) != 0;
  scanner->redirect_follows_target_recovery = (flags & 16) != 0;
  return true;
}

unsigned
tree_sitter_posix_sh_external_scanner_serialize(void *payload, char *buffer) {
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

void tree_sitter_posix_sh_external_scanner_deserialize(
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

static bool scan_dispatch(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols,
  bool after_redirection_target_recovery
) {
  if (
    after_redirection_target_recovery &&
    valid_symbols[REDIRECT_SEPARATOR_BEGIN] &&
    is_decimal_digit(lexer->lookahead)
  ) {
    // The previous token recovered a missing redirection target before this
    // digit run, so the run is the next redirection's IO_NUMBER and its
    // separator begins here.
    lexer->mark_end(lexer);
    lexer->result_symbol = REDIRECT_SEPARATOR_BEGIN;
    return true;
  }

  if (valid_symbols[COMMAND_SUBSTITUTION_BODY_BEGIN]) {
    return scan_command_substitution_body_begin(scanner, lexer, valid_symbols);
  }

  if (
    valid_symbols[TRAILING_CONTINUATION_BEGIN] &&
    !valid_symbols[LINE_CONTINUATION] &&
    lexer->lookahead ==
    '\\' &&
    !element_boundary_symbols_are_valid(valid_symbols) &&
    !backquote_prefix_token_is_valid(scanner, valid_symbols)
  ) {
    // The position holds completed commands and admits only trailing
    // layout, so a continuation pair here begins the continuation-led run.
    lexer->mark_end(lexer);
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return false;
    }
    lexer->result_symbol = TRAILING_CONTINUATION_BEGIN;
    return true;
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
    valid_symbols[ASSIGNMENT_TILDE_END] &&
    (lexer->lookahead ==
      ':' ||
      lexer->lookahead ==
      '/' ||
      is_token_delimiter(scanner, lexer))
  ) {
    lexer->mark_end(lexer);
    lexer->result_symbol = ASSIGNMENT_TILDE_END;
    return true;
  }

  if (
    valid_symbols[WORD_TILDE_END] &&
    (lexer->lookahead == '/' || is_token_delimiter(scanner, lexer))
  ) {
    lexer->mark_end(lexer);
    lexer->result_symbol = WORD_TILDE_END;
    return true;
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

  if (
    lexer->lookahead ==
    '\\' &&
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
    /*
     * The newline ends a comment line of a newline run, which continues with
     * further layout lines or ends before the next command or closer.
     * Reading to that horizon records the run's decisive lookahead on its
     * own final token, so an edit at the horizon invalidates the run instead
     * of leaving a stale shape for reuse. With pending here-documents the
     * run's extent depends on the document bodies and stays with the raced
     * here-document variants.
     */
    if (!has_startable_pending_document(scanner)) {
      finish_term_continuation(scanner, lexer, valid_symbols);
    }
    if (scanner->active_count > 0) {
      // The consumed newline reaches a body line start of the active
      // here-document, so the next scan classifies that line.
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
      ((valid_symbols[HERE_DOCUMENT_END_RECOVERY] ||
         valid_symbols[BACKQUOTE_END_RECOVERY]) &&
        lexer_at_eof(lexer)) ||
      (valid_symbols[LINE_CONTINUATION] && lexer->lookahead == '\\') ||
      (valid_symbols[NEWLINE] && lexer->lookahead == '\n'))
  ) {
    return scan_active_here_document(scanner, lexer, valid_symbols);
  }

  if (
    scanner->active_count >
    0 &&
    lexer->lookahead ==
    '\n' &&
    !scanner->expecting_delimiter &&
    !(valid_symbols[HERE_END_COMMIT] && scanner->captured_count > 0) &&
    !valid_symbols[FUNCTION_BODY_CONTINUATION_BOUNDARY] &&
    !valid_symbols[FUNCTION_BODY_RECOVERY_BOUNDARY]
  ) {
    return scan_here_document_body_newline(scanner, lexer, valid_symbols);
  }

  if (
    valid_symbols[MISSING_HERE_DOCUMENT_DELIMITER] &&
    scanner->expecting_delimiter &&
    is_missing_here_document_delimiter_boundary(scanner, lexer)
  ) {
    return scan_missing_here_document_delimiter(scanner, lexer);
  }

  if (scanner->expecting_delimiter && valid_symbols[HERE_END_BEGIN]) {
    return scan_here_document_delimiter(scanner, lexer, valid_symbols);
  }

  if (
    valid_symbols[HERE_END_COMMIT] &&
    scanner->captured_count >
    0 &&
    scan_here_end_commit(scanner, lexer)
  ) {
    return true;
  }

  if (valid_symbols[DLESS] || valid_symbols[DLESSDASH]) {
    return scan_here_document_operator_commit(scanner, lexer, valid_symbols);
  }

  if (
    (valid_symbols[ARITHMETIC_LEFT_PARENTHESIS] ||
      valid_symbols[ARITHMETIC_DYNAMIC_LEFT_PARENTHESIS] ||
      valid_symbols[ARITHMETIC_INCOMPLETE_LEFT_PARENTHESIS]) &&
    lexer->lookahead == '('
  ) {
    return scan_arithmetic_left_parenthesis(scanner, lexer, valid_symbols);
  }

  bool arithmetic_boundary_is_valid =
    arithmetic_operand_boundary_is_valid(valid_symbols) ||
    arithmetic_operator_boundary_is_valid(valid_symbols) ||
    valid_symbols[ARITHMETIC_CLOSING_BOUNDARY];
  /*
   * A substitution introducer can only satisfy an operand boundary; when no
   * operand boundary is expected it must stay unconsumed for the runtime
   * fragment scans below.
   */
  bool arithmetic_boundary_matches_lookahead =
    (lexer->lookahead == '$' || lexer->lookahead == '`')
    ? arithmetic_operand_boundary_is_valid(valid_symbols)
    : (is_arithmetic_operand_start(lexer->lookahead) ||
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

  bool function_boundary_is_valid =
    valid_symbols[FUNCTION_BODY_CONTINUATION_BOUNDARY] ||
    valid_symbols[FUNCTION_BODY_RECOVERY_BOUNDARY];
  if (function_boundary_is_valid) {
    return scan_function_body_boundary(scanner, lexer, valid_symbols);
  }

  bool element_boundary_is_valid =
    element_boundary_symbols_are_valid(valid_symbols) ||
    (lexer->lookahead == ';' && valid_symbols[INVALID_CASE_TERMINATOR_START]);
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
    while (is_horizontal_blank(lexer->lookahead)) {
      lexer->advance(lexer, false);
    }
    lexer->mark_end(lexer);
    while (true) {
      if (lexer->lookahead == '\n') {
        if (here_document_delimiter_line_follows(scanner, lexer)) {
          return false;
        }
        lexer->result_symbol = PRE_NEWLINE_BLANK;
        return true;
      }
      if (lexer->lookahead == '#') {
        return classify_comment_boundary(lexer, valid_symbols);
      }
      if (lexer->lookahead == ';' && valid_symbols[CASE_ITEM_END]) {
        lexer->advance(lexer, false);
        if (lexer->lookahead == ';' || lexer->lookahead == '&') {
          lexer->result_symbol = CASE_ITEM_END;
          return true;
        }
        return false;
      }
      if (lexer->lookahead != '\\') {
        return false;
      }
      lexer->advance(lexer, false);
      if (lexer->lookahead != '\n') {
        return false;
      }
      lexer->advance(lexer, false);
      while (is_horizontal_blank(lexer->lookahead)) {
        lexer->advance(lexer, false);
      }
    }
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
    comment_boundary_is_valid ||
    valid_symbols[COMPOUND_LIST_BOUNDARY];
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
         lexer->lookahead ==
         '{' ||
         is_decimal_digit(lexer->lookahead)) &&
        valid_symbols[REDIRECT_LIST_BEGIN]) ||
      (lexer->lookahead == '\\' && valid_symbols[REDIRECT_LIST_BEGIN]))
  ) {
    return scan_shell_boundary(scanner, lexer, valid_symbols);
  }

  // The end of input and the closing backquote of an enclosing substitution
  // are the synchronization boundaries an unterminated quote or command
  // substitution cannot cross; each closes there so the enclosing structure
  // keeps its skeleton without entering blanket error recovery.
  if (
    valid_symbols[COMMAND_SUBSTITUTION_INPUT_END_RECOVERY] &&
    (lexer_at_eof(lexer) ||
      is_active_backquote_boundary(scanner, lexer->lookahead))
  ) {
    lexer->mark_end(lexer);
    if (scanner->substitution_depth > 0) {
      scanner->substitution_depth -= 1;
    }
    lexer->result_symbol = COMMAND_SUBSTITUTION_INPUT_END_RECOVERY;
    return true;
  }

  if (
    valid_symbols[QUOTE_INPUT_END_RECOVERY] &&
    (lexer_at_eof(lexer) ||
      is_active_backquote_boundary(scanner, lexer->lookahead))
  ) {
    lexer->mark_end(lexer);
    lexer->result_symbol = QUOTE_INPUT_END_RECOVERY;
    return true;
  }

  static const enum TokenType parameter_recovery_symbols[] = {
    PARAMETER_MISSING_RECOVERY_BOUNDARY,
    PARAMETER_OPERATOR_RECOVERY_BOUNDARY,
    PARAMETER_TAIL_RECOVERY_BOUNDARY,
    DOUBLE_QUOTED_PARAMETER_TAIL_RECOVERY_BOUNDARY,
    INPUT_END_RECOVERY,
  };
  for (
    size_t index = 0; index <
    sizeof(parameter_recovery_symbols) /
    sizeof(parameter_recovery_symbols[0]);
    index += 1
  ) {
    enum TokenType symbol = parameter_recovery_symbols[index];
    if (valid_symbols[symbol]) {
      if (
        scan_parameter_expansion_recovery_boundary(
          scanner,
          lexer,
          symbol,
          valid_symbols
        )
      ) {
        return true;
      }
      break;
    }
  }

  if (
    (valid_symbols[COMMAND_SUBSTITUTION_CLOSE] ||
      valid_symbols[SUBSHELL_CLOSE]) &&
    lexer->lookahead == ')'
  ) {
    lexer->advance(lexer, false);
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

  if (valid_symbols[REDIRECTION_TARGET_RECOVERY] && lexer->lookahead == '\n') {
    return scan_recovery_boundary(
      scanner,
      lexer,
      REDIRECTION_TARGET_RECOVERY,
      valid_symbols
    );
  }

  if (
    is_lowercase_letter(lexer->lookahead) &&
    (reserved_word_symbol_is_valid(valid_symbols) ||
      valid_symbols[INVALID_RESERVED_COMMAND_START] ||
      valid_symbols[SUBSHELL_RECOVERY_BOUNDARY] ||
      valid_symbols[MISSING_COMMAND_RECOVERY_BOUNDARY] ||
      valid_symbols[COMPOUND_COMMAND_RECOVERY_BOUNDARY] ||
      valid_symbols[SEPARATOR_RECOVERY])
  ) {
    return scan_lowercase_dispatch(scanner, lexer, valid_symbols, true);
  }

  if (
    valid_symbols[COMPOUND_LIST_BOUNDARY] &&
    (lexer_at_eof(lexer) ||
      lexer->lookahead ==
      ')' ||
      lexer->lookahead ==
      '}' ||
      lexer->lookahead ==
      ';' ||
      is_active_backquote_boundary(scanner, lexer->lookahead) ||
      is_horizontal_blank(lexer->lookahead) ||
      is_lowercase_letter(lexer->lookahead))
  ) {
    return scan_compound_list_boundary(scanner, lexer);
  }

  bool suppress_broad_recovery = false;
  if (valid_symbols[HEADER_RECOVERY_BOUNDARY]) {
    if (
      scan_direct_recovery_boundary(
        scanner,
        lexer,
        HEADER_RECOVERY_BOUNDARY,
        valid_symbols
      )
    ) {
      return true;
    }
    suppress_broad_recovery = is_lowercase_letter(lexer->lookahead);
  }
  if (valid_symbols[DIRECT_RECOVERY_BOUNDARY]) {
    if (
      scan_direct_recovery_boundary(
        scanner,
        lexer,
        DIRECT_RECOVERY_BOUNDARY,
        valid_symbols
      )
    ) {
      return true;
    }
    suppress_broad_recovery = is_lowercase_letter(lexer->lookahead);
  }
  if (valid_symbols[FOR_TAIL_RECOVERY_BOUNDARY]) {
    if (
      scan_direct_recovery_boundary(
        scanner,
        lexer,
        FOR_TAIL_RECOVERY_BOUNDARY,
        valid_symbols
      )
    ) {
      return true;
    }
    suppress_broad_recovery = is_lowercase_letter(lexer->lookahead);
  }

  if (
    valid_symbols[SUBSHELL_RECOVERY_BOUNDARY] &&
    !suppress_broad_recovery &&
    (lexer_at_eof(lexer) ||
      lexer->lookahead ==
      '}' ||
      lexer->lookahead ==
      ';' ||
      is_active_backquote_boundary(scanner, lexer->lookahead) ||
      is_lowercase_letter(lexer->lookahead))
  ) {
    return scan_recovery_boundary(
      scanner,
      lexer,
      SUBSHELL_RECOVERY_BOUNDARY,
      valid_symbols
    );
  }

  if (
    (valid_symbols[MISSING_COMMAND_RECOVERY_BOUNDARY] ||
      valid_symbols[COMPOUND_COMMAND_RECOVERY_BOUNDARY] ||
      valid_symbols[CASE_ITEMS_RECOVERY_BOUNDARY] ||
      valid_symbols[SEPARATOR_RECOVERY] ||
      valid_symbols[REDIRECTION_TARGET_RECOVERY]) &&
    !suppress_broad_recovery &&
    !(lexer->lookahead == '}' && valid_symbols[RIGHT_BRACE]) &&
    !(lexer->lookahead ==
      '#' &&
      (valid_symbols[COMMENT_BOUNDARY] ||
        valid_symbols[TRAILING_COMMENT_BOUNDARY])) &&
    (lexer_at_eof(lexer) ||
      lexer->lookahead ==
      ')' ||
      (lexer->lookahead ==
        '}' &&
        (valid_symbols[COMPOUND_COMMAND_RECOVERY_BOUNDARY] ||
          valid_symbols[MISSING_COMMAND_RECOVERY_BOUNDARY])) ||
      lexer->lookahead ==
      ';' ||
      lexer->lookahead ==
      '&' ||
      /*
       * A raw newline marks a missing command only where no linebreak may
       * follow; a position that accepts a blank line accepts the newline
       * itself.
       */
      (lexer->lookahead ==
        '\n' &&
        valid_symbols[MISSING_COMMAND_RECOVERY_BOUNDARY] &&
        !valid_symbols[PRE_NEWLINE_BLANK]) ||
      (lexer->lookahead ==
        '|' &&
        valid_symbols[MISSING_COMMAND_RECOVERY_BOUNDARY]) ||
      (valid_symbols[REDIRECTION_TARGET_RECOVERY] &&
        (lexer->lookahead ==
          '<' ||
          lexer->lookahead ==
          '>' ||
          lexer->lookahead == '|')) ||
      is_active_backquote_boundary(scanner, lexer->lookahead) ||
      lexer->lookahead ==
      '#' ||
      is_lowercase_letter(lexer->lookahead))
  ) {
    bool word_position_recovery_applies = lexer->lookahead != '}';
    enum TokenType recovery_symbol = MISSING_COMMAND_RECOVERY_BOUNDARY;
    if (valid_symbols[SEPARATOR_RECOVERY] && word_position_recovery_applies) {
      recovery_symbol = SEPARATOR_RECOVERY;
    } else if (
      valid_symbols[REDIRECTION_TARGET_RECOVERY] &&
      word_position_recovery_applies
    ) {
      recovery_symbol = REDIRECTION_TARGET_RECOVERY;
    } else if (valid_symbols[COMPOUND_COMMAND_RECOVERY_BOUNDARY]) {
      recovery_symbol = COMPOUND_COMMAND_RECOVERY_BOUNDARY;
    } else if (
      valid_symbols[CASE_ITEMS_RECOVERY_BOUNDARY] &&
      word_position_recovery_applies
    ) {
      recovery_symbol = CASE_ITEMS_RECOVERY_BOUNDARY;
    }
    if (
      recovery_symbol ==
      COMPOUND_COMMAND_RECOVERY_BOUNDARY &&
      (lexer->lookahead == ';' || lexer->lookahead == '&') &&
      valid_symbols[MISSING_COMMAND_RECOVERY_BOUNDARY]
    ) {
      // A separator operator marks a missing command, not the compound
      // recovery, which synchronizes only at closers and the end of input.
      recovery_symbol = MISSING_COMMAND_RECOVERY_BOUNDARY;
    }
    if (recovery_symbol == SEPARATOR_RECOVERY && lexer_at_eof(lexer)) {
      // The separator recovery only fires before real closers, so at the end
      // of input the enclosing structure recovers instead. With none of the
      // compound recoveries open the innermost enclosing structure is an
      // unterminated backquote substitution, whose own end recovery must not
      // be starved by the refused separator reading.
      if (valid_symbols[COMPOUND_COMMAND_RECOVERY_BOUNDARY]) {
        recovery_symbol = COMPOUND_COMMAND_RECOVERY_BOUNDARY;
      } else if (valid_symbols[CASE_ITEMS_RECOVERY_BOUNDARY]) {
        recovery_symbol = CASE_ITEMS_RECOVERY_BOUNDARY;
      } else if (valid_symbols[MISSING_COMMAND_RECOVERY_BOUNDARY]) {
        recovery_symbol = MISSING_COMMAND_RECOVERY_BOUNDARY;
      } else if (
        valid_symbols[BACKQUOTE_END_RECOVERY] && scanner->backquote_depth > 0
      ) {
        return scan_backquote_end_recovery(scanner, lexer);
      }
    }
    if (
      recovery_symbol ==
      MISSING_COMMAND_RECOVERY_BOUNDARY &&
      lexer->lookahead ==
      ';' &&
      scan_case_item_terminator(lexer)
    ) {
      return false;
    }
    return scan_recovery_boundary(
      scanner,
      lexer,
      recovery_symbol,
      valid_symbols
    );
  }

  if (lexer->lookahead == '$' && valid_symbols[UNBRACED_PARAMETER_START]) {
    return scan_unbraced_parameter_start(lexer);
  }

  if (lexer->lookahead == '[' && valid_symbols[PATTERN_SPECIAL_LEFT_BRACKET]) {
    return scan_pattern_special_left_bracket(lexer);
  }

  if (
    valid_symbols[BACKQUOTE_END_RECOVERY] &&
    scanner->backquote_depth >
    0 &&
    lexer_at_eof(lexer)
  ) {
    return scan_backquote_end_recovery(scanner, lexer);
  }

  if (valid_symbols[BACKQUOTE_START] && (lexer->lookahead == '`')) {
    return scan_backquote_start(scanner, lexer);
  }

  if (valid_symbols[LINE_CONTINUATION] && lexer->lookahead == '\\') {
    return scan_line_continuation(lexer, valid_symbols);
  }

  if (valid_symbols[NEWLINE] && lexer->lookahead == '\n') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    lexer->result_symbol = NEWLINE;
    return true;
  }

  if (valid_symbols[PATTERN_BRACKET_HYPHEN] && lexer->lookahead == '-') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    lexer->result_symbol = PATTERN_BRACKET_HYPHEN;
    return true;
  }

  /*
   * Inside a bracket expression a brace or right parenthesis is an ordinary
   * member character, so the member scans run before the reserved-character
   * dispatch below and fall through where the character is not a member.
   */
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
    return (
      (valid_symbols[LEFT_BRACE] ||
        valid_symbols[IO_LOCATION] ||
        valid_symbols[INVALID_COMMAND_CHARACTER_SOURCE]) &&
      scan_left_brace_or_io_location(
        scanner,
        lexer,
        valid_symbols[LEFT_BRACE],
        valid_symbols[IO_LOCATION],
        valid_symbols[INVALID_COMMAND_CHARACTER_SOURCE]
      )
    );
  }

  if (lexer->lookahead == '}') {
    if (valid_symbols[RIGHT_BRACE]) {
      return scan_reserved_character(scanner, lexer, '}', RIGHT_BRACE);
    }
    return (
      valid_symbols[INVALID_COMMAND_CHARACTER_SOURCE] &&
      scan_reserved_character(
        scanner,
        lexer,
        '}',
        INVALID_COMMAND_CHARACTER_SOURCE
      )
    );
  }

  if (lexer->lookahead == ')') {
    return (
      valid_symbols[INVALID_COMMAND_CHARACTER_SOURCE] &&
      scan_reserved_character(
        scanner,
        lexer,
        ')',
        INVALID_COMMAND_CHARACTER_SOURCE
      )
    );
  }

  if (
    lexer->lookahead ==
    '[' &&
    (valid_symbols[WORD_BRACKET_LITERAL_START] ||
      valid_symbols[PARAMETER_BRACKET_LITERAL_START])
  ) {
    bool word_start_is_valid = valid_symbols[WORD_BRACKET_LITERAL_START];
    bool parameter_start_is_valid =
      valid_symbols[PARAMETER_BRACKET_LITERAL_START];
    if (word_start_is_valid == parameter_start_is_valid) {
      return false;
    }
    bool parameter_pattern = parameter_start_is_valid;
    return scan_bracket_literal_start(
      scanner,
      lexer,
      parameter_pattern ? PARAMETER_BRACKET_LITERAL_START
                        : WORD_BRACKET_LITERAL_START,
      parameter_pattern
    );
  }

  if (
    is_decimal_digit(lexer->lookahead) &&
    valid_symbols[REDIRECTION_TARGET_RECOVERY] &&
    !valid_symbols[FILE_DESCRIPTOR]
  ) {
    // POSIX 2.10.1 classifies a token consisting solely of digits whose
    // delimiter is '<' or '>' as IO_NUMBER even where the grammar needs a
    // filename, so the missing target recovers here and the digit run
    // begins the next redirection.
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
    scanner->redirect_follows_target_recovery = true;
    lexer->result_symbol = REDIRECTION_TARGET_RECOVERY;
    return true;
  }

  if (
    is_decimal_digit(lexer->lookahead) &&
    (valid_symbols[BRACED_PARAMETER_NUMBER_START] ||
      valid_symbols[BRACED_POSITIONAL_PARAMETER_START])
  ) {
    return scan_braced_numeric_parameter_start(lexer, valid_symbols);
  }

  if (is_decimal_digit(lexer->lookahead)) {
    return (valid_symbols[FILE_DESCRIPTOR] && scan_file_descriptor(lexer));
  }

  if (lexer->lookahead == '!') {
    if (valid_symbols[PIPELINE_NEGATION]) {
      return scan_pipeline_negation(scanner, lexer);
    }
    return (
      valid_symbols[INVALID_COMMAND_CHARACTER_SOURCE] &&
      scan_reserved_character(
        scanner,
        lexer,
        '!',
        INVALID_COMMAND_CHARACTER_SOURCE
      )
    );
  }

  if (is_lowercase_letter(lexer->lookahead)) {
    return scan_lowercase_dispatch(scanner, lexer, valid_symbols, false);
  }

  if (valid_symbols[LITERAL_HASH] && lexer->lookahead == '#') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    lexer->result_symbol = LITERAL_HASH;
    return true;
  }

  if (valid_symbols[COMMENT] && lexer->lookahead == '#') {
    return scan_comment(lexer);
  }

  return false;
}

bool tree_sitter_posix_sh_external_scanner_scan(
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

  // The redirect-separator debt expires with the next token; a scan that
  // produces no token restores it so a refused scan leaves no state change.
  bool after_redirection_target_recovery =
    scanner->redirect_follows_target_recovery;
  scanner->redirect_follows_target_recovery = false;
  if (
    scan_dispatch(
      scanner,
      lexer,
      valid_symbols,
      after_redirection_target_recovery
    )
  ) {
    return true;
  }

  scanner->redirect_follows_target_recovery = after_redirection_target_recovery;
  return false;
}
