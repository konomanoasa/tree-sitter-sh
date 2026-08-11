#include "tree_sitter/parser.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef TREE_SITTER_SERIALIZATION_BUFFER_SIZE
#define TREE_SITTER_SERIALIZATION_BUFFER_SIZE 1024
#endif

#define SCANNER_SERIALIZATION_VERSION 11
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
  RESERVED_WORD_NONFINAL_SEGMENT,
  RESERVED_WORD_FINAL_SEGMENT,
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
  CONTINUED_LINE_CONTINUATION,
  BLANK_LINE_START_LINE_CONTINUATION,
  SPACED_LINE_CONTINUATION,
  LAYOUT_LINE_CONTINUATION,
  COMMAND_BOUNDARY_LINE_CONTINUATION,
  PATTERN_CONTINUATION_LINE_CONTINUATION,
  PATTERN_END_LINE_CONTINUATION,
  PATTERN_BRACKET_CLOSING_LINE_CONTINUATION,
  CASE_ITEM_END_LINE_CONTINUATION,
  RESERVED_WORD_SEPARATOR_LINE_CONTINUATION,
  SEPARATOR_BOUNDARY_LINE_CONTINUATION,
  WORD_SEPARATOR_LINE_CONTINUATION,
  SOURCE_LINE_CONTINUATION,
  COMMAND_SUBSTITUTION_END_LINE_CONTINUATION,
  BACKQUOTE_END_LINE_CONTINUATION,
  NAME_LINE_CONTINUATION,
  TILDE_USER_LINE_CONTINUATION,
  DIGIT_LINE_CONTINUATION,
  SECOND_LEFT_PARENTHESIS_START_LINE_CONTINUATION,
  ARITHMETIC_CLOSING_LINE_CONTINUATION,
  ARITHMETIC_ASSIGNMENT_OPERATOR_LINE_CONTINUATION,
  ARITHMETIC_QUESTION_OPERATOR_LINE_CONTINUATION,
  ARITHMETIC_COLON_OPERATOR_LINE_CONTINUATION,
  ARITHMETIC_LOGICAL_OR_OPERATOR_LINE_CONTINUATION,
  ARITHMETIC_LOGICAL_AND_OPERATOR_LINE_CONTINUATION,
  ARITHMETIC_BITWISE_OR_OPERATOR_LINE_CONTINUATION,
  ARITHMETIC_BITWISE_XOR_OPERATOR_LINE_CONTINUATION,
  ARITHMETIC_BITWISE_AND_OPERATOR_LINE_CONTINUATION,
  ARITHMETIC_EQUALITY_OPERATOR_LINE_CONTINUATION,
  ARITHMETIC_RELATIONAL_OPERATOR_LINE_CONTINUATION,
  ARITHMETIC_SHIFT_OPERATOR_LINE_CONTINUATION,
  ARITHMETIC_ADDITIVE_OPERATOR_LINE_CONTINUATION,
  ARITHMETIC_MULTIPLICATIVE_OPERATOR_LINE_CONTINUATION,
  ARITHMETIC_PLUS_OPERAND_LINE_CONTINUATION,
  ARITHMETIC_MINUS_OPERAND_LINE_CONTINUATION,
  ARITHMETIC_OPERAND_LINE_CONTINUATION,
  ASSIGNMENT_NAME_END_LINE_CONTINUATION,
  CONTINUED_DECIMAL_ARITHMETIC_NUMBER_START,
  CONTINUED_OCTAL_ARITHMETIC_NUMBER_START,
  CONTINUED_HEXADECIMAL_ARITHMETIC_NUMBER_START,
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
  SOURCE_WORD_CONTINUATION_BOUNDARY,
  ASSIGNMENT_VALUE_END_LINE_CONTINUATION,
  LINE_CONTINUATION_BOUNDARY,
  CONTINUED_PARAMETER_PATTERN_OPERATOR_START,
  CONTINUED_REDIRECTION_OPERATOR_START,
  CONTINUED_DLESSDASH_START,
  CONTINUED_DOLLAR_EXPANSION_START,
  CONTINUED_DOLLAR_SINGLE_QUOTE_START,
  PATTERN_SPECIAL_LEFT_BRACKET,
  LITERAL_HASH,
  COMMENT_BOUNDARY,
  BLANK_LINE_BOUNDARY,
  COMMENT,
  SPACED_COMMENT,
  BLANK_LINE,
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
  SEPARATOR_BEGIN,
  CLOSED_COMMAND_END,
  CLOSED_SIMPLE_COMMAND_END,
  CASE_ITEM_END,
  COMPOUND_COMMAND_RECOVERY_BOUNDARY,
  SUBSHELL_RECOVERY_BOUNDARY,
  DIRECT_RECOVERY_BOUNDARY,
  HEADER_RECOVERY_BOUNDARY,
  FOR_TAIL_RECOVERY_BOUNDARY,
  CASE_ITEMS_RECOVERY_BOUNDARY,
  EMPTY_COMPOUND_LIST_RECOVERY_BOUNDARY,
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
  WORD_BRACKET_CONTINUATION,
  PARAMETER_BRACKET_CONTINUATION,
  PATTERN_BRACKET_CHARACTER,
  PARAMETER_PATTERN_BRACKET_CHARACTER,
  PATTERN_BRACKET_HYPHEN,
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

struct ArithmeticOperatorTokens {
  enum TokenType boundary;
  enum TokenType line_continuation;
};

static const struct ArithmeticOperatorTokens
  ARITHMETIC_OPERATOR_TOKENS[ARITHMETIC_OPERATOR_CATEGORY_COUNT] = {
    [ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT] =
      {
        ARITHMETIC_ASSIGNMENT_OPERATOR_BOUNDARY,
        ARITHMETIC_ASSIGNMENT_OPERATOR_LINE_CONTINUATION,
      },
    [ARITHMETIC_OPERATOR_CATEGORY_QUESTION] =
      {
        ARITHMETIC_QUESTION_OPERATOR_BOUNDARY,
        ARITHMETIC_QUESTION_OPERATOR_LINE_CONTINUATION,
      },
    [ARITHMETIC_OPERATOR_CATEGORY_COLON] =
      {
        ARITHMETIC_COLON_OPERATOR_BOUNDARY,
        ARITHMETIC_COLON_OPERATOR_LINE_CONTINUATION,
      },
    [ARITHMETIC_OPERATOR_CATEGORY_LOGICAL_OR] =
      {
        ARITHMETIC_LOGICAL_OR_OPERATOR_BOUNDARY,
        ARITHMETIC_LOGICAL_OR_OPERATOR_LINE_CONTINUATION,
      },
    [ARITHMETIC_OPERATOR_CATEGORY_LOGICAL_AND] =
      {
        ARITHMETIC_LOGICAL_AND_OPERATOR_BOUNDARY,
        ARITHMETIC_LOGICAL_AND_OPERATOR_LINE_CONTINUATION,
      },
    [ARITHMETIC_OPERATOR_CATEGORY_BITWISE_OR] =
      {
        ARITHMETIC_BITWISE_OR_OPERATOR_BOUNDARY,
        ARITHMETIC_BITWISE_OR_OPERATOR_LINE_CONTINUATION,
      },
    [ARITHMETIC_OPERATOR_CATEGORY_BITWISE_XOR] =
      {
        ARITHMETIC_BITWISE_XOR_OPERATOR_BOUNDARY,
        ARITHMETIC_BITWISE_XOR_OPERATOR_LINE_CONTINUATION,
      },
    [ARITHMETIC_OPERATOR_CATEGORY_BITWISE_AND] =
      {
        ARITHMETIC_BITWISE_AND_OPERATOR_BOUNDARY,
        ARITHMETIC_BITWISE_AND_OPERATOR_LINE_CONTINUATION,
      },
    [ARITHMETIC_OPERATOR_CATEGORY_EQUALITY] =
      {
        ARITHMETIC_EQUALITY_OPERATOR_BOUNDARY,
        ARITHMETIC_EQUALITY_OPERATOR_LINE_CONTINUATION,
      },
    [ARITHMETIC_OPERATOR_CATEGORY_RELATIONAL] =
      {
        ARITHMETIC_RELATIONAL_OPERATOR_BOUNDARY,
        ARITHMETIC_RELATIONAL_OPERATOR_LINE_CONTINUATION,
      },
    [ARITHMETIC_OPERATOR_CATEGORY_SHIFT] =
      {
        ARITHMETIC_SHIFT_OPERATOR_BOUNDARY,
        ARITHMETIC_SHIFT_OPERATOR_LINE_CONTINUATION,
      },
    [ARITHMETIC_OPERATOR_CATEGORY_ADDITIVE] =
      {
        ARITHMETIC_ADDITIVE_OPERATOR_BOUNDARY,
        ARITHMETIC_ADDITIVE_OPERATOR_LINE_CONTINUATION,
      },
    [ARITHMETIC_OPERATOR_CATEGORY_MULTIPLICATIVE] = {
      ARITHMETIC_MULTIPLICATIVE_OPERATOR_BOUNDARY,
      ARITHMETIC_MULTIPLICATIVE_OPERATOR_LINE_CONTINUATION,
    },
};

struct HereDocument {
  char *delimiter;
  size_t delimiter_length;
  uint32_t source_end_column;
  bool quoted;
  bool strip_tabs;
};

struct HereDocumentFrame {
  struct HereDocument *documents;
  size_t count;
  bool at_line_start;
};

struct Scanner {
  struct HereDocument *captured_documents;
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
  free(document->delimiter);
  document->delimiter = NULL;
  document->delimiter_length = 0;
  document->source_end_column = 0;
  document->quoted = false;
  document->strip_tabs = false;
}

static void
clear_document_array(struct HereDocument **documents, size_t *count) {
  for (size_t index = 0; index < *count; index += 1) {
    clear_document(&(*documents)[index]);
  }

  free(*documents);
  *documents = NULL;
  *count = 0;
}

static void clear_scanner(struct Scanner *scanner) {
  clear_document_array(&scanner->captured_documents, &scanner->captured_count);
  clear_document_array(&scanner->pending_documents, &scanner->pending_count);
  clear_document_array(&scanner->active_documents, &scanner->active_count);
  for (size_t index = 0; index < scanner->suspended_frame_count; index += 1) {
    clear_document_array(
      &scanner->suspended_frames[index].documents,
      &scanner->suspended_frames[index].count
    );
  }
  free(scanner->suspended_frames);
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
    realloc(*documents, next_count * sizeof(struct HereDocument));
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
  struct HereDocument document
) {
  if (!append_document(
        &scanner->captured_documents,
        &scanner->captured_count,
        document
      )) {
    return false;
  }

  if (!scanner_state_fits(scanner)) {
    scanner->captured_count -= 1;
    scanner->captured_documents[scanner->captured_count] =
      (struct HereDocument){0};
    return false;
  }
  return true;
}

static bool move_captured_document_to_pending(struct Scanner *scanner) {
  if (scanner->captured_count == 0) {
    return false;
  }

  struct HereDocument document =
    scanner->captured_documents[scanner->captured_count - 1];
  if (!append_pending_document(scanner, document)) {
    return false;
  }

  scanner->captured_count -= 1;
  if (!scanner_state_fits(scanner)) {
    scanner->captured_count += 1;
    scanner->pending_count -= 1;
    scanner->pending_documents[scanner->pending_count] =
      (struct HereDocument){0};
    return false;
  }

  if (scanner->captured_count == 0) {
    free(scanner->captured_documents);
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
  struct HereDocumentFrame *resized = realloc(
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
    free(scanner->suspended_frames);
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

  free(scanner->active_documents);
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

  char *resized = realloc(buffer->data, capacity);
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

static bool append_codepoint(struct ByteBuffer *buffer, int32_t character) {
  if (character <= 0x7f) {
    return append_byte(buffer, (uint8_t)character);
  }

  if (character <= 0x7ff) {
    return (
      append_byte(buffer, (uint8_t)(0xc0 | (character >> 6))) &&
      append_byte(buffer, (uint8_t)(0x80 | (character & 0x3f)))
    );
  }

  if (character <= 0xffff) {
    return (
      append_byte(buffer, (uint8_t)(0xe0 | (character >> 12))) &&
      append_byte(buffer, (uint8_t)(0x80 | ((character >> 6) & 0x3f))) &&
      append_byte(buffer, (uint8_t)(0x80 | (character & 0x3f)))
    );
  }

  return (
    append_byte(buffer, (uint8_t)(0xf0 | (character >> 18))) &&
    append_byte(buffer, (uint8_t)(0x80 | ((character >> 12) & 0x3f))) &&
    append_byte(buffer, (uint8_t)(0x80 | ((character >> 6) & 0x3f))) &&
    append_byte(buffer, (uint8_t)(0x80 | (character & 0x3f)))
  );
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

static int32_t
decode_codepoint(const char *bytes, size_t length, size_t *offset) {
  uint8_t first = (uint8_t)bytes[*offset];
  *offset += 1;

  if (first < 0x80) {
    return first;
  }

  int32_t character;
  uint8_t continuation_count;
  if ((first & 0xe0) == 0xc0) {
    character = first & 0x1f;
    continuation_count = 1;
  } else if ((first & 0xf0) == 0xe0) {
    character = first & 0x0f;
    continuation_count = 2;
  } else {
    character = first & 0x07;
    continuation_count = 3;
  }

  while (continuation_count > 0 && *offset < length) {
    character = (character << 6) | ((uint8_t)bytes[*offset] & 0x3f);
    *offset += 1;
    continuation_count -= 1;
  }

  return character;
}

static bool is_decimal_digit(int32_t character) {
  return character >= '0' && character <= '9';
}

static bool is_name_character(int32_t character) {
  return (
    (character >= 'A' && character <= 'Z') ||
    (character >= 'a' && character <= 'z') ||
    is_decimal_digit(character) ||
    character == '_'
  );
}

static bool is_name_start_character(int32_t character) {
  return (
    (character >= 'A' && character <= 'Z') ||
    (character >= 'a' && character <= 'z') ||
    character == '_'
  );
}

static bool is_tilde_user_character(int32_t character) {
  return (is_name_character(character) || character == '.' || character == '-');
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

static bool is_token_delimiter(int32_t character) {
  return (
    character ==
    0 ||
    character ==
    ' ' ||
    character ==
    '\t' ||
    character ==
    '\n' ||
    is_control_operator_start(character)
  );
}

static bool is_missing_here_document_delimiter_boundary(int32_t character) {
  return (
    character ==
    0 ||
    character ==
    '\n' ||
    character ==
    '#' ||
    character ==
    '&' ||
    character ==
    '(' ||
    character ==
    ')' ||
    character ==
    ';' ||
    character == '|'
  );
}

static bool
is_bracket_scan_boundary(int32_t character, bool parameter_pattern) {
  return (
    parameter_pattern ? character == 0 || character == '}'
                      : is_token_delimiter(character)
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
  while (!is_bracket_scan_boundary(lexer->lookahead, parameter_pattern)) {
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
        while (!is_bracket_scan_boundary(lexer->lookahead, parameter_pattern)) {
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
  TSLexer *lexer,
  enum TokenType symbol,
  bool parameter_pattern
) {
  int32_t character = lexer->lookahead;
  if (
    character ==
    0 ||
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
    (!parameter_pattern && is_token_delimiter(character))
  ) {
    return false;
  }

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  lexer->result_symbol = (TSSymbol)symbol;
  return true;
}

static bool scan_bracket_boundary(
  TSLexer *lexer,
  enum TokenType end_symbol,
  enum TokenType continuation_symbol,
  bool end_is_valid,
  bool continuation_is_valid,
  bool parameter_pattern
) {
  lexer->mark_end(lexer);

  if (lexer->lookahead == '\\' && (!end_is_valid || !continuation_is_valid)) {
    return false;
  }

  bool has_line_continuation = false;
  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return false;
    }
    lexer->advance(lexer, false);
    has_line_continuation = true;
  }

  if (is_bracket_scan_boundary(lexer->lookahead, parameter_pattern)) {
    if (!end_is_valid) {
      return false;
    }
    lexer->result_symbol = (TSSymbol)end_symbol;
    return true;
  }

  if (has_line_continuation && continuation_is_valid) {
    lexer->result_symbol = (TSSymbol)continuation_symbol;
    return true;
  }

  return false;
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

struct DelimiterWordTracker {
  char text[6];
  uint8_t length;
  size_t group_depth;
  bool active;
  bool candidate;
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
      realloc(cases->data, capacity * sizeof(struct DelimiterCaseFrame));
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
  struct ByteBuffer *group_ends,
  struct ByteBuffer *group_kinds,
  struct ByteBuffer *command_starts,
  struct ByteBuffer *parent_quotes,
  char closing,
  enum DelimiterGroupKind kind,
  enum DelimiterQuote parent_quote
) {
  size_t original_length = group_ends->length;
  if (
    !append_byte(group_ends, (uint8_t)closing) ||
    !append_byte(group_kinds, (uint8_t)kind) ||
    !append_byte(
      command_starts,
      kind == DELIMITER_GROUP_COMMAND || kind == DELIMITER_GROUP_SUBSHELL ? 1
                                                                          : 0
    ) ||
    !append_byte(parent_quotes, (uint8_t)parent_quote)
  ) {
    group_ends->length = original_length;
    group_kinds->length = original_length;
    command_starts->length = original_length;
    parent_quotes->length = original_length;
    return false;
  }
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
  word->group_depth = 0;
  word->active = false;
  word->candidate = true;
}

static bool finish_delimiter_word(
  struct DelimiterWordTracker *word,
  struct DelimiterCaseBuffer *cases,
  struct ByteBuffer *command_starts
) {
  if (!word->active || word->group_depth == 0) {
    reset_delimiter_word(word);
    return true;
  }

  size_t group_depth = word->group_depth;
  bool at_command_start = command_starts->data[group_depth - 1] != 0;
  struct DelimiterCaseFrame *active_case =
    cases->length == 0 ? NULL : &cases->data[cases->length - 1];
  if (active_case != NULL && active_case->group_depth == group_depth) {
    if (active_case->state == DELIMITER_CASE_EXPECT_WORD) {
      active_case->state = DELIMITER_CASE_EXPECT_IN;
      command_starts->data[group_depth - 1] = 0;
      reset_delimiter_word(word);
      return true;
    }

    if (active_case->state == DELIMITER_CASE_EXPECT_IN) {
      if (delimiter_word_equals(word, "in")) {
        active_case->state = DELIMITER_CASE_EXPECT_PATTERN;
      }
      command_starts->data[group_depth - 1] = 0;
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
      command_starts->data[group_depth - 1] = 0;
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
    command_starts->data[group_depth - 1] = 1;
    reset_delimiter_word(word);
    return true;
  }

  if (at_command_start && delimiter_word_equals(word, "case")) {
    if (!append_delimiter_case(cases, group_depth)) {
      return false;
    }
  }
  command_starts->data[group_depth - 1] = 0;
  reset_delimiter_word(word);
  return true;
}

static size_t
delimiter_command_group_depth(const struct ByteBuffer *group_kinds) {
  if (group_kinds->length == 0) {
    return 0;
  }

  uint8_t kind = (uint8_t)group_kinds->data[group_kinds->length - 1];
  return (kind == DELIMITER_GROUP_COMMAND || kind == DELIMITER_GROUP_SUBSHELL)
    ? group_kinds->length
    : 0;
}

static void track_delimiter_word_character(
  struct DelimiterWordTracker *word,
  size_t group_depth,
  int32_t character
) {
  if (!word->active || word->group_depth != group_depth) {
    reset_delimiter_word(word);
    word->active = true;
    word->group_depth = group_depth;
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

static bool is_octal_digit(int32_t character) {
  return character >= '0' && character <= '7';
}

static bool skip_line_continuations(TSLexer *lexer, bool *found) {
  *found = false;

  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return false;
    }
    lexer->advance(lexer, false);
    *found = true;
  }

  return true;
}

static bool
scan_dollar_logical_follower(TSLexer *lexer, bool *escaped_follower) {
  *escaped_follower = false;
  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      *escaped_follower = true;
      return lexer->lookahead != 0;
    }
    lexer->advance(lexer, false);
  }
  return true;
}

static enum TokenType scan_continued_arithmetic_number_start(TSLexer *lexer) {
  lexer->mark_end(lexer);

  bool leading_zero = lexer->lookahead == '0';
  lexer->advance(lexer, false);
  bool continuation_before_next = false;
  if (!skip_line_continuations(lexer, &continuation_before_next)) {
    return TOKEN_COUNT;
  }

  if (leading_zero && (lexer->lookahead == 'x' || lexer->lookahead == 'X')) {
    lexer->advance(lexer, false);
    bool continuation_after_prefix = false;
    if (!skip_line_continuations(lexer, &continuation_after_prefix)) {
      return TOKEN_COUNT;
    }
    if (!is_hexadecimal_digit(lexer->lookahead)) {
      return TOKEN_COUNT;
    }

    bool has_continuation =
      continuation_before_next || continuation_after_prefix;
    do {
      lexer->advance(lexer, false);
      bool continuation_before_digit = false;
      if (!skip_line_continuations(lexer, &continuation_before_digit)) {
        return TOKEN_COUNT;
      }
      if (!is_hexadecimal_digit(lexer->lookahead)) {
        break;
      }
      has_continuation = has_continuation || continuation_before_digit;
    } while (true);

    return has_continuation ? CONTINUED_HEXADECIMAL_ARITHMETIC_NUMBER_START
                            : TOKEN_COUNT;
  }

  bool has_continuation = false;
  while (
    leading_zero ? is_octal_digit(lexer->lookahead)
                 : is_decimal_digit(lexer->lookahead)
  ) {
    has_continuation = has_continuation || continuation_before_next;
    lexer->advance(lexer, false);
    if (!skip_line_continuations(lexer, &continuation_before_next)) {
      return TOKEN_COUNT;
    }
  }

  if (!has_continuation) {
    return TOKEN_COUNT;
  }

  return leading_zero ? CONTINUED_OCTAL_ARITHMETIC_NUMBER_START
                      : CONTINUED_DECIMAL_ARITHMETIC_NUMBER_START;
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

static bool
scan_dollar_single_quote_escape(TSLexer *lexer, struct ByteBuffer *delimiter) {
  int32_t character = lexer->lookahead;
  if (character == 0) {
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
    if (lexer->lookahead == 0) {
      return false;
    }

    if (lexer->lookahead == '\\') {
      lexer->advance(lexer, false);
      if (lexer->lookahead != '\\') {
        return false;
      }
      lexer->advance(lexer, false);
      return append_byte(delimiter, 0x1c);
    }

    uint8_t value = (uint8_t)lexer->lookahead;
    lexer->advance(lexer, false);
    return append_byte(delimiter, (uint8_t)(value & 0x1f));
  }

  lexer->advance(lexer, false);
  return append_quoted_escape(delimiter, character);
}

static bool
scan_backquote_delimiter_part(TSLexer *lexer, struct ByteBuffer *delimiter) {
  if (!append_byte(delimiter, '`')) {
    return false;
  }
  lexer->advance(lexer, false);

  while (lexer->lookahead != 0) {
    int32_t character = lexer->lookahead;
    if (!append_codepoint(delimiter, character)) {
      return false;
    }
    lexer->advance(lexer, false);

    if (character == '`') {
      return true;
    }

    if (character == '\\' && lexer->lookahead != 0) {
      if (!append_codepoint(delimiter, lexer->lookahead)) {
        return false;
      }
      lexer->advance(lexer, false);
    }
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
  char *delimiter = NULL;
  if (delimiter_length > 0) {
    delimiter = malloc(delimiter_length);
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

static bool scan_nested_here_document_line(
  TSLexer *lexer,
  struct ByteBuffer *source,
  const struct HereDocument *document,
  bool *is_end,
  bool *at_end_of_input
) {
  struct ByteBuffer candidate = {0};
  bool at_physical_line_start = true;
  bool valid = true;

  while (valid) {
    if (at_physical_line_start && document->strip_tabs) {
      while (lexer->lookahead == '\t') {
        valid = append_byte(source, '\t');
        if (!valid) {
          break;
        }
        lexer->advance(lexer, false);
      }
    }
    if (!valid) {
      break;
    }
    at_physical_line_start = false;

    int32_t character = lexer->lookahead;
    if (character == 0) {
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
        at_physical_line_start = true;
        continue;
      }
      valid = append_byte(&candidate, '\\');
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
  free(candidate.data);
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
  struct ByteBuffer group_ends = {0};
  struct ByteBuffer group_kinds = {0};
  struct ByteBuffer command_starts = {0};
  struct ByteBuffer parent_quotes = {0};
  struct DelimiterCaseBuffer cases = {0};
  struct DelimiterWordTracker word = {0};
  struct HereDocument *nested_documents = NULL;
  size_t nested_document_count = 0;
  size_t nested_delimiter_start = 0;
  size_t nested_delimiter_group_depth = 0;
  bool has_word_content = false;
  bool quoted = false;
  bool expecting_nested_delimiter = false;
  bool collecting_nested_delimiter = false;
  bool nested_delimiter_quoted = false;
  bool nested_delimiter_strips_tabs = false;
  bool valid = true;
  reset_delimiter_word(&word);

  while (valid) {
    int32_t character = lexer->lookahead;

    if (quote == DELIMITER_SINGLE_QUOTED) {
      if (character == 0) {
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
      if (character == 0) {
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
      if (character == 0) {
        valid = false;
      } else if (character == '"') {
        quote = DELIMITER_UNQUOTED;
        lexer->advance(lexer, false);
      } else if (character == '$') {
        lexer->advance(lexer, false);
        bool escaped_follower = false;
        valid = scan_dollar_logical_follower(lexer, &escaped_follower) &&
          append_byte(&delimiter, '$');
        if (valid && escaped_follower) {
          if (
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
          continue;
        }
        if (valid && (lexer->lookahead == '(' || lexer->lookahead == '{')) {
          int32_t opening = lexer->lookahead;
          char closing = opening == '(' ? ')' : '}';
          enum DelimiterGroupKind kind = opening == '('
            ? DELIMITER_GROUP_COMMAND
            : DELIMITER_GROUP_PARAMETER;
          valid = push_delimiter_group(
                    &group_ends,
                    &group_kinds,
                    &command_starts,
                    &parent_quotes,
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
            group_kinds.data[group_kinds.length - 1] =
              DELIMITER_GROUP_ARITHMETIC;
            command_starts.data[command_starts.length - 1] = 0;
            valid = push_delimiter_group(
                      &group_ends,
                      &group_kinds,
                      &command_starts,
                      &parent_quotes,
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
        valid = scan_backquote_delimiter_part(lexer, &delimiter);
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

    size_t command_group_depth = delimiter_command_group_depth(&group_kinds);
    if (
      collecting_nested_delimiter &&
      group_ends.length ==
      nested_delimiter_group_depth &&
      is_token_delimiter(character)
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

      if (character == '\\') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '\n') {
          lexer->advance(lexer, false);
          continue;
        }
        expecting_nested_delimiter = false;
        collecting_nested_delimiter = true;
        nested_delimiter_start = delimiter.length;
        nested_delimiter_group_depth = group_ends.length;
        if (lexer->lookahead == 0) {
          valid = false;
          continue;
        }
        has_word_content = true;
        quoted = true;
        nested_delimiter_quoted = true;
        valid = append_codepoint(&delimiter, lexer->lookahead);
        if (valid) {
          lexer->advance(lexer, false);
        }
        continue;
      }

      expecting_nested_delimiter = false;
      if (character == '#' || is_token_delimiter(character)) {
        nested_delimiter_strips_tabs = false;
      } else {
        collecting_nested_delimiter = true;
        nested_delimiter_start = delimiter.length;
        nested_delimiter_group_depth = group_ends.length;
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
      valid = finish_delimiter_word(&word, &cases, &command_starts);
      if (!valid) {
        continue;
      }

      has_word_content = true;
      valid = append_byte(&delimiter, '<');
      if (!valid) {
        continue;
      }
      lexer->advance(lexer, false);
      bool consumed_escaped_word_character = false;
      while (lexer->lookahead == '\\') {
        lexer->advance(lexer, false);
        if (lexer->lookahead != '\n') {
          if (lexer->lookahead == 0) {
            valid = false;
          } else {
            quoted = true;
            valid = append_codepoint(&delimiter, lexer->lookahead);
            if (valid) {
              lexer->advance(lexer, false);
            }
          }
          consumed_escaped_word_character = true;
          break;
        }
        lexer->advance(lexer, false);
      }
      if (!valid || consumed_escaped_word_character) {
        continue;
      }
      if (lexer->lookahead != '<') {
        continue;
      }

      valid = append_byte(&delimiter, '<');
      if (!valid) {
        continue;
      }
      lexer->advance(lexer, false);
      bool escaped_nested_delimiter_start = false;
      while (lexer->lookahead == '\\') {
        lexer->advance(lexer, false);
        if (lexer->lookahead != '\n') {
          collecting_nested_delimiter = true;
          nested_delimiter_start = delimiter.length;
          nested_delimiter_group_depth = group_ends.length;
          nested_delimiter_quoted = true;
          quoted = true;
          if (lexer->lookahead == 0) {
            valid = false;
          } else {
            valid = append_codepoint(&delimiter, lexer->lookahead);
            if (valid) {
              lexer->advance(lexer, false);
            }
          }
          escaped_nested_delimiter_start = true;
          break;
        }
        lexer->advance(lexer, false);
      }
      if (!valid || escaped_nested_delimiter_start) {
        continue;
      }
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
      reset_delimiter_word(&word);
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
      !word.active
    ) {
      do {
        has_word_content = true;
        valid = append_codepoint(&delimiter, lexer->lookahead);
        lexer->advance(lexer, false);
      } while (valid && lexer->lookahead != 0 && lexer->lookahead != '\n');

      if (valid && lexer->lookahead == '\n') {
        valid = append_byte(&delimiter, '\n');
        lexer->advance(lexer, false);
        command_starts.data[command_group_depth - 1] = 1;
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
      is_token_delimiter(character)
    ) {
      valid = finish_delimiter_word(&word, &cases, &command_starts);
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
      track_delimiter_word_character(&word, command_group_depth, character);
    }

    if (group_ends.length == 0 && is_token_delimiter(character)) {
      break;
    }

    if (!has_word_content && group_ends.length == 0 && character == '#') {
      valid = false;
      break;
    }

    if (character == '\'') {
      if (command_group_depth > 0 && !at_nested_delimiter_base) {
        track_delimiter_word_character(&word, command_group_depth, character);
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
        track_delimiter_word_character(&word, command_group_depth, character);
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
        !has_word_content && delimiter.length == 0 && group_ends.length == 0;
      if (command_group_depth > 0 && !at_nested_delimiter_base) {
        track_delimiter_word_character(&word, command_group_depth, character);
      }
      lexer->advance(lexer, false);
      if (lexer->lookahead == 0) {
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
        track_delimiter_word_character(&word, command_group_depth, character);
      }
      has_word_content = true;
      valid = scan_backquote_delimiter_part(lexer, &delimiter);
      continue;
    }

    if (character == '$') {
      if (command_group_depth > 0 && !at_nested_delimiter_base) {
        track_delimiter_word_character(&word, command_group_depth, character);
        command_starts.data[command_group_depth - 1] = 0;
        reset_delimiter_word(&word);
      }
      has_word_content = true;
      lexer->advance(lexer, false);

      bool escaped_follower = false;
      valid = scan_dollar_logical_follower(lexer, &escaped_follower);
      if (!valid) {
        continue;
      }

      if (escaped_follower) {
        valid = append_byte(&delimiter, '$');
        if (!valid) {
          continue;
        }
        quoted = true;
        if (collecting_nested_delimiter) {
          nested_delimiter_quoted = true;
        }
        valid = append_codepoint(&delimiter, lexer->lookahead);
        lexer->advance(lexer, false);
        continue;
      }

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
          !push_delimiter_group(
            &group_ends,
            &group_kinds,
            &command_starts,
            &parent_quotes,
            closing,
            kind,
            quote
          ) ||
          !append_codepoint(&delimiter, opening)
        ) {
          valid = false;
          continue;
        }

        lexer->advance(lexer, false);

        if (opening == '(' && lexer->lookahead == '(') {
          group_kinds.data[group_kinds.length - 1] = DELIMITER_GROUP_ARITHMETIC;
          command_starts.data[command_starts.length - 1] = 0;
          if (
            !push_delimiter_group(
              &group_ends,
              &group_kinds,
              &command_starts,
              &parent_quotes,
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
      group_ends.length >
      0 &&
      character == group_ends.data[group_ends.length - 1]
    ) {
      struct DelimiterCaseFrame *active_case =
        cases.length == 0 ? NULL : &cases.data[cases.length - 1];
      if (
        character ==
        ')' &&
        active_case !=
        NULL &&
        active_case->group_depth ==
        group_ends.length &&
        active_case->state == DELIMITER_CASE_EXPECT_PATTERN
      ) {
        valid = append_codepoint(&delimiter, character);
        active_case->state = DELIMITER_CASE_BODY;
        command_starts.data[group_ends.length - 1] = 1;
        lexer->advance(lexer, false);
        continue;
      }

      valid = append_codepoint(&delimiter, character);
      quote = (enum DelimiterQuote)(
        uint8_t
      )parent_quotes.data[parent_quotes.length - 1];
      while (
        cases.length >
        0 &&
        cases.data[cases.length - 1].group_depth >= group_ends.length
      ) {
        cases.length -= 1;
      }
      group_ends.length -= 1;
      group_kinds.length -= 1;
      command_starts.length -= 1;
      parent_quotes.length -= 1;
      reset_delimiter_word(&word);
      lexer->advance(lexer, false);
      continue;
    }

    if (
      group_ends.length >
      0 &&
      group_ends.data[group_ends.length - 1] ==
      ')' &&
      character == '('
    ) {
      struct DelimiterCaseFrame *active_case =
        cases.length == 0 ? NULL : &cases.data[cases.length - 1];
      if (
        active_case !=
        NULL &&
        active_case->group_depth ==
        group_ends.length &&
        active_case->state == DELIMITER_CASE_EXPECT_PATTERN
      ) {
        has_word_content = true;
        valid = append_codepoint(&delimiter, character);
        lexer->advance(lexer, false);
        continue;
      }

      enum DelimiterGroupKind parent_kind = (enum DelimiterGroupKind)(
        uint8_t
      )group_kinds.data[group_kinds.length - 1];
      enum DelimiterGroupKind nested_kind =
        parent_kind == DELIMITER_GROUP_ARITHMETIC ? DELIMITER_GROUP_ARITHMETIC
                                                  : DELIMITER_GROUP_SUBSHELL;
      if (
        parent_kind !=
        DELIMITER_GROUP_PARAMETER &&
        !push_delimiter_group(
          &group_ends,
          &group_kinds,
          &command_starts,
          &parent_quotes,
          ')',
          nested_kind,
          quote
        )
      ) {
        valid = false;
        continue;
      }
    }

    if (character == 0) {
      break;
    }

    has_word_content = true;
    valid = append_codepoint(&delimiter, character);
    lexer->advance(lexer, false);

    size_t active_command_depth = delimiter_command_group_depth(&group_kinds);
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
        command_starts.data[active_command_depth - 1] = 1;
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
    group_ends.length !=
    0 ||
    expecting_nested_delimiter ||
    collecting_nested_delimiter ||
    nested_document_count != 0
  ) {
    free(delimiter.data);
    free(group_ends.data);
    free(group_kinds.data);
    free(command_starts.data);
    free(parent_quotes.data);
    free(cases.data);
    clear_document_array(&nested_documents, &nested_document_count);
    return false;
  }

  struct HereDocument document = {
    .delimiter = delimiter.data,
    .delimiter_length = delimiter.length,
    .source_end_column = lexer->get_column(lexer),
    .quoted = quoted,
    .strip_tabs = scanner->delimiter_strips_tabs,
  };
  if (!append_captured_document(scanner, document)) {
    clear_document(&document);
    free(group_ends.data);
    free(group_kinds.data);
    free(command_starts.data);
    free(parent_quotes.data);
    free(cases.data);
    clear_document_array(&nested_documents, &nested_document_count);
    return false;
  }

  free(group_ends.data);
  free(group_kinds.data);
  free(command_starts.data);
  free(parent_quotes.data);
  free(cases.data);
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
    !is_token_delimiter(lexer->lookahead) ||
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

static bool read_reserved_character(TSLexer *lexer, int32_t character) {
  if (lexer->lookahead != character) {
    return false;
  }

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);

  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return false;
    }
    lexer->advance(lexer, false);
  }

  if (!is_token_delimiter(lexer->lookahead)) {
    return false;
  }

  return true;
}

static bool scan_reserved_character(
  TSLexer *lexer,
  int32_t character,
  enum TokenType symbol
) {
  if (!read_reserved_character(lexer, character)) {
    return false;
  }

  lexer->result_symbol = (TSSymbol)symbol;
  return true;
}

static bool is_recovery_reserved_word(const char *word);

static bool read_reserved_word(TSLexer *lexer, char *word) {
  unsigned length = 0;

  while (true) {
    if (is_lowercase_letter(lexer->lookahead)) {
      if (length == 5) {
        return false;
      }
      word[length] = (char)lexer->lookahead;
      length += 1;
      lexer->advance(lexer, false);
      continue;
    }

    if (lexer->lookahead != '\\') {
      break;
    }

    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return false;
    }
    lexer->advance(lexer, false);
  }

  if (length == 0 || !is_token_delimiter(lexer->lookahead)) {
    return false;
  }

  word[length] = '\0';
  return true;
}

static bool scan_reserved_word_segment(TSLexer *lexer) {
  if (!is_lowercase_letter(lexer->lookahead)) {
    return false;
  }

  do {
    lexer->advance(lexer, false);
  } while (is_lowercase_letter(lexer->lookahead));
  lexer->mark_end(lexer);

  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return false;
    }
    lexer->advance(lexer, false);
  }

  if (is_lowercase_letter(lexer->lookahead)) {
    lexer->result_symbol = RESERVED_WORD_NONFINAL_SEGMENT;
    return true;
  }
  if (!is_token_delimiter(lexer->lookahead)) {
    return false;
  }

  lexer->result_symbol = RESERVED_WORD_FINAL_SEGMENT;
  return true;
}

static bool scan_reserved_word(TSLexer *lexer, const bool *valid_symbols) {
  char word[6];
  lexer->mark_end(lexer);
  if (!read_reserved_word(lexer, word)) {
    return false;
  }

  unsigned word_count = sizeof(RESERVED_WORDS) / sizeof(RESERVED_WORDS[0]);
  for (unsigned index = 0; index < word_count; index += 1) {
    const struct ReservedWord *reserved_word = &RESERVED_WORDS[index];
    if (
      valid_symbols[reserved_word->symbol] &&
      strcmp(word, reserved_word->text) == 0
    ) {
      lexer->result_symbol = (TSSymbol)reserved_word->symbol;
      return true;
    }
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

static bool scan_case_item_terminator(TSLexer *lexer);

static bool scan_horizontal_layout(TSLexer *lexer);

static bool
is_active_backquote_boundary(const struct Scanner *scanner, int32_t character) {
  return scanner->backquote_depth > 0 && character == '`';
}

static bool scan_empty_compound_list_recovery_boundary(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  lexer->mark_end(lexer);

  if (
    lexer->lookahead ==
    0 ||
    lexer->lookahead ==
    ')' ||
    lexer->lookahead ==
    '}' ||
    is_active_backquote_boundary(scanner, lexer->lookahead)
  ) {
    lexer->result_symbol = EMPTY_COMPOUND_LIST_RECOVERY_BOUNDARY;
    return true;
  }

  if (lexer->lookahead == ';') {
    if (!scan_case_item_terminator(lexer)) {
      return false;
    }
    lexer->result_symbol = EMPTY_COMPOUND_LIST_RECOVERY_BOUNDARY;
    return true;
  }

  if (!is_lowercase_letter(lexer->lookahead)) {
    return false;
  }

  char word[6];
  if (!read_reserved_word(lexer, word)) {
    return false;
  }

  if (is_recovery_reserved_word(word)) {
    lexer->result_symbol = EMPTY_COMPOUND_LIST_RECOVERY_BOUNDARY;
    return true;
  }

  size_t word_count = sizeof(RESERVED_WORDS) / sizeof(RESERVED_WORDS[0]);
  for (size_t index = 0; index < word_count; index += 1) {
    const struct ReservedWord *reserved_word = &RESERVED_WORDS[index];
    if (
      valid_symbols[reserved_word->symbol] &&
      strcmp(word, reserved_word->text) == 0
    ) {
      lexer->result_symbol = (TSSymbol)reserved_word->symbol;
      return true;
    }
  }

  return false;
}

static bool scan_direct_recovery_boundary(
  const struct Scanner *scanner,
  TSLexer *lexer,
  enum TokenType symbol
) {
  lexer->mark_end(lexer);

  bool is_boundary = lexer->lookahead ==
    0 ||
    lexer->lookahead ==
    ')' ||
    lexer->lookahead ==
    '}' ||
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

static bool
scan_function_body_boundary(TSLexer *lexer, const bool *valid_symbols) {
  lexer->mark_end(lexer);

  while (true) {
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
      lexer->advance(lexer, false);
    }

    if (lexer->lookahead == '\\') {
      lexer->advance(lexer, false);
      if (lexer->lookahead != '\n') {
        break;
      }
      lexer->advance(lexer, false);
      continue;
    }

    if (lexer->lookahead == '#') {
      do {
        lexer->advance(lexer, false);
      } while (lexer->lookahead != 0 && lexer->lookahead != '\n');
    }

    if (lexer->lookahead != '\n') {
      break;
    }
    lexer->advance(lexer, false);
  }

  bool has_function_body = lexer->lookahead == '(';
  if (lexer->lookahead == '{') {
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
      has_valid_continuations && is_token_delimiter(lexer->lookahead);
  } else if (is_lowercase_letter(lexer->lookahead)) {
    char word[6];
    if (read_reserved_word(lexer, word)) {
      has_function_body = strcmp(word, "if") ==
        0 ||
        strcmp(word, "for") ==
        0 ||
        strcmp(word, "case") ==
        0 ||
        strcmp(word, "while") ==
        0 ||
        strcmp(word, "until") == 0;
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

  bool is_direct_boundary = lexer->lookahead == '}';
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
        lexer->lookahead ==
        0 ||
        ((symbol ==
           COMPOUND_COMMAND_RECOVERY_BOUNDARY ||
           symbol == CASE_ITEMS_RECOVERY_BOUNDARY) &&
          lexer->lookahead == ')') ||
        is_active_backquote_boundary(scanner, lexer->lookahead));
  } else if (symbol != SEPARATOR_RECOVERY) {
    is_direct_boundary =
      (is_direct_boundary ||
        lexer->lookahead ==
        0 ||
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
  if (!read_reserved_word(lexer, word)) {
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

  size_t word_count = sizeof(RESERVED_WORDS) / sizeof(RESERVED_WORDS[0]);
  for (size_t index = 0; index < word_count; index += 1) {
    const struct ReservedWord *reserved_word = &RESERVED_WORDS[index];
    if (
      valid_symbols[reserved_word->symbol] &&
      strcmp(word, reserved_word->text) == 0
    ) {
      lexer->result_symbol = (TSSymbol)reserved_word->symbol;
      if (symbol == SEPARATOR_RECOVERY) {
        lexer->result_symbol = SEPARATOR_RECOVERY;
      }
      return true;
    }
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
    if (lexer->lookahead != 0) {
      return false;
    }
    lexer->result_symbol = PARAMETER_EXPANSION_RECOVERY_BOUNDARY;
    return true;
  }

  bool is_outer_boundary = lexer->lookahead ==
    0 ||
    lexer->lookahead ==
    '\n' ||
    lexer->lookahead ==
    ' ' ||
    lexer->lookahead ==
    '\t' ||
    lexer->lookahead ==
    ')' ||
    is_active_backquote_boundary(scanner, lexer->lookahead) ||
    lexer->lookahead ==
    ';' ||
    lexer->lookahead == '&';
  bool includes_double_quote = symbol != PARAMETER_TAIL_RECOVERY_BOUNDARY;
  is_outer_boundary =
    is_outer_boundary || (includes_double_quote && lexer->lookahead == '"');
  bool includes_closing_brace = symbol ==
    PARAMETER_MISSING_RECOVERY_BOUNDARY ||
    symbol == PARAMETER_OPERATOR_RECOVERY_BOUNDARY;
  if (
    is_outer_boundary || (includes_closing_brace && lexer->lookahead == '}')
  ) {
    lexer->result_symbol = (TSSymbol)symbol;
    return true;
  }
  return false;
}

static bool scan_horizontal_layout(TSLexer *lexer) {
  while (true) {
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
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

static bool scan_pattern_boundary(TSLexer *lexer, const bool *valid_symbols) {
  lexer->mark_end(lexer);

  if (!scan_horizontal_layout(lexer)) {
    return false;
  }

  if (lexer->lookahead == '|' && valid_symbols[PATTERN_CONTINUATION]) {
    lexer->result_symbol = PATTERN_CONTINUATION;
    return true;
  }

  if (lexer->lookahead == ')' && valid_symbols[PATTERN_END]) {
    lexer->result_symbol = PATTERN_END;
    return true;
  }

  return false;
}

static bool scan_following_reserved_word(TSLexer *lexer) {
  if (lexer->lookahead != 'i' && lexer->lookahead != 'd') {
    return false;
  }

  char word[6];
  return (
    read_reserved_word(lexer, word) &&
    (strcmp(word, "in") == 0 || strcmp(word, "do") == 0)
  );
}

static bool scan_and_if_after_first_ampersand(TSLexer *lexer) {
  if (lexer->lookahead != '&') {
    return false;
  }

  lexer->advance(lexer, false);
  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return false;
    }
    lexer->advance(lexer, false);
  }

  return lexer->lookahead == '&';
}

static bool scan_command_continuation_operator(TSLexer *lexer) {
  if (lexer->lookahead == '|') {
    lexer->result_symbol = COMMAND_CONTINUATION;
    return true;
  }

  if (scan_and_if_after_first_ampersand(lexer)) {
    lexer->result_symbol = COMMAND_CONTINUATION;
    return true;
  }

  return false;
}

static bool scan_command_boundary(
  const struct Scanner *scanner,
  TSLexer *lexer,
  bool allow_continuation,
  bool allow_separator,
  bool allow_closed_command_end,
  bool allow_closed_simple_command_end,
  bool allow_reserved_word_closer
) {
  lexer->mark_end(lexer);

  if (!scan_horizontal_layout(lexer)) {
    return false;
  }

  if (
    allow_continuation && (lexer->lookahead == '|' || lexer->lookahead == '&')
  ) {
    return scan_command_continuation_operator(lexer);
  }

  if (
    !allow_separator &&
    !allow_closed_command_end &&
    !allow_closed_simple_command_end
  ) {
    return false;
  }

  if (allow_separator && lexer->lookahead == '#') {
    lexer->result_symbol = SEPARATOR_BEGIN;
    return true;
  }

  if (!allow_closed_command_end && !allow_closed_simple_command_end) {
    return false;
  }

  bool is_closer = lexer->lookahead ==
    ')' ||
    lexer->lookahead ==
    '}' ||
    is_active_backquote_boundary(scanner, lexer->lookahead);
  if (!is_closer && lexer->lookahead == ';') {
    is_closer = scan_case_item_terminator(lexer);
  }
  if (
    !is_closer &&
    allow_reserved_word_closer &&
    is_lowercase_letter(lexer->lookahead)
  ) {
    char word[6];
    is_closer =
      read_reserved_word(lexer, word) && is_recovery_reserved_word(word);
  }

  if (!is_closer) {
    return false;
  }

  lexer->result_symbol = allow_closed_simple_command_end
    ? CLOSED_SIMPLE_COMMAND_END
    : CLOSED_COMMAND_END;
  return true;
}

static bool scan_pipeline_negation(TSLexer *lexer) {
  if (lexer->lookahead != '!') {
    return false;
  }

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);

  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return false;
    }
    lexer->advance(lexer, false);
  }

  if (!is_token_delimiter(lexer->lookahead)) {
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

  while (true) {
    while (is_decimal_digit(lexer->lookahead)) {
      lexer->advance(lexer, false);
    }

    if (lexer->lookahead != '\\') {
      break;
    }

    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return false;
    }
    lexer->advance(lexer, false);
  }

  if (lexer->lookahead != '<' && lexer->lookahead != '>') {
    return false;
  }

  lexer->result_symbol = FILE_DESCRIPTOR;
  return true;
}

static bool scan_left_brace_or_io_location(
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

  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return false;
    }
    lexer->advance(lexer, false);
  }

  if (is_token_delimiter(lexer->lookahead)) {
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

  while (true) {
    if (lexer->lookahead == '<' || lexer->lookahead == '>') {
      if (character_count < 2 || !ends_with_right_brace) {
        return false;
      }

      lexer->result_symbol = IO_LOCATION;
      return true;
    }

    if (is_token_delimiter(lexer->lookahead)) {
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

    if (lexer->lookahead == '\\') {
      lexer->advance(lexer, false);
      if (lexer->lookahead != '\n') {
        return false;
      }
      lexer->advance(lexer, false);
      continue;
    }

    ends_with_right_brace = lexer->lookahead == '}';
    character_count += 1;
    lexer->advance(lexer, false);
  }
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
  int32_t character = lexer->lookahead;
  if (
    !scanner->expecting_delimiter ||
    !is_missing_here_document_delimiter_boundary(character)
  ) {
    return false;
  }

  scanner->expecting_delimiter = false;
  scanner->delimiter_strips_tabs = false;
  lexer->mark_end(lexer);
  lexer->result_symbol = MISSING_HERE_DOCUMENT_DELIMITER;
  return true;
}

static bool scan_comment_text(TSLexer *lexer, enum TokenType symbol);

static bool scan_comment(TSLexer *lexer);

static bool scan_spaced_comment(TSLexer *lexer);

static bool scan_line_continuation(TSLexer *lexer, const bool *valid_symbols);

static bool scan_line_continuation_after_backslash(
  TSLexer *lexer,
  const bool *valid_symbols,
  bool crossed_blank_prefix
);

static bool scan_case_item_terminator(TSLexer *lexer) {
  if (lexer->lookahead != ';') {
    return false;
  }
  lexer->advance(lexer, false);

  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return false;
    }
    lexer->advance(lexer, false);
  }

  if (lexer->lookahead != ';' && lexer->lookahead != '&') {
    return false;
  }

  return true;
}

/*
 * A completed case body can either end at a clause terminator or continue its
 * current AND-OR list. Both alternatives share the same leading blanks in a
 * Tree-sitter state, so classify that boundary after scanning the prefix once.
 * Tight line continuations are handled by the central continuation scanner.
 */
static bool scan_case_item_boundary(TSLexer *lexer, const bool *valid_symbols) {
  bool crossed_blank_prefix = false;
  while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
    crossed_blank_prefix = true;
    lexer->advance(lexer, true);
  }
  lexer->mark_end(lexer);

  if (
    valid_symbols[COMMAND_CONTINUATION] &&
    (lexer->lookahead == '|' || lexer->lookahead == '&')
  ) {
    return scan_command_continuation_operator(lexer);
  }

  if (lexer->lookahead == '#' && valid_symbols[SPACED_COMMENT]) {
    return scan_comment_text(lexer, SPACED_COMMENT);
  }

  if (lexer->lookahead == '\n' && valid_symbols[BLANK_LINE]) {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    lexer->result_symbol = BLANK_LINE;
    return true;
  }

  if (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    return scan_line_continuation_after_backslash(
      lexer,
      valid_symbols,
      crossed_blank_prefix
    );
  }

  if (!scan_case_item_terminator(lexer)) {
    return false;
  }

  lexer->result_symbol = CASE_ITEM_END;
  return true;
}

static bool scan_here_document_line_end(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  if (scanner->pending_count == 0 || scanner->sequence_end_pending) {
    return false;
  }

  lexer->mark_end(lexer);
  bool has_layout = false;
  while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
    has_layout = true;
    lexer->advance(lexer, true);
  }

  if (valid_symbols[COMMAND_CONTINUATION] && lexer->lookahead == '|') {
    lexer->result_symbol = COMMAND_CONTINUATION;
    return true;
  }

  if (valid_symbols[COMMAND_CONTINUATION] && lexer->lookahead == '&') {
    lexer->advance(lexer, false);
    while (lexer->lookahead == '\\') {
      lexer->advance(lexer, false);
      if (lexer->lookahead != '\n') {
        return false;
      }
      lexer->advance(lexer, false);
    }
    if (lexer->lookahead == '&') {
      lexer->result_symbol = COMMAND_CONTINUATION;
      return true;
    }
    return false;
  }

  if (has_layout && lexer->lookahead == '#' && valid_symbols[SPACED_COMMENT]) {
    return scan_comment_text(lexer, SPACED_COMMENT);
  }

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

static bool scan_comment_text(TSLexer *lexer, enum TokenType symbol) {
  if (lexer->lookahead != '#') {
    return false;
  }

  do {
    lexer->advance(lexer, false);
  } while (lexer->lookahead != 0 && lexer->lookahead != '\n');

  lexer->mark_end(lexer);
  lexer->result_symbol = symbol;
  return true;
}

static bool scan_comment(TSLexer *lexer) {
  return scan_comment_text(lexer, COMMENT);
}

static bool scan_spaced_comment(TSLexer *lexer) {
  bool has_layout = false;
  while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
    has_layout = true;
    lexer->advance(lexer, true);
  }

  return (has_layout && scan_comment_text(lexer, SPACED_COMMENT));
}

static bool scan_spaced_comment_or_blank_line(
  TSLexer *lexer,
  const bool *valid_symbols,
  bool allow_blank_line
) {
  bool has_layout = false;
  while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
    has_layout = true;
    lexer->advance(lexer, true);
  }

  if (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    return scan_line_continuation_after_backslash(
      lexer,
      valid_symbols,
      has_layout
    );
  }

  if (has_layout && valid_symbols[SPACED_COMMENT] && lexer->lookahead == '#') {
    return scan_comment_text(lexer, SPACED_COMMENT);
  }

  if (
    !has_layout ||
    !allow_blank_line ||
    !valid_symbols[BLANK_LINE] ||
    lexer->lookahead != '\n'
  ) {
    return false;
  }

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  lexer->result_symbol = BLANK_LINE;
  return true;
}

struct LineContinuationLookahead {
  int32_t direct_character;
  int32_t layout_character;
  bool crossed_blank;
  bool substitution_layout_valid;
};

static int32_t scan_line_joined_character(TSLexer *lexer);
static enum ArithmeticOperatorCategory
classify_arithmetic_operator(int32_t first, int32_t second, int32_t third);
static bool is_arithmetic_operator_start(int32_t character);
static bool is_arithmetic_operand_start(int32_t character);

static bool arithmetic_operand_token_is_valid(
  const bool *valid_symbols,
  enum TokenType plus_token,
  enum TokenType minus_token,
  enum TokenType generic_token
) {
  return (
    valid_symbols[plus_token] ||
    valid_symbols[minus_token] ||
    valid_symbols[generic_token]
  );
}

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

static bool
arithmetic_operand_line_continuation_is_valid(const bool *valid_symbols) {
  return arithmetic_operand_token_is_valid(
    valid_symbols,
    ARITHMETIC_PLUS_OPERAND_LINE_CONTINUATION,
    ARITHMETIC_MINUS_OPERAND_LINE_CONTINUATION,
    ARITHMETIC_OPERAND_LINE_CONTINUATION
  );
}

static bool arithmetic_operand_boundary_is_valid(const bool *valid_symbols) {
  return arithmetic_operand_token_is_valid(
    valid_symbols,
    ARITHMETIC_PLUS_OPERAND_BOUNDARY,
    ARITHMETIC_MINUS_OPERAND_BOUNDARY,
    ARITHMETIC_OPERAND_BOUNDARY
  );
}

static bool
arithmetic_operator_line_continuation_is_valid(const bool *valid_symbols) {
  for (
    size_t category = 0; category < ARITHMETIC_OPERATOR_CATEGORY_COUNT;
    category += 1
  ) {
    enum TokenType symbol =
      ARITHMETIC_OPERATOR_TOKENS[category].line_continuation;
    if (valid_symbols[symbol]) {
      return true;
    }
  }
  return false;
}

static bool
line_continuation_needs_direct_lookahead(const bool *valid_symbols) {
  return (
    valid_symbols[COMMAND_SUBSTITUTION_END_LINE_CONTINUATION] ||
    valid_symbols[BACKQUOTE_END_LINE_CONTINUATION] ||
    valid_symbols[BLANK_LINE_START_LINE_CONTINUATION] ||
    valid_symbols[COMMAND_BOUNDARY_LINE_CONTINUATION] ||
    valid_symbols[PATTERN_CONTINUATION_LINE_CONTINUATION] ||
    valid_symbols[PATTERN_END_LINE_CONTINUATION] ||
    valid_symbols[PATTERN_BRACKET_CLOSING_LINE_CONTINUATION] ||
    valid_symbols[CASE_ITEM_END_LINE_CONTINUATION] ||
    valid_symbols[RESERVED_WORD_SEPARATOR_LINE_CONTINUATION] ||
    valid_symbols[SEPARATOR_BOUNDARY_LINE_CONTINUATION] ||
    valid_symbols[WORD_SEPARATOR_LINE_CONTINUATION] ||
    valid_symbols[SOURCE_LINE_CONTINUATION] ||
    valid_symbols[NAME_LINE_CONTINUATION] ||
    valid_symbols[TILDE_USER_LINE_CONTINUATION] ||
    valid_symbols[DIGIT_LINE_CONTINUATION] ||
    valid_symbols[SECOND_LEFT_PARENTHESIS_START_LINE_CONTINUATION] ||
    valid_symbols[ARITHMETIC_CLOSING_LINE_CONTINUATION] ||
    arithmetic_operator_line_continuation_is_valid(valid_symbols) ||
    arithmetic_operand_line_continuation_is_valid(valid_symbols) ||
    valid_symbols[ASSIGNMENT_NAME_END_LINE_CONTINUATION] ||
    valid_symbols[ASSIGNMENT_VALUE_END_LINE_CONTINUATION]
  );
}

static bool line_continuation_is_valid(const bool *valid_symbols) {
  return (
    valid_symbols[LINE_CONTINUATION] ||
    valid_symbols[CONTINUED_LINE_CONTINUATION] ||
    valid_symbols[SPACED_LINE_CONTINUATION] ||
    valid_symbols[LAYOUT_LINE_CONTINUATION] ||
    line_continuation_needs_direct_lookahead(valid_symbols)
  );
}

static struct LineContinuationLookahead scan_line_continuation_lookahead(
  TSLexer *lexer,
  bool needs_direct,
  bool needs_layout
) {
  struct LineContinuationLookahead result = {
    .substitution_layout_valid = true,
  };

  if (!needs_direct) {
    result.direct_character = lexer->lookahead;
    result.layout_character = lexer->lookahead;
    result.crossed_blank = lexer->lookahead ==
      ' ' ||
      lexer->lookahead ==
      '\t' ||
      lexer->lookahead == '\n';
    result.substitution_layout_valid = lexer->lookahead != '\n';
    return result;
  }

  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      result.direct_character = '\\';
      result.layout_character = '\\';
      return result;
    }
    lexer->advance(lexer, false);
  }

  result.direct_character = lexer->lookahead;
  if (!needs_layout) {
    result.layout_character = lexer->lookahead;
    result.crossed_blank = lexer->lookahead ==
      ' ' ||
      lexer->lookahead ==
      '\t' ||
      lexer->lookahead == '\n';
    result.substitution_layout_valid = lexer->lookahead != '\n';
    return result;
  }

  while (true) {
    if (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
      result.crossed_blank = true;
      lexer->advance(lexer, false);
      continue;
    }

    if (lexer->lookahead == '\n') {
      result.crossed_blank = true;
      result.substitution_layout_valid = false;
      lexer->advance(lexer, false);
      continue;
    }

    if (lexer->lookahead != '\\') {
      result.layout_character = lexer->lookahead;
      return result;
    }

    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      result.layout_character = '\\';
      return result;
    }
    lexer->advance(lexer, false);
  }
}

static bool scan_line_continuation_after_backslash(
  TSLexer *lexer,
  const bool *valid_symbols,
  bool crossed_blank_prefix
) {
  if (lexer->lookahead != '\n') {
    return false;
  }
  lexer->advance(lexer, false);
  lexer->mark_end(lexer);

  if (valid_symbols[CONTINUED_LINE_CONTINUATION]) {
    lexer->result_symbol = CONTINUED_LINE_CONTINUATION;
    return true;
  }

  bool needs_direct = line_continuation_needs_direct_lookahead(valid_symbols);
  bool needs_layout =
    valid_symbols[COMMAND_SUBSTITUTION_END_LINE_CONTINUATION] ||
    valid_symbols[BACKQUOTE_END_LINE_CONTINUATION] ||
    valid_symbols[BLANK_LINE_START_LINE_CONTINUATION] ||
    valid_symbols[COMMAND_BOUNDARY_LINE_CONTINUATION] ||
    valid_symbols[PATTERN_CONTINUATION_LINE_CONTINUATION] ||
    valid_symbols[PATTERN_END_LINE_CONTINUATION] ||
    valid_symbols[CASE_ITEM_END_LINE_CONTINUATION] ||
    valid_symbols[RESERVED_WORD_SEPARATOR_LINE_CONTINUATION] ||
    valid_symbols[SEPARATOR_BOUNDARY_LINE_CONTINUATION] ||
    valid_symbols[WORD_SEPARATOR_LINE_CONTINUATION] ||
    valid_symbols[ARITHMETIC_CLOSING_LINE_CONTINUATION] ||
    arithmetic_operator_line_continuation_is_valid(valid_symbols) ||
    arithmetic_operand_line_continuation_is_valid(valid_symbols);
  struct LineContinuationLookahead lookahead =
    scan_line_continuation_lookahead(lexer, needs_direct, needs_layout);
  lookahead.crossed_blank = lookahead.crossed_blank || crossed_blank_prefix;

  bool reaches_assignment_boundary =
    valid_symbols[ASSIGNMENT_VALUE_END_LINE_CONTINUATION] &&
    is_token_delimiter(lookahead.direct_character);

  if (reaches_assignment_boundary) {
    lexer->result_symbol = ASSIGNMENT_VALUE_END_LINE_CONTINUATION;
    return true;
  }

  if (
    valid_symbols[SECOND_LEFT_PARENTHESIS_START_LINE_CONTINUATION] &&
    lookahead.direct_character == '('
  ) {
    lexer->result_symbol = SECOND_LEFT_PARENTHESIS_START_LINE_CONTINUATION;
    return true;
  }

  if (
    valid_symbols[ARITHMETIC_CLOSING_LINE_CONTINUATION] &&
    lookahead.layout_character == ')'
  ) {
    lexer->result_symbol = ARITHMETIC_CLOSING_LINE_CONTINUATION;
    return true;
  }

  if (!lookahead.crossed_blank) {
    if (
      valid_symbols[NAME_LINE_CONTINUATION] &&
      is_name_character(lookahead.direct_character)
    ) {
      lexer->result_symbol = NAME_LINE_CONTINUATION;
      return true;
    }
    if (
      valid_symbols[TILDE_USER_LINE_CONTINUATION] &&
      is_tilde_user_character(lookahead.direct_character)
    ) {
      lexer->result_symbol = TILDE_USER_LINE_CONTINUATION;
      return true;
    }
    if (
      valid_symbols[DIGIT_LINE_CONTINUATION] &&
      is_decimal_digit(lookahead.direct_character)
    ) {
      lexer->result_symbol = DIGIT_LINE_CONTINUATION;
      return true;
    }
  }

  TSSymbol arithmetic_operand_symbol;
  if (
    classify_arithmetic_operand(
      valid_symbols,
      lookahead.layout_character,
      lookahead.crossed_blank,
      ARITHMETIC_PLUS_OPERAND_LINE_CONTINUATION,
      ARITHMETIC_MINUS_OPERAND_LINE_CONTINUATION,
      ARITHMETIC_OPERAND_LINE_CONTINUATION,
      &arithmetic_operand_symbol
    )
  ) {
    lexer->result_symbol = arithmetic_operand_symbol;
    return true;
  }

  if (
    arithmetic_operator_line_continuation_is_valid(valid_symbols) &&
    is_arithmetic_operator_start(lookahead.layout_character)
  ) {
    int32_t first = scan_line_joined_character(lexer);
    int32_t second = scan_line_joined_character(lexer);
    int32_t third = scan_line_joined_character(lexer);
    if (first >= 0 && second >= 0 && third >= 0) {
      enum ArithmeticOperatorCategory category =
        classify_arithmetic_operator(first, second, third);
      if (category != ARITHMETIC_OPERATOR_CATEGORY_COUNT) {
        enum TokenType symbol =
          ARITHMETIC_OPERATOR_TOKENS[category].line_continuation;
        if (valid_symbols[symbol]) {
          lexer->result_symbol = (TSSymbol)symbol;
          return true;
        }
      }
    }
  }

  if (
    valid_symbols[BLANK_LINE_START_LINE_CONTINUATION] &&
    !lookahead.substitution_layout_valid
  ) {
    lexer->result_symbol = BLANK_LINE_START_LINE_CONTINUATION;
    return true;
  }

  if (
    valid_symbols[PATTERN_BRACKET_CLOSING_LINE_CONTINUATION] &&
    lookahead.direct_character == ']'
  ) {
    lexer->result_symbol = PATTERN_BRACKET_CLOSING_LINE_CONTINUATION;
    return true;
  }

  if (
    valid_symbols[CASE_ITEM_END_LINE_CONTINUATION] &&
    lookahead.substitution_layout_valid &&
    scan_case_item_terminator(lexer)
  ) {
    lexer->result_symbol = CASE_ITEM_END_LINE_CONTINUATION;
    return true;
  }

  if (
    valid_symbols[RESERVED_WORD_SEPARATOR_LINE_CONTINUATION] &&
    lookahead.substitution_layout_valid &&
    lookahead.crossed_blank &&
    scan_following_reserved_word(lexer)
  ) {
    lexer->result_symbol = RESERVED_WORD_SEPARATOR_LINE_CONTINUATION;
    return true;
  }

  if (
    valid_symbols[WORD_SEPARATOR_LINE_CONTINUATION] &&
    lookahead.substitution_layout_valid &&
    lookahead.crossed_blank &&
    lookahead.layout_character !=
    '#' &&
    !is_token_delimiter(lookahead.layout_character)
  ) {
    lexer->result_symbol = WORD_SEPARATOR_LINE_CONTINUATION;
    return true;
  }

  if (
    valid_symbols[PATTERN_CONTINUATION_LINE_CONTINUATION] &&
    lookahead.substitution_layout_valid &&
    lookahead.layout_character == '|'
  ) {
    lexer->result_symbol = PATTERN_CONTINUATION_LINE_CONTINUATION;
    return true;
  }

  if (
    valid_symbols[PATTERN_END_LINE_CONTINUATION] &&
    lookahead.substitution_layout_valid &&
    lookahead.layout_character == ')'
  ) {
    lexer->result_symbol = PATTERN_END_LINE_CONTINUATION;
    return true;
  }

  bool reaches_and_if = lookahead.substitution_layout_valid &&
    lookahead.layout_character ==
    '&' &&
    (valid_symbols[COMMAND_BOUNDARY_LINE_CONTINUATION] ||
      valid_symbols[SEPARATOR_BOUNDARY_LINE_CONTINUATION]) &&
    scan_and_if_after_first_ampersand(lexer);

  if (
    valid_symbols[COMMAND_BOUNDARY_LINE_CONTINUATION] &&
    lookahead.substitution_layout_valid &&
    (lookahead.layout_character == '|' || reaches_and_if)
  ) {
    lexer->result_symbol = COMMAND_BOUNDARY_LINE_CONTINUATION;
    return true;
  }

  if (
    valid_symbols[SEPARATOR_BOUNDARY_LINE_CONTINUATION] &&
    (lookahead.layout_character ==
      ';' ||
      (lookahead.layout_character == '&' && !reaches_and_if))
  ) {
    lexer->result_symbol = SEPARATOR_BOUNDARY_LINE_CONTINUATION;
    return true;
  }

  if (
    valid_symbols[COMMAND_SUBSTITUTION_END_LINE_CONTINUATION] ||
    valid_symbols[BACKQUOTE_END_LINE_CONTINUATION]
  ) {
    enum TokenType substitution_end = TOKEN_COUNT;
    if (
      lookahead.substitution_layout_valid &&
      valid_symbols[COMMAND_SUBSTITUTION_END_LINE_CONTINUATION] &&
      lookahead.layout_character == ')'
    ) {
      substitution_end = COMMAND_SUBSTITUTION_END_LINE_CONTINUATION;
    } else if (
      lookahead.substitution_layout_valid &&
      valid_symbols[BACKQUOTE_END_LINE_CONTINUATION] &&
      lookahead.layout_character == '`'
    ) {
      substitution_end = BACKQUOTE_END_LINE_CONTINUATION;
    }
    if (substitution_end != TOKEN_COUNT) {
      lexer->result_symbol = (TSSymbol)substitution_end;
      return true;
    }
  }

  if (lookahead.crossed_blank) {
    /*
     * A prefix blank was already skipped before this continuation. Preserve
     * that separator classification before falling back to a generic token.
     */
    if (crossed_blank_prefix && valid_symbols[SPACED_LINE_CONTINUATION]) {
      lexer->result_symbol = SPACED_LINE_CONTINUATION;
      return true;
    }
    if (valid_symbols[LINE_CONTINUATION]) {
      lexer->result_symbol = LINE_CONTINUATION;
      return true;
    }
    if (valid_symbols[SPACED_LINE_CONTINUATION]) {
      lexer->result_symbol = SPACED_LINE_CONTINUATION;
      return true;
    }
    if (valid_symbols[LAYOUT_LINE_CONTINUATION]) {
      lexer->result_symbol = LAYOUT_LINE_CONTINUATION;
      return true;
    }
    return false;
  }

  if (
    valid_symbols[ASSIGNMENT_NAME_END_LINE_CONTINUATION] &&
    lookahead.direct_character == '='
  ) {
    lexer->result_symbol = ASSIGNMENT_NAME_END_LINE_CONTINUATION;
    return true;
  }

  if (
    valid_symbols[SOURCE_LINE_CONTINUATION] &&
    !is_token_delimiter(lookahead.direct_character)
  ) {
    lexer->result_symbol = SOURCE_LINE_CONTINUATION;
    return true;
  }

  if (valid_symbols[LINE_CONTINUATION]) {
    lexer->result_symbol = LINE_CONTINUATION;
    return true;
  }

  if (valid_symbols[LAYOUT_LINE_CONTINUATION]) {
    lexer->result_symbol = LAYOUT_LINE_CONTINUATION;
    return true;
  }

  if (valid_symbols[SOURCE_LINE_CONTINUATION]) {
    lexer->result_symbol = SOURCE_LINE_CONTINUATION;
    return true;
  }

  if (valid_symbols[SPACED_LINE_CONTINUATION]) {
    lexer->result_symbol = SPACED_LINE_CONTINUATION;
    return true;
  }

  return false;
}

static bool scan_line_continuation(TSLexer *lexer, const bool *valid_symbols) {
  if (lexer->lookahead != '\\') {
    return false;
  }

  lexer->advance(lexer, false);
  return scan_line_continuation_after_backslash(lexer, valid_symbols, false);
}

static int32_t scan_line_joined_character(TSLexer *lexer) {
  int32_t character = lexer->lookahead;
  if (character == 0) {
    return 0;
  }

  lexer->advance(lexer, false);
  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return -1;
    }
    lexer->advance(lexer, false);
  }

  return character;
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
    return second == '=' ? ARITHMETIC_OPERATOR_CATEGORY_ASSIGNMENT
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
    enum TokenType symbol = ARITHMETIC_OPERATOR_TOKENS[category].boundary;
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
scan_arithmetic_operator_boundary(TSLexer *lexer, const bool *valid_symbols) {
  lexer->mark_end(lexer);
  if (!scan_arithmetic_layout(lexer, NULL)) {
    return false;
  }

  int32_t first = scan_line_joined_character(lexer);
  int32_t second = scan_line_joined_character(lexer);
  int32_t third = scan_line_joined_character(lexer);
  if (first >= 0 && second >= 0 && third >= 0) {
    enum ArithmeticOperatorCategory category =
      classify_arithmetic_operator(first, second, third);
    if (category != ARITHMETIC_OPERATOR_CATEGORY_COUNT) {
      enum TokenType symbol = ARITHMETIC_OPERATOR_TOKENS[category].boundary;
      if (valid_symbols[symbol]) {
        lexer->result_symbol = (TSSymbol)symbol;
        return true;
      }
    }
  }

  return false;
}

static bool
scan_arithmetic_operand_boundary(TSLexer *lexer, const bool *valid_symbols) {
  lexer->mark_end(lexer);
  bool crossed_layout = false;
  if (!scan_arithmetic_layout(lexer, &crossed_layout)) {
    return false;
  }

  TSSymbol symbol;
  if (!classify_arithmetic_operand(
        valid_symbols,
        lexer->lookahead,
        crossed_layout,
        ARITHMETIC_PLUS_OPERAND_BOUNDARY,
        ARITHMETIC_MINUS_OPERAND_BOUNDARY,
        ARITHMETIC_OPERAND_BOUNDARY,
        &symbol
      )) {
    return false;
  }

  lexer->result_symbol = symbol;
  return true;
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

static bool scan_line_continuation_boundary(TSLexer *lexer) {
  lexer->mark_end(lexer);

  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      break;
    }
    lexer->advance(lexer, false);
  }

  lexer->result_symbol = LINE_CONTINUATION_BOUNDARY;
  return true;
}

static bool skip_escaped_newlines(TSLexer *lexer, bool *has_escaped_newline) {
  while (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      return false;
    }
    lexer->advance(lexer, false);
    *has_escaped_newline = true;
  }
  return true;
}

static bool
scan_compound_token_start(TSLexer *lexer, const bool *valid_symbols) {
  lexer->mark_end(lexer);
  int32_t first = lexer->lookahead;
  bool has_escaped_newline = false;

  if (
    valid_symbols[CONTINUED_PARAMETER_PATTERN_OPERATOR_START] &&
    (first == '#' || first == '%')
  ) {
    lexer->advance(lexer, false);
    if (
      !skip_escaped_newlines(lexer, &has_escaped_newline) ||
      !has_escaped_newline ||
      lexer->lookahead != first
    ) {
      return false;
    }
    lexer->result_symbol = CONTINUED_PARAMETER_PATTERN_OPERATOR_START;
    return true;
  }

  if (valid_symbols[PATTERN_SPECIAL_LEFT_BRACKET] && first == '[') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    if (
      !skip_escaped_newlines(lexer, &has_escaped_newline) ||
      (lexer->lookahead !=
        ':' &&
        lexer->lookahead !=
        '.' &&
        lexer->lookahead != '=')
    ) {
      return false;
    }
    lexer->result_symbol = PATTERN_SPECIAL_LEFT_BRACKET;
    return true;
  }

  if (first == '>' && valid_symbols[CONTINUED_REDIRECTION_OPERATOR_START]) {
    lexer->advance(lexer, false);
    if (
      !skip_escaped_newlines(lexer, &has_escaped_newline) ||
      !has_escaped_newline ||
      (lexer->lookahead !=
        '>' &&
        lexer->lookahead !=
        '|' &&
        lexer->lookahead != '&')
    ) {
      return false;
    }
    lexer->result_symbol = CONTINUED_REDIRECTION_OPERATOR_START;
    return true;
  }

  if (
    first !=
    '<' ||
    (!valid_symbols[CONTINUED_REDIRECTION_OPERATOR_START] &&
      !valid_symbols[CONTINUED_DLESSDASH_START])
  ) {
    return false;
  }

  lexer->advance(lexer, false);
  if (!skip_escaped_newlines(lexer, &has_escaped_newline)) {
    return false;
  }

  bool continued_after_first = has_escaped_newline;
  if (
    continued_after_first &&
    valid_symbols[CONTINUED_REDIRECTION_OPERATOR_START] &&
    (lexer->lookahead == '&' || lexer->lookahead == '>')
  ) {
    lexer->result_symbol = CONTINUED_REDIRECTION_OPERATOR_START;
    return true;
  }

  if (lexer->lookahead != '<') {
    return false;
  }

  lexer->advance(lexer, false);
  bool continued_after_second = false;
  if (!skip_escaped_newlines(lexer, &continued_after_second)) {
    return false;
  }

  if (
    valid_symbols[CONTINUED_DLESSDASH_START] &&
    (continued_after_first || continued_after_second) &&
    lexer->lookahead == '-'
  ) {
    lexer->result_symbol = CONTINUED_DLESSDASH_START;
    return true;
  }

  if (
    continued_after_first && valid_symbols[CONTINUED_REDIRECTION_OPERATOR_START]
  ) {
    lexer->result_symbol = CONTINUED_REDIRECTION_OPERATOR_START;
    return true;
  }

  return false;
}

static bool scan_dollar_start(TSLexer *lexer, const bool *valid_symbols) {
  if (lexer->lookahead != '$') {
    return false;
  }

  lexer->mark_end(lexer);
  lexer->advance(lexer, false);
  bool has_escaped_newline = false;
  if (!skip_escaped_newlines(lexer, &has_escaped_newline)) {
    return false;
  }

  if (
    valid_symbols[UNBRACED_PARAMETER_START] &&
    (is_name_start_character(lexer->lookahead) ||
      is_decimal_digit(lexer->lookahead) ||
      is_special_parameter_character(lexer->lookahead))
  ) {
    lexer->result_symbol = UNBRACED_PARAMETER_START;
    return true;
  }

  if (
    valid_symbols[CONTINUED_DOLLAR_EXPANSION_START] &&
    has_escaped_newline &&
    (lexer->lookahead == '{' || lexer->lookahead == '(')
  ) {
    lexer->result_symbol = CONTINUED_DOLLAR_EXPANSION_START;
    return true;
  }

  if (
    valid_symbols[CONTINUED_DOLLAR_SINGLE_QUOTE_START] &&
    has_escaped_newline &&
    lexer->lookahead == '\''
  ) {
    lexer->result_symbol = CONTINUED_DOLLAR_SINGLE_QUOTE_START;
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

  while (true) {
    if (is_decimal_digit(lexer->lookahead)) {
      if (has_digit) {
        has_multiple_digits = true;
      }
      has_digit = true;
      if (lexer->lookahead != '0') {
        has_nonzero_digit = true;
      }
      lexer->advance(lexer, false);
      continue;
    }

    if (lexer->lookahead != '\\') {
      break;
    }
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
      break;
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

static bool scan_source_word_continuation_boundary(
  TSLexer *lexer,
  const bool *valid_symbols
) {
  lexer->mark_end(lexer);

  if (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '\n') {
      return scan_line_continuation_after_backslash(
        lexer,
        valid_symbols,
        false
      );
    }
    if (lexer->lookahead == 0) {
      return false;
    }

    lexer->result_symbol = SOURCE_WORD_CONTINUATION_BOUNDARY;
    return true;
  }

  if (is_token_delimiter(lexer->lookahead)) {
    return false;
  }

  lexer->result_symbol = SOURCE_WORD_CONTINUATION_BOUNDARY;
  return true;
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
  if (scanner->backquote_depth == 0 || lexer->lookahead != 0) {
    return false;
  }

  scanner->backquote_depth -= 1;
  lexer->mark_end(lexer);
  lexer->result_symbol = BACKQUOTE_END_RECOVERY;
  return true;
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

static bool scan_backquote_prefix(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  if (scanner->backquote_depth == 0 || lexer->lookahead != '\\') {
    return false;
  }

  size_t escape_count = 0;
  while (lexer->lookahead == '\\') {
    if (escape_count == SIZE_MAX) {
      return false;
    }
    escape_count += 1;
    lexer->advance(lexer, false);
  }

  if (escape_count == 1 && lexer->lookahead == '\n') {
    return scan_line_continuation_after_backslash(lexer, valid_symbols, false);
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

    while (lexer->lookahead == '\\') {
      lexer->advance(lexer, false);
      if (lexer->lookahead != '\n') {
        return false;
      }
      lexer->advance(lexer, false);
    }

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

  size_t expected_escape_count;
  if (
    valid_symbols[BACKQUOTE_END_PREFIX] &&
    backquote_escape_count(
      scanner->backquote_depth,
      false,
      &expected_escape_count
    ) &&
    escape_count == expected_escape_count
  ) {
    lexer->mark_end(lexer);
    scanner->backquote_depth -= 1;
    lexer->result_symbol = BACKQUOTE_END_PREFIX;
    return true;
  }

  if (
    valid_symbols[BACKQUOTE_START_PREFIX] &&
    backquote_escape_count(
      scanner->backquote_depth,
      true,
      &expected_escape_count
    ) &&
    escape_count ==
    expected_escape_count &&
    increase_backquote_depth(scanner)
  ) {
    lexer->mark_end(lexer);
    lexer->result_symbol = BACKQUOTE_START_PREFIX;
    return true;
  }

  return false;
}

static void skip_leading_tabs(TSLexer *lexer, bool strip_tabs) {
  if (!strip_tabs) {
    return;
  }

  while (lexer->lookahead == '\t') {
    lexer->advance(lexer, false);
  }
}

static bool scan_here_document_end_line(
  TSLexer *lexer,
  const struct HereDocument *document
) {
  skip_leading_tabs(lexer, document->strip_tabs);
  size_t delimiter_offset = 0;
  while (delimiter_offset < document->delimiter_length) {
    if (lexer->lookahead == 0 || lexer->lookahead == '\n') {
      return false;
    }

    int32_t source_character = lexer->lookahead;
    if (!document->quoted && source_character == '\\') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '\n') {
        lexer->advance(lexer, false);
        skip_leading_tabs(lexer, document->strip_tabs);
        continue;
      }
    } else {
      lexer->advance(lexer, false);
    }

    int32_t delimiter_character = decode_codepoint(
      document->delimiter,
      document->delimiter_length,
      &delimiter_offset
    );
    if (source_character != delimiter_character) {
      return false;
    }
  }

  if (!document->quoted) {
    while (lexer->lookahead == '\\') {
      lexer->advance(lexer, false);
      if (lexer->lookahead != '\n') {
        return false;
      }

      lexer->advance(lexer, false);
      skip_leading_tabs(lexer, document->strip_tabs);
    }
  }

  if (lexer->lookahead == '\n') {
    lexer->advance(lexer, false);
    return true;
  }

  if (lexer->lookahead == 0) {
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
  } else if (lexer->lookahead != 0) {
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
    bool at_end_of_input = lexer->lookahead == 0;
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
      is_end &&
      valid_symbols[BACKQUOTE_END_RECOVERY] &&
      scanner->backquote_depth > 0
    ) {
      scanner->backquote_depth -= 1;
      lexer->result_symbol = BACKQUOTE_END_RECOVERY;
      return true;
    }

    if (
      at_end_of_input &&
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
    scanner->backquote_depth >
    0 &&
    lexer->lookahead == 0
  ) {
    scanner->backquote_depth -= 1;
    lexer->mark_end(lexer);
    lexer->result_symbol = BACKQUOTE_END_RECOVERY;
    return true;
  }

  if (valid_symbols[HERE_DOCUMENT_END_RECOVERY] && lexer->lookahead == 0) {
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
  return calloc(1, sizeof(struct Scanner));
}

void tree_sitter_posix_sh_external_scanner_destroy(void *payload) {
  struct Scanner *scanner = payload;
  if (scanner == NULL) {
    return;
  }

  clear_scanner(scanner);
  free(scanner);
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
    write_state_size(writer, document->source_end_column) &&
    write_state_size(writer, document->delimiter_length) &&
    write_state_bytes(writer, document->delimiter, document->delimiter_length)
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
    !serialize_document_array(
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

  char *delimiter = NULL;
  if (delimiter_length > 0) {
    delimiter = malloc(delimiter_length);
    if (delimiter == NULL) {
      return false;
    }
    memcpy(delimiter, state->data + state->offset, delimiter_length);
  }
  state->offset += delimiter_length;

  *document = (struct HereDocument){
    .delimiter = delimiter,
    .delimiter_length = delimiter_length,
    .source_end_column = (uint32_t)source_end_column,
    .quoted = (flags & 1) != 0,
    .strip_tabs = (flags & 2) != 0,
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

  *documents = calloc(count, sizeof(struct HereDocument));
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
    !deserialize_document_array(
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
      calloc(suspended_frame_count, sizeof(struct HereDocumentFrame));
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

  bool parameter_bracket_boundary_is_valid =
    valid_symbols[PARAMETER_BRACKET_FALLBACK_END] ||
    valid_symbols[PARAMETER_BRACKET_CONTINUATION];
  bool word_bracket_boundary_is_valid =
    valid_symbols[WORD_BRACKET_FALLBACK_END] ||
    valid_symbols[WORD_BRACKET_CONTINUATION];
  if (parameter_bracket_boundary_is_valid != word_bracket_boundary_is_valid) {
    bool parameter_pattern = parameter_bracket_boundary_is_valid;
    if (
      scan_bracket_boundary(
        lexer,
        parameter_pattern ? PARAMETER_BRACKET_FALLBACK_END
                          : WORD_BRACKET_FALLBACK_END,
        parameter_pattern ? PARAMETER_BRACKET_CONTINUATION
                          : WORD_BRACKET_CONTINUATION,
        parameter_pattern ? valid_symbols[PARAMETER_BRACKET_FALLBACK_END]
                          : valid_symbols[WORD_BRACKET_FALLBACK_END],
        parameter_pattern ? valid_symbols[PARAMETER_BRACKET_CONTINUATION]
                          : valid_symbols[WORD_BRACKET_CONTINUATION],
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

  if (valid_symbols[HERE_DOCUMENT_LINE_END] && scanner->pending_count > 0) {
    if (
      (valid_symbols[BLANK_LINE_START_LINE_CONTINUATION] ||
        valid_symbols[SPACED_LINE_CONTINUATION] ||
        valid_symbols[LAYOUT_LINE_CONTINUATION] ||
        valid_symbols[SPACED_COMMENT]) &&
      (lexer->lookahead == ' ' || lexer->lookahead == '\t') &&
      scan_spaced_comment_or_blank_line(lexer, valid_symbols, false)
    ) {
      return true;
    }
    if (lexer->lookahead == '\\' && line_continuation_is_valid(valid_symbols)) {
      return scan_line_continuation(lexer, valid_symbols);
    }
    return scan_here_document_line_end(scanner, lexer, valid_symbols);
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
        lexer->lookahead == 0) ||
      (valid_symbols[LINE_CONTINUATION] && lexer->lookahead == '\\') ||
      (valid_symbols[NEWLINE] && lexer->lookahead == '\n') ||
      (valid_symbols[HERE_DOCUMENT_BOUNDARY] && lexer->lookahead == '\n'))
  ) {
    return scan_active_here_document(scanner, lexer, valid_symbols);
  }

  if (
    valid_symbols[MISSING_HERE_DOCUMENT_DELIMITER] &&
    scanner->expecting_delimiter &&
    is_missing_here_document_delimiter_boundary(lexer->lookahead)
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
    (valid_symbols[RESERVED_WORD_NONFINAL_SEGMENT] ||
      valid_symbols[RESERVED_WORD_FINAL_SEGMENT]) &&
    is_lowercase_letter(lexer->lookahead)
  ) {
    return scan_reserved_word_segment(lexer);
  }

  if (
    valid_symbols[CASE_ITEM_END] &&
    (lexer->lookahead ==
      ' ' ||
      lexer->lookahead ==
      '\t' ||
      lexer->lookahead == ';')
  ) {
    return scan_case_item_boundary(lexer, valid_symbols);
  }

  if (
    arithmetic_operand_boundary_is_valid(valid_symbols) &&
    (is_arithmetic_operand_start(lexer->lookahead) ||
      lexer->lookahead ==
      ' ' ||
      lexer->lookahead ==
      '\t' ||
      lexer->lookahead == '\n')
  ) {
    return scan_arithmetic_operand_boundary(lexer, valid_symbols);
  }

  if (
    arithmetic_operator_boundary_is_valid(valid_symbols) &&
    (is_arithmetic_operator_start(lexer->lookahead) ||
      lexer->lookahead ==
      ' ' ||
      lexer->lookahead ==
      '\t' ||
      lexer->lookahead == '\n')
  ) {
    return scan_arithmetic_operator_boundary(lexer, valid_symbols);
  }

  if (
    (valid_symbols[PATTERN_CONTINUATION] || valid_symbols[PATTERN_END]) &&
    (lexer->lookahead ==
      ' ' ||
      lexer->lookahead ==
      '\t' ||
      lexer->lookahead ==
      '|' ||
      lexer->lookahead == ')')
  ) {
    return scan_pattern_boundary(lexer, valid_symbols);
  }

  if (
    (valid_symbols[COMMAND_CONTINUATION] ||
      valid_symbols[SEPARATOR_BEGIN] ||
      valid_symbols[CLOSED_COMMAND_END] ||
      valid_symbols[CLOSED_SIMPLE_COMMAND_END]) &&
    (lexer->lookahead ==
      ' ' ||
      lexer->lookahead ==
      '\t' ||
      lexer->lookahead ==
      '|' ||
      lexer->lookahead ==
      '&' ||
      (lexer->lookahead == '#' && valid_symbols[SEPARATOR_BEGIN]) ||
      lexer->lookahead ==
      ')' ||
      lexer->lookahead ==
      '}' ||
      is_active_backquote_boundary(scanner, lexer->lookahead) ||
      (lexer->lookahead ==
        ';' &&
        (valid_symbols[CLOSED_COMMAND_END] ||
          valid_symbols[CLOSED_SIMPLE_COMMAND_END])) ||
      (is_lowercase_letter(lexer->lookahead) &&
        !valid_symbols[SOURCE_WORD_CONTINUATION_BOUNDARY]))
  ) {
    return scan_command_boundary(
      scanner,
      lexer,
      valid_symbols[COMMAND_CONTINUATION],
      valid_symbols[SEPARATOR_BEGIN],
      valid_symbols[CLOSED_COMMAND_END],
      valid_symbols[CLOSED_SIMPLE_COMMAND_END],
      valid_symbols[CLOSED_COMMAND_END] &&
        !valid_symbols[SOURCE_WORD_CONTINUATION_BOUNDARY]
    );
  }

  if (
    valid_symbols[FUNCTION_BODY_CONTINUATION_BOUNDARY] ||
    valid_symbols[FUNCTION_BODY_RECOVERY_BOUNDARY]
  ) {
    return scan_function_body_boundary(lexer, valid_symbols);
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
    valid_symbols[EMPTY_COMPOUND_LIST_RECOVERY_BOUNDARY] &&
    (lexer->lookahead ==
      0 ||
      lexer->lookahead ==
      ')' ||
      lexer->lookahead ==
      '}' ||
      lexer->lookahead ==
      ';' ||
      is_active_backquote_boundary(scanner, lexer->lookahead) ||
      is_lowercase_letter(lexer->lookahead))
  ) {
    return scan_empty_compound_list_recovery_boundary(
      scanner,
      lexer,
      valid_symbols
    );
  }

  if (
    valid_symbols[INVALID_RESERVED_COMMAND_START] &&
    is_lowercase_letter(lexer->lookahead)
  ) {
    return scan_reserved_word(lexer, valid_symbols);
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
    valid_symbols[BACKQUOTE_END] &&
    !valid_symbols[SEPARATOR_RECOVERY] &&
    !valid_symbols[COMPOUND_COMMAND_RECOVERY_BOUNDARY] &&
    is_active_backquote_boundary(scanner, lexer->lookahead)
  ) {
    return scan_backquote_end(scanner, lexer);
  }

  if (
    valid_symbols[SUBSHELL_RECOVERY_BOUNDARY] &&
    !suppress_broad_recovery &&
    (lexer->lookahead ==
      0 ||
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
    (lexer->lookahead ==
      0 ||
      lexer->lookahead ==
      ')' ||
      lexer->lookahead ==
      '}' ||
      lexer->lookahead ==
      ';' ||
      lexer->lookahead ==
      '&' ||
      is_active_backquote_boundary(scanner, lexer->lookahead) ||
      lexer->lookahead ==
      '#' ||
      (is_lowercase_letter(lexer->lookahead) &&
        !valid_symbols[SOURCE_WORD_CONTINUATION_BOUNDARY]))
  ) {
    bool use_boundary_command_recovery =
      valid_symbols[BOUNDARY_COMMAND_RECOVERY] &&
      (lexer->lookahead ==
        ')' ||
        lexer->lookahead ==
        '}' ||
        is_lowercase_letter(lexer->lookahead));
    enum TokenType recovery_symbol = BOUNDARY_COMMAND_RECOVERY;
    if (valid_symbols[SEPARATOR_RECOVERY]) {
      recovery_symbol = SEPARATOR_RECOVERY;
    } else if (valid_symbols[REDIRECTION_TARGET_RECOVERY]) {
      recovery_symbol = REDIRECTION_TARGET_RECOVERY;
    } else if (valid_symbols[COMPOUND_COMMAND_RECOVERY_BOUNDARY]) {
      recovery_symbol = COMPOUND_COMMAND_RECOVERY_BOUNDARY;
    } else if (valid_symbols[CASE_ITEMS_RECOVERY_BOUNDARY]) {
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

  if (
    lexer->lookahead ==
    '$' &&
    (valid_symbols[UNBRACED_PARAMETER_START] ||
      valid_symbols[CONTINUED_DOLLAR_EXPANSION_START] ||
      valid_symbols[CONTINUED_DOLLAR_SINGLE_QUOTE_START])
  ) {
    return scan_dollar_start(lexer, valid_symbols);
  }

  if (
    ((lexer->lookahead == '#' || lexer->lookahead == '%') &&
      valid_symbols[CONTINUED_PARAMETER_PATTERN_OPERATOR_START]) ||
    ((lexer->lookahead == '<' || lexer->lookahead == '>') &&
      (valid_symbols[CONTINUED_REDIRECTION_OPERATOR_START] ||
        valid_symbols[CONTINUED_DLESSDASH_START])) ||
    (lexer->lookahead == '[' && valid_symbols[PATTERN_SPECIAL_LEFT_BRACKET])
  ) {
    return scan_compound_token_start(lexer, valid_symbols);
  }

  if (
    valid_symbols[SOURCE_WORD_CONTINUATION_BOUNDARY] &&
    (lexer->lookahead == '\\' || !is_token_delimiter(lexer->lookahead))
  ) {
    return scan_source_word_continuation_boundary(lexer, valid_symbols);
  }

  if (valid_symbols[COMMENT_BOUNDARY] && lexer->lookahead == '#') {
    lexer->mark_end(lexer);
    lexer->result_symbol = COMMENT_BOUNDARY;
    return true;
  }

  if (valid_symbols[LINE_CONTINUATION_BOUNDARY]) {
    return scan_line_continuation_boundary(lexer);
  }

  if (
    valid_symbols[BACKQUOTE_END_RECOVERY] &&
    scanner->backquote_depth >
    0 &&
    lexer->lookahead == 0
  ) {
    return scan_backquote_end_recovery(scanner, lexer);
  }

  if (
    scanner->backquote_depth >
    0 &&
    lexer->lookahead ==
    '\\' &&
    (valid_symbols[BACKQUOTE_DOLLAR_PREFIX] ||
      valid_symbols[BACKQUOTE_START_PREFIX] ||
      valid_symbols[BACKQUOTE_END_PREFIX])
  ) {
    return scan_backquote_prefix(scanner, lexer, valid_symbols);
  }

  if (
    valid_symbols[BACKQUOTE_END] &&
    is_active_backquote_boundary(scanner, lexer->lookahead)
  ) {
    return scan_backquote_end(scanner, lexer);
  }

  if (valid_symbols[BACKQUOTE_START] && (lexer->lookahead == '`')) {
    return scan_backquote_start(scanner, lexer);
  }

  if (
    (valid_symbols[BLANK_LINE_START_LINE_CONTINUATION] ||
      valid_symbols[SPACED_LINE_CONTINUATION] ||
      valid_symbols[LAYOUT_LINE_CONTINUATION] ||
      valid_symbols[RESERVED_WORD_SEPARATOR_LINE_CONTINUATION] ||
      valid_symbols[WORD_SEPARATOR_LINE_CONTINUATION] ||
      valid_symbols[SPACED_COMMENT] ||
      valid_symbols[BLANK_LINE]) &&
    (lexer->lookahead == ' ' || lexer->lookahead == '\t')
  ) {
    return scan_spaced_comment_or_blank_line(lexer, valid_symbols, true);
  }

  if (line_continuation_is_valid(valid_symbols) && lexer->lookahead == '\\') {
    return scan_line_continuation(lexer, valid_symbols);
  }

  if (valid_symbols[BLANK_LINE_BOUNDARY] && lexer->lookahead == '\n') {
    lexer->mark_end(lexer);
    lexer->result_symbol = BLANK_LINE_BOUNDARY;
    return true;
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
        lexer,
        valid_symbols[LEFT_BRACE],
        valid_symbols[IO_LOCATION],
        valid_symbols[INVALID_COMMAND_CHARACTER_SOURCE]
      )
    );
  }

  if (lexer->lookahead == '}') {
    if (valid_symbols[RIGHT_BRACE]) {
      return scan_reserved_character(lexer, '}', RIGHT_BRACE);
    }
    return (
      valid_symbols[INVALID_COMMAND_CHARACTER_SOURCE] &&
      scan_reserved_character(lexer, '}', INVALID_COMMAND_CHARACTER_SOURCE)
    );
  }

  if (lexer->lookahead == ')') {
    return (
      valid_symbols[INVALID_COMMAND_CHARACTER_SOURCE] &&
      !valid_symbols[COMMAND_SUBSTITUTION_END_LINE_CONTINUATION] &&
      scan_reserved_character(lexer, ')', INVALID_COMMAND_CHARACTER_SOURCE)
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
      lexer,
      parameter_pattern ? PARAMETER_BRACKET_LITERAL_START
                        : WORD_BRACKET_LITERAL_START,
      parameter_pattern
    );
  }

  if (valid_symbols[PARAMETER_PATTERN_BRACKET_CHARACTER]) {
    return scan_pattern_bracket_character(
      lexer,
      PARAMETER_PATTERN_BRACKET_CHARACTER,
      true
    );
  }

  if (valid_symbols[PATTERN_BRACKET_CHARACTER]) {
    return scan_pattern_bracket_character(
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

  if (
    is_decimal_digit(lexer->lookahead) &&
    (valid_symbols[CONTINUED_DECIMAL_ARITHMETIC_NUMBER_START] ||
      valid_symbols[CONTINUED_OCTAL_ARITHMETIC_NUMBER_START] ||
      valid_symbols[CONTINUED_HEXADECIMAL_ARITHMETIC_NUMBER_START])
  ) {
    enum TokenType symbol = scan_continued_arithmetic_number_start(lexer);
    if (symbol != TOKEN_COUNT && valid_symbols[symbol]) {
      lexer->result_symbol = (TSSymbol)symbol;
      return true;
    }
    return false;
  }

  if (is_decimal_digit(lexer->lookahead)) {
    return (valid_symbols[FILE_DESCRIPTOR] && scan_file_descriptor(lexer));
  }

  if (lexer->lookahead == '!') {
    if (valid_symbols[PIPELINE_NEGATION]) {
      return scan_pipeline_negation(lexer);
    }
    return (
      valid_symbols[INVALID_COMMAND_CHARACTER_SOURCE] &&
      scan_reserved_character(lexer, '!', INVALID_COMMAND_CHARACTER_SOURCE)
    );
  }

  if (is_lowercase_letter(lexer->lookahead)) {
    return scan_reserved_word(lexer, valid_symbols);
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

  if (
    valid_symbols[SPACED_COMMENT] &&
    (lexer->lookahead == ' ' || lexer->lookahead == '\t')
  ) {
    return scan_spaced_comment(lexer);
  }

  return false;
}
