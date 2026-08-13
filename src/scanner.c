#include "tree_sitter/alloc.h"
#include "tree_sitter/parser.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef TREE_SITTER_SERIALIZATION_BUFFER_SIZE
#define TREE_SITTER_SERIALIZATION_BUFFER_SIZE 1024
#endif

#define SCANNER_SERIALIZATION_VERSION 12
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
  CONNECTOR_LINE_CONTINUATION,
  WORD_SEPARATOR_LINE_CONTINUATION,
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
  PATTERN_SPECIAL_LEFT_BRACKET,
  LITERAL_HASH,
  COMMENT_BOUNDARY,
  COMMENT,
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
  COMMAND_CONTINUATION,
  REDIRECT_LIST_BEGIN,
  CLOSED_COMMAND_END,
  CLOSED_SIMPLE_COMMAND_END,
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
  PARAMETER_EXPANSION_RECOVERY_BOUNDARY,
  BOUNDARY_COMMAND_RECOVERY,
  MISSING_COMMAND_RECOVERY_BOUNDARY,
  INVALID_RESERVED_COMMAND_START,
  INVALID_CASE_TERMINATOR_START,
  INVALID_COMMAND_CHARACTER_SOURCE,
  SEPARATOR_RECOVERY,
  REDIRECTION_TARGET_RECOVERY,
  HERE_DOCUMENT_END_RECOVERY,
  BACKQUOTE_END_RECOVERY,
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
  TOKEN_COUNT,
};

_Static_assert(
  CONNECTOR_LINE_CONTINUATION == LINE_CONTINUATION + 1,
  "connector continuation must follow the public continuation token"
);
_Static_assert(
  WORD_SEPARATOR_LINE_CONTINUATION == CONNECTOR_LINE_CONTINUATION + 1,
  "word-separator continuation must follow the connector continuation"
);
_Static_assert(TOKEN_COUNT <= 107, "external token count exceeds the contract");

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
  size_t backquote_depth;
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
  scanner->backquote_depth = 0;
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

static bool activate_pending_documents(struct Scanner *scanner) {
  if (!suspend_active_documents(scanner)) {
    return false;
  }

  scanner->active_documents = scanner->pending_documents;
  scanner->active_count = scanner->pending_count;
  scanner->pending_documents = NULL;
  scanner->pending_count = 0;
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
  scanner->at_here_document_line_start = frame.at_line_start;

  if (scanner->suspended_frame_count == 0) {
    ts_free(scanner->suspended_frames);
    scanner->suspended_frames = NULL;
  }
}

static void finish_active_document(struct Scanner *scanner) {
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

// POSIX satisfies the search for the matching backquote with the first
// unquoted, non-escaped backquote, so inside an open backquote substitution
// that character ends whatever token is being recognized.
static bool
is_active_backquote_boundary(const struct Scanner *scanner, int32_t character) {
  return scanner->backquote_depth > 0 && character == '`';
}

static bool
is_token_delimiter(const struct Scanner *scanner, const TSLexer *lexer) {
  int32_t character = lexer->lookahead;
  return (
    lexer_at_eof(lexer) ||
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
  return (
    parameter_pattern ? lexer_at_eof(lexer) || lexer->lookahead == '}'
                      : is_token_delimiter(scanner, lexer)
  );
}

static bool scan_bracket_line_continuation(TSLexer *lexer) {
  if (lexer->lookahead != '\\') {
    return false;
  }
  lexer->advance(lexer, false);
  if (lexer->lookahead != '\n') {
    return false;
  }
  lexer->advance(lexer, false);
  return true;
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
      if (!scan_bracket_line_continuation(lexer)) {
        return false;
      }
      continue;
    }

    if (
      character ==
      '\'' ||
      character ==
      '"' ||
      character ==
      '$' ||
      character == '`'
    ) {
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
      while (lexer->lookahead == '\\') {
        if (!scan_bracket_line_continuation(lexer)) {
          return false;
        }
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
            if (!scan_bracket_line_continuation(lexer)) {
              return false;
            }
            continue;
          }
          if (
            nested_character ==
            '\'' ||
            nested_character ==
            '"' ||
            nested_character ==
            '$' ||
            nested_character == '`'
          ) {
            return false;
          }
          if (nested_character == ']' && nested_marker != '.') {
            lexer->result_symbol = (TSSymbol)symbol;
            return true;
          }

          lexer->advance(lexer, false);
          if (nested_character == nested_marker && content_length > 0) {
            while (lexer->lookahead == '\\') {
              if (!scan_bracket_line_continuation(lexer)) {
                return false;
              }
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
    character ==
    '\'' ||
    character ==
    '"' ||
    character ==
    '$' ||
    character ==
    '`' ||
    (parameter_pattern && character == '}') ||
    (!parameter_pattern && is_token_delimiter(scanner, lexer))
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

static bool
append_delimiter_case(struct DelimiterCaseBuffer *cases, size_t group_depth) {
  if (cases->length == SIZE_MAX) {
    return false;
  }

  if (cases->length == cases->capacity) {
    size_t capacity = 8;
    if (cases->capacity != 0) {
      if (cases->capacity > SIZE_MAX / 2) {
        return false;
      }
      capacity = cases->capacity * 2;
    }

    struct DelimiterCaseFrame *resized =
      ts_realloc(cases->data, capacity * sizeof(struct DelimiterCaseFrame));
    if (resized == NULL) {
      return false;
    }
    cases->data = resized;
    cases->capacity = capacity;
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
  if (groups->length >= SCANNER_STATE_CAPACITY) {
    return false;
  }

  if (groups->length == groups->capacity) {
    size_t capacity = groups->capacity == 0 ? 16 : groups->capacity * 2;
    if (capacity < groups->capacity || capacity > SCANNER_STATE_CAPACITY) {
      capacity = SCANNER_STATE_CAPACITY;
    }
    if (capacity > SIZE_MAX / sizeof(struct DelimiterGroupFrame)) {
      return false;
    }

    struct DelimiterGroupFrame *resized =
      ts_realloc(groups->data, capacity * sizeof(struct DelimiterGroupFrame));
    if (resized == NULL) {
      return false;
    }
    groups->data = resized;
    groups->capacity = capacity;
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
  if (lexer->lookahead == '\n') {
    lexer->advance(lexer, false);
    return at_delimiter_source_start && escape_count == 1
      ? DELIMITER_BACKSLASH_LEADING_CONTINUATION
      : DELIMITER_BACKSLASH_OK;
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

    int32_t character = lexer->lookahead;
    lexer->advance(lexer, false);
    uint8_t value;
    return control_escape_byte(character, &value) &&
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
          int32_t opening = lexer->lookahead;
          char closing = opening == '(' ? ')' : '}';
          enum DelimiterGroupKind kind = opening == '('
            ? DELIMITER_GROUP_COMMAND
            : DELIMITER_GROUP_PARAMETER;
          valid = push_delimiter_group(
                    &groups,
                    closing,
                    kind,
                    DELIMITER_DOUBLE_QUOTED
                  ) &&
            append_codepoint(&delimiter, opening);
          if (!valid) {
            continue;
          }

          lexer->advance(lexer, false);
          quote = DELIMITER_UNQUOTED;
          if (opening == '(' && lexer->lookahead == '(') {
            groups.data[groups.length - 1].kind = DELIMITER_GROUP_ARITHMETIC;
            groups.data[groups.length - 1].command_start = false;
            valid = push_delimiter_group(
                      &groups,
                      ')',
                      DELIMITER_GROUP_ARITHMETIC,
                      DELIMITER_UNQUOTED
                    ) &&
              append_byte(&delimiter, '(');
            if (valid) {
              lexer->advance(lexer, false);
            }
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
        int32_t opening = lexer->lookahead;
        char closing = opening == '(' ? ')' : '}';
        enum DelimiterGroupKind kind =
          opening == '(' ? DELIMITER_GROUP_COMMAND : DELIMITER_GROUP_PARAMETER;
        if (
          !push_delimiter_group(&groups, closing, kind, quote) ||
          !append_codepoint(&delimiter, opening)
        ) {
          valid = false;
          continue;
        }

        lexer->advance(lexer, false);

        if (opening == '(' && lexer->lookahead == '(') {
          groups.data[groups.length - 1].kind = DELIMITER_GROUP_ARITHMETIC;
          groups.data[groups.length - 1].command_start = false;
          if (
            !push_delimiter_group(
              &groups,
              ')',
              DELIMITER_GROUP_ARITHMETIC,
              DELIMITER_UNQUOTED
            ) ||
            !append_byte(&delimiter, '(')
          ) {
            valid = false;
            continue;
          }
          lexer->advance(lexer, false);
        }
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

static bool arithmetic_operand_boundary_is_valid(const bool *valid_symbols);

static bool arithmetic_layout_reaches_operand_start(TSLexer *lexer);

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
  } else if (valid_symbols[BOUNDARY_COMMAND_RECOVERY]) {
    *symbol = BOUNDARY_COMMAND_RECOVERY;
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

static bool scan_name_equals_begin_or_boundary(
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
    lexer->result_symbol = NAME_EQUALS_BEGIN;
    return true;
  }

  if (arithmetic_operand_boundary_is_valid(valid_symbols)) {
    if (arithmetic_layout_reaches_operand_start(lexer)) {
      return false;
    }
    lexer->result_symbol = valid_symbols[ARITHMETIC_PLUS_OPERAND_BOUNDARY]
      ? ARITHMETIC_PLUS_OPERAND_BOUNDARY
      : (valid_symbols[ARITHMETIC_MINUS_OPERAND_BOUNDARY]
            ? ARITHMETIC_MINUS_OPERAND_BOUNDARY
            : ARITHMETIC_OPERAND_BOUNDARY);
    return true;
  }

  if (
    is_name_character(lexer->lookahead) ||
    !is_reserved_candidate ||
    length ==
    0 ||
    !is_token_delimiter(scanner, lexer)
  ) {
    return false;
  }
  word[length] = '\0';

  if (
    valid_symbols[CLOSED_COMMAND_END] &&
    !valid_symbols[CLOSED_SIMPLE_COMMAND_END] &&
    is_recovery_reserved_word(word)
  ) {
    lexer->result_symbol = CLOSED_COMMAND_END;
    return true;
  }

  TSSymbol symbol;
  if (classify_reserved_word(word, valid_symbols, &symbol)) {
    lexer->result_symbol = symbol;
    return true;
  }
  if (classify_lowercase_recovery(word, valid_symbols, &symbol)) {
    lexer->result_symbol = symbol;
    return true;
  }
  if (
    valid_symbols[COMPOUND_LIST_BOUNDARY] && is_recovery_reserved_word(word)
  ) {
    lexer->result_symbol = COMPOUND_LIST_BOUNDARY;
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

static bool scan_horizontal_blanks(TSLexer *lexer);

static bool scan_horizontal_layout(TSLexer *lexer);

static bool
scan_compound_list_boundary(const struct Scanner *scanner, TSLexer *lexer) {
  lexer->mark_end(lexer);

  bool crossed_layout = is_horizontal_blank(lexer->lookahead);
  if (crossed_layout && !scan_horizontal_layout(lexer)) {
    return false;
  }

  if (
    (!crossed_layout && lexer_at_eof(lexer)) ||
    lexer->lookahead ==
    ')' ||
    lexer->lookahead ==
    '}' ||
    is_active_backquote_boundary(scanner, lexer->lookahead)
  ) {
    lexer->result_symbol = COMPOUND_LIST_BOUNDARY;
    return true;
  }

  if (lexer->lookahead == ';') {
    if (!scan_case_item_terminator(lexer)) {
      return false;
    }
    lexer->result_symbol = COMPOUND_LIST_BOUNDARY;
    return true;
  }

  if (!is_lowercase_letter(lexer->lookahead)) {
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
  enum TokenType symbol
) {
  lexer->mark_end(lexer);

  /*
   * These recoveries synchronize from word positions, where a right brace is
   * ordinary word source rather than a closer, so they never stop at one.
   */
  bool is_boundary = lexer_at_eof(lexer) ||
    lexer->lookahead ==
    ')' ||
    is_active_backquote_boundary(scanner, lexer->lookahead);
  if (symbol != FOR_TAIL_RECOVERY_BOUNDARY) {
    is_boundary =
      is_boundary || lexer->lookahead == '\n' || lexer->lookahead == '#';
  }
  if (symbol == HEADER_RECOVERY_BOUNDARY) {
    is_boundary =
      is_boundary || lexer->lookahead == ';' || lexer->lookahead == '&';
  }

  if (!is_boundary) {
    return false;
  }

  lexer->result_symbol = (TSSymbol)symbol;
  return true;
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
      lexer->advance(lexer, false);
      if (lexer->lookahead != '\n') {
        has_valid_layout = false;
        break;
      }
      lexer->advance(lexer, false);
      continue;
    }

    if (lexer->lookahead == '#') {
      do {
        lexer->advance(lexer, false);
      } while (!lexer_at_eof(lexer) && lexer->lookahead != '\n');
    }

    if (lexer->lookahead != '\n') {
      break;
    }
    lexer->advance(lexer, false);
  }

  bool has_function_body = has_valid_layout && lexer->lookahead == '(';
  if (has_valid_layout && lexer->lookahead == '{') {
    bool has_valid_continuations = true;
    lexer->advance(lexer, false);
    while (lexer->lookahead == '\\') {
      lexer->advance(lexer, false);
      if (lexer->lookahead != '\n') {
        has_valid_continuations = false;
        break;
      }
      lexer->advance(lexer, false);
    }
    has_function_body =
      has_valid_continuations && is_token_delimiter(scanner, lexer);
  } else if (has_valid_layout && is_lowercase_letter(lexer->lookahead)) {
    char word[6];
    if (read_reserved_word(scanner, lexer, word)) {
      has_function_body = is_function_body_reserved_word(word);
    }
  }

  enum TokenType symbol = has_function_body
    ? FUNCTION_BODY_CONTINUATION_BOUNDARY
    : FUNCTION_BODY_RECOVERY_BOUNDARY;
  if (!valid_symbols[symbol]) {
    return false;
  }

  lexer->result_symbol = (TSSymbol)symbol;
  return true;
}

static bool scan_recovery_boundary(
  const struct Scanner *scanner,
  TSLexer *lexer,
  enum TokenType symbol,
  const bool *valid_symbols
) {
  lexer->mark_end(lexer);

  if (
    (symbol ==
      COMPOUND_COMMAND_RECOVERY_BOUNDARY ||
      symbol ==
      SUBSHELL_RECOVERY_BOUNDARY ||
      symbol ==
      CASE_ITEMS_RECOVERY_BOUNDARY ||
      symbol == SEPARATOR_RECOVERY) &&
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
      symbol ==
      MISSING_COMMAND_RECOVERY_BOUNDARY ||
      symbol == BOUNDARY_COMMAND_RECOVERY);
  if (symbol == SEPARATOR_RECOVERY) {
    is_direct_boundary = is_direct_boundary ||
      lexer->lookahead ==
      ')' ||
      is_active_backquote_boundary(scanner, lexer->lookahead);
  }
  if (
    symbol ==
    COMPOUND_COMMAND_RECOVERY_BOUNDARY ||
    symbol ==
    SUBSHELL_RECOVERY_BOUNDARY ||
    symbol == CASE_ITEMS_RECOVERY_BOUNDARY
  ) {
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
        lexer->lookahead ==
        '\n' ||
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
    lexer->result_symbol =
      symbol == SEPARATOR_RECOVERY ? SEPARATOR_RECOVERY : reserved_symbol;
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
  enum TokenType symbol
) {
  lexer->mark_end(lexer);

  if (symbol == PARAMETER_EXPANSION_RECOVERY_BOUNDARY) {
    if (!lexer_at_eof(lexer)) {
      return false;
    }
    lexer->result_symbol = PARAMETER_EXPANSION_RECOVERY_BOUNDARY;
    return true;
  }

  int32_t character = lexer->lookahead;
  bool is_outer_boundary = lexer_at_eof(lexer) ||
    character ==
    '\n' ||
    character ==
    ' ' ||
    character ==
    '\t' ||
    character ==
    ')' ||
    is_active_backquote_boundary(scanner, character) ||
    character ==
    ';' ||
    character == '&';
  bool includes_closing_brace = symbol ==
    PARAMETER_MISSING_RECOVERY_BOUNDARY ||
    symbol == PARAMETER_OPERATOR_RECOVERY_BOUNDARY;
  if (is_outer_boundary || (includes_closing_brace && character == '}')) {
    lexer->result_symbol = (TSSymbol)symbol;
    return true;
  }

  /*
   * Inside a double-quoted expansion an inner double quote opens an ordinary
   * quoted string, so it ends the expansion for recovery only when that
   * quoted string never terminates.
   */
  bool includes_double_quote = symbol != PARAMETER_TAIL_RECOVERY_BOUNDARY;
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

static bool scan_horizontal_layout(TSLexer *lexer) {
  while (true) {
    scan_horizontal_blanks(lexer);
    if (lexer->lookahead != '\\') {
      return true;
    }

    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return false;
    }

    lexer->advance(lexer, false);
  }
}

static bool scan_command_continuation_operator(TSLexer *lexer) {
  if (lexer->lookahead == '|') {
    lexer->result_symbol = COMMAND_CONTINUATION;
    return true;
  }

  if (lexer->lookahead != '&') {
    return false;
  }

  lexer->advance(lexer, false);
  if (lexer->lookahead != '&') {
    return false;
  }

  lexer->result_symbol = COMMAND_CONTINUATION;
  return true;
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
    if (ends_with_right_brace) {
      lexer->mark_end(lexer);
    }
  }

  if (character_count < 2 || !ends_with_right_brace) {
    return false;
  }

  if (!skip_line_continuations(lexer)) {
    return false;
  }

  if (lexer->lookahead != '<' && lexer->lookahead != '>') {
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

  int32_t character = lexer->lookahead;
  if (crossed_layout && valid_symbols[COMPOUND_LIST_BOUNDARY]) {
    if (
      character ==
      ')' ||
      character ==
      '}' ||
      is_active_backquote_boundary(scanner, character)
    ) {
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

    if (is_lowercase_letter(character)) {
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
  }

  if (character == '|') {
    if (valid_symbols[PATTERN_CONTINUATION]) {
      lexer->result_symbol = PATTERN_CONTINUATION;
      return true;
    }
    if (valid_symbols[COMMAND_CONTINUATION]) {
      return scan_command_continuation_operator(lexer);
    }
  }

  if (character == '&' && valid_symbols[COMMAND_CONTINUATION]) {
    return scan_command_continuation_operator(lexer);
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
    scan_case_item_ns_boundary(scanner, lexer)
  ) {
    return true;
  }

  if (character == '#' && (crossed_layout || !valid_symbols[LITERAL_HASH])) {
    if (valid_symbols[COMMENT_BOUNDARY]) {
      lexer->result_symbol = COMMENT_BOUNDARY;
      return true;
    }
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

  bool closed_command_is_valid = valid_symbols[CLOSED_COMMAND_END] ||
    valid_symbols[CLOSED_SIMPLE_COMMAND_END];
  if (closed_command_is_valid) {
    /*
     * A right brace ends a closed command only where no further word source
     * may follow. A pending simple-command end or a pending word separator
     * marks the word positions where the brace stays ordinary word source.
     */
    bool is_closer = (valid_symbols[CLOSED_COMMAND_END] &&
                       scanner->backquote_depth ==
                       0 &&
                       lexer_at_eof(lexer)) ||
      character ==
      ')' ||
      (character ==
        '}' &&
        valid_symbols[CLOSED_COMMAND_END] &&
        !valid_symbols[CLOSED_SIMPLE_COMMAND_END] &&
        !valid_symbols[WORD_SEPARATOR_LINE_CONTINUATION]) ||
      is_active_backquote_boundary(scanner, character);
    if (!is_closer && character == ';') {
      is_closer = scan_case_item_terminator(lexer);
    }
    if (is_closer) {
      lexer->result_symbol = valid_symbols[CLOSED_SIMPLE_COMMAND_END]
        ? CLOSED_SIMPLE_COMMAND_END
        : CLOSED_COMMAND_END;
      return true;
    }
  }

  if (is_lowercase_letter(character)) {
    if (crossed_layout || valid_symbols[CLOSED_SIMPLE_COMMAND_END]) {
      return false;
    }

    char word[6];
    bool has_reserved_word = read_reserved_word(scanner, lexer, word);
    if (has_reserved_word) {
      if (
        valid_symbols[CLOSED_COMMAND_END] && is_recovery_reserved_word(word)
      ) {
        lexer->result_symbol = CLOSED_COMMAND_END;
        return true;
      }

      TSSymbol reserved_symbol;
      if (classify_reserved_word(word, valid_symbols, &reserved_symbol)) {
        lexer->result_symbol = reserved_symbol;
        return true;
      }
    }
  }

  return false;
}

static bool
scan_here_document_line_end(struct Scanner *scanner, TSLexer *lexer) {
  if (scanner->pending_count == 0 || scanner->sequence_end_pending) {
    return false;
  }

  lexer->mark_end(lexer);

  if (lexer->lookahead != '\n') {
    return false;
  }

  lexer->advance(lexer, false);
  if (!activate_pending_documents(scanner)) {
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

  do {
    lexer->advance(lexer, false);
  } while (!lexer_at_eof(lexer) && lexer->lookahead != '\n');

  lexer->mark_end(lexer);
  lexer->result_symbol = COMMENT;
  return true;
}

static enum ArithmeticOperatorCategory
classify_arithmetic_operator(int32_t first, int32_t second, int32_t third);
static bool is_arithmetic_operator_start(int32_t character);
static bool is_arithmetic_operand_start(int32_t character);

static bool classify_arithmetic_operand(
  const bool *valid_symbols,
  int32_t character,
  bool crossed_layout,
  enum TokenType plus_token,
  enum TokenType minus_token,
  enum TokenType generic_token,
  TSSymbol *symbol
) {
  if (!is_arithmetic_operand_start(character)) {
    return false;
  }

  if (valid_symbols[plus_token] && (crossed_layout || character != '+')) {
    *symbol = (TSSymbol)plus_token;
    return true;
  }

  if (valid_symbols[minus_token] && (crossed_layout || character != '-')) {
    *symbol = (TSSymbol)minus_token;
    return true;
  }

  if (valid_symbols[generic_token]) {
    *symbol = (TSSymbol)generic_token;
    return true;
  }

  return false;
}
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

static bool
scan_exact_line_continuation(TSLexer *lexer, enum TokenType symbol) {
  if (lexer->lookahead != '\\') {
    return false;
  }

  lexer->advance(lexer, false);
  return finish_line_continuation(lexer, symbol);
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

static bool is_arithmetic_juxtaposed_operand_start(int32_t character) {
  return (
    is_name_character(character) ||
    character ==
    '$' ||
    character ==
    '`' ||
    character == '('
  );
}

/*
 * Structured POSIX arithmetic never places two operands side by side, so an
 * operand run that is followed, across arithmetic layout, by another operand
 * start can only belong to the flat runtime interpretation. Emitting an
 * operand boundary there would remove the one interpretation that can hold
 * the source.
 */
static bool arithmetic_layout_reaches_operand_start(TSLexer *lexer) {
  while (true) {
    if (is_horizontal_blank(lexer->lookahead) || lexer->lookahead == '\n') {
      lexer->advance(lexer, false);
      continue;
    }
    if (lexer->lookahead == '\\') {
      lexer->advance(lexer, false);
      if (lexer->lookahead != '\n') {
        return false;
      }
      lexer->advance(lexer, false);
      continue;
    }
    break;
  }
  return is_arithmetic_juxtaposed_operand_start(lexer->lookahead);
}

static bool arithmetic_operand_run_is_juxtaposed(TSLexer *lexer) {
  if (lexer->lookahead == '(') {
    size_t depth = 0;
    do {
      if (lexer->lookahead == '(') {
        depth += 1;
      } else if (lexer->lookahead == ')') {
        depth -= 1;
      }
      lexer->advance(lexer, false);
      if (depth > 0 && lexer_at_eof(lexer)) {
        return false;
      }
    } while (depth > 0);
  } else {
    while (is_name_character(lexer->lookahead)) {
      lexer->advance(lexer, false);
    }
  }
  return arithmetic_layout_reaches_operand_start(lexer);
}

static bool scan_arithmetic_layout(TSLexer *lexer, bool *crossed_layout) {
  while (true) {
    while (
      lexer->lookahead ==
      ' ' ||
      lexer->lookahead ==
      '\t' ||
      lexer->lookahead == '\n'
    ) {
      if (crossed_layout != NULL) {
        *crossed_layout = true;
      }
      lexer->advance(lexer, false);
    }

    if (lexer->lookahead != '\\') {
      return true;
    }

    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return false;
    }
    lexer->advance(lexer, false);
  }
}

static bool
scan_arithmetic_boundary(TSLexer *lexer, const bool *valid_symbols) {
  lexer->mark_end(lexer);
  bool crossed_layout = false;
  if (!scan_arithmetic_layout(lexer, &crossed_layout)) {
    return false;
  }

  if (valid_symbols[ARITHMETIC_CLOSING_BOUNDARY] && lexer->lookahead == ')') {
    lexer->result_symbol = ARITHMETIC_CLOSING_BOUNDARY;
    return true;
  }

  TSSymbol symbol;
  if (
    classify_arithmetic_operand(
      valid_symbols,
      lexer->lookahead,
      crossed_layout,
      ARITHMETIC_PLUS_OPERAND_BOUNDARY,
      ARITHMETIC_MINUS_OPERAND_BOUNDARY,
      ARITHMETIC_OPERAND_BOUNDARY,
      &symbol
    )
  ) {
    if (
      (is_name_character(lexer->lookahead) || lexer->lookahead == '(') &&
      arithmetic_operand_run_is_juxtaposed(lexer)
    ) {
      return false;
    }
    lexer->result_symbol = symbol;
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
 * parenthesis, so the parser only ever pursues one interpretation. Nested
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

#define VALIDATION_EMBEDDED_NESTING_LIMIT 2048
#define VALIDATION_STRUCTURED_DEPTH_LIMIT 256

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
  if (tokens->length == tokens->capacity) {
    size_t capacity = tokens->capacity == 0 ? 64 : tokens->capacity * 2;
    if (
      capacity <
      tokens->capacity ||
      capacity >
      SIZE_MAX /
      sizeof(struct ValidationToken)
    ) {
      return false;
    }

    struct ValidationToken *resized =
      ts_realloc(tokens->data, capacity * sizeof(struct ValidationToken));
    if (resized == NULL) {
      return false;
    }
    tokens->data = resized;
    tokens->capacity = capacity;
  }

  tokens->data[tokens->length] =
    (struct ValidationToken){.kind = kind, .category = category};
  tokens->length += 1;
  return true;
}

/*
 * Skip one embedded construct whose interior POSIX excludes from the
 * enclosing scan: a "$(...)" or "${...}" body, and the quoted regions and
 * plain backquote substitutions that appear inside them. The closer stack
 * mirrors which delimiter is currently open.
 */
static enum ArithmeticValidation
skip_embedded_construct(TSLexer *lexer, char initial_closer) {
  char closers[VALIDATION_EMBEDDED_NESTING_LIMIT];
  size_t depth = 1;
  bool at_word = false;
  closers[0] = initial_closer;

  while (depth > 0) {
    if (lexer_at_eof(lexer)) {
      return ARITHMETIC_VALIDATION_INCOMPLETE;
    }

    char closer = closers[depth - 1];
    int32_t character = lexer->lookahead;

    if (closer == '`') {
      lexer->advance(lexer, false);
      if (character == '\\') {
        if (lexer_at_eof(lexer)) {
          return ARITHMETIC_VALIDATION_INCOMPLETE;
        }
        lexer->advance(lexer, false);
      } else if (character == '`') {
        depth -= 1;
      }
      continue;
    }

    if (closer == '"') {
      if (character == '\\') {
        lexer->advance(lexer, false);
        if (lexer_at_eof(lexer)) {
          return ARITHMETIC_VALIDATION_INCOMPLETE;
        }
        lexer->advance(lexer, false);
        continue;
      }
      if (character == '"') {
        lexer->advance(lexer, false);
        depth -= 1;
        continue;
      }
      if (character == '`') {
        if (depth == VALIDATION_EMBEDDED_NESTING_LIMIT) {
          return ARITHMETIC_VALIDATION_INVALID;
        }
        lexer->advance(lexer, false);
        closers[depth] = '`';
        depth += 1;
        continue;
      }
      if (character == '$') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '(' || lexer->lookahead == '{') {
          if (depth == VALIDATION_EMBEDDED_NESTING_LIMIT) {
            return ARITHMETIC_VALIDATION_INVALID;
          }
          closers[depth] = lexer->lookahead == '(' ? ')' : '}';
          depth += 1;
          lexer->advance(lexer, false);
        }
        continue;
      }
      lexer->advance(lexer, false);
      continue;
    }

    if (character == '\\') {
      lexer->advance(lexer, false);
      if (lexer_at_eof(lexer)) {
        return ARITHMETIC_VALIDATION_INCOMPLETE;
      }
      lexer->advance(lexer, false);
      at_word = true;
      continue;
    }

    if (character == '\'') {
      lexer->advance(lexer, false);
      while (!lexer_at_eof(lexer) && lexer->lookahead != '\'') {
        lexer->advance(lexer, false);
      }
      if (lexer_at_eof(lexer)) {
        return ARITHMETIC_VALIDATION_INCOMPLETE;
      }
      lexer->advance(lexer, false);
      at_word = true;
      continue;
    }

    if (character == '"' || character == '`') {
      if (depth == VALIDATION_EMBEDDED_NESTING_LIMIT) {
        return ARITHMETIC_VALIDATION_INVALID;
      }
      lexer->advance(lexer, false);
      closers[depth] = (char)character;
      depth += 1;
      at_word = true;
      continue;
    }

    if (character == '$') {
      lexer->advance(lexer, false);
      at_word = true;
      if (lexer->lookahead == '(' || lexer->lookahead == '{') {
        if (depth == VALIDATION_EMBEDDED_NESTING_LIMIT) {
          return ARITHMETIC_VALIDATION_INVALID;
        }
        closers[depth] = lexer->lookahead == '(' ? ')' : '}';
        depth += 1;
        lexer->advance(lexer, false);
        continue;
      }
      if (lexer->lookahead == '\'') {
        lexer->advance(lexer, false);
        while (true) {
          if (lexer_at_eof(lexer)) {
            return ARITHMETIC_VALIDATION_INCOMPLETE;
          }
          if (lexer->lookahead == '\\') {
            lexer->advance(lexer, false);
            if (lexer_at_eof(lexer)) {
              return ARITHMETIC_VALIDATION_INCOMPLETE;
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

    if (character == '#' && closer == ')' && !at_word) {
      while (!lexer_at_eof(lexer) && lexer->lookahead != '\n') {
        lexer->advance(lexer, false);
      }
      continue;
    }

    if (character == '(' && closer == ')') {
      if (depth == VALIDATION_EMBEDDED_NESTING_LIMIT) {
        return ARITHMETIC_VALIDATION_INVALID;
      }
      lexer->advance(lexer, false);
      closers[depth] = ')';
      depth += 1;
      at_word = false;
      continue;
    }

    if (character == ')' && closer == ')') {
      lexer->advance(lexer, false);
      depth -= 1;
      at_word = true;
      continue;
    }

    if (character == '}' && closer == '}') {
      lexer->advance(lexer, false);
      depth -= 1;
      at_word = true;
      continue;
    }

    lexer->advance(lexer, false);
    at_word = character !=
      ' ' &&
      character !=
      '\t' &&
      character !=
      '\n' &&
      character !=
      ';' &&
      character !=
      '&' &&
      character !=
      '|' &&
      character !=
      '<' &&
      character != '>';
  }

  return ARITHMETIC_VALIDATION_VALID;
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

      while (lexer->lookahead == '\\') {
        lexer->advance(lexer, false);
        if (lexer->lookahead != '\n') {
          return ARITHMETIC_VALIDATION_INVALID;
        }
        lexer->advance(lexer, false);
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

static size_t structured_parse_expression(
  const struct StructuredValidation *validation,
  size_t index,
  size_t depth
);

static size_t structured_parse_lvalue(
  const struct StructuredValidation *validation,
  size_t index,
  size_t depth
) {
  if (depth > VALIDATION_STRUCTURED_DEPTH_LIMIT || index >= validation->count) {
    return SIZE_MAX;
  }

  if (validation->tokens[index].kind == VALIDATION_TOKEN_VARIABLE) {
    return index + 1;
  }

  if (validation->tokens[index].kind == VALIDATION_TOKEN_LEFT_PARENTHESIS) {
    size_t next = structured_parse_lvalue(validation, index + 1, depth + 1);
    if (
      next !=
      SIZE_MAX &&
      next <
      validation->count &&
      validation->tokens[next].kind == VALIDATION_TOKEN_RIGHT_PARENTHESIS
    ) {
      return next + 1;
    }
  }

  return SIZE_MAX;
}

static size_t structured_parse_primary(
  const struct StructuredValidation *validation,
  size_t index,
  size_t depth
) {
  if (depth > VALIDATION_STRUCTURED_DEPTH_LIMIT || index >= validation->count) {
    return SIZE_MAX;
  }

  switch (validation->tokens[index].kind) {
  case VALIDATION_TOKEN_NUMBER:
  case VALIDATION_TOKEN_VARIABLE:
  case VALIDATION_TOKEN_EXPANSION:
    return index + 1;
  case VALIDATION_TOKEN_LEFT_PARENTHESIS: {
    size_t next = structured_parse_expression(validation, index + 1, depth + 1);
    if (
      next !=
      SIZE_MAX &&
      next <
      validation->count &&
      validation->tokens[next].kind == VALIDATION_TOKEN_RIGHT_PARENTHESIS
    ) {
      return next + 1;
    }
    return SIZE_MAX;
  }
  default:
    return SIZE_MAX;
  }
}

static size_t structured_parse_unary(
  const struct StructuredValidation *validation,
  size_t index,
  size_t depth
) {
  while (
    index <
    validation->count &&
    validation->tokens[index].kind ==
    VALIDATION_TOKEN_OPERATOR &&
    (validation->tokens[index].category ==
      ARITHMETIC_OPERATOR_CATEGORY_ADDITIVE ||
      validation->tokens[index].category ==
      VALIDATION_OPERATOR_BANG ||
      validation->tokens[index].category == VALIDATION_OPERATOR_TILDE)
  ) {
    index += 1;
  }

  return structured_parse_primary(validation, index, depth);
}

static size_t structured_parse_binary(
  const struct StructuredValidation *validation,
  size_t index,
  uint8_t level,
  size_t depth
) {
  if (level > ARITHMETIC_OPERATOR_CATEGORY_MULTIPLICATIVE) {
    return structured_parse_unary(validation, index, depth);
  }

  index = structured_parse_binary(validation, index, level + 1, depth);
  while (
    index != SIZE_MAX && validation_token_is_operator(validation, index, level)
  ) {
    size_t next =
      structured_parse_binary(validation, index + 1, level + 1, depth);
    if (next == SIZE_MAX) {
      return SIZE_MAX;
    }
    index = next;
  }

  return index;
}

static size_t structured_parse_conditional(
  const struct StructuredValidation *validation,
  size_t index,
  size_t depth
) {
  if (depth > VALIDATION_STRUCTURED_DEPTH_LIMIT) {
    return SIZE_MAX;
  }

  index = structured_parse_binary(
    validation,
    index,
    ARITHMETIC_OPERATOR_CATEGORY_LOGICAL_OR,
    depth
  );
  if (index == SIZE_MAX) {
    return SIZE_MAX;
  }

  if (
    validation_token_is_operator(
      validation,
      index,
      ARITHMETIC_OPERATOR_CATEGORY_QUESTION
    )
  ) {
    size_t consequence =
      structured_parse_expression(validation, index + 1, depth + 1);
    if (
      consequence ==
      SIZE_MAX ||
      !validation_token_is_operator(
        validation,
        consequence,
        ARITHMETIC_OPERATOR_CATEGORY_COLON
      )
    ) {
      return SIZE_MAX;
    }
    return structured_parse_conditional(validation, consequence + 1, depth + 1);
  }

  return index;
}

static size_t structured_parse_expression(
  const struct StructuredValidation *validation,
  size_t index,
  size_t depth
) {
  if (depth > VALIDATION_STRUCTURED_DEPTH_LIMIT) {
    return SIZE_MAX;
  }

  size_t after_lvalue = structured_parse_lvalue(validation, index, depth + 1);
  if (
    after_lvalue !=
    SIZE_MAX &&
    validation_token_is_operator(
      validation,
      after_lvalue,
      ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT
    )
  ) {
    size_t next =
      structured_parse_expression(validation, after_lvalue + 1, depth + 1);
    if (next != SIZE_MAX) {
      return next;
    }
  }

  return structured_parse_conditional(validation, index, depth);
}

static bool scan_arithmetic_left_parenthesis(
  const struct Scanner *scanner,
  TSLexer *lexer
) {
  lexer->advance(lexer, false);
  lexer->mark_end(lexer);

  struct ValidationTokenBuffer tokens = {0};
  bool has_expansion = false;
  enum ArithmeticValidation result =
    validate_arithmetic_content(scanner, lexer, &tokens, &has_expansion);

  bool is_arithmetic = result == ARITHMETIC_VALIDATION_INCOMPLETE;
  if (result == ARITHMETIC_VALIDATION_VALID) {
    struct StructuredValidation validation = {
      .tokens = tokens.data,
      .count = tokens.length,
    };
    is_arithmetic = has_expansion ||
      structured_parse_expression(&validation, 0, 0) == tokens.length;
  }

  ts_free(tokens.data);
  if (!is_arithmetic) {
    return false;
  }

  lexer->result_symbol = ARITHMETIC_LEFT_PARENTHESIS;
  return true;
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
      valid_symbols[BACKQUOTE_END_PREFIX] ||
      valid_symbols[CLOSED_COMMAND_END]);
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

  if (
    valid_symbols[CLOSED_COMMAND_END] &&
    !valid_symbols[BACKQUOTE_END_PREFIX] &&
    classify_backquote_tick_prefix(
      scanner->backquote_depth,
      escape_count,
      false,
      true
    ) == BACKQUOTE_TICK_PREFIX_END
  ) {
    lexer->result_symbol = CLOSED_COMMAND_END;
    return true;
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

static bool is_word_separator_element_start(
  const struct Scanner *scanner,
  const TSLexer *lexer,
  const bool *valid_symbols
) {
  int32_t character = lexer->lookahead;
  if (
    lexer_at_eof(lexer) ||
    character ==
    '\n' ||
    character ==
    '#' ||
    character ==
    ';' ||
    character ==
    '&' ||
    character ==
    '|' ||
    character ==
    '(' ||
    character == ')'
  ) {
    return false;
  }

  bool closing_brace_is_valid = valid_symbols[RIGHT_BRACE] ||
    valid_symbols[CLOSED_COMMAND_END] ||
    valid_symbols[CLOSED_SIMPLE_COMMAND_END];
  if (character == '}' && closing_brace_is_valid) {
    return false;
  }

  return !is_active_backquote_boundary(scanner, character);
}

static bool scan_word_separator_line_continuation(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  if (
    !valid_symbols[WORD_SEPARATOR_LINE_CONTINUATION] || lexer->lookahead != '\\'
  ) {
    return false;
  }

  lexer->mark_end(lexer);
  lexer->advance(lexer, false);
  if (lexer->lookahead != '\n') {
    if (backquote_prefix_token_is_valid(scanner, valid_symbols)) {
      return scan_backquote_prefix_after_first_backslash(
        scanner,
        lexer,
        valid_symbols
      );
    }
    return false;
  }

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);

  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      goto generic_line_continuation;
    }
    lexer->advance(lexer, false);
  }

  if (!scan_horizontal_blanks(lexer)) {
    goto generic_line_continuation;
  }

  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      if (!lexer_at_eof(lexer)) {
        lexer->result_symbol = WORD_SEPARATOR_LINE_CONTINUATION;
        return true;
      }
      goto generic_line_continuation;
    }
    lexer->advance(lexer, false);
    scan_horizontal_blanks(lexer);
  }

  if (is_word_separator_element_start(scanner, lexer, valid_symbols)) {
    lexer->result_symbol = WORD_SEPARATOR_LINE_CONTINUATION;
    return true;
  }

generic_line_continuation:
  if (valid_symbols[CONNECTOR_LINE_CONTINUATION]) {
    lexer->result_symbol = CONNECTOR_LINE_CONTINUATION;
    return true;
  }
  if (!valid_symbols[LINE_CONTINUATION]) {
    return false;
  }
  lexer->result_symbol = LINE_CONTINUATION;
  return true;
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

  if (valid_symbols[HERE_DOCUMENT_BOUNDARY] && lexer->lookahead == '\n') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    if (scan_here_document_end_line(lexer, document)) {
      scanner->at_here_document_line_start = true;
      lexer->result_symbol = HERE_DOCUMENT_BOUNDARY;
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

static bool serialize_document(
  struct StateWriter *writer,
  const struct HereDocument *document
) {
  return (
    write_state_byte(
      writer,
      (document->quoted ? 1 : 0) | (document->strip_tabs ? 2 : 0)
    ) &&
    write_state_size(writer, document->delimiter_length) &&
    write_state_bytes(
      writer,
      (const char *)document->delimiter,
      document->delimiter_length
    )
  );
}

static bool serialize_captured_document(
  struct StateWriter *writer,
  const struct CapturedHereDocument *captured
) {
  const struct HereDocument *document = &captured->document;
  return (
    write_state_byte(
      writer,
      (document->quoted ? 1 : 0) | (document->strip_tabs ? 2 : 0)
    ) &&
    write_state_size(writer, captured->source_end_column) &&
    write_state_size(writer, document->delimiter_length) &&
    write_state_bytes(
      writer,
      (const char *)document->delimiter,
      document->delimiter_length
    )
  );
}

static bool serialize_document_array(
  struct StateWriter *writer,
  const struct HereDocument *documents,
  size_t count
) {
  if (!write_state_size(writer, count)) {
    return false;
  }

  for (size_t index = 0; index < count; index += 1) {
    if (!serialize_document(writer, &documents[index])) {
      return false;
    }
  }
  return true;
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
        (scanner->at_here_document_line_start ? 8 : 0)
    ) ||
    !write_state_size(writer, scanner->backquote_depth) ||
    !serialize_captured_document_array(
      writer,
      scanner->captured_documents,
      scanner->captured_count
    ) ||
    !serialize_document_array(
      writer,
      scanner->pending_documents,
      scanner->pending_count
    ) ||
    !serialize_document_array(
      writer,
      scanner->active_documents,
      scanner->active_count
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
    if (
      !write_state_byte(writer, frame->at_line_start ? 1 : 0) ||
      !serialize_document_array(writer, frame->documents, frame->count)
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

static bool deserialize_document(
  struct SerializedScannerState *state,
  struct HereDocument *document
) {
  uint8_t flags;
  size_t delimiter_length;
  if (
    !read_byte(state, &flags) ||
    (flags & ~UINT8_C(3)) !=
    0 ||
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
    .quoted = (flags & 1) != 0,
    .strip_tabs = (flags & 2) != 0,
  };
  return true;
}

static bool deserialize_captured_document(
  struct SerializedScannerState *state,
  struct CapturedHereDocument *captured
) {
  uint8_t flags;
  size_t source_end_column;
  size_t delimiter_length;
  if (
    !read_byte(state, &flags) ||
    (flags & ~UINT8_C(3)) !=
    0 ||
    !read_size(state, &source_end_column) ||
    source_end_column >
    UINT32_MAX ||
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

  *captured = (struct CapturedHereDocument){
    .document =
      {
        .delimiter = delimiter,
        .delimiter_length = delimiter_length,
        .quoted = (flags & 1) != 0,
        .strip_tabs = (flags & 2) != 0,
      },
    .source_end_column = (uint32_t)source_end_column,
  };
  return true;
}

static bool deserialize_document_array(
  struct SerializedScannerState *state,
  struct HereDocument **documents,
  size_t *actual_count
) {
  size_t count;
  if (
    !read_size(state, &count) ||
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
    if (!deserialize_document(state, &(*documents)[index])) {
      return false;
    }
    *actual_count += 1;
  }
  return true;
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
    (flags & ~UINT8_C(15)) !=
    0 ||
    !read_size(&state, &scanner->backquote_depth) ||
    !deserialize_captured_document_array(
      &state,
      &scanner->captured_documents,
      &scanner->captured_count
    ) ||
    !deserialize_document_array(
      &state,
      &scanner->pending_documents,
      &scanner->pending_count
    ) ||
    !deserialize_document_array(
      &state,
      &scanner->active_documents,
      &scanner->active_count
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
    uint8_t at_line_start;
    struct HereDocumentFrame *frame = &scanner->suspended_frames[frame_index];
    if (
      !read_byte(&state, &at_line_start) ||
      at_line_start >
      1 ||
      !deserialize_document_array(&state, &frame->documents, &frame->count)
    ) {
      return false;
    }
    frame->at_line_start = at_line_start != 0;
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

  if (
    valid_symbols[NAME_EQUALS_BEGIN] &&
    is_name_start_character(lexer->lookahead)
  ) {
    return scan_name_equals_begin_or_boundary(scanner, lexer, valid_symbols);
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
    lexer->lookahead == '\\' && valid_symbols[WORD_SEPARATOR_LINE_CONTINUATION]
  ) {
    return scan_word_separator_line_continuation(scanner, lexer, valid_symbols);
  }

  if (lexer->lookahead == '\\' && valid_symbols[CONNECTOR_LINE_CONTINUATION]) {
    return scan_exact_line_continuation(lexer, CONNECTOR_LINE_CONTINUATION);
  }

  if (
    lexer->lookahead ==
    '\\' &&
    backquote_prefix_token_is_valid(scanner, valid_symbols)
  ) {
    return scan_backquote_prefix(scanner, lexer, valid_symbols);
  }

  if (
    scanner->pending_count >
    0 &&
    valid_symbols[LINE_CONTINUATION] &&
    lexer->lookahead == '\\'
  ) {
    return scan_line_continuation(lexer, valid_symbols);
  }

  if (
    valid_symbols[HERE_DOCUMENT_LINE_END] &&
    scanner->pending_count >
    0 &&
    lexer->lookahead == '\n'
  ) {
    return scan_here_document_line_end(scanner, lexer);
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
      (valid_symbols[NEWLINE] && lexer->lookahead == '\n') ||
      (valid_symbols[HERE_DOCUMENT_BOUNDARY] && lexer->lookahead == '\n'))
  ) {
    return scan_active_here_document(scanner, lexer, valid_symbols);
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

  if (valid_symbols[ARITHMETIC_LEFT_PARENTHESIS] && lexer->lookahead == '(') {
    return scan_arithmetic_left_parenthesis(scanner, lexer);
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

  bool shell_boundary_is_valid = valid_symbols[PATTERN_CONTINUATION] ||
    valid_symbols[PATTERN_END] ||
    valid_symbols[COMMAND_CONTINUATION] ||
    valid_symbols[REDIRECT_LIST_BEGIN] ||
    valid_symbols[CLOSED_COMMAND_END] ||
    valid_symbols[CLOSED_SIMPLE_COMMAND_END] ||
    valid_symbols[CASE_ITEM_END] ||
    valid_symbols[CASE_ITEM_NS_BOUNDARY] ||
    valid_symbols[COMMENT_BOUNDARY] ||
    valid_symbols[COMPOUND_LIST_BOUNDARY];
  bool direct_hash_boundary_is_valid = lexer->lookahead ==
    '#' &&
    !valid_symbols[LITERAL_HASH] &&
    valid_symbols[COMMENT_BOUNDARY];
  bool direct_closed_boundary_is_valid = (valid_symbols[CLOSED_COMMAND_END] &&
                                           scanner->backquote_depth ==
                                           0 &&
                                           lexer_at_eof(lexer)) ||
    (lexer->lookahead ==
      '}' &&
      valid_symbols[CLOSED_COMMAND_END] &&
      !valid_symbols[CLOSED_SIMPLE_COMMAND_END] &&
      !valid_symbols[WORD_SEPARATOR_LINE_CONTINUATION]) ||
    ((valid_symbols[CLOSED_COMMAND_END] ||
       valid_symbols[CLOSED_SIMPLE_COMMAND_END]) &&
      (lexer->lookahead ==
        ')' ||
        lexer->lookahead ==
        ';' ||
        is_active_backquote_boundary(scanner, lexer->lookahead)));
  bool reserved_closed_boundary_is_valid =
    (valid_symbols[CLOSED_COMMAND_END] ||
      valid_symbols[CLOSED_SIMPLE_COMMAND_END]) &&
    is_lowercase_letter(lexer->lookahead);
  if (
    shell_boundary_is_valid &&
    (is_horizontal_blank(lexer->lookahead) ||
      (lexer->lookahead ==
        '|' &&
        (valid_symbols[PATTERN_CONTINUATION] ||
          valid_symbols[COMMAND_CONTINUATION])) ||
      (lexer->lookahead == '&' && valid_symbols[COMMAND_CONTINUATION]) ||
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
      (lexer->lookahead == '\\' && valid_symbols[REDIRECT_LIST_BEGIN]) ||
      direct_closed_boundary_is_valid ||
      reserved_closed_boundary_is_valid)
  ) {
    return scan_shell_boundary(scanner, lexer, valid_symbols);
  }

  if (valid_symbols[INVALID_CASE_TERMINATOR_START] && lexer->lookahead == ';') {
    lexer->mark_end(lexer);
    if (!scan_case_item_terminator(lexer)) {
      return false;
    }
    lexer->result_symbol = INVALID_CASE_TERMINATOR_START;
    return true;
  }

  if (
    (valid_symbols[PARAMETER_MISSING_RECOVERY_BOUNDARY] ||
      valid_symbols[PARAMETER_OPERATOR_RECOVERY_BOUNDARY] ||
      valid_symbols[PARAMETER_TAIL_RECOVERY_BOUNDARY] ||
      valid_symbols[DOUBLE_QUOTED_PARAMETER_TAIL_RECOVERY_BOUNDARY] ||
      valid_symbols[PARAMETER_EXPANSION_RECOVERY_BOUNDARY])
  ) {
    enum TokenType symbol = valid_symbols[PARAMETER_MISSING_RECOVERY_BOUNDARY]
      ? PARAMETER_MISSING_RECOVERY_BOUNDARY
      : (valid_symbols[PARAMETER_OPERATOR_RECOVERY_BOUNDARY]
            ? PARAMETER_OPERATOR_RECOVERY_BOUNDARY
            : (valid_symbols[PARAMETER_TAIL_RECOVERY_BOUNDARY]
                  ? PARAMETER_TAIL_RECOVERY_BOUNDARY
                  : (valid_symbols
                          [DOUBLE_QUOTED_PARAMETER_TAIL_RECOVERY_BOUNDARY]
                        ? DOUBLE_QUOTED_PARAMETER_TAIL_RECOVERY_BOUNDARY
                        : PARAMETER_EXPANSION_RECOVERY_BOUNDARY)));
    if (scan_parameter_expansion_recovery_boundary(scanner, lexer, symbol)) {
      return true;
    }
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
      valid_symbols[BOUNDARY_COMMAND_RECOVERY] ||
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
      scan_direct_recovery_boundary(scanner, lexer, HEADER_RECOVERY_BOUNDARY)
    ) {
      return true;
    }
    suppress_broad_recovery = is_lowercase_letter(lexer->lookahead);
  }
  if (valid_symbols[DIRECT_RECOVERY_BOUNDARY]) {
    if (
      scan_direct_recovery_boundary(scanner, lexer, DIRECT_RECOVERY_BOUNDARY)
    ) {
      return true;
    }
    suppress_broad_recovery = is_lowercase_letter(lexer->lookahead);
  }
  if (valid_symbols[FOR_TAIL_RECOVERY_BOUNDARY]) {
    if (
      scan_direct_recovery_boundary(scanner, lexer, FOR_TAIL_RECOVERY_BOUNDARY)
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
      valid_symbols[BOUNDARY_COMMAND_RECOVERY] ||
      valid_symbols[SEPARATOR_RECOVERY] ||
      valid_symbols[REDIRECTION_TARGET_RECOVERY]) &&
    !suppress_broad_recovery &&
    !(lexer->lookahead == '}' && valid_symbols[RIGHT_BRACE]) &&
    !(lexer->lookahead == '#' && valid_symbols[COMMENT_BOUNDARY]) &&
    (lexer_at_eof(lexer) ||
      lexer->lookahead ==
      ')' ||
      (lexer->lookahead ==
        '}' &&
        (valid_symbols[COMPOUND_COMMAND_RECOVERY_BOUNDARY] ||
          valid_symbols[MISSING_COMMAND_RECOVERY_BOUNDARY] ||
          valid_symbols[BOUNDARY_COMMAND_RECOVERY])) ||
      lexer->lookahead ==
      ';' ||
      lexer->lookahead ==
      '&' ||
      /*
       * A raw newline marks a missing-command position only where no rule
       * accepts a newline token; anywhere a linebreak may follow, the
       * newline itself must remain available to the grammar.
       */
      (lexer->lookahead ==
        '\n' &&
        valid_symbols[MISSING_COMMAND_RECOVERY_BOUNDARY] &&
        !valid_symbols[NEWLINE]) ||
      is_active_backquote_boundary(scanner, lexer->lookahead) ||
      lexer->lookahead ==
      '#' ||
      is_lowercase_letter(lexer->lookahead))
  ) {
    bool use_boundary_command_recovery =
      valid_symbols[BOUNDARY_COMMAND_RECOVERY] &&
      (lexer->lookahead ==
        ')' ||
        lexer->lookahead ==
        '}' ||
        is_lowercase_letter(lexer->lookahead));
    bool word_position_recovery_applies = lexer->lookahead != '}';
    enum TokenType recovery_symbol = BOUNDARY_COMMAND_RECOVERY;
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
    } else if (use_boundary_command_recovery) {
      recovery_symbol = BOUNDARY_COMMAND_RECOVERY;
    } else if (valid_symbols[MISSING_COMMAND_RECOVERY_BOUNDARY]) {
      recovery_symbol = MISSING_COMMAND_RECOVERY_BOUNDARY;
    }
    if (
      recovery_symbol ==
      MISSING_COMMAND_RECOVERY_BOUNDARY &&
      (lexer->lookahead ==
        ')' ||
        (lexer->lookahead == ';' && scan_case_item_terminator(lexer)))
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

  if (valid_symbols[PARAMETER_PATTERN_BRACKET_CHARACTER]) {
    return scan_pattern_bracket_character(
      scanner,
      lexer,
      PARAMETER_PATTERN_BRACKET_CHARACTER,
      true
    );
  }

  if (valid_symbols[PATTERN_BRACKET_CHARACTER]) {
    return scan_pattern_bracket_character(
      scanner,
      lexer,
      PATTERN_BRACKET_CHARACTER,
      false
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
