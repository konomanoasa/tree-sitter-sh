#include "arithmetic.h"
#include "input.h"
#include "lexical-tokens.h"
#include "tree_sitter/alloc.h"
#include "tree_sitter/parser.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef TREE_SITTER_SERIALIZATION_BUFFER_SIZE
#define TREE_SITTER_SERIALIZATION_BUFFER_SIZE 1024
#endif

#define SCANNER_SERIALIZATION_VERSION 27
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
  QUOTED_HERE_DOCUMENT_TEXT,
  HERE_DOCUMENT_END_BEGIN,
  HERE_DOCUMENT_END_LEADING_TABS,
  HERE_DOCUMENT_END_COMMIT,
  HERE_DOCUMENT_SEQUENCE_END,
  HERE_DOCUMENT_CONTENT_LINE_START,
  NEWLINE,
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
  COMMENT_BOUNDARY,
  TRAILING_COMMENT_BOUNDARY,
  COMMENT_START,
  COMMENT_LINE_END,
  DOLLAR_EXPANSION_START,
  BRACED_PARAMETER_NUMBER_START,
  BRACED_POSITIONAL_PARAMETER_START,
  BACKQUOTE_START,
  BACKQUOTE_END,
  PATTERN_CONTINUATION,
  PATTERN_END,
  PIPE_CONTINUATION,
  REDIRECT_LIST_BEGIN,
  CASE_ITEM_END,
  FUNCTION_BODY_CONTINUATION_BOUNDARY,
  COMMAND_SUBSTITUTION_BODY_BEGIN,
  PATTERN_BRACKET_CHARACTER,
  PARAMETER_PATTERN_BRACKET_CHARACTER,
  PATTERN_BRACKET_HYPHEN,
  WORD_TILDE_END,
  ASSIGNMENT_TILDE_END,
  ASSIGNMENT_NAME_TOKEN,
  FNAME_TOKEN,
  AND_OR_CONTINUATION,
  WORD_SEPARATOR_BEGIN,
  TERM_CONTINUATION,
  ASSIGNMENT_SEPARATOR_BEGIN,
  REDIRECT_SEPARATOR_BEGIN,
  PRE_NEWLINE_BLANK,
  COMMAND_SUBSTITUTION_END,
  SEPARATOR_NEWLINE,
  LAYOUT_BEGIN,
  TERM_BOUNDARY,
  WORD_PATTERN_BRACKET_OPEN,
  PARAMETER_PATTERN_BRACKET_OPEN,
  DOUBLE_QUOTED_BACKQUOTE_START,
  DOLLAR_SINGLE_QUOTE_ESCAPE,
  PATTERN_CHARACTER_CLASS_END_COLON,
  LINE_CONTINUATION,
  REMOVED_NEWLINE,
  LITERAL_BEGIN,
  ASSIGNMENT_LITERAL_BEGIN,
  PARAMETER_LITERAL_BEGIN,
  NAME_BEGIN,
  VARIABLE_NAME_BEGIN,
  ESCAPED_CHARACTER_BEGIN,
  SINGLE_QUOTE_CONTENT_BEGIN,
  DOUBLE_QUOTE_TEXT_BEGIN,
  DOUBLE_QUOTE_ESCAPE_BEGIN,
  DOLLAR_SINGLE_QUOTE_TEXT_BEGIN,
  DOUBLE_QUOTED_PARAMETER_TEXT_BEGIN,
  DOUBLE_QUOTED_PARAMETER_ESCAPE_BEGIN,
  HERE_DOCUMENT_TEXT_BEGIN,
  HERE_DOCUMENT_ESCAPE_BEGIN,
  HERE_DOCUMENT_END_TEXT_BEGIN,
  UNBRACED_POSITIONAL_PARAMETER_BEGIN,
  SPECIAL_PARAMETER_BEGIN,
  SPECIAL_PARAMETER_HASH_BEGIN,
  PARAMETER_VALUE_OPERATOR_BEGIN,
  PARAMETER_PATTERN_OPERATOR_BEGIN,
  ARITHMETIC_NUMBER_BEGIN,
  ARITHMETIC_VARIABLE_BEGIN,
  ARITHMETIC_UNARY_OPERATOR_BEGIN,
  ARITHMETIC_ASSIGNMENT_OPERATOR_BEGIN,
  ARITHMETIC_QUESTION_OPERATOR_BEGIN,
  ARITHMETIC_COLON_OPERATOR_BEGIN,
  ARITHMETIC_LOGICAL_OR_OPERATOR_BEGIN,
  ARITHMETIC_LOGICAL_AND_OPERATOR_BEGIN,
  ARITHMETIC_BITWISE_OR_OPERATOR_BEGIN,
  ARITHMETIC_BITWISE_XOR_OPERATOR_BEGIN,
  ARITHMETIC_BITWISE_AND_OPERATOR_BEGIN,
  ARITHMETIC_EQUALITY_OPERATOR_BEGIN,
  ARITHMETIC_RELATIONAL_OPERATOR_BEGIN,
  ARITHMETIC_SHIFT_OPERATOR_BEGIN,
  ARITHMETIC_ADDITIVE_OPERATOR_BEGIN,
  ARITHMETIC_MULTIPLICATIVE_OPERATOR_BEGIN,
  AND_IF_BEGIN,
  OR_IF_BEGIN,
  DSEMI_BEGIN,
  SEMI_AND_BEGIN,
  LESSAND_BEGIN,
  GREATAND_BEGIN,
  DGREAT_BEGIN,
  LESSGREAT_BEGIN,
  CLOBBER_BEGIN,
  DLESS_OPERATOR_BEGIN,
  DLESSDASH_OPERATOR_BEGIN,
  PATTERN_STAR_BEGIN,
  PATTERN_QUESTION_BEGIN,
  PATTERN_NEGATION_BEGIN,
  PATTERN_CLASS_CONTENT_BEGIN,
  PARAMETER_PATTERN_CLASS_CONTENT_BEGIN,
  PATTERN_COLLATING_CHARACTER_BEGIN,
  PARAMETER_PATTERN_COLLATING_CHARACTER_BEGIN,
  PATTERN_EQUIVALENCE_CHARACTER_BEGIN,
  PARAMETER_PATTERN_EQUIVALENCE_CHARACTER_BEGIN,
  PUNCT_LEFT_PARENTHESIS,
  PUNCT_RIGHT_PARENTHESIS,
  PUNCT_SEMICOLON,
  PUNCT_AMPERSAND,
  PUNCT_PIPE,
  PUNCT_LESS,
  PUNCT_GREATER,
  PUNCT_EQUALS,
  PUNCT_RIGHT_BRACKET,
  PUNCT_COLON,
  PUNCT_DOT,
  SQ_OPEN,
  SQ_CLOSE,
  DQ_OPEN,
  DQ_CLOSE,
  DOLLAR_SQ_DOLLAR,
  DOLLAR_SQ_OPEN,
  DOLLAR_SQ_CLOSE,
  PARAMETER_OPEN,
  PARAMETER_CLOSE,
  COMMAND_OPEN,
  NUMERIC_PARAMETER_DIGIT,
  WORD_TILDE_START,
  ASSIGNMENT_TILDE_START,
  PARAMETER_TILDE_START,
  LOGICAL_NEWLINE_BEGIN,
  LOGICAL_BLANK_BEGIN,
  ARITHMETIC_EXPANSION_END,
  PATTERN_INITIAL_RIGHT_BRACKET_BEGIN,
  SOURCE_BEGIN,
  REMOVED_SOURCE,
  HERE_DOCUMENT_END_LINE_END,
  WORD_FALLBACK_LITERAL_BEGIN,
  ASSIGNMENT_FALLBACK_LITERAL_BEGIN,
  PARAMETER_FALLBACK_LITERAL_BEGIN,
  FALLBACK_BRACKET_OPEN,
  WORD_BRACKET_FALLBACK_END,
  ASSIGNMENT_BRACKET_FALLBACK_END,
  PARAMETER_BRACKET_FALLBACK_END,
  FALLBACK_BRACKET_CLOSE,
  FALLBACK_COLON,
  FALLBACK_DOT,
  FALLBACK_EQUALS,
  WORD_FALLBACK_CHARACTER_BEGIN,
  ASSIGNMENT_FALLBACK_CHARACTER_BEGIN,
  PARAMETER_FALLBACK_CHARACTER_BEGIN,
  WORD_INITIAL_LITERAL_BEGIN,
  WORD_INITIAL_FALLBACK_LITERAL_BEGIN,
  WORD_INITIAL_PATTERN_BRACKET_OPEN,
  WORD_INITIAL_PATTERN_STAR_BEGIN,
  WORD_INITIAL_PATTERN_QUESTION_BEGIN,
  WORD_INITIAL_ESCAPED_CHARACTER_BEGIN,
  WORD_INITIAL_SQ_OPEN,
  WORD_INITIAL_DQ_OPEN,
  WORD_INITIAL_DOLLAR_SQ_DOLLAR,
  WORD_INITIAL_DOLLAR_EXPANSION_START,
  WORD_INITIAL_BACKQUOTE_START,
  FALLBACK_SPECIAL_BRACKET_OPEN,
  HERE_DOCUMENT_LINE_LAYOUT_BEGIN,
  PARAMETER_HYPHEN_BEGIN,
  PARAMETER_QUESTION_BEGIN,
  ARITHMETIC_BLANK_BEGIN,
#define LEXICAL_SOURCE_ENUM(begin, piece) piece,
  SH_LEXICAL_SOURCE_TOKENS(LEXICAL_SOURCE_ENUM)
#undef LEXICAL_SOURCE_ENUM
#define PHYSICAL_SOURCE_ENUM(begin, prefix, character) prefix, character,
    SH_PHYSICAL_SOURCE_TOKENS(PHYSICAL_SOURCE_ENUM)
#undef PHYSICAL_SOURCE_ENUM
#define WHOLE_SOURCE_ENUM(begin, whole) whole,
      SH_WHOLE_SOURCE_TOKENS(WHOLE_SOURCE_ENUM)
#undef WHOLE_SOURCE_ENUM
        TOKEN_COUNT,
};

enum LexicalSourceKind {
  LEXICAL_SOURCE_NONE,
  LEXICAL_SOURCE_TEXT,
  LEXICAL_SOURCE_PHYSICAL,
  LEXICAL_SOURCE_WHOLE,
};

struct LexicalSource {
  enum LexicalSourceKind kind;
  enum TokenType piece;
  enum TokenType prefix;
};

static const struct LexicalSource LEXICAL_SOURCES[TOKEN_COUNT] = {
#define LEXICAL_SOURCE_ENTRY(begin, piece) \
  [begin] = {LEXICAL_SOURCE_TEXT, piece, TOKEN_COUNT},
  SH_LEXICAL_SOURCE_TOKENS(LEXICAL_SOURCE_ENTRY)
#undef LEXICAL_SOURCE_ENTRY
#define PHYSICAL_SOURCE_ENTRY(begin, prefix, character) \
  [begin] = {LEXICAL_SOURCE_PHYSICAL, character, prefix},
    SH_PHYSICAL_SOURCE_TOKENS(PHYSICAL_SOURCE_ENTRY)
#undef PHYSICAL_SOURCE_ENTRY
#define WHOLE_SOURCE_ENTRY(begin, whole) \
  [whole] = {LEXICAL_SOURCE_WHOLE, whole, TOKEN_COUNT},
      SH_WHOLE_SOURCE_TOKENS(WHOLE_SOURCE_ENTRY)
#undef WHOLE_SOURCE_ENTRY
};

static const enum TokenType WHOLE_SOURCES[TOKEN_COUNT] = {
#define WHOLE_SOURCE_SYMBOL(begin, whole) [begin] = whole,
  SH_WHOLE_SOURCE_TOKENS(WHOLE_SOURCE_SYMBOL)
#undef WHOLE_SOURCE_SYMBOL
};

static const enum TokenType WORD_INITIAL_SYMBOLS[] = {
  LITERAL_BEGIN,
  WORD_FALLBACK_LITERAL_BEGIN,
  WORD_PATTERN_BRACKET_OPEN,
  PATTERN_STAR_BEGIN,
  PATTERN_QUESTION_BEGIN,
  ESCAPED_CHARACTER_BEGIN,
  SQ_OPEN,
  DQ_OPEN,
  DOLLAR_SQ_DOLLAR,
  DOLLAR_EXPANSION_START,
  BACKQUOTE_START,
};

static enum TokenType word_initial_source_symbol(enum TokenType symbol) {
  switch (symbol) {
#define WHOLE_SOURCE_BEGIN(begin, whole) \
  case whole: \
    symbol = begin; \
    break;
    SH_WHOLE_SOURCE_TOKENS(WHOLE_SOURCE_BEGIN)
#undef WHOLE_SOURCE_BEGIN
  default:
    break;
  }
  return symbol >=
      WORD_INITIAL_LITERAL_BEGIN &&
      symbol <= WORD_INITIAL_BACKQUOTE_START
    ? WORD_INITIAL_SYMBOLS[symbol - WORD_INITIAL_LITERAL_BEGIN]
    : symbol;
}

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

struct SourceContext {
  enum TokenType opener;
  size_t stage_count;
  size_t count;
  bool local_disabled;
};

struct LexicalEmission {
  enum TokenType symbol;
  size_t remaining;
  bool active;
  bool removed_newline;
  bool ends_assignment_colon;
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
  struct SourceSnapshot source;
  struct SourceContext *contexts;
  size_t context_count;
  struct LexicalEmission emission;
  bool assignment_tilde_allowed;
  bool here_document_end_line_consumed;
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

static void backquote_stages(bool quoted, struct SourceStage *stages) {
  stages[0] = source_stage(SOURCE_REMOVE_CONTINUATIONS);
  stages[0].disabled = !quoted;
  stages[1] = source_stage(SOURCE_BACKQUOTE_DECODE);
  stages[1].quoted = quoted;
  stages[2] = source_stage(SOURCE_REMOVE_CONTINUATIONS);
}

static void here_document_body_stages(
  const struct HereDocument *document,
  struct SourceStage *stages
) {
  stages[0] = source_stage(SOURCE_REMOVE_CONTINUATIONS);
  stages[0].disabled = true;
  stages[1] = source_stage(SOURCE_REMOVE_CONTINUATIONS);
  stages[1].disabled = document->quoted;
  stages[2] = source_stage(SOURCE_STRIP_LEADING_TABS);
  stages[2].disabled = !document->strip_tabs;
  stages[3] = source_stage(SOURCE_REMOVE_CONTINUATIONS);
  stages[3].disabled = true;
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
  source_snapshot_clear(&scanner->source);
  ts_free(scanner->contexts);
  scanner->contexts = NULL;
  scanner->context_count = 0;
  scanner->emission = (struct LexicalEmission){0};
  scanner->assignment_tilde_allowed = false;
  scanner->here_document_end_line_consumed = false;
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

static bool is_horizontal_blank(int32_t character) {
  return character == ' ' || character == '\t';
}

static bool is_one_of(int32_t character, const char *set) {
  return character >
    0 &&
    character <
    128 &&
    strchr(set, (int)character) != NULL;
}

static bool is_special_parameter_character(int32_t character) {
  return is_one_of(character, "0*@#?$!-");
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
  return is_one_of(character, "&();<>|");
}

static bool accept_character(TSLexer *lexer, enum TokenType symbol) {
  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  lexer->result_symbol = (TSSymbol)symbol;
  return true;
}

/* An odd run escapes the next character unless the enclosed source ends. */
static bool skip_escape_run(TSLexer *lexer, bool *continuation) {
  size_t count = 0;
  while (lexer->lookahead == '\\') {
    if (count == SIZE_MAX) {
      return false;
    }
    count += 1;
    lexer->advance(lexer, false);
  }
  *continuation = count == 1 && lexer->lookahead == '\n';
  if ((count & 1) != 0 && lexer->lookahead != SOURCE_BACKQUOTE_BOUNDARY) {
    lexer->advance(lexer, false);
  }
  return true;
}

static bool lexer_at_eof(const TSLexer *lexer) {
  return lexer->lookahead == 0 && lexer->eof(lexer);
}

static struct LogicalLexer *lookahead_input(TSLexer *lexer);
static bool lookahead_local_policy(TSLexer *lexer, bool disabled);

static bool advance_source(TSLexer *lexer, struct ByteBuffer *source) {
  if (source != NULL && !append_codepoint(source, lexer->lookahead)) {
    return false;
  }
  lexer->advance(lexer, false);
  return true;
}

static bool advance_to_comment_end(TSLexer *lexer, struct ByteBuffer *source) {
  struct LogicalLexer *input = lookahead_input(lexer);
  bool disabled = input !=
    NULL &&
    input->cursor.stage_count >
    0 &&
    input->cursor.initial[input->cursor.stage_count - 1].disabled;
  if (lexer->lookahead != '#' || !advance_source(lexer, source)) {
    return false;
  }
  if (!lookahead_local_policy(lexer, true)) {
    return false;
  }
  while (!lexer_at_eof(lexer) && lexer->lookahead != '\n') {
    if (lexer->lookahead == SOURCE_BACKQUOTE_BOUNDARY) {
      break;
    }
    if (!advance_source(lexer, source)) {
      return false;
    }
  }
  return lookahead_local_policy(lexer, disabled);
}

static bool comment_reaches_end(const TSLexer *lexer) {
  return lexer_at_eof(lexer) || (lexer->lookahead == SOURCE_BACKQUOTE_BOUNDARY);
}

static bool is_token_delimiter_character(int32_t character) {
  return is_horizontal_blank(character) ||
    character ==
    '\n' ||
    is_control_operator_start(character) ||
    character == SOURCE_BACKQUOTE_BOUNDARY;
}

static bool is_token_delimiter(const TSLexer *lexer) {
  return lexer_at_eof(lexer) || is_token_delimiter_character(lexer->lookahead);
}

static bool pattern_boundary(const TSLexer *lexer, bool parameter) {
  if (!parameter) {
    return is_token_delimiter(lexer);
  }
  return lexer_at_eof(lexer) ||
    lexer->lookahead ==
    '}' ||
    lexer->lookahead == SOURCE_BACKQUOTE_BOUNDARY;
}

static bool is_quote_or_expansion_start(int32_t character) {
  return is_one_of(character, "'\"$`");
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

enum PatternMode {
  PATTERN_WORD,
  PATTERN_ASSIGNMENT,
  PATTERN_PARAMETER,
  PATTERN_WORD_TILDE,
  PATTERN_ASSIGNMENT_TILDE,
  PATTERN_PARAMETER_TILDE,
};

// Tree-sitter reserves -1 for undecodable bytes.
#define LOOKAHEAD_END INT32_MIN

struct LookaheadCharacter {
  int32_t value;
  size_t substitution;
  size_t view;
  size_t logical_position;
  size_t raw_cut;
  size_t next;
  size_t fork;
};

struct LookaheadView {
  struct LogicalLexer *input;
  size_t inherited;
  size_t suffix_count;
  size_t entry;
  size_t next;
};

struct LookaheadLexer {
  TSLexer lexer;
  TSLexer *source;
  struct LookaheadCharacter *characters;
  size_t length;
  size_t capacity;
  size_t position;
  struct LookaheadView *views;
  size_t view_count;
  size_t view_capacity;
  struct AmbiguousSubstitution *substitutions;
  size_t substitution_count;
  size_t substitution_capacity;
  size_t requested;
  size_t active;
  bool failed;
};

static void lookahead_fail(struct LookaheadLexer *lookahead) {
  lookahead->failed = true;
  lookahead->lexer.lookahead = 0;
  if (
    lookahead->source != NULL && lookahead->source->advance == logical_advance
  ) {
    ((struct LogicalLexer *)lookahead->source)->native->failed = true;
  }
}

static void lookahead_seek(struct LookaheadLexer *lookahead, size_t position) {
  lookahead->position = position;
  if (lookahead->failed) {
    return;
  }
  int32_t character = lookahead->characters[position].value;
  lookahead->lexer.lookahead = character == LOOKAHEAD_END ? 0 : character;
}

static bool lookahead_append(struct LookaheadLexer *lookahead, size_t view) {
  struct LogicalLexer *input = lookahead->views[view].input;
  if (input->result == SOURCE_FAILURE) {
    lookahead_fail(lookahead);
    return false;
  }
  struct LookaheadCharacter *characters = source_grow(
    lookahead->characters,
    &lookahead->capacity,
    lookahead->length + 1,
    sizeof(struct LookaheadCharacter)
  );
  if (characters == NULL) {
    lookahead_fail(lookahead);
    return false;
  }
  lookahead->characters = characters;
  lookahead->characters[lookahead->length++] = (struct LookaheadCharacter){
    .value =
      lexer_at_eof(&input->lexer) ? LOOKAHEAD_END : input->lexer.lookahead,
    .substitution = SIZE_MAX,
    .view = view,
    .logical_position = input->result == SOURCE_CHARACTER
      ? input->cursor.logical_position - 1
      : input->cursor.logical_position,
    .raw_cut = input->consumed,
    .next = SIZE_MAX,
    .fork = SIZE_MAX,
  };
  return true;
}

static struct LogicalLexer *
lookahead_restore(struct LookaheadLexer *lookahead) {
  const struct LookaheadCharacter *character =
    &lookahead->characters[lookahead->position];
  struct LogicalLexer *input = lookahead->views[character->view].input;
  input->cursor.logical_position = character->logical_position;
  input->consumed = character->raw_cut;
  logical_load(input);
  if (input->result == SOURCE_FAILURE) {
    lookahead_fail(lookahead);
  }
  return input;
}

static void lookahead_advance(TSLexer *lexer, bool skip) {
  struct LookaheadLexer *lookahead = (struct LookaheadLexer *)lexer;
  if (lookahead->failed) {
    return;
  }
  if (lookahead->length == 0 && !lookahead_append(lookahead, 0)) {
    return;
  }
  size_t position = lookahead->position;
  size_t next = lookahead->characters[position].next;
  if (next == SIZE_MAX) {
    struct LogicalLexer *input = lookahead_restore(lookahead);
    if (lexer_at_eof(&input->lexer)) {
      return;
    }
    input->lexer.advance(&input->lexer, skip);
    next = lookahead->length;
    if (!lookahead_append(lookahead, lookahead->characters[position].view)) {
      return;
    }
    lookahead->characters[position].next = next;
  }
  lookahead_seek(lookahead, next);
}

static void lookahead_mark_end(TSLexer *lexer) {
  struct LookaheadLexer *lookahead = (struct LookaheadLexer *)lexer;
  if (!lookahead->failed) {
    struct LogicalLexer *input = lookahead->views[0].input;
    input->mark = lookahead->length == 0
      ? input->consumed
      : lookahead->characters[lookahead->position].raw_cut;
  }
}

static bool lookahead_eof(const TSLexer *lexer) {
  const struct LookaheadLexer *lookahead = (const struct LookaheadLexer *)lexer;
  return lookahead->failed ||
    (lookahead->length == 0
        ? lexer_at_eof(lookahead->source)
        : lookahead->characters[lookahead->position].value == LOOKAHEAD_END);
}

static bool lookahead_is_pending(const TSLexer *lexer) {
  const struct LookaheadLexer *lookahead = (const struct LookaheadLexer *)lexer;
  return lookahead->failed || lookahead->requested != SIZE_MAX;
}

static struct LogicalLexer *lookahead_input(TSLexer *lexer) {
  if (lexer->advance == logical_advance) {
    return (struct LogicalLexer *)lexer;
  }
  if (lexer->advance != lookahead_advance) {
    return NULL;
  }
  struct LookaheadLexer *lookahead = (struct LookaheadLexer *)lexer;
  if (lookahead->length == 0 && !lookahead_append(lookahead, 0)) {
    return NULL;
  }
  return lookahead_restore(lookahead);
}

static bool lookahead_set_stages_at(
  TSLexer *lexer,
  size_t raw_cut,
  size_t inherited,
  const struct SourceStage *suffix,
  size_t suffix_count
) {
  if (lexer->advance == logical_advance) {
    if (((struct LogicalLexer *)lexer)->consumed != raw_cut) {
      return false;
    }
    return logical_replace_view(
      (struct LogicalLexer *)lexer,
      inherited,
      suffix,
      suffix_count
    );
  }
  if (lexer->advance != lookahead_advance) {
    return false;
  }
  struct LookaheadLexer *lookahead = (struct LookaheadLexer *)lexer;
  struct LogicalLexer *parent = lookahead_input(lexer);
  if (parent == NULL) {
    return false;
  }
  while (parent->base + parent->cursor.raw_count < raw_cut) {
    int32_t value;
    if (
      logical_read(parent, &value) !=
      SOURCE_CHARACTER ||
      !source_feed(&parent->cursor, value)
    ) {
      lookahead_fail(lookahead);
      return false;
    }
  }
  size_t position = lookahead->position;
  for (
    size_t index = lookahead->characters[position].fork; index != SIZE_MAX;
    index = lookahead->views[index].next
  ) {
    const struct LookaheadView *view = &lookahead->views[index];
    if (
      view->inherited !=
      inherited ||
      view->suffix_count !=
      suffix_count ||
      lookahead->characters[view->entry].raw_cut != raw_cut
    ) {
      continue;
    }
    bool matches = true;
    for (size_t stage = 0; stage < suffix_count; stage += 1) {
      const struct SourceStage *existing =
        &view->input->cursor.initial[inherited + stage];
      matches = matches &&
        existing->kind ==
        suffix[stage].kind &&
        existing->disabled ==
        suffix[stage].disabled &&
        existing->quoted ==
        suffix[stage].quoted &&
        existing->at_line_start == suffix[stage].at_line_start;
    }
    if (matches) {
      lookahead_seek(lookahead, view->entry);
      return true;
    }
  }
  struct LogicalLexer *input = ts_malloc(sizeof(*input));
  if (input == NULL) {
    lookahead_fail(lookahead);
    return false;
  }
  if (!logical_fork(input, parent, raw_cut, inherited, suffix, suffix_count)) {
    ts_free(input);
    lookahead_fail(lookahead);
    return false;
  }
  struct LookaheadView *views = source_grow(
    lookahead->views,
    &lookahead->view_capacity,
    lookahead->view_count + 1,
    sizeof(*views)
  );
  if (views == NULL) {
    logical_clear(input);
    ts_free(input);
    lookahead_fail(lookahead);
    return false;
  }
  lookahead->views = views;
  size_t index = lookahead->view_count++;
  size_t entry = lookahead->length;
  lookahead->views[index] = (struct LookaheadView){
    .input = input,
    .inherited = inherited,
    .suffix_count = suffix_count,
    .entry = entry,
    .next = lookahead->characters[position].fork,
  };
  lookahead->characters[position].fork = index;
  if (!lookahead_append(lookahead, index)) {
    return false;
  }
  lookahead_seek(lookahead, entry);
  return true;
}

static bool lookahead_set_stages(
  TSLexer *lexer,
  size_t inherited,
  const struct SourceStage *suffix,
  size_t suffix_count
) {
  struct LogicalLexer *input = lookahead_input(lexer);
  return input !=
    NULL &&
    lookahead_set_stages_at(
      lexer,
      input->consumed,
      inherited,
      suffix,
      suffix_count
    );
}

static bool lookahead_local_policy(TSLexer *lexer, bool disabled) {
  struct LogicalLexer *input = lookahead_input(lexer);
  if (input == NULL || input->cursor.stage_count == 0) {
    return false;
  }
  size_t local = input->cursor.stage_count - 1;
  if (input->cursor.initial[local].kind != SOURCE_REMOVE_CONTINUATIONS) {
    return false;
  }
  if (input->cursor.initial[local].disabled == disabled) {
    return true;
  }
  struct SourceStage stage = source_stage(SOURCE_REMOVE_CONTINUATIONS);
  stage.disabled = disabled;
  return lookahead_set_stages(lexer, local, &stage, 1);
}

struct LookaheadContext {
  size_t stage_count;
  bool local_disabled;
};

static bool lookahead_backquote_begin(
  TSLexer *lexer,
  bool quoted,
  struct LookaheadContext *context
) {
  struct LogicalLexer *input = lookahead_input(lexer);
  if (input == NULL || input->cursor.stage_count == 0) {
    return false;
  }
  size_t count = input->cursor.stage_count;
  *context = (struct LookaheadContext){
    .stage_count = count,
    .local_disabled = input->cursor.initial[count - 1].disabled,
  };
  struct SourceStage stages[3];
  backquote_stages(quoted, stages);
  return lookahead_set_stages(lexer, count - 1, stages, 3);
}

static bool
lookahead_context_end(TSLexer *lexer, const struct LookaheadContext *context) {
  if (context->stage_count == 0) {
    return true;
  }
  struct SourceStage stage = source_stage(SOURCE_REMOVE_CONTINUATIONS);
  stage.disabled = context->local_disabled;
  return lookahead_set_stages(lexer, context->stage_count - 1, &stage, 1);
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
  if (lookahead->length == 0 && !lookahead_append(lookahead, 0)) {
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
        (substitution->context.backquote_depth == scanner->backquote_depth))
    ) {
      if (!substitution->complete) {
        return request_ambiguous_substitution(lookahead, index);
      }
      return substitution;
    }
  }
  struct AmbiguousSubstitution *substitutions = source_grow(
    lookahead->substitutions,
    &lookahead->substitution_capacity,
    lookahead->substitution_count + 1,
    sizeof(struct AmbiguousSubstitution)
  );
  if (substitutions == NULL) {
    lookahead_fail(lookahead);
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
  struct ByteBuffer *delimiter,
  bool dollar
);

static bool
skip_bracket_member_expansion(const struct Scanner *scanner, TSLexer *lexer) {
  int32_t character = lexer->lookahead;
  if (character == '\'') {
    lexer->advance(lexer, false);
    return scan_delimiter_single_quoted_segment(lexer, NULL, false);
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
      return scan_delimiter_single_quoted_segment(lexer, NULL, true);
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
    } else if (
      is_decimal_digit(introducer) || is_special_parameter_character(introducer)
    ) {
      lexer->advance(lexer, false);
    }
  }
  return true;
}

enum BracketEscape {
  BRACKET_ESCAPE_MEMBER,
  BRACKET_ESCAPE_END_OF_INPUT,
};

static enum BracketEscape skip_bracket_escape(TSLexer *lexer) {
  lexer->advance(lexer, false);
  if (
    lexer_at_eof(lexer) ||
    lexer->lookahead ==
    SOURCE_BACKQUOTE_BOUNDARY ||
    lexer->lookahead == '\n'
  ) {
    return BRACKET_ESCAPE_END_OF_INPUT;
  }
  lexer->advance(lexer, false);
  return BRACKET_ESCAPE_MEMBER;
}

static bool
is_bracket_literal_boundary(const TSLexer *lexer, enum PatternMode mode) {
  return pattern_boundary(
           lexer,
           mode == PATTERN_PARAMETER || mode == PATTERN_PARAMETER_TILDE
         ) ||
    (mode == PATTERN_ASSIGNMENT_TILDE && lexer->lookahead == ':') ||
    ((mode ==
       PATTERN_WORD_TILDE ||
       mode ==
       PATTERN_ASSIGNMENT_TILDE ||
       mode == PATTERN_PARAMETER_TILDE) &&
      lexer->lookahead == '/');
}

static bool skip_bracket_class_element(
  const struct Scanner *scanner,
  TSLexer *lexer,
  enum PatternMode mode
) {
  int32_t marker = lexer->lookahead;
  lexer->advance(lexer, false);
  if (mode == PATTERN_ASSIGNMENT && marker == ':' && lexer->lookahead == '~') {
    return false;
  }
  bool has_content = false;
  bool accepted = false;
  while (!is_bracket_literal_boundary(lexer, mode)) {
    int32_t character = lexer->lookahead;
    if (character == '\\') {
      if (skip_bracket_escape(lexer) == BRACKET_ESCAPE_END_OF_INPUT) {
        break;
      }
      has_content = true;
      continue;
    }
    if (is_quote_or_expansion_start(character)) {
      if (!skip_bracket_member_expansion(scanner, lexer)) {
        break;
      }
      has_content = true;
      continue;
    }
    if (character == ']' && marker != '.') {
      break;
    }
    lexer->advance(lexer, false);
    if (
      mode == PATTERN_ASSIGNMENT && character == ':' && lexer->lookahead == '~'
    ) {
      break;
    }
    if (character == marker && has_content && lexer->lookahead == ']') {
      lexer->advance(lexer, false);
      accepted = true;
      break;
    }
    has_content = true;
  }
  return accepted;
}

static bool scan_pattern_bracket(
  const struct Scanner *scanner,
  TSLexer *lexer,
  enum PatternMode mode,
  bool *complete_result
) {
  if (lexer->lookahead != '[') {
    return false;
  }
  struct LookaheadLexer *lookahead = (struct LookaheadLexer *)lexer;
  lexer->advance(lexer, false);
  size_t opener_end = lookahead->position;
  bool has_member = false;
  bool may_be_negation = true;
  bool complete = false;
  while (!is_bracket_literal_boundary(lexer, mode)) {
    int32_t character = lexer->lookahead;
    if (character == '\\') {
      if (skip_bracket_escape(lexer) == BRACKET_ESCAPE_END_OF_INPUT) {
        break;
      }
      has_member = true;
      may_be_negation = false;
      continue;
    }
    if (is_quote_or_expansion_start(character)) {
      if (!skip_bracket_member_expansion(scanner, lexer)) {
        break;
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
        complete = true;
        break;
      }
      lexer->advance(lexer, false);
      has_member = true;
      continue;
    }
    if (character == '[') {
      lexer->advance(lexer, false);
      has_member = true;
      int32_t marker = lexer->lookahead;
      if (
        (marker == ':' || marker == '.' || marker == '=') &&
        !skip_bracket_class_element(scanner, lexer, mode)
      ) {
        break;
      }
      continue;
    }
    lexer->advance(lexer, false);
    if (
      mode == PATTERN_ASSIGNMENT && character == ':' && lexer->lookahead == '~'
    ) {
      break;
    }
    has_member = true;
  }
  size_t end = lookahead->position;
  lookahead_seek(lookahead, opener_end);
  lexer->mark_end(lexer);
  lookahead_seek(lookahead, end);
  *complete_result = complete;
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
    is_one_of(character, "[]-!*?:.=\\") ||
    is_quote_or_expansion_start(character) ||
    pattern_boundary(lexer, parameter_pattern)
  ) {
    return false;
  }
  if (character == '\n' && scanner->active_count > 0) {
    reset_here_document_delimiter_scan(scanner);
    scanner->at_here_document_line_start = true;
  }
  return accept_character(lexer, symbol);
}

enum DelimiterQuote {
  DELIMITER_UNQUOTED,
  DELIMITER_SINGLE_QUOTED,
  DELIMITER_DOUBLE_QUOTED,
  DELIMITER_PARAMETER_DOUBLE_QUOTED,
  DELIMITER_ARITHMETIC,
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
  struct LookaheadContext source_context;
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

// EXPECT_PATTERN distinguishes a closing esac from a later pattern word.
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

static bool append_case_tracker(struct CaseTrackerBuffer *cases, size_t depth) {
  struct CaseTracker *data = source_grow(
    cases->data,
    &cases->capacity,
    cases->length + 1,
    sizeof(struct CaseTracker)
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
  struct DelimiterGroupFrame *data = source_grow(
    groups->data,
    &groups->capacity,
    groups->length + 1,
    sizeof(struct DelimiterGroupFrame)
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

static bool push_backquote_delimiter_group(
  struct DelimiterGroupBuffer *groups,
  enum DelimiterQuote *quote,
  size_t *backquote_depth,
  TSLexer *lexer
) {
  if (
    *backquote_depth ==
    SIZE_MAX ||
    !push_delimiter_group(groups, '`', DELIMITER_GROUP_BACKQUOTE, *quote)
  ) {
    return false;
  }
  if (!lookahead_backquote_begin(
        lexer,
        *quote ==
          DELIMITER_DOUBLE_QUOTED ||
          *quote ==
          DELIMITER_PARAMETER_DOUBLE_QUOTED ||
          *quote == DELIMITER_ARITHMETIC,
        &groups->data[groups->length - 1].source_context
      )) {
    return false;
  }
  *backquote_depth += 1;
  *quote = DELIMITER_UNQUOTED;
  return true;
}

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
    while (is_hexadecimal_digit(lexer->lookahead)) {
      value = (uint8_t)((value << 4) | hexadecimal_value(lexer->lookahead));
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

struct CommandStart {
  int32_t first_character;
  int32_t second_character;
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

static bool here_document_source_view(
  struct LogicalLexer *view,
  struct LogicalLexer *input,
  const struct Scanner *scanner,
  const struct HereDocument *document
) {
  if (input == NULL || input->cursor.stage_count == 0) {
    return false;
  }
  if (
    scanner !=
    NULL &&
    scanner->active_count >
    0 &&
    document == &scanner->active_documents[0]
  ) {
    for (size_t index = scanner->context_count; index > 0; index -= 1) {
      const struct SourceContext *context = &scanner->contexts[index - 1];
      if (
        context->opener ==
        HERE_DOCUMENT_BODY_START ||
        context->opener == QUOTED_HERE_DOCUMENT_BODY_START
      ) {
        struct SourceStage local = source_stage(SOURCE_REMOVE_CONTINUATIONS);
        local.disabled = true;
        return logical_fork(
          view,
          input,
          input->consumed,
          context->stage_count + 2,
          &local,
          1
        );
      }
    }
  }
  struct SourceStage stages[4];
  here_document_body_stages(document, stages);
  return logical_fork(
    view,
    input,
    input->consumed,
    input->cursor.stage_count - 1,
    stages,
    4
  );
}

static bool here_document_line_start(
  struct LogicalLexer *input,
  size_t end,
  struct CommandStart *start
) {
  *start = (struct CommandStart){0};
  struct LogicalLexer view;
  if (!logical_fork(
        &view,
        input,
        input->consumed,
        input->cursor.stage_count,
        NULL,
        0
      )) {
    return false;
  }
  size_t word_length = 0;
  bool has_content = false;
  bool has_second_character = false;
  bool in_word = false;
  bool awaiting_delimiter = false;
  while (
    view.result ==
    SOURCE_CHARACTER &&
    view.base +
    (size_t)view.character.origin.character < end
  ) {
    int32_t current = view.lexer.lookahead;
    if (current == '\n') {
      break;
    }
    if (has_content && !has_second_character) {
      start->second_character = current;
      has_second_character = true;
    }
    if (!has_content && !is_horizontal_blank(current)) {
      has_content = true;
      start->first_character = current;
      in_word = is_lowercase_letter(current);
      start->first_word_is_reserved_candidate = in_word;
      awaiting_delimiter = !in_word;
      if (in_word) {
        start->first_word[word_length++] = (char)current;
      }
    } else if (in_word) {
      if (is_lowercase_letter(current)) {
        if (word_length < sizeof(start->first_word) - 1) {
          start->first_word[word_length++] = (char)current;
        } else {
          start->first_word_is_reserved_candidate = false;
        }
      } else {
        in_word = false;
        start->first_is_delimited = is_token_delimiter_character(current);
      }
    } else if (awaiting_delimiter) {
      awaiting_delimiter = false;
      start->first_is_delimited = is_token_delimiter_character(current);
    }
    view.lexer.advance(&view.lexer, false);
  }
  if (in_word || awaiting_delimiter) {
    start->first_is_delimited = true;
  }
  bool valid = view.result != SOURCE_FAILURE;
  logical_clear(&view);
  return valid;
}

static enum HereDocumentLineKind read_here_document_line(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const struct HereDocument *document,
  struct ByteBuffer *source,
  struct CommandStart *start
) {
  struct LogicalLexer *input = lookahead_input(lexer);
  if (input == NULL) {
    return HERE_DOCUMENT_LINE_INVALID;
  }
  if (start != NULL) {
    *start = (struct CommandStart){0};
  }
  struct LogicalLexer view;
  if (!here_document_source_view(&view, input, scanner, document)) {
    input->native->failed = true;
    return HERE_DOCUMENT_LINE_INVALID;
  }
  bool at_end = view.result ==
    SOURCE_END ||
    view.lexer.lookahead == SOURCE_BACKQUOTE_BOUNDARY;
  if (at_end && document->delimiter_length != 0) {
    logical_clear(&view);
    return HERE_DOCUMENT_LINE_END_OF_INPUT;
  }
  size_t delimiter_offset = 0;
  bool matches = true;
  while (view.result == SOURCE_CHARACTER) {
    int32_t character = view.lexer.lookahead;
    if (character == SOURCE_BACKQUOTE_BOUNDARY) {
      break;
    }
    if (character != '\n' && matches) {
      matches = match_here_document_delimiter_character(
        document,
        &delimiter_offset,
        character
      );
    }
    if (!advance_source(&view.lexer, source)) {
      input->native->failed = true;
      logical_clear(&view);
      return HERE_DOCUMENT_LINE_INVALID;
    }
    if (character == '\n' || (!matches && start == NULL && source == NULL)) {
      break;
    }
  }
  size_t end = view.consumed;
  if (at_end) {
    end = view.base +
      (view.result == SOURCE_END ? view.cursor.raw_count
          : view.character.origin.start > 0
          ? (size_t)view.character.origin.start
          : 0);
  }
  bool valid = view.result !=
    SOURCE_FAILURE &&
    (start == NULL || at_end || here_document_line_start(input, end, start));
  logical_clear(&view);
  if (
    !valid ||
    !lookahead_set_stages_at(lexer, end, input->cursor.stage_count, NULL, 0)
  ) {
    input->native->failed = true;
    return HERE_DOCUMENT_LINE_INVALID;
  }
  if (matches && delimiter_offset == document->delimiter_length) {
    return HERE_DOCUMENT_LINE_DELIMITER;
  }
  if (
    start !=
    NULL &&
    (start->first_character == 0 || start->first_character == '#')
  ) {
    return HERE_DOCUMENT_LINE_LAYOUT;
  }
  return HERE_DOCUMENT_LINE_CONTENT;
}

static bool scan_nested_here_document_sequence(
  const struct Scanner *scanner,
  TSLexer *lexer,
  struct ByteBuffer *source,
  const struct HereDocument *documents,
  size_t count
) {
  for (size_t index = 0; index < count; index += 1) {
    while (true) {
      enum HereDocumentLineKind kind = read_here_document_line(
        scanner,
        lexer,
        &documents[index],
        source,
        NULL
      );
      if (kind == HERE_DOCUMENT_LINE_DELIMITER) {
        break;
      }
      if (
        kind ==
        HERE_DOCUMENT_LINE_INVALID ||
        kind ==
        HERE_DOCUMENT_LINE_END_OF_INPUT ||
        lexer_at_eof(lexer)
      ) {
        return false;
      }
    }
  }
  return true;
}

static bool scan_dollar_single_quote_token(TSLexer *lexer) {
  lexer->advance(lexer, false);
  if (!scan_dollar_single_quote_escape(lexer, NULL)) {
    return false;
  }
  lexer->mark_end(lexer);
  lexer->result_symbol = DOLLAR_SINGLE_QUOTE_ESCAPE;
  return true;
}

static bool push_dollar_delimiter_group(
  const struct Scanner *scanner,
  size_t backquote_depth,
  TSLexer *lexer,
  struct ByteBuffer *delimiter,
  struct DelimiterGroupBuffer *groups,
  enum DelimiterQuote *quote
) {
  enum DelimiterQuote parent_quote = *quote;
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
  *quote = opening ==
      '{' &&
      (parent_quote ==
        DELIMITER_DOUBLE_QUOTED ||
        parent_quote ==
        DELIMITER_PARAMETER_DOUBLE_QUOTED ||
        parent_quote == DELIMITER_ARITHMETIC)
    ? DELIMITER_PARAMETER_DOUBLE_QUOTED
    : DELIMITER_UNQUOTED;

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
        DELIMITER_ARITHMETIC
      ) ||
      !append_byte(delimiter, '(')
    ) {
      return false;
    }
    lexer->advance(lexer, false);
    *quote = DELIMITER_ARITHMETIC;
  }
  return true;
}

static bool scan_delimiter_single_quoted_segment(
  TSLexer *lexer,
  struct ByteBuffer *delimiter,
  bool dollar
) {
  struct LogicalLexer *input = lookahead_input(lexer);
  bool disabled = input !=
    NULL &&
    input->cursor.stage_count >
    0 &&
    input->cursor.initial[input->cursor.stage_count - 1].disabled;
  if (!lookahead_local_policy(lexer, true)) {
    return false;
  }
  while (!lexer_at_eof(lexer)) {
    int32_t character = lexer->lookahead;
    if (character == '\'') {
      lexer->advance(lexer, false);
      return lookahead_local_policy(lexer, disabled);
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

static bool scan_delimiter_double_quoted_character(
  const struct Scanner *scanner,
  TSLexer *lexer,
  struct ByteBuffer *delimiter,
  struct DelimiterGroupBuffer *groups,
  enum DelimiterQuote *quote,
  size_t *backquote_depth,
  bool *escaped
) {
  *escaped = false;
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
            scanner,
            *backquote_depth,
            lexer,
            delimiter,
            groups,
            quote
          )) {
        return false;
      }
    }
    return true;
  }

  if (character == '`') {
    if (!append_byte(delimiter, '`')) {
      return false;
    }
    lexer->advance(lexer, false);
    return push_backquote_delimiter_group(
      groups,
      quote,
      backquote_depth,
      lexer
    );
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
      (lexer->lookahead == '"' && *quote != DELIMITER_ARITHMETIC) ||
      lexer->lookahead ==
      '\\' ||
      (*quote == DELIMITER_PARAMETER_DOUBLE_QUOTED && lexer->lookahead == '}')
    ) {
      *escaped = true;
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
  DELIMITER_READ_WORD,
};

static bool
delimiter_unit_is_quoted(struct LogicalLexer *ambient, TSLexer *lexer) {
  struct LogicalLexer *current = lookahead_input(lexer);
  if (current == NULL || current->result != SOURCE_CHARACTER) {
    return false;
  }
  int64_t start = (int64_t)current->base + current->character.origin.start;
  int64_t end = (int64_t)current->base + current->character.origin.end;
  bool quoted = false;
  while (ambient->result == SOURCE_CHARACTER) {
    int64_t position =
      (int64_t)ambient->base + ambient->character.origin.character;
    if (position >= end) {
      break;
    }
    quoted = quoted || (position >= start && ambient->lexer.lookahead == '\\');
    ambient->lexer.advance(&ambient->lexer, false);
  }
  return quoted;
}

static enum DelimiterReadResult read_here_document_delimiter(
  TSLexer *lexer,
  const struct Scanner *scanner,
  size_t backquote_depth,
  bool strip_tabs,
  struct HereDocument *document
) {
  struct LogicalLexer *input = lookahead_input(lexer);
  struct LogicalLexer ambient;
  if (
    input ==
    NULL ||
    !logical_fork(
      &ambient,
      input,
      input->consumed,
      input->cursor.stage_count,
      NULL,
      0
    )
  ) {
    if (input != NULL) {
      input->native->failed = true;
    }
    return DELIMITER_READ_RESOURCE_FAILURE;
  }
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
  bool inherited_strip_tabs = scanner !=
    NULL &&
    scanner->active_count >
    0 &&
    scanner->active_documents[0].strip_tabs;
  bool valid = true;

  while (valid) {
    int32_t character = lexer->lookahead;
    if (
      delimiter_backquote_depth >
      backquote_depth &&
      delimiter_unit_is_quoted(&ambient, lexer)
    ) {
      mark_delimiter_quoted(
        &quoted,
        collecting_nested_delimiter,
        &nested_delimiter_quoted
      );
    }
    if (ambient.result == SOURCE_FAILURE) {
      delimiter.failed = true;
      valid = false;
      break;
    }

    if (character == SOURCE_BACKQUOTE_BOUNDARY) {
      if (
        groups.length ==
        0 ||
        groups.data[groups.length - 1].kind !=
        DELIMITER_GROUP_BACKQUOTE ||
        !logical_backquote_end(lookahead_input(lexer))
      ) {
        break;
      }
      valid = append_byte(&delimiter, '`');
      lexer->advance(lexer, false);
      valid = valid &&
        lookahead_context_end(
          lexer,
          &groups.data[groups.length - 1].source_context
        );
      pop_delimiter_group(&groups, &cases, &quote);
      delimiter_backquote_depth -= 1;
      continue;
    }

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
      DELIMITER_DOUBLE_QUOTED ||
      quote ==
      DELIMITER_PARAMETER_DOUBLE_QUOTED ||
      (quote ==
        DELIMITER_ARITHMETIC &&
        (character == '$' || character == '`' || character == '\\'))
    ) {
      bool escaped;
      valid = scan_delimiter_double_quoted_character(
        scanner,
        lexer,
        &delimiter,
        &groups,
        &quote,
        &delimiter_backquote_depth,
        &escaped
      );
      if (escaped) {
        mark_delimiter_quoted(
          &quoted,
          collecting_nested_delimiter,
          &nested_delimiter_quoted
        );
      }
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
      is_token_delimiter(lexer)
    ) {
      valid = append_nested_here_document(
        &nested_documents,
        &nested_document_count,
        &delimiter,
        nested_delimiter_start,
        nested_delimiter_quoted,
        nested_delimiter_strips_tabs || inherited_strip_tabs
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
      if (character == '#' || is_token_delimiter(lexer)) {
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
      has_word_content = true;
      valid = advance_to_comment_end(lexer, &delimiter);

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
            scanner,
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
      is_token_delimiter(lexer)
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

    if (groups.length == 0 && is_token_delimiter(lexer)) {
      break;
    }

    if (!has_word_content && groups.length == 0 && character == '#') {
      valid = false;
      break;
    }

    if (character == '\'' && quote != DELIMITER_ARITHMETIC) {
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

    if (character == '"' && quote != DELIMITER_ARITHMETIC) {
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
      lexer->advance(lexer, false);
      if (lexer_at_eof(lexer)) {
        valid = false;
        continue;
      }
      has_word_content = true;
      track_delimiter_command_character(
        command_word,
        at_nested_delimiter_base,
        character
      );
      if (lexer->lookahead == SOURCE_BACKQUOTE_BOUNDARY) {
        valid = append_byte(&delimiter, '\\');
        continue;
      }
      mark_delimiter_quoted(
        &quoted,
        collecting_nested_delimiter,
        &nested_delimiter_quoted
      );
      valid = append_codepoint(&delimiter, lexer->lookahead);
      lexer->advance(lexer, false);
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
      valid = push_backquote_delimiter_group(
        &groups,
        &quote,
        &delimiter_backquote_depth,
        lexer
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
          &quote
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
          scanner,
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
  logical_clear(&ambient);
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
    .strip_tabs = strip_tabs || inherited_strip_tabs,
  };
  return DELIMITER_READ_WORD;
}

static bool
scan_here_document_delimiter(struct Scanner *scanner, TSLexer *lexer) {
  lexer->mark_end(lexer);
  struct HereDocument document;
  enum DelimiterReadResult result = read_here_document_delimiter(
    lexer,
    scanner,
    scanner->backquote_depth,
    scanner->delimiter_strips_tabs,
    &document
  );
  if (result != DELIMITER_READ_WORD) {
    if (result == DELIMITER_READ_RESOURCE_FAILURE) {
      lookahead_fail((struct LookaheadLexer *)lexer);
    }
    return false;
  }
  if (
    lookahead_is_pending(lexer) || !append_pending_document(scanner, document)
  ) {
    clear_document(&document);
    if (!lookahead_is_pending(lexer)) {
      lookahead_fail((struct LookaheadLexer *)lexer);
    }
    return false;
  }
  scanner->expecting_delimiter = false;
  scanner->delimiter_strips_tabs = false;
  lexer->result_symbol = HERE_END_BEGIN;
  return true;
}

static bool scan_here_end_commit(TSLexer *lexer) {
  if (!is_token_delimiter(lexer)) {
    return false;
  }

  lexer->mark_end(lexer);
  lexer->result_symbol = HERE_END_COMMIT;
  return true;
}

static bool
scan_delimited_character_token(TSLexer *lexer, enum TokenType symbol) {
  return accept_character(lexer, symbol) && is_token_delimiter(lexer);
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
read_reserved_word(TSLexer *lexer, char *word, const bool *valid_symbols) {
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

  if (length == 0 || !is_token_delimiter(lexer)) {
    return false;
  }

  return true;
}

static bool word_source_can_continue(const bool *valid_symbols) {
  return valid_symbols[LITERAL_BEGIN] ||
    valid_symbols[ASSIGNMENT_LITERAL_BEGIN] ||
    valid_symbols[PARAMETER_LITERAL_BEGIN] ||
    valid_symbols[WORD_FALLBACK_LITERAL_BEGIN] ||
    valid_symbols[ASSIGNMENT_FALLBACK_LITERAL_BEGIN] ||
    valid_symbols[PARAMETER_FALLBACK_LITERAL_BEGIN] ||
    valid_symbols[WORD_FALLBACK_CHARACTER_BEGIN] ||
    valid_symbols[ASSIGNMENT_FALLBACK_CHARACTER_BEGIN] ||
    valid_symbols[PARAMETER_FALLBACK_CHARACTER_BEGIN];
}

static bool scan_reserved_word(TSLexer *lexer, const bool *valid_symbols) {
  char word[6];
  TSSymbol symbol;
  if (
    !read_reserved_word(lexer, word, valid_symbols) ||
    !classify_reserved_word(word, valid_symbols, &symbol)
  ) {
    return false;
  }
  lexer->result_symbol = symbol;
  return true;
}

static bool scan_lowercase_dispatch(TSLexer *lexer, const bool *valid_symbols) {
  if (word_source_can_continue(valid_symbols)) {
    return false;
  }
  lexer->mark_end(lexer);
  return scan_reserved_word(lexer, valid_symbols);
}

static bool scan_horizontal_blanks(TSLexer *lexer);

static bool
scan_name_or_reserved_word(TSLexer *lexer, const bool *valid_symbols) {
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

  if (lexer->lookahead == '=') {
    if (!valid_symbols[ASSIGNMENT_NAME_TOKEN]) {
      return false;
    }
    lexer->result_symbol = ASSIGNMENT_NAME_TOKEN;
    return true;
  }

  if (is_name_character(lexer->lookahead) || !is_token_delimiter(lexer)) {
    return false;
  }

  if (valid_symbols[FNAME_TOKEN] && reserved_word == NULL) {
    while (true) {
      if (!scan_horizontal_blanks(lexer) && lexer->lookahead == '\\') {
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
      lexer->result_symbol = FNAME_TOKEN;
      return true;
    }
    return false;
  }

  if (symbol == TOKEN_COUNT) {
    return false;
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

static bool
classify_comment_boundary(TSLexer *lexer, const bool *valid_symbols);

static bool classify_scanned_comment_boundary(
  TSLexer *lexer,
  const bool *valid_symbols,
  bool comment_reaches_input_end
);

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

enum TermContinuation {
  TERM_SOURCE_CONTINUES,
  TERM_SOURCE_CLOSES,
  TERM_SOURCE_END,
  TERM_SOURCE_DEFER,
  TERM_SOURCE_FAILURE,
};

static enum TermContinuation classify_term_start(
  const struct CommandStart *start,
  const bool *valid_symbols
) {
  int32_t character = start->first_character;
  if (character == SOURCE_BACKQUOTE_BOUNDARY) {
    return TERM_SOURCE_END;
  }
  switch (character) {
  case ')':
    return valid_symbols[PUNCT_RIGHT_PARENTHESIS] ? TERM_SOURCE_CLOSES
                                                  : TERM_SOURCE_CONTINUES;
  case ';':
    return valid_symbols[CASE_ITEM_END] &&
        (start->second_character == ';' || start->second_character == '&')
      ? TERM_SOURCE_CLOSES
      : TERM_SOURCE_CONTINUES;
  case '}':
    return valid_symbols[RIGHT_BRACE] && start->first_is_delimited
      ? TERM_SOURCE_CLOSES
      : TERM_SOURCE_CONTINUES;
  default:
    break;
  }
  return start->first_word_is_reserved_candidate &&
      start->first_is_delimited &&
      closing_reserved_word_ends_term(start->first_word, valid_symbols)
    ? TERM_SOURCE_CLOSES
    : TERM_SOURCE_CONTINUES;
}

static bool here_document_delimiter_line_follows(
  const struct Scanner *scanner,
  TSLexer *lexer
) {
  if (scanner->active_count == 0) {
    return false;
  }

  lexer->advance(lexer, false);
  struct CommandStart start;
  return read_here_document_line(
           scanner,
           lexer,
           &scanner->active_documents[0],
           NULL,
           &start
         ) == HERE_DOCUMENT_LINE_DELIMITER;
}

static enum TermContinuation probe_here_document_continuation(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  const struct HereDocument *document = &scanner->active_documents[0];
  while (true) {
    struct CommandStart start;
    enum HereDocumentLineKind kind =
      read_here_document_line(scanner, lexer, document, NULL, &start);
    switch (kind) {
    case HERE_DOCUMENT_LINE_LAYOUT:
      continue;
    case HERE_DOCUMENT_LINE_CONTENT:
      return classify_term_start(&start, valid_symbols);
    case HERE_DOCUMENT_LINE_DELIMITER:
      return TERM_SOURCE_DEFER;
    case HERE_DOCUMENT_LINE_END_OF_INPUT:
      return TERM_SOURCE_END;
    case HERE_DOCUMENT_LINE_INVALID:
      return TERM_SOURCE_FAILURE;
    }
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

static bool scan_pipe_boundary(TSLexer *lexer, const bool *valid_symbols) {
  if (valid_symbols[PATTERN_CONTINUATION]) {
    lexer->result_symbol = PATTERN_CONTINUATION;
    return true;
  }
  return scan_command_continuation_operator(lexer, valid_symbols);
}

static bool is_word_element_start(const TSLexer *lexer) {
  int32_t character = lexer->lookahead;
  if (lexer_at_eof(lexer) || (character == SOURCE_BACKQUOTE_BOUNDARY)) {
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

static enum TermContinuation finish_term_continuation(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
);

static bool classify_shell_boundary(
  TSLexer *lexer,
  const bool *valid_symbols,
  bool crossed_layout
);

static bool classify_word_separator(
  TSLexer *lexer,
  const bool *valid_symbols,
  bool mark_blank_run
);

static bool scan_element_boundary(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  lexer->mark_end(lexer);
  bool crossed_layout = scan_horizontal_blanks(lexer);
  int32_t character = lexer->lookahead;
  if (character == '|') {
    return scan_pipe_boundary(lexer, valid_symbols);
  }
  if (character == '&') {
    return scan_command_continuation_operator(lexer, valid_symbols);
  }
  if (character == ';') {
    if (scan_case_item_terminator(lexer)) {
      lexer->result_symbol = CASE_ITEM_END;
      return valid_symbols[CASE_ITEM_END];
    }
    return false;
  }
  if (character == '\n') {
    if (has_startable_pending_document(scanner)) {
      return false;
    }
    if (crossed_layout) {
      bool term_choice =
        valid_symbols[TERM_BOUNDARY] && valid_symbols[TERM_CONTINUATION];
      if (!term_choice) {
        lexer->mark_end(lexer);
      }
      enum TermContinuation continuation =
        finish_term_continuation(scanner, lexer, valid_symbols);
      if (
        continuation == TERM_SOURCE_DEFER || continuation == TERM_SOURCE_FAILURE
      ) {
        return false;
      }
      if (
        continuation ==
        TERM_SOURCE_CONTINUES &&
        valid_symbols[TERM_CONTINUATION]
      ) {
        lexer->result_symbol = TERM_CONTINUATION;
        return true;
      }
      if (
        continuation !=
        TERM_SOURCE_CONTINUES &&
        valid_symbols[TERM_BOUNDARY] &&
        (term_choice || valid_symbols[PRE_NEWLINE_BLANK])
      ) {
        lexer->result_symbol = TERM_BOUNDARY;
        return true;
      }
      if (valid_symbols[PRE_NEWLINE_BLANK]) {
        lexer->result_symbol = PRE_NEWLINE_BLANK;
        return true;
      }
      return false;
    }
    if (!valid_symbols[SEPARATOR_NEWLINE]) {
      return false;
    }
    lexer->mark_end(lexer);
    lexer->advance(lexer, false);
    int32_t following = lexer->lookahead;
    enum TermContinuation continuation =
      finish_term_continuation(scanner, lexer, valid_symbols);
    if (continuation == TERM_SOURCE_CONTINUES) {
      lexer->result_symbol = SEPARATOR_NEWLINE;
      return true;
    }
    if (
      (continuation == TERM_SOURCE_CLOSES || continuation == TERM_SOURCE_END) &&
      valid_symbols[TERM_BOUNDARY] &&
      (!lexer_at_eof(lexer) ||
        is_horizontal_blank(following) ||
        following ==
        '\n' ||
        following == '#')
    ) {
      lexer->result_symbol = TERM_BOUNDARY;
      return true;
    }
    return false;
  }
  if (character == '#') {
    if (!crossed_layout && word_source_can_continue(valid_symbols)) {
      return false;
    }
    if (
      !valid_symbols[TERM_CONTINUATION] ||
      has_startable_pending_document(scanner)
    ) {
      return classify_comment_boundary(lexer, valid_symbols);
    }
    advance_to_comment_end(lexer, NULL);
    bool reaches_end = comment_reaches_end(lexer);
    if (!reaches_end) {
      enum TermContinuation continuation =
        finish_term_continuation(scanner, lexer, valid_symbols);
      if (continuation == TERM_SOURCE_FAILURE) {
        return false;
      }
      if (continuation == TERM_SOURCE_CONTINUES) {
        lexer->result_symbol = TERM_CONTINUATION;
        return true;
      }
    }
    return classify_scanned_comment_boundary(lexer, valid_symbols, reaches_end);
  }
  if (
    crossed_layout &&
    (valid_symbols[WORD_SEPARATOR_BEGIN] ||
      valid_symbols[ASSIGNMENT_SEPARATOR_BEGIN] ||
      valid_symbols[REDIRECT_SEPARATOR_BEGIN]) &&
    is_word_element_start(lexer)
  ) {
    return classify_word_separator(lexer, valid_symbols, true);
  }
  return classify_shell_boundary(lexer, valid_symbols, crossed_layout);
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
    redirect_ahead = lexer->lookahead == '<' || lexer->lookahead == '>';
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

static enum TermContinuation finish_term_continuation(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  bool cross_lines = !has_startable_pending_document(scanner);

  while (true) {
    scan_horizontal_blanks(lexer);
    if (lexer->lookahead == '#') {
      if (!cross_lines) {
        return TERM_SOURCE_DEFER;
      }
      advance_to_comment_end(lexer, NULL);
      continue;
    }
    if (lexer->lookahead == '\n') {
      if (!cross_lines) {
        return TERM_SOURCE_DEFER;
      }
      lexer->advance(lexer, false);
      if (scanner->active_count > 0) {
        return probe_here_document_continuation(scanner, lexer, valid_symbols);
      }
      continue;
    }

    if (
      lexer->advance ==
      lookahead_advance &&
      ((const struct LookaheadLexer *)lexer)->failed
    ) {
      return TERM_SOURCE_FAILURE;
    }
    if (lexer_at_eof(lexer)) {
      return TERM_SOURCE_END;
    }
    struct CommandStart start = {
      .first_character = lexer->lookahead,
    };
    if (is_lowercase_letter(start.first_character)) {
      start.first_word_is_reserved_candidate =
        read_reserved_word(lexer, start.first_word, NULL);
      start.first_is_delimited = start.first_word_is_reserved_candidate;
    } else if (start.first_character == '}' || start.first_character == ';') {
      lexer->advance(lexer, false);
      start.second_character = lexer->lookahead;
      start.first_is_delimited = is_token_delimiter(lexer);
    }
    if (
      lexer->advance ==
      lookahead_advance &&
      ((const struct LookaheadLexer *)lexer)->failed
    ) {
      return TERM_SOURCE_FAILURE;
    }
    return classify_term_start(&start, valid_symbols);
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

  lexer->mark_end(lexer);

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
  TSLexer *lexer,
  const bool *valid_symbols,
  bool crossed_layout
) {
  int32_t character = lexer->lookahead;
  if (character == '|') {
    return scan_pipe_boundary(lexer, valid_symbols);
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
    (crossed_layout || !word_source_can_continue(valid_symbols)) &&
    classify_comment_boundary(lexer, valid_symbols)
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

  return is_lowercase_letter(character) &&
    !crossed_layout &&
    scan_reserved_word(lexer, valid_symbols);
}

static bool scan_shell_boundary(TSLexer *lexer, const bool *valid_symbols) {
  lexer->mark_end(lexer);
  bool crossed_layout = is_horizontal_blank(lexer->lookahead) ||
    (lexer->lookahead == '\\' && valid_symbols[REDIRECT_LIST_BEGIN]);
  if (crossed_layout) {
    scan_horizontal_blanks(lexer);
  }

  return classify_shell_boundary(lexer, valid_symbols, crossed_layout);
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
    advance_to_comment_end(lexer, NULL);
    comment_reaches_input_end = comment_reaches_end(lexer);
  }
  return classify_scanned_comment_boundary(
    lexer,
    valid_symbols,
    comment_reaches_input_end
  );
}

static bool classify_layout_run(
  const struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  scan_horizontal_blanks(lexer);
  int32_t character = lexer->lookahead;
  if (
    character ==
    '#' &&
    (valid_symbols[COMMENT_BOUNDARY] ||
      valid_symbols[TRAILING_COMMENT_BOUNDARY])
  ) {
    return classify_comment_boundary(lexer, valid_symbols);
  }
  if (character == '\n' && valid_symbols[PRE_NEWLINE_BLANK]) {
    if (
      has_startable_pending_document(scanner) ||
      here_document_delimiter_line_follows(scanner, lexer)
    ) {
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
  return false;
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
  return is_one_of(character, "=!|&^<>+-*/%?:");
}

static bool
scan_arithmetic_boundary(TSLexer *lexer, const bool *valid_symbols) {
  lexer->mark_end(lexer);
  while (arithmetic_whitespace(lexer->lookahead)) {
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

// Parser ambiguity can reuse a flat subtree where a fresh parse selects the
// structured one.

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
  struct ValidationToken *data = source_grow(
    tokens->data,
    &tokens->capacity,
    tokens->length + 1,
    sizeof(struct ValidationToken)
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
  if (arithmetic_whitespace(character)) {
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
  int32_t *source = source_grow(
    scan->source,
    &scan->source_capacity,
    scan->source_length + 1,
    sizeof(int32_t)
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
  struct LookaheadContext source_context;
};

struct EmbeddedSkip {
  struct EmbeddedFrame *frames;
  size_t frame_count;
  size_t frame_capacity;
  struct CaseTrackerBuffer cases;
  struct HereDocument *pending;
  size_t pending_count;
  bool started;
};

static void clear_embedded_skip(struct EmbeddedSkip *skip) {
  ts_free(skip->frames);
  ts_free(skip->cases.data);
  clear_document_array(&skip->pending, &skip->pending_count);
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
  struct EmbeddedFrame *frames = source_grow(
    skip->frames,
    &skip->frame_capacity,
    skip->frame_count + 1,
    sizeof(struct EmbeddedFrame)
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

static bool embedded_push_source(
  struct EmbeddedSkip *skip,
  TSLexer *lexer,
  char closer,
  bool quoted
) {
  if (!embedded_push_frame(
        skip,
        closer,
        closer == '`' ? EMBEDDED_COMMAND_SUBSTITUTION : EMBEDDED_SOURCE,
        false
      )) {
    return false;
  }
  return closer !=
    '`' ||
    lookahead_backquote_begin(
      lexer,
      quoted,
      &skip->frames[skip->frame_count - 1].source_context
    );
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
      struct CommandStart start;
      enum HereDocumentLineKind kind =
        read_here_document_line(scanner, lexer, document, NULL, &start);
      if (kind == HERE_DOCUMENT_LINE_DELIMITER) {
        break;
      }
      if (kind == HERE_DOCUMENT_LINE_END_OF_INPUT) {
        result = ARITHMETIC_VALIDATION_INCOMPLETE;
        break;
      }
      if (kind == HERE_DOCUMENT_LINE_INVALID) {
        result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
        break;
      }
    }
    clear_document(document);
  }
  skip->pending_count = retained;
  return result;
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
      lookahead_fail(lookahead);
      return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
    }
    skip->started = true;
    if (initial_closer == '`') {
      skip->frames[0].kind = EMBEDDED_COMMAND_SUBSTITUTION;
      if (!lookahead_backquote_begin(
            lexer,
            double_quoted,
            &skip->frames[0].source_context
          )) {
        lookahead_fail(lookahead);
        return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
      }
    }
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
      is_token_delimiter(lexer) &&
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

    if (character == SOURCE_BACKQUOTE_BOUNDARY) {
      if (
        frame->closer != '`' || !logical_backquote_end(lookahead_input(lexer))
      ) {
        result = ARITHMETIC_VALIDATION_INCOMPLETE;
        break;
      }
      lexer->advance(lexer, false);
      if (!lookahead_context_end(lexer, &frame->source_context)) {
        result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
        break;
      }
      skip->frame_count -= 1;
      continue;
    }

    if (frame->closer == '"' || quoted_parameter) {
      if (character == '\\') {
        bool continuation;
        if (!skip_escape_run(lexer, &continuation)) {
          result = ARITHMETIC_VALIDATION_INCOMPLETE;
          break;
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
        if (!embedded_push_source(skip, lexer, (char)character, true)) {
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
      bool continuation;
      if (!skip_escape_run(lexer, &continuation)) {
        result = ARITHMETIC_VALIDATION_INCOMPLETE;
        break;
      }
      if (!continuation) {
        embedded_note_word(skip, in_command);
      }
      continue;
    }

    if (character == '\'') {
      lexer->advance(lexer, false);
      if (!scan_delimiter_single_quoted_segment(lexer, NULL, false)) {
        result = ARITHMETIC_VALIDATION_INCOMPLETE;
        break;
      }
      embedded_note_word(skip, in_command);
      continue;
    }

    if (character == '"' || character == '`') {
      lexer->advance(lexer, false);
      embedded_note_word(skip, in_command);
      if (!embedded_push_source(skip, lexer, (char)character, false)) {
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
        if (!scan_delimiter_single_quoted_segment(lexer, NULL, true)) {
          result = ARITHMETIC_VALIDATION_INCOMPLETE;
          break;
        }
      }
      continue;
    }

    if (character == '#' && in_command && !frame->command.word.active) {
      if (!advance_to_comment_end(lexer, NULL)) {
        result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
        break;
      }
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
      if (!append_document(&skip->pending, &skip->pending_count, document)) {
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
    lookahead_fail(lookahead);
  }
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
  struct EmbeddedSkip skip = {0};
  enum ArithmeticValidation result =
    resume_embedded_construct(scanner, lexer, initial_closer, false, &skip);
  clear_embedded_skip(&skip);
  return result;
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

static size_t
arithmetic_operator_length(int32_t first, int32_t second, int32_t third) {
  switch (first) {
  case '<':
  case '>':
    if (second == first) {
      return third == '=' ? 3 : 2;
    }
    return second == '=' ? 2 : 1;
  case '|':
  case '&':
  case '+':
  case '-':
    return second == '=' || second == first ? 2 : 1;
  case '=':
  case '!':
  case '^':
  case '*':
  case '/':
  case '%':
    return second == '=' ? 2 : 1;
  default:
    return 1;
  }
}

static bool validate_operator_source(
  TSLexer *lexer,
  struct ArithmeticScan *scan,
  int32_t first
) {
  struct LookaheadLexer *lookahead = (struct LookaheadLexer *)lexer;
  if (!append_arithmetic_source(scan, first)) {
    return false;
  }
  int32_t second = lexer->lookahead;
  size_t position = lookahead->position;
  lexer->advance(lexer, false);
  int32_t third = lexer->lookahead;
  lookahead_seek(lookahead, position);
  enum ArithmeticOperatorCategory classified =
    classify_arithmetic_operator(first, second, third);
  uint8_t category = classified != ARITHMETIC_OPERATOR_CATEGORY_COUNT
    ? (uint8_t)classified
    : first == '~' ? VALIDATION_OPERATOR_TILDE
    : first == '!' ? VALIDATION_OPERATOR_BANG
                   : VALIDATION_OPERATOR_REPEATED_SIGN;
  for (
    size_t length = arithmetic_operator_length(first, second, third);
    length > 1;
    length -= 1
  ) {
    if (!advance_arithmetic_source(lexer, scan)) {
      return false;
    }
  }
  return append_validation_token(
    &scan->tokens,
    VALIDATION_TOKEN_OPERATOR,
    category
  );
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
      return ARITHMETIC_VALIDATION_INVALID;
    }

    if (lexer_at_eof(lexer)) {
      return ARITHMETIC_VALIDATION_INCOMPLETE;
    }

    if (arithmetic_whitespace(character)) {
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
      lexer->advance(lexer, false);
      scan->embedded_closer = '`';
      scan->embedded = ts_calloc(1, sizeof(struct EmbeddedSkip));
      if (scan->embedded == NULL) {
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
        uint8_t *grown = source_grow(
          contexts,
          &context_capacity,
          context_count + 1,
          sizeof(uint8_t)
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
      uint8_t *grown = source_grow(
        contexts,
        &context_capacity,
        context_count + 1,
        sizeof(uint8_t)
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
    lookahead_fail((struct LookaheadLexer *)lexer);
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
    lexer->mark_end(lexer);
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

static bool
command_element_boundary_symbols_are_valid(const bool *valid_symbols) {
  return (
    valid_symbols[ASSIGNMENT_SEPARATOR_BEGIN] ||
    valid_symbols[REDIRECT_SEPARATOR_BEGIN] ||
    valid_symbols[PIPE_CONTINUATION] ||
    valid_symbols[AND_OR_CONTINUATION] ||
    valid_symbols[TERM_CONTINUATION]
  );
}

static bool element_boundary_symbols_are_valid(const bool *valid_symbols) {
  return valid_symbols[WORD_SEPARATOR_BEGIN] ||
    command_element_boundary_symbols_are_valid(valid_symbols);
}

static bool here_document_end_is_ready(
  const struct Scanner *scanner,
  const TSLexer *lexer
) {
  return scanner->active_count >
    0 &&
    !scanner->at_here_document_line_start &&
    (scanner->here_document_end_line_consumed ||
      lexer_at_eof(lexer) ||
      lexer->lookahead == SOURCE_BACKQUOTE_BOUNDARY);
}

static bool
scan_here_document_end_commit(struct Scanner *scanner, TSLexer *lexer) {
  if (!here_document_end_is_ready(scanner, lexer)) {
    return false;
  }
  scanner->here_document_end_line_consumed = false;
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
  if (!scanner->at_here_document_line_start) {
    return false;
  }
  const struct HereDocument *document = &scanner->active_documents[0];
  enum TokenType end_begin =
    document->quoted ? QUOTED_HERE_DOCUMENT_END_BEGIN : HERE_DOCUMENT_END_BEGIN;
  if (
    !valid_symbols[end_begin] &&
    !valid_symbols[HERE_DOCUMENT_CONTENT_LINE_START]
  ) {
    return false;
  }
  lexer->mark_end(lexer);
  bool is_end = read_here_document_line(scanner, lexer, document, NULL, NULL) ==
    HERE_DOCUMENT_LINE_DELIMITER;
  if (is_end && valid_symbols[end_begin]) {
    lexer->result_symbol = (TSSymbol)end_begin;
    scanner->at_here_document_line_start = false;
    return true;
  }
  if (!valid_symbols[HERE_DOCUMENT_CONTENT_LINE_START]) {
    return false;
  }
  scanner->at_here_document_line_start = false;
  lexer->result_symbol = HERE_DOCUMENT_CONTENT_LINE_START;
  return true;
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
  if (
    probe_here_document_continuation(scanner, lexer, valid_symbols) !=
    TERM_SOURCE_CONTINUES
  ) {
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
  return source_write_unsigned(
    (uint8_t *)writer->data,
    writer->capacity,
    &writer->length,
    value
  );
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
        (scanner->assignment_tilde_allowed ? 16 : 0)
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

  uint8_t source_data[SCANNER_STATE_CAPACITY];
  size_t source_length;
  if (
    !source_snapshot_serialize(
      &scanner->source,
      source_data,
      sizeof(source_data),
      &source_length
    ) ||
    !write_state_size(writer, source_length) ||
    !write_state_bytes(writer, (const char *)source_data, source_length) ||
    !write_state_size(writer, scanner->context_count)
  ) {
    return false;
  }
  size_t context_depth = 0;
  for (size_t index = 0; index < scanner->context_count; index += 1) {
    const struct SourceContext *context = &scanner->contexts[index];
    if (
      context->count ==
      0 ||
      context->count >
      SIZE_MAX -
      context_depth ||
      !write_state_size(writer, context->opener) ||
      !write_state_size(writer, context->stage_count) ||
      !write_state_size(writer, context->count) ||
      !write_state_byte(writer, context->local_disabled ? 1 : 0)
    ) {
      return false;
    }
    context_depth += context->count;
  }
  uint8_t emission_flags = (scanner->emission.active ? 1 : 0) |
    (scanner->emission.removed_newline ? 2 : 0) |
    (scanner->emission.ends_assignment_colon ? 4 : 0) |
    (scanner->here_document_end_line_consumed ? 8 : 0);
  return write_state_byte(writer, emission_flags) &&
    (!scanner->emission.active ||
      (write_state_size(writer, scanner->emission.symbol) &&
        write_state_size(writer, scanner->emission.remaining)));
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
  uint64_t decoded;
  if (
    !source_read_unsigned(
      (const uint8_t *)state->data,
      state->length,
      &state->offset,
      &decoded
    ) ||
    decoded > SIZE_MAX
  ) {
    return false;
  }
  *value = (size_t)decoded;
  return true;
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
    flags >
    31 ||
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

  size_t source_length;
  if (
    !read_size(&state, &source_length) ||
    source_length >
    state.length -
    state.offset ||
    !source_snapshot_deserialize(
      &scanner->source,
      (const uint8_t *)state.data + state.offset,
      source_length
    )
  ) {
    return false;
  }
  state.offset += source_length;
  size_t context_count;
  if (
    !read_size(&state, &context_count) ||
    context_count >
    (state.length - state.offset) /
    4 ||
    context_count >
    SIZE_MAX /
    sizeof(*scanner->contexts)
  ) {
    return false;
  }
  if (context_count > 0) {
    scanner->contexts = ts_calloc(context_count, sizeof(*scanner->contexts));
    if (scanner->contexts == NULL) {
      return false;
    }
  }
  scanner->context_count = context_count;
  size_t context_depth = 0;
  for (size_t index = 0; index < context_count; index += 1) {
    size_t symbol;
    size_t stage_count;
    size_t count;
    uint8_t disabled;
    if (
      !read_size(&state, &symbol) ||
      symbol >=
      TOKEN_COUNT ||
      !read_size(&state, &stage_count) ||
      stage_count ==
      0 ||
      stage_count >
      scanner->source.stage_count ||
      !read_size(&state, &count) ||
      count ==
      0 ||
      count >
      SIZE_MAX -
      context_depth ||
      !read_byte(&state, &disabled) ||
      disabled > 1
    ) {
      return false;
    }
    scanner->contexts[index] = (struct SourceContext){
      .opener = (enum TokenType)symbol,
      .stage_count = stage_count,
      .count = count,
      .local_disabled = disabled != 0,
    };
    context_depth += count;
  }
  uint8_t emission_flags;
  if (!read_byte(&state, &emission_flags) || emission_flags > 15) {
    return false;
  }
  scanner->emission.active = (emission_flags & 1) != 0;
  scanner->emission.removed_newline = (emission_flags & 2) != 0;
  scanner->emission.ends_assignment_colon = (emission_flags & 4) != 0;
  scanner->here_document_end_line_consumed = (emission_flags & 8) != 0;
  if (scanner->emission.active) {
    size_t symbol;
    if (
      !read_size(&state, &symbol) ||
      symbol >=
      TOKEN_COUNT ||
      (symbol !=
        SOURCE_BEGIN &&
        LEXICAL_SOURCES[symbol].kind == LEXICAL_SOURCE_NONE) ||
      !read_size(&state, &scanner->emission.remaining) ||
      scanner->emission.remaining == 0
    ) {
      return false;
    }
    scanner->emission.symbol = (enum TokenType)symbol;
  }
  if (state.offset != state.length) {
    return false;
  }

  scanner->expecting_delimiter = (flags & 1) != 0;
  scanner->delimiter_strips_tabs = (flags & 2) != 0;
  scanner->sequence_end_pending = (flags & 4) != 0;
  scanner->at_here_document_line_start = (flags & 8) != 0;
  scanner->assignment_tilde_allowed = (flags & 16) != 0;
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

static bool
scan_dispatch(struct Scanner *scanner, TSLexer *lexer, const bool *valid) {
  if (valid[COMMAND_SUBSTITUTION_BODY_BEGIN]) {
    return scan_command_substitution_body_begin(scanner, lexer, valid);
  }
  if (
    (valid[ASSIGNMENT_NAME_TOKEN] || valid[FNAME_TOKEN]) &&
    is_name_start_character(lexer->lookahead)
  ) {
    return scan_name_or_reserved_word(lexer, valid);
  }
  if (
    valid[HERE_DOCUMENT_SEQUENCE_END] &&
    scan_here_document_sequence_end(scanner, lexer)
  ) {
    return true;
  }
  if (
    valid[HERE_DOCUMENT_END_COMMIT] &&
    scanner->active_count >
    0 &&
    scan_here_document_end_commit(scanner, lexer)
  ) {
    return true;
  }
  if (scanner->active_count > 0 && scanner->at_here_document_line_start) {
    return scan_active_here_document(scanner, lexer, valid);
  }
  bool arithmetic_boundary = arithmetic_operator_boundary_is_valid(valid) ||
    valid[ARITHMETIC_CLOSING_BOUNDARY];
  if (
    arithmetic_boundary &&
    (is_arithmetic_operator_start(lexer->lookahead) ||
      lexer->lookahead ==
      ')' ||
      arithmetic_whitespace(lexer->lookahead))
  ) {
    return scan_arithmetic_boundary(lexer, valid);
  }
  if (
    scanner->active_count >
    0 &&
    lexer->lookahead ==
    '\n' &&
    valid[SEPARATOR_NEWLINE] &&
    !scanner->expecting_delimiter &&
    !valid[HERE_END_COMMIT] &&
    !valid[FUNCTION_BODY_CONTINUATION_BOUNDARY]
  ) {
    return scan_here_document_body_newline(scanner, lexer, valid);
  }
  if (scanner->expecting_delimiter && valid[HERE_END_BEGIN]) {
    return scan_here_document_delimiter(scanner, lexer);
  }
  if (valid[HERE_END_COMMIT] && scan_here_end_commit(lexer)) {
    return true;
  }
  if (valid[DLESS] || valid[DLESSDASH]) {
    return scan_here_document_operator_commit(scanner, lexer, valid);
  }
  if (valid[DOLLAR_SINGLE_QUOTE_ESCAPE] && lexer->lookahead == '\\') {
    return scan_dollar_single_quote_token(lexer);
  }
  if (
    (valid[ARITHMETIC_LEFT_PARENTHESIS] ||
      valid[ARITHMETIC_DYNAMIC_LEFT_PARENTHESIS]) &&
    lexer->lookahead == '('
  ) {
    return scan_arithmetic_left_parenthesis(scanner, lexer, valid);
  }
  if (valid[FUNCTION_BODY_CONTINUATION_BOUNDARY]) {
    lexer->mark_end(lexer);
    lexer->result_symbol = FUNCTION_BODY_CONTINUATION_BOUNDARY;
    return true;
  }
  if (
    valid[HERE_DOCUMENT_LINE_LAYOUT_BEGIN] &&
    has_startable_pending_document(scanner) &&
    is_horizontal_blank(lexer->lookahead)
  ) {
    struct LookaheadLexer *lookahead = (struct LookaheadLexer *)lexer;
    size_t position = lookahead->position;
    lexer->mark_end(lexer);
    scan_horizontal_blanks(lexer);
    if (lexer->lookahead == '\n') {
      lexer->result_symbol = HERE_DOCUMENT_LINE_LAYOUT_BEGIN;
      return true;
    }
    lookahead_seek(lookahead, position);
  }
  if (
    element_boundary_symbols_are_valid(valid) &&
    (is_horizontal_blank(lexer->lookahead) ||
      lexer->lookahead ==
      ';' ||
      lexer->lookahead ==
      '&' ||
      lexer->lookahead ==
      '|' ||
      lexer->lookahead ==
      '\n' ||
      lexer->lookahead == '#')
  ) {
    return scan_element_boundary(scanner, lexer, valid);
  }
  if (valid[PRE_NEWLINE_BLANK] && is_horizontal_blank(lexer->lookahead)) {
    scan_horizontal_blanks(lexer);
    lexer->mark_end(lexer);
    return classify_layout_run(scanner, lexer, valid);
  }
  bool comment_boundary =
    valid[COMMENT_BOUNDARY] || valid[TRAILING_COMMENT_BOUNDARY];
  bool shell_boundary = valid[PATTERN_CONTINUATION] ||
    valid[PATTERN_END] ||
    valid[PIPE_CONTINUATION] ||
    valid[AND_OR_CONTINUATION] ||
    valid[REDIRECT_LIST_BEGIN] ||
    valid[CASE_ITEM_END] ||
    comment_boundary;
  if (
    shell_boundary &&
    (is_horizontal_blank(lexer->lookahead) ||
      (lexer->lookahead ==
        '|' &&
        (valid[PATTERN_CONTINUATION] ||
          valid[PIPE_CONTINUATION] ||
          valid[AND_OR_CONTINUATION])) ||
      (lexer->lookahead == '&' && valid[AND_OR_CONTINUATION]) ||
      (lexer->lookahead == ')' && valid[PATTERN_END]) ||
      (lexer->lookahead == ';' && valid[CASE_ITEM_END]) ||
      (lexer->lookahead == '#' && comment_boundary) ||
      ((lexer->lookahead ==
         '<' ||
         lexer->lookahead ==
         '>' ||
         is_decimal_digit(lexer->lookahead)) &&
        valid[REDIRECT_LIST_BEGIN]))
  ) {
    return scan_shell_boundary(lexer, valid);
  }
  if (lexer->lookahead == '$' && valid[DOLLAR_EXPANSION_START]) {
    return scan_dollar_expansion_start(lexer);
  }
  if (lexer->lookahead == ':' && valid[PATTERN_CHARACTER_CLASS_END_COLON]) {
    return accept_character(lexer, PATTERN_CHARACTER_CLASS_END_COLON) &&
      lexer->lookahead == ']';
  }
  if (lexer->lookahead == '[' && valid[PATTERN_SPECIAL_LEFT_BRACKET]) {
    return scan_pattern_special_left_bracket(lexer);
  }
  if (valid[PATTERN_BRACKET_HYPHEN] && lexer->lookahead == '-') {
    return accept_character(lexer, PATTERN_BRACKET_HYPHEN);
  }
  if (
    valid[PARAMETER_PATTERN_BRACKET_CHARACTER] &&
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
    valid[PATTERN_BRACKET_CHARACTER] &&
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
    return valid[LEFT_BRACE] &&
      scan_delimited_character_token(lexer, LEFT_BRACE);
  }
  if (lexer->lookahead == '}') {
    bool reachable = valid[RIGHT_BRACE] &&
      (!valid[TERM_CONTINUATION] ||
        !(valid[WORD_SEPARATOR_BEGIN] ||
          valid[ASSIGNMENT_SEPARATOR_BEGIN] ||
          valid[REDIRECT_SEPARATOR_BEGIN]));
    return (reachable || valid[FNAME_TOKEN]) &&
      scan_delimited_character_token(lexer, RIGHT_BRACE);
  }
  if (
    is_decimal_digit(lexer->lookahead) &&
    (valid[BRACED_PARAMETER_NUMBER_START] ||
      valid[BRACED_POSITIONAL_PARAMETER_START])
  ) {
    return scan_braced_numeric_parameter_start(lexer, valid);
  }
  if (is_decimal_digit(lexer->lookahead)) {
    return valid[FILE_DESCRIPTOR] && scan_file_descriptor(lexer);
  }
  if (lexer->lookahead == '!') {
    return (valid[PIPELINE_NEGATION] || valid[FNAME_TOKEN]) &&
      scan_delimited_character_token(lexer, PIPELINE_NEGATION);
  }
  return is_lowercase_letter(lexer->lookahead) &&
    scan_lowercase_dispatch(lexer, valid);
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
          lookahead_fail(lookahead);
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

static bool lookahead_init(struct LookaheadLexer *lookahead, TSLexer *lexer) {
  *lookahead = (struct LookaheadLexer){0};
  if (lexer->advance != logical_advance) {
    return false;
  }
  *lookahead = (struct LookaheadLexer){
    .lexer = *lexer,
    .source = lexer,
    .requested = SIZE_MAX,
    .active = SIZE_MAX,
  };
  lookahead->lexer.advance = lookahead_advance;
  lookahead->lexer.mark_end = lookahead_mark_end;
  lookahead->lexer.eof = lookahead_eof;
  lookahead->views = ts_calloc(1, sizeof(*lookahead->views));
  if (lookahead->views == NULL) {
    lookahead_fail(lookahead);
    return false;
  }
  lookahead->view_count = 1;
  lookahead->view_capacity = 1;
  lookahead->views[0].input = (struct LogicalLexer *)lexer;
  return true;
}

static void lookahead_clear(struct LookaheadLexer *lookahead) {
  ts_free(lookahead->characters);
  for (size_t index = 0; index < lookahead->substitution_count; index += 1) {
    clear_arithmetic_scan(&lookahead->substitutions[index].scan);
    if (lookahead->substitutions[index].command != NULL) {
      clear_embedded_skip(lookahead->substitutions[index].command);
      ts_free(lookahead->substitutions[index].command);
    }
  }
  ts_free(lookahead->substitutions);
  for (size_t index = 1; index < lookahead->view_count; index += 1) {
    logical_clear(lookahead->views[index].input);
    ts_free(lookahead->views[index].input);
  }
  ts_free(lookahead->views);
  *lookahead = (struct LookaheadLexer){0};
}

static bool scan_with_lookahead(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  struct LookaheadLexer lookahead;
  if (!lookahead_init(&lookahead, lexer)) {
    return false;
  }
  bool accepted = scan_dispatch(scanner, &lookahead.lexer, valid_symbols);
  lexer->result_symbol = lookahead.lexer.result_symbol;
  accepted = accepted && !lookahead.failed;
  lookahead_clear(&lookahead);
  return accepted;
}

static bool scanner_source_ready(struct Scanner *scanner) {
  if (scanner->source.stage_count > 0) {
    return true;
  }
  scanner->source.stages = ts_malloc(sizeof(*scanner->source.stages));
  if (scanner->source.stages == NULL) {
    return false;
  }
  scanner->source.stages[0] = source_stage(SOURCE_REMOVE_CONTINUATIONS);
  scanner->source.stage_count = 1;
  return true;
}

static bool
source_context_push(struct Scanner *scanner, enum TokenType opener) {
  size_t depth = 0;
  for (size_t index = 0; index < scanner->context_count; index += 1) {
    size_t count = scanner->contexts[index].count;
    if (count == 0 || count >= SIZE_MAX - depth) {
      return false;
    }
    depth += count;
  }
  size_t count = scanner->source.stage_count;
  struct SourceStage *local = &scanner->source.stages[count - 1];
  struct SourceContext *previous = scanner->context_count == 0
    ? NULL
    : &scanner->contexts[scanner->context_count - 1];
  if (
    previous !=
    NULL &&
    previous->opener ==
    opener &&
    previous->stage_count ==
    count &&
    previous->local_disabled == local->disabled
  ) {
    previous->count += 1;
  } else {
    if (scanner->context_count >= SIZE_MAX / sizeof(*scanner->contexts)) {
      return false;
    }
    struct SourceContext *contexts = ts_realloc(
      scanner->contexts,
      (scanner->context_count + 1) * sizeof(*contexts)
    );
    if (contexts == NULL) {
      return false;
    }
    scanner->contexts = contexts;
    contexts[scanner->context_count++] = (struct SourceContext){
      .opener = opener,
      .stage_count = count,
      .count = 1,
      .local_disabled = local->disabled,
    };
  }
  if (opener == BACKQUOTE_START || opener == DOUBLE_QUOTED_BACKQUOTE_START) {
    if (count > SIZE_MAX / sizeof(*local) - 2) {
      return false;
    }
    struct SourceStage *stages =
      ts_realloc(scanner->source.stages, (count + 2) * sizeof(*stages));
    if (stages == NULL) {
      return false;
    }
    scanner->source.stages = stages;
    backquote_stages(
      opener == DOUBLE_QUOTED_BACKQUOTE_START,
      stages + count - 1
    );
    scanner->source.stage_count += 2;
    scanner->backquote_depth += 1;
  } else if (
    opener ==
    HERE_DOCUMENT_BODY_START ||
    opener == QUOTED_HERE_DOCUMENT_BODY_START
  ) {
    if (scanner->active_count == 0 || count > SIZE_MAX / sizeof(*local) - 3) {
      return false;
    }
    struct SourceStage *stages =
      ts_realloc(scanner->source.stages, (count + 3) * sizeof(*stages));
    if (stages == NULL) {
      return false;
    }
    scanner->source.stages = stages;
    here_document_body_stages(
      &scanner->active_documents[0],
      stages + count - 1
    );
    scanner->source.stage_count += 3;
  } else if (opener == SQ_OPEN || opener == DOLLAR_SQ_OPEN) {
    local->disabled = true;
  } else if (opener == DQ_OPEN || opener == COMMAND_OPEN) {
    local->disabled = false;
  }
  return scanner_state_fits(scanner);
}

static bool source_context_pop(struct Scanner *scanner) {
  if (scanner->context_count == 0) {
    return false;
  }
  struct SourceContext *top = &scanner->contexts[scanner->context_count - 1];
  if (top->count == 0) {
    return false;
  }
  struct SourceContext context = *top;
  if (--top->count == 0) {
    scanner->context_count -= 1;
  }
  scanner->source.stage_count = context.stage_count;
  scanner->source.stages[context.stage_count - 1].disabled =
    context.local_disabled;
  if (
    context.opener ==
    BACKQUOTE_START ||
    context.opener == DOUBLE_QUOTED_BACKQUOTE_START
  ) {
    if (scanner->backquote_depth == 0) {
      return false;
    }
    scanner->backquote_depth -= 1;
  }
  return true;
}

static bool
source_context_commit(struct Scanner *scanner, enum TokenType symbol) {
  switch (symbol) {
  case SQ_OPEN:
  case DQ_OPEN:
  case DOLLAR_SQ_OPEN:
  case PARAMETER_OPEN:
  case COMMAND_OPEN:
  case BACKQUOTE_START:
  case DOUBLE_QUOTED_BACKQUOTE_START:
  case WORD_TILDE_START:
  case ASSIGNMENT_TILDE_START:
  case PARAMETER_TILDE_START:
  case WORD_FALLBACK_LITERAL_BEGIN:
  case ASSIGNMENT_FALLBACK_LITERAL_BEGIN:
  case PARAMETER_FALLBACK_LITERAL_BEGIN:
    return source_context_push(scanner, symbol);
  case SQ_CLOSE:
  case DQ_CLOSE:
  case DOLLAR_SQ_CLOSE:
  case PARAMETER_CLOSE:
  case ARITHMETIC_EXPANSION_END:
    return source_context_pop(scanner);
  case BACKQUOTE_END:
    if (
      scanner->context_count >
      0 &&
      scanner->contexts[scanner->context_count - 1].opener ==
      COMMENT_START &&
      !source_context_pop(scanner)
    ) {
      return false;
    }
    return source_context_pop(scanner);
  case COMMAND_SUBSTITUTION_END:
    if (scanner->substitution_depth == 0) {
      return false;
    }
    scanner->substitution_depth -= 1;
    return source_context_pop(scanner);
  case HERE_DOCUMENT_LINE_END:
  case NEWLINE:
  case COMMENT_LINE_END:
  case LOGICAL_NEWLINE_BEGIN:
    if (
      scanner->context_count >
      0 &&
      scanner->contexts[scanner->context_count - 1].opener ==
      COMMENT_START &&
      !source_context_pop(scanner)
    ) {
      return false;
    }
    if (
      symbol ==
      HERE_DOCUMENT_LINE_END &&
      !activate_startable_pending_documents(scanner)
    ) {
      return false;
    }
    reset_here_document_delimiter_scan(scanner);
    if (scanner->active_count > 0) {
      scanner->at_here_document_line_start = true;
    }
    return true;
  default:
    return true;
  }
}

static bool begin_lexical_emission(
  struct Scanner *scanner,
  const struct LogicalLexer *input,
  enum TokenType symbol,
  const bool *valid
) {
  if (input->mark == 0) {
    return false;
  }
  scanner->emission = (struct LexicalEmission){
    .symbol = symbol,
    .remaining = input->mark,
    .active = true,
  };
  enum TokenType whole = WHOLE_SOURCES[symbol];
  if (whole != 0 && valid[whole]) {
    bool fragmented = false;
    for (size_t index = 0; index < input->cursor.removal_count; index += 1) {
      const struct SourceRemoval *removal = &input->cursor.removals[index];
      if (
        removal->kind ==
        SOURCE_REMOVED_CONTINUATION &&
        removal->slash >=
        0 &&
        (size_t)removal->slash +
        input->base < input->mark
      ) {
        fragmented = true;
        break;
      }
    }
    if (!fragmented)
      scanner->emission.symbol = whole;
  }
  symbol = word_initial_source_symbol(symbol);
  if (
    symbol ==
    ASSIGNMENT_LITERAL_BEGIN ||
    symbol == ASSIGNMENT_FALLBACK_LITERAL_BEGIN
  ) {
    for (size_t index = 0; index < input->cursor.logical_count; index += 1) {
      const struct SourceCharacter *character = &input->cursor.logical[index];
      if (
        character->origin.end >
        0 &&
        (size_t)character->origin.end +
        input->base <= input->mark
      ) {
        scanner->emission.ends_assignment_colon = character->value == ':';
      }
    }
  }
  if (symbol == COMMENT_START) {
    if (!source_context_push(scanner, COMMENT_START)) {
      return false;
    }
    scanner->source.stages[scanner->source.stage_count - 1].disabled = true;
  }
  return true;
}

static bool emission_checkpoint(
  struct Scanner *scanner,
  const struct SourceCursor *cursor,
  size_t cut
) {
  struct SourceSnapshot snapshot = {0};
  if (!source_snapshot_at(cursor, cut, &snapshot)) {
    return false;
  }
  source_snapshot_clear(&scanner->source);
  scanner->source = snapshot;
  return true;
}

static bool
removal_has_slash(const struct SourceCursor *cursor, int64_t slash) {
  for (size_t index = 0; index < cursor->removal_count; index += 1) {
    const struct SourceRemoval *removal = &cursor->removals[index];
    if (
      removal->kind == SOURCE_REMOVED_CONTINUATION && removal->slash == slash
    ) {
      return true;
    }
  }
  return false;
}

static bool
removal_has_newline(const struct SourceCursor *cursor, int64_t newline) {
  for (size_t index = 0; index < cursor->removal_count; index += 1) {
    const struct SourceRemoval *removal = &cursor->removals[index];
    if (
      removal->kind ==
      SOURCE_REMOVED_CONTINUATION &&
      removal->origin.character == newline
    ) {
      return true;
    }
  }
  return false;
}

static bool emit_physical_source(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid
) {
  struct LexicalEmission *emission = &scanner->emission;
  if (
    emission->symbol >=
    TOKEN_COUNT ||
    (emission->symbol !=
      SOURCE_BEGIN &&
      LEXICAL_SOURCES[emission->symbol].kind == LEXICAL_SOURCE_NONE)
  ) {
    return false;
  }
  const struct LexicalSource *source = &LEXICAL_SOURCES[emission->symbol];
  if (emission->removed_newline) {
    if (lexer->lookahead != '\n' || !valid[REMOVED_NEWLINE]) {
      return false;
    }
  } else if (emission->remaining == 0) {
    return false;
  }

  struct SourceCursor cursor;
  if (!source_cursor_resume(&cursor, &scanner->source, NULL, NULL)) {
    return false;
  }
  bool punctuation = source->kind == LEXICAL_SOURCE_PHYSICAL;
  enum TokenType symbol = emission->symbol == SOURCE_BEGIN ? REMOVED_SOURCE
    : punctuation                                          ? source->prefix
                                                           : source->piece;
  size_t cut = 0;
  bool accepted = true;
  if (source->kind == LEXICAL_SOURCE_WHOLE) {
    while (accepted && cut < emission->remaining) {
      accepted = !lexer->eof(lexer) && source_feed(&cursor, lexer->lookahead);
      if (accepted) {
        lexer->advance(lexer, false);
        cut += 1;
      }
    }
    lexer->mark_end(lexer);
  } else if (emission->removed_newline) {
    accepted = source_feed(&cursor, '\n') && removal_has_newline(&cursor, 0);
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    cut = 1;
    symbol = REMOVED_NEWLINE;
    emission->removed_newline = false;
  } else if (punctuation && emission->remaining == 1) {
    accepted = !lexer->eof(lexer) && source_feed(&cursor, lexer->lookahead);
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    cut = 1;
    symbol = source->piece;
  } else if (lexer->lookahead == '\\') {
    accepted = source_feed(&cursor, '\\');
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    cut = 1;
    if (accepted && lexer->lookahead == '\n') {
      accepted = source_feed(&cursor, '\n');
      lexer->advance(lexer, false);
      if (accepted && removal_has_slash(&cursor, 0)) {
        symbol = LINE_CONTINUATION;
        emission->removed_newline = true;
      }
    }
  } else {
    do {
      if (lexer->eof(lexer) || !source_feed(&cursor, lexer->lookahead)) {
        accepted = false;
        break;
      }
      bool removed_newline =
        lexer->lookahead == '\n' && removal_has_newline(&cursor, (int64_t)cut);
      lexer->advance(lexer, false);
      cut += 1;
      lexer->mark_end(lexer);
      if (removed_newline) {
        symbol = REMOVED_NEWLINE;
        break;
      }
    } while (
      cut <
      emission->remaining &&
      lexer->lookahead !=
      '\\' &&
      lexer->lookahead !=
      '\n' &&
      !punctuation
    );
  }
  accepted = accepted &&
    cut <=
    emission->remaining &&
    valid[symbol] &&
    emission_checkpoint(scanner, &cursor, cut);
  source_cursor_clear(&cursor);
  if (!accepted) {
    return false;
  }
  emission->remaining -= cut;
  lexer->result_symbol = (TSSymbol)symbol;
  if (emission->remaining == 0) {
    enum TokenType completed = word_initial_source_symbol(emission->symbol);
    if (punctuation) {
      scanner->assignment_tilde_allowed = completed ==
        PUNCT_EQUALS ||
        (completed ==
          FALLBACK_COLON &&
          scanner->context_count >
          0 &&
          scanner->contexts[scanner->context_count - 1].opener ==
          ASSIGNMENT_FALLBACK_LITERAL_BEGIN);
    } else if (completed != SOURCE_BEGIN) {
      scanner->assignment_tilde_allowed = emission->ends_assignment_colon;
    }
    if (completed == HERE_DOCUMENT_END_LINE_END) {
      scanner->here_document_end_line_consumed = true;
    }
    *emission = (struct LexicalEmission){0};
    return source_context_commit(scanner, completed);
  }
  return true;
}

struct LexicalOperator {
  const char *spelling;
  enum TokenType symbol;
};

static const struct LexicalOperator LEXICAL_OPERATORS[] = {
  {"&&", AND_IF_BEGIN},
  {"||", OR_IF_BEGIN},
  {";;", DSEMI_BEGIN},
  {";&", SEMI_AND_BEGIN},
  {"<&", LESSAND_BEGIN},
  {">&", GREATAND_BEGIN},
  {">>", DGREAT_BEGIN},
  {"<>", LESSGREAT_BEGIN},
  {">|", CLOBBER_BEGIN},
  {"<<", DLESS_OPERATOR_BEGIN},
  {"<<-", DLESSDASH_OPERATOR_BEGIN},
  {":-", PARAMETER_VALUE_OPERATOR_BEGIN},
  {":=", PARAMETER_VALUE_OPERATOR_BEGIN},
  {":?", PARAMETER_VALUE_OPERATOR_BEGIN},
  {":+", PARAMETER_VALUE_OPERATOR_BEGIN},
  {"=", PARAMETER_VALUE_OPERATOR_BEGIN},
  {"+", PARAMETER_VALUE_OPERATOR_BEGIN},
  {"%%", PARAMETER_PATTERN_OPERATOR_BEGIN},
  {"%", PARAMETER_PATTERN_OPERATOR_BEGIN},
  {"+", ARITHMETIC_UNARY_OPERATOR_BEGIN},
  {"-", ARITHMETIC_UNARY_OPERATOR_BEGIN},
  {"!", ARITHMETIC_UNARY_OPERATOR_BEGIN},
  {"~", ARITHMETIC_UNARY_OPERATOR_BEGIN},
  {"<<=", ARITHMETIC_ASSIGNMENT_OPERATOR_BEGIN},
  {">>=", ARITHMETIC_ASSIGNMENT_OPERATOR_BEGIN},
  {"*=", ARITHMETIC_ASSIGNMENT_OPERATOR_BEGIN},
  {"/=", ARITHMETIC_ASSIGNMENT_OPERATOR_BEGIN},
  {"%=", ARITHMETIC_ASSIGNMENT_OPERATOR_BEGIN},
  {"+=", ARITHMETIC_ASSIGNMENT_OPERATOR_BEGIN},
  {"-=", ARITHMETIC_ASSIGNMENT_OPERATOR_BEGIN},
  {"&=", ARITHMETIC_ASSIGNMENT_OPERATOR_BEGIN},
  {"^=", ARITHMETIC_ASSIGNMENT_OPERATOR_BEGIN},
  {"|=", ARITHMETIC_ASSIGNMENT_OPERATOR_BEGIN},
  {"=", ARITHMETIC_ASSIGNMENT_OPERATOR_BEGIN},
  {"?", ARITHMETIC_QUESTION_OPERATOR_BEGIN},
  {":", ARITHMETIC_COLON_OPERATOR_BEGIN},
  {"||", ARITHMETIC_LOGICAL_OR_OPERATOR_BEGIN},
  {"&&", ARITHMETIC_LOGICAL_AND_OPERATOR_BEGIN},
  {"|", ARITHMETIC_BITWISE_OR_OPERATOR_BEGIN},
  {"^", ARITHMETIC_BITWISE_XOR_OPERATOR_BEGIN},
  {"&", ARITHMETIC_BITWISE_AND_OPERATOR_BEGIN},
  {"==", ARITHMETIC_EQUALITY_OPERATOR_BEGIN},
  {"!=", ARITHMETIC_EQUALITY_OPERATOR_BEGIN},
  {"<=", ARITHMETIC_RELATIONAL_OPERATOR_BEGIN},
  {">=", ARITHMETIC_RELATIONAL_OPERATOR_BEGIN},
  {"<", ARITHMETIC_RELATIONAL_OPERATOR_BEGIN},
  {">", ARITHMETIC_RELATIONAL_OPERATOR_BEGIN},
  {"<<", ARITHMETIC_SHIFT_OPERATOR_BEGIN},
  {">>", ARITHMETIC_SHIFT_OPERATOR_BEGIN},
  {"+", ARITHMETIC_ADDITIVE_OPERATOR_BEGIN},
  {"-", ARITHMETIC_ADDITIVE_OPERATOR_BEGIN},
  {"*", ARITHMETIC_MULTIPLICATIVE_OPERATOR_BEGIN},
  {"/", ARITHMETIC_MULTIPLICATIVE_OPERATOR_BEGIN},
  {"%", ARITHMETIC_MULTIPLICATIVE_OPERATOR_BEGIN},
};

enum {
  LEXICAL_OPERATOR_COUNT = sizeof(LEXICAL_OPERATORS) /
  sizeof(LEXICAL_OPERATORS[0])
};

static bool lexical_operator(TSLexer *lexer, const bool *valid) {
  bool candidates[LEXICAL_OPERATOR_COUNT];
  for (size_t index = 0; index < LEXICAL_OPERATOR_COUNT; index += 1) {
    candidates[index] = valid[LEXICAL_OPERATORS[index].symbol];
  }
  enum TokenType symbol = TOKEN_COUNT;
  for (size_t length = 0;; length += 1) {
    bool matched = false;
    for (size_t index = 0; index < LEXICAL_OPERATOR_COUNT; index += 1) {
      const struct LexicalOperator *candidate = &LEXICAL_OPERATORS[index];
      if (candidates[index]) {
        candidates[index] = candidate->spelling[length] !=
          '\0' &&
          candidate->spelling[length] == lexer->lookahead;
        matched = matched || candidates[index];
      }
    }
    if (!matched || lexer_at_eof(lexer)) {
      break;
    }
    lexer->advance(lexer, false);
    for (size_t index = 0; index < LEXICAL_OPERATOR_COUNT; index += 1) {
      if (
        candidates[index] &&
        LEXICAL_OPERATORS[index].spelling[length + 1] == '\0'
      ) {
        symbol = LEXICAL_OPERATORS[index].symbol;
        lexer->mark_end(lexer);
        break;
      }
    }
  }
  if (symbol == TOKEN_COUNT) {
    return false;
  }
  lexer->result_symbol = (TSSymbol)symbol;
  return true;
}

static bool lexical_name(TSLexer *lexer, enum TokenType symbol) {
  if (!is_name_start_character(lexer->lookahead)) {
    return false;
  }
  do {
    lexer->advance(lexer, false);
  } while (is_name_character(lexer->lookahead));
  lexer->mark_end(lexer);
  lexer->result_symbol = (TSSymbol)symbol;
  return true;
}

static bool lexical_number(TSLexer *lexer) {
  int32_t first = lexer->lookahead;
  if (!is_decimal_digit(first)) {
    return false;
  }
  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  if (first == '0' && (lexer->lookahead == 'x' || lexer->lookahead == 'X')) {
    lexer->advance(lexer, false);
    if (!is_hexadecimal_digit(lexer->lookahead)) {
      lexer->result_symbol = ARITHMETIC_NUMBER_BEGIN;
      return true;
    }
    do {
      lexer->advance(lexer, false);
    } while (is_hexadecimal_digit(lexer->lookahead));
  } else if (first == '0') {
    while (lexer->lookahead >= '0' && lexer->lookahead <= '7') {
      lexer->advance(lexer, false);
    }
  } else {
    while (is_decimal_digit(lexer->lookahead)) {
      lexer->advance(lexer, false);
    }
  }
  lexer->mark_end(lexer);
  lexer->result_symbol = ARITHMETIC_NUMBER_BEGIN;
  return true;
}

static bool lexical_escape(TSLexer *lexer, enum TokenType symbol) {
  if (lexer->lookahead != '\\') {
    return false;
  }
  lexer->advance(lexer, false);
  int32_t character = lexer->lookahead;
  if (
    lexer_at_eof(lexer) ||
    character ==
    '\n' ||
    character == SOURCE_BACKQUOTE_BOUNDARY
  ) {
    return false;
  }
  bool accepted = symbol ==
    ESCAPED_CHARACTER_BEGIN ||
    character ==
    '\\' ||
    character ==
    '$' ||
    character == '`';
  if (
    symbol ==
    DOUBLE_QUOTE_ESCAPE_BEGIN ||
    symbol == DOUBLE_QUOTED_PARAMETER_ESCAPE_BEGIN
  ) {
    accepted = accepted || character == '"';
  }
  if (symbol == DOUBLE_QUOTED_PARAMETER_ESCAPE_BEGIN) {
    accepted = accepted || character == '}';
  }
  if (!accepted) {
    return false;
  }
  lexer->advance(lexer, false);
  lexer->mark_end(lexer);
  lexer->result_symbol = (TSSymbol)symbol;
  return true;
}

static bool lexical_text(struct LogicalLexer *input, enum TokenType symbol) {
  TSLexer *lexer = &input->lexer;
  bool consumed = false;
  bool quoted = symbol ==
    DOUBLE_QUOTE_TEXT_BEGIN ||
    symbol == DOUBLE_QUOTED_PARAMETER_TEXT_BEGIN;
  while (!lexer_at_eof(lexer)) {
    int32_t character = lexer->lookahead;
    if (character == SOURCE_BACKQUOTE_BOUNDARY) {
      break;
    }
    if (symbol == SINGLE_QUOTE_CONTENT_BEGIN) {
      if (character == '\'') {
        break;
      }
    } else if (symbol == DOLLAR_SINGLE_QUOTE_TEXT_BEGIN) {
      if (character == '\'' || character == '\\') {
        break;
      }
    } else if (
      symbol ==
      HERE_DOCUMENT_END_TEXT_BEGIN ||
      symbol ==
      QUOTED_HERE_DOCUMENT_TEXT ||
      symbol == QUOTED_HERE_DOCUMENT_END_TEXT
    ) {
      if (character == '\n') {
        break;
      }
    } else {
      if (
        (quoted && character == '"') ||
        character ==
        '`' ||
        (symbol == DOUBLE_QUOTED_PARAMETER_TEXT_BEGIN && character == '}') ||
        (symbol == HERE_DOCUMENT_TEXT_BEGIN && character == '\n')
      ) {
        break;
      }
      if (character == '$') {
        int32_t next = logical_peek(input);
        if (is_parameter_start_character(next) || next == '{' || next == '(') {
          break;
        }
      }
      if (character == '\\') {
        int32_t next = logical_peek(input);
        if (
          next ==
          '$' ||
          next ==
          '`' ||
          next ==
          '\\' ||
          (quoted && next == '"') ||
          (symbol == DOUBLE_QUOTED_PARAMETER_TEXT_BEGIN && next == '}')
        ) {
          break;
        }
      }
    }
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    consumed = true;
  }
  lexer->result_symbol = (TSSymbol)symbol;
  return consumed;
}

struct LexicalPunctuation {
  int32_t character;
  enum TokenType symbol;
};

static const struct LexicalPunctuation LEXICAL_PUNCTUATION[] = {
  {'\'', SQ_CLOSE},
  {'\'', DOLLAR_SQ_CLOSE},
  {'"', DQ_CLOSE},
  {'\'', SQ_OPEN},
  {'\'', DOLLAR_SQ_OPEN},
  {'"', DQ_OPEN},
  {'{', PARAMETER_OPEN},
  {'}', PARAMETER_CLOSE},
  {'(', COMMAND_OPEN},
  {'(', PUNCT_LEFT_PARENTHESIS},
  {')', PUNCT_RIGHT_PARENTHESIS},
  {';', PUNCT_SEMICOLON},
  {'&', PUNCT_AMPERSAND},
  {'|', PUNCT_PIPE},
  {'<', PUNCT_LESS},
  {'>', PUNCT_GREATER},
  {'=', PUNCT_EQUALS},
  {']', PUNCT_RIGHT_BRACKET},
  {':', PUNCT_COLON},
  {'.', PUNCT_DOT},
  {'~', WORD_TILDE_START},
  {'~', ASSIGNMENT_TILDE_START},
  {'~', PARAMETER_TILDE_START},
  {'\n', LOGICAL_NEWLINE_BEGIN},
  {SOURCE_BACKQUOTE_BOUNDARY, BACKQUOTE_END},
  {'`', DOUBLE_QUOTED_BACKQUOTE_START},
  {'`', BACKQUOTE_START},
};

static bool lexical_punctuation(TSLexer *lexer, const bool *valid) {
  for (
    size_t index = 0;
    index < sizeof(LEXICAL_PUNCTUATION) / sizeof(LEXICAL_PUNCTUATION[0]);
    index += 1
  ) {
    const struct LexicalPunctuation *punctuation = &LEXICAL_PUNCTUATION[index];
    if (
      lexer->lookahead == punctuation->character && valid[punctuation->symbol]
    ) {
      if (
        punctuation->symbol ==
        BACKQUOTE_END &&
        !logical_backquote_end((struct LogicalLexer *)lexer)
      ) {
        return false;
      }
      lexer->advance(lexer, false);
      if (
        (punctuation->symbol ==
          PUNCT_SEMICOLON &&
          (lexer->lookahead == ';' || lexer->lookahead == '&')) ||
        (punctuation->symbol == PUNCT_AMPERSAND && lexer->lookahead == '&')
      ) {
        return false;
      }
      lexer->mark_end(lexer);
      lexer->result_symbol = (TSSymbol)punctuation->symbol;
      return true;
    }
  }
  if (lexer->lookahead == '$' && valid[DOLLAR_SQ_DOLLAR]) {
    return accept_character(lexer, DOLLAR_SQ_DOLLAR) &&
      lexer->lookahead == '\'';
  }
  if (valid[NUMERIC_PARAMETER_DIGIT] && is_decimal_digit(lexer->lookahead)) {
    return accept_character(lexer, NUMERIC_PARAMETER_DIGIT);
  }
  return false;
}

static bool lexical_classify(struct LogicalLexer *input, const bool *valid) {
  TSLexer *lexer = &input->lexer;
  if (lexical_operator(lexer, valid)) {
    return true;
  }
  logical_rewind(input);
  if (lexical_punctuation(lexer, valid)) {
    return true;
  }
  logical_rewind(input);
  const enum TokenType names[] =
    {NAME_BEGIN, VARIABLE_NAME_BEGIN, ARITHMETIC_VARIABLE_BEGIN};
  for (size_t index = 0; index < sizeof(names) / sizeof(names[0]); index += 1) {
    if (valid[names[index]] && lexical_name(lexer, names[index])) {
      return true;
    }
  }
  if (valid[ARITHMETIC_NUMBER_BEGIN] && lexical_number(lexer)) {
    return true;
  }
  logical_rewind(input);
  const enum TokenType escapes[] = {
    ESCAPED_CHARACTER_BEGIN,
    DOUBLE_QUOTE_ESCAPE_BEGIN,
    DOUBLE_QUOTED_PARAMETER_ESCAPE_BEGIN,
    HERE_DOCUMENT_ESCAPE_BEGIN
  };
  for (
    size_t index = 0; index < sizeof(escapes) / sizeof(escapes[0]); index += 1
  ) {
    if (valid[escapes[index]] && lexical_escape(lexer, escapes[index])) {
      return true;
    }
    logical_rewind(input);
  }
  if (is_horizontal_blank(lexer->lookahead) && valid[LOGICAL_BLANK_BEGIN]) {
    scan_horizontal_blanks(lexer);
    lexer->mark_end(lexer);
    lexer->result_symbol = LOGICAL_BLANK_BEGIN;
    return true;
  }
  if (
    valid[ARITHMETIC_BLANK_BEGIN] &&
    lexer->lookahead !=
    '\n' &&
    arithmetic_whitespace(lexer->lookahead)
  ) {
    do {
      lexer->advance(lexer, false);
    } while (
      lexer->lookahead != '\n' && arithmetic_whitespace(lexer->lookahead)
    );
    lexer->mark_end(lexer);
    lexer->result_symbol = ARITHMETIC_BLANK_BEGIN;
    return true;
  }
  const struct LexicalPunctuation characters[] = {
    {'*', PATTERN_STAR_BEGIN},
    {'?', PATTERN_QUESTION_BEGIN},
    {'!', PATTERN_NEGATION_BEGIN},
    {']', PATTERN_INITIAL_RIGHT_BRACKET_BEGIN},
    {'#', SPECIAL_PARAMETER_HASH_BEGIN},
    {'-', PARAMETER_HYPHEN_BEGIN},
    {'?', PARAMETER_QUESTION_BEGIN},
  };
  for (
    size_t index = 0; index < sizeof(characters) / sizeof(characters[0]);
    index += 1
  ) {
    if (
      valid[characters[index].symbol] &&
      lexer->lookahead == characters[index].character
    ) {
      return accept_character(lexer, characters[index].symbol);
    }
  }
  if (
    (valid[SPECIAL_PARAMETER_BEGIN] &&
      is_special_parameter_character(lexer->lookahead)) ||
    (valid[UNBRACED_POSITIONAL_PARAMETER_BEGIN] &&
      lexer->lookahead >=
      '1' &&
      lexer->lookahead <= '9')
  ) {
    return accept_character(
      lexer,
      valid[SPECIAL_PARAMETER_BEGIN] &&
          is_special_parameter_character(lexer->lookahead)
        ? SPECIAL_PARAMETER_BEGIN
        : UNBRACED_POSITIONAL_PARAMETER_BEGIN
    );
  }
  const enum TokenType texts[] = {
    SINGLE_QUOTE_CONTENT_BEGIN,
    DOUBLE_QUOTE_TEXT_BEGIN,
    DOUBLE_QUOTED_PARAMETER_TEXT_BEGIN,
    DOLLAR_SINGLE_QUOTE_TEXT_BEGIN,
    QUOTED_HERE_DOCUMENT_TEXT,
    QUOTED_HERE_DOCUMENT_END_TEXT,
    HERE_DOCUMENT_END_TEXT_BEGIN,
    HERE_DOCUMENT_TEXT_BEGIN
  };
  for (size_t index = 0; index < sizeof(texts) / sizeof(texts[0]); index += 1) {
    if (valid[texts[index]] && lexical_text(input, texts[index])) {
      return true;
    }
  }
  return false;
}

static bool pattern_structured_start(struct LogicalLexer *input) {
  int32_t character = input->lexer.lookahead;
  if (character == '\\') {
    return logical_peek(input) != SOURCE_BACKQUOTE_BOUNDARY;
  }
  if (character != '$') {
    return character == '\'' || character == '"' || character == '`';
  }
  int32_t follower = logical_peek(input);
  return is_parameter_start_character(follower) ||
    follower ==
    '(' ||
    follower ==
    '{' ||
    follower == '\'';
}

static bool lexical_pattern_probe(
  const struct Scanner *scanner,
  struct LogicalLexer *input,
  enum TokenType literal_symbol,
  bool *complete
) {
  struct LogicalPosition position = logical_position(input);
  bool parameter = literal_symbol == PARAMETER_LITERAL_BEGIN;
  bool assignment = literal_symbol == ASSIGNMENT_LITERAL_BEGIN;
  enum PatternMode mode = parameter ? PATTERN_PARAMETER
    : assignment                    ? PATTERN_ASSIGNMENT
                                    : PATTERN_WORD;
  if (scanner->context_count > 0) {
    enum TokenType context =
      scanner->contexts[scanner->context_count - 1].opener;
    if (context == ASSIGNMENT_TILDE_START) {
      mode = PATTERN_ASSIGNMENT_TILDE;
    } else if (context == WORD_TILDE_START) {
      mode = PATTERN_WORD_TILDE;
    } else if (context == PARAMETER_TILDE_START) {
      mode = PATTERN_PARAMETER_TILDE;
    }
  }
  *complete = false;
  struct LookaheadLexer lookahead = {0};
  bool accepted = lookahead_init(&lookahead, &input->lexer) &&
    lookahead_append(&lookahead, 0);
  if (accepted) {
    accepted =
      scan_pattern_bracket(scanner, &lookahead.lexer, mode, complete) &&
      !lookahead_is_pending(&lookahead.lexer);
  }
  *complete = accepted && *complete;
  lookahead_clear(&lookahead);
  logical_restore_position(input, &position);
  return accepted;
}

static bool
lexical_pattern_content(struct LogicalLexer *input, enum TokenType symbol) {
  TSLexer *lexer = &input->lexer;
  bool parameter = symbol ==
    PARAMETER_PATTERN_CLASS_CONTENT_BEGIN ||
    symbol ==
    PARAMETER_PATTERN_COLLATING_CHARACTER_BEGIN ||
    symbol == PARAMETER_PATTERN_EQUIVALENCE_CHARACTER_BEGIN;
  bool text = symbol ==
    PATTERN_CLASS_CONTENT_BEGIN ||
    symbol == PARAMETER_PATTERN_CLASS_CONTENT_BEGIN;
  int32_t marker = text ? ':'
    : symbol ==
      PATTERN_COLLATING_CHARACTER_BEGIN ||
      symbol == PARAMETER_PATTERN_COLLATING_CHARACTER_BEGIN
    ? '.'
    : '=';
  bool consumed = false;
  while (
    !pattern_boundary(lexer, parameter) && !pattern_structured_start(input)
  ) {
    int32_t character = lexer->lookahead;
    if (
      (character == marker && logical_peek(input) == ']') ||
      (character == ']' && marker != '.')
    ) {
      break;
    }
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    consumed = true;
    if (!text) {
      break;
    }
  }
  lexer->result_symbol = (TSSymbol)symbol;
  return consumed;
}

static bool lexical_pattern_literal(
  const struct Scanner *scanner,
  struct LogicalLexer *input,
  enum TokenType symbol,
  const bool *valid
) {
  TSLexer *lexer = &input->lexer;
  bool parameter = symbol == PARAMETER_LITERAL_BEGIN;
  bool assignment = symbol == ASSIGNMENT_LITERAL_BEGIN;
  enum TokenType context = scanner->context_count == 0
    ? TOKEN_COUNT
    : scanner->contexts[scanner->context_count - 1].opener;
  bool tilde = context ==
    WORD_TILDE_START ||
    context ==
    ASSIGNMENT_TILDE_START ||
    context == PARAMETER_TILDE_START;
  bool consumed = false;
  bool fallback = valid[FALLBACK_BRACKET_OPEN];
  bool entered_fallback = false;
  while (
    !pattern_boundary(lexer, parameter) && !pattern_structured_start(input)
  ) {
    int32_t character = lexer->lookahead;
    if (
      character ==
      '*' ||
      character ==
      '?' ||
      (tilde && (character == '/' || (assignment && character == ':')))
    ) {
      break;
    }
    if (character == '[') {
      if (fallback) {
        break;
      }
      bool complete;
      if (
        !lexical_pattern_probe(scanner, input, symbol, &complete) || complete
      ) {
        break;
      }
      enum TokenType first = parameter ? PARAMETER_FALLBACK_LITERAL_BEGIN
        : assignment                   ? ASSIGNMENT_FALLBACK_LITERAL_BEGIN
                                       : WORD_FALLBACK_LITERAL_BEGIN;
      if (!valid[first]) {
        break;
      }
      fallback = true;
      entered_fallback = true;
    }
    bool tilde_prefix =
      assignment && character == ':' && logical_peek(input) == '~';
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    consumed = true;
    if (tilde_prefix) {
      break;
    }
  }
  lexer->result_symbol =
    (TSSymbol)(entered_fallback ? parameter ? PARAMETER_FALLBACK_LITERAL_BEGIN
          : assignment                      ? ASSIGNMENT_FALLBACK_LITERAL_BEGIN
                                            : WORD_FALLBACK_LITERAL_BEGIN
                                : symbol);
  return consumed;
}

static bool lexical_pattern_classify(
  const struct Scanner *scanner,
  struct LogicalLexer *input,
  const bool *valid
) {
  TSLexer *lexer = &input->lexer;
  if (lexer->lookahead == '#' && valid[SPECIAL_PARAMETER_HASH_BEGIN]) {
    return false;
  }
  enum TokenType context = scanner->context_count == 0
    ? TOKEN_COUNT
    : scanner->contexts[scanner->context_count - 1].opener;
  bool fallback = context ==
    WORD_FALLBACK_LITERAL_BEGIN ||
    context ==
    ASSIGNMENT_FALLBACK_LITERAL_BEGIN ||
    context == PARAMETER_FALLBACK_LITERAL_BEGIN;
  bool boundary = false;
  if (fallback) {
    boundary =
      pattern_boundary(lexer, context == PARAMETER_FALLBACK_LITERAL_BEGIN) ||
      (context ==
        ASSIGNMENT_FALLBACK_LITERAL_BEGIN &&
        lexer->lookahead ==
        '~' &&
        scanner->assignment_tilde_allowed);
    if (scanner->context_count > 1) {
      enum TokenType parent =
        scanner->contexts[scanner->context_count - 2].opener;
      if (
        parent ==
        WORD_TILDE_START ||
        parent ==
        ASSIGNMENT_TILDE_START ||
        parent == PARAMETER_TILDE_START
      ) {
        boundary = boundary ||
          lexer->lookahead ==
          '/' ||
          (parent == ASSIGNMENT_TILDE_START && lexer->lookahead == ':');
      }
    }
  }
  const enum TokenType ends[] = {
    WORD_BRACKET_FALLBACK_END,
    ASSIGNMENT_BRACKET_FALLBACK_END,
    PARAMETER_BRACKET_FALLBACK_END
  };
  for (size_t index = 0; index < sizeof(ends) / sizeof(ends[0]); index += 1) {
    if (fallback && boundary && valid[ends[index]]) {
      lexer->mark_end(lexer);
      lexer->result_symbol = (TSSymbol)ends[index];
      return true;
    }
  }
  if (fallback) {
    if (boundary || pattern_structured_start(input)) {
      return false;
    }
    int32_t follower = lexer->lookahead == '[' ? logical_peek(input) : 0;
    enum TokenType cell = lexer->lookahead == '['
      ? follower == ':' || follower == '.' || follower == '='
        ? FALLBACK_SPECIAL_BRACKET_OPEN
        : FALLBACK_BRACKET_OPEN
      : lexer->lookahead == ']' ? FALLBACK_BRACKET_CLOSE
      : lexer->lookahead == ':' ? FALLBACK_COLON
      : lexer->lookahead == '.' ? FALLBACK_DOT
      : lexer->lookahead == '=' ? FALLBACK_EQUALS
      : lexer->lookahead == '*' ? PATTERN_STAR_BEGIN
      : lexer->lookahead == '?' ? PATTERN_QUESTION_BEGIN
      : lexer->lookahead == '!' ? PATTERN_NEGATION_BEGIN
      : lexer->lookahead == '-' ? PATTERN_BRACKET_HYPHEN
      : context == PARAMETER_FALLBACK_LITERAL_BEGIN
      ? PARAMETER_FALLBACK_CHARACTER_BEGIN
      : context == ASSIGNMENT_FALLBACK_LITERAL_BEGIN
      ? ASSIGNMENT_FALLBACK_CHARACTER_BEGIN
      : WORD_FALLBACK_CHARACTER_BEGIN;
    return valid[cell] && accept_character(lexer, cell);
  }
  const enum TokenType contents[] = {
    PARAMETER_PATTERN_CLASS_CONTENT_BEGIN,
    PATTERN_CLASS_CONTENT_BEGIN,
    PARAMETER_PATTERN_COLLATING_CHARACTER_BEGIN,
    PATTERN_COLLATING_CHARACTER_BEGIN,
    PARAMETER_PATTERN_EQUIVALENCE_CHARACTER_BEGIN,
    PATTERN_EQUIVALENCE_CHARACTER_BEGIN,
  };
  for (
    size_t index = 0; index < sizeof(contents) / sizeof(contents[0]); index += 1
  ) {
    if (
      valid[contents[index]] && lexical_pattern_content(input, contents[index])
    ) {
      return true;
    }
  }
  if (
    lexer->lookahead ==
    '[' &&
    (valid[WORD_PATTERN_BRACKET_OPEN] || valid[PARAMETER_PATTERN_BRACKET_OPEN])
  ) {
    enum TokenType literal = valid[PARAMETER_PATTERN_BRACKET_OPEN]
      ? PARAMETER_LITERAL_BEGIN
      : valid[ASSIGNMENT_LITERAL_BEGIN] ? ASSIGNMENT_LITERAL_BEGIN
                                        : LITERAL_BEGIN;
    bool complete;
    bool accepted = lexical_pattern_probe(scanner, input, literal, &complete);
    if (!accepted) {
      return false;
    }
    if (complete) {
      return accept_character(
        lexer,
        valid[PARAMETER_PATTERN_BRACKET_OPEN] ? PARAMETER_PATTERN_BRACKET_OPEN
                                              : WORD_PATTERN_BRACKET_OPEN
      );
    }
  }
  enum TokenType member = valid[PARAMETER_PATTERN_BRACKET_CHARACTER]
    ? PARAMETER_PATTERN_BRACKET_CHARACTER
    : PATTERN_BRACKET_CHARACTER;
  bool parameter = member == PARAMETER_PATTERN_BRACKET_CHARACTER;
  int32_t character = lexer->lookahead;
  if (valid[member]) {
    if (
      pattern_boundary(lexer, parameter) ||
      pattern_structured_start(input) ||
      character ==
      ']' ||
      character ==
      '-' ||
      (character == '!' && valid[PATTERN_NEGATION_BEGIN])
    ) {
      return false;
    }
    if (character == '[' && valid[PATTERN_SPECIAL_LEFT_BRACKET]) {
      int32_t follower = logical_peek(input);
      if (follower == ':' || follower == '.' || follower == '=') {
        return false;
      }
    }
    return accept_character(lexer, member);
  }
  if (
    character ==
    '~' &&
    (valid[WORD_TILDE_START] ||
      valid[ASSIGNMENT_TILDE_START] ||
      valid[PARAMETER_TILDE_START])
  ) {
    return false;
  }
  const enum TokenType literals[] = {
    ASSIGNMENT_LITERAL_BEGIN,
    PARAMETER_LITERAL_BEGIN,
    LITERAL_BEGIN,
  };
  for (
    size_t index = 0; index < sizeof(literals) / sizeof(literals[0]); index += 1
  ) {
    if (
      valid[literals[index]] &&
      lexical_pattern_literal(scanner, input, literals[index], valid)
    ) {
      return true;
    }
  }
  return false;
}

static bool scanner_copy(const struct Scanner *scanner, struct Scanner *copy) {
  char state[SCANNER_STATE_CAPACITY];
  struct StateWriter writer = {.data = state, .capacity = sizeof(state)};
  *copy = (struct Scanner){0};
  if (
    !serialize_scanner_state(scanner, &writer) ||
    !deserialize_scanner_state(copy, state, writer.length)
  ) {
    clear_scanner(copy);
    return false;
  }
  return true;
}

static bool classify_word_start(
  struct Scanner *scanner,
  struct LogicalLexer *input,
  const bool *valid
);

static bool classify_logical_source(
  struct Scanner *scanner,
  struct LogicalLexer *input,
  const bool *valid
) {
  if (valid[HERE_DOCUMENT_END_LINE_END] && input->lexer.lookahead == '\n') {
    input->lexer.advance(&input->lexer, false);
    input->lexer.mark_end(&input->lexer);
    input->lexer.result_symbol = HERE_DOCUMENT_END_LINE_END;
    return true;
  }
  if (scanner->context_count > 0) {
    enum TokenType context =
      scanner->contexts[scanner->context_count - 1].opener;
    int32_t character = input->lexer.lookahead;
    bool tilde = context ==
      WORD_TILDE_START ||
      context ==
      ASSIGNMENT_TILDE_START ||
      context == PARAMETER_TILDE_START;
    enum TokenType end =
      context == ASSIGNMENT_TILDE_START ? ASSIGNMENT_TILDE_END : WORD_TILDE_END;
    if (
      tilde &&
      valid[end] &&
      (character ==
        '/' ||
        (context == ASSIGNMENT_TILDE_START && character == ':') ||
        pattern_boundary(&input->lexer, context == PARAMETER_TILDE_START))
    ) {
      input->lexer.mark_end(&input->lexer);
      input->lexer.result_symbol = (TSSymbol)end;
      return source_context_pop(scanner);
    }
  }
  struct Scanner control;
  if (!scanner_copy(scanner, &control)) {
    return false;
  }
  bool control_recognized = scan_with_lookahead(&control, &input->lexer, valid);
  if (control_recognized) {
    if (
      input->lexer.result_symbol >=
      TOKEN_COUNT ||
      !valid[input->lexer.result_symbol]
    ) {
      clear_scanner(&control);
      return false;
    }
    clear_scanner(scanner);
    *scanner = control;
    return true;
  }
  clear_scanner(&control);
  logical_rewind(input);
  if (!(valid[WORD_TILDE_START] && input->lexer.lookahead == '~')) {
    if (classify_word_start(scanner, input, valid)) {
      return true;
    }
    logical_rewind(input);
  }
  if (input->lexer.lookahead == '\n') {
    enum TokenType symbol =
      valid[HERE_DOCUMENT_LINE_END] && has_startable_pending_document(scanner)
      ? HERE_DOCUMENT_LINE_END
      : valid[COMMENT_LINE_END] ? COMMENT_LINE_END
      : valid[NEWLINE]          ? NEWLINE
                                : TOKEN_COUNT;
    if (symbol != TOKEN_COUNT) {
      input->lexer.advance(&input->lexer, false);
      input->lexer.mark_end(&input->lexer);
      input->lexer.result_symbol = (TSSymbol)symbol;
      return true;
    }
  }
  if (valid[COMMENT_START] && input->lexer.lookahead == '#') {
    if (!advance_to_comment_end(&input->lexer, NULL)) {
      return false;
    }
    input->lexer.mark_end(&input->lexer);
    input->lexer.result_symbol = COMMENT_START;
    return true;
  }
  bool lexical_valid[TOKEN_COUNT];
  memcpy(lexical_valid, valid, sizeof(lexical_valid));
  lexical_valid[ASSIGNMENT_TILDE_START] =
    valid[ASSIGNMENT_TILDE_START] && scanner->assignment_tilde_allowed;
  if (lexical_pattern_classify(scanner, input, lexical_valid)) {
    return true;
  }
  logical_rewind(input);
  return lexical_classify(input, lexical_valid);
}

static bool classify_word_start(
  struct Scanner *scanner,
  struct LogicalLexer *input,
  const bool *valid
) {
  bool word_valid[TOKEN_COUNT] = {false};
  bool available = false;
  for (
    size_t index = 0;
    index < sizeof(WORD_INITIAL_SYMBOLS) / sizeof(WORD_INITIAL_SYMBOLS[0]);
    index += 1
  ) {
    word_valid[WORD_INITIAL_SYMBOLS[index]] =
      valid[WORD_INITIAL_LITERAL_BEGIN + index];
    available = available || valid[WORD_INITIAL_LITERAL_BEGIN + index];
  }
  if (
    !available ||
    is_token_delimiter(&input->lexer) ||
    !is_word_element_start(&input->lexer)
  ) {
    return false;
  }
  if (is_decimal_digit(input->lexer.lookahead)) {
    do {
      input->lexer.advance(&input->lexer, false);
    } while (is_decimal_digit(input->lexer.lookahead));
    if (input->lexer.lookahead == '<' || input->lexer.lookahead == '>') {
      return false;
    }
    logical_rewind(input);
  }
  if (!classify_logical_source(scanner, input, word_valid)) {
    return false;
  }
  for (
    size_t index = 0;
    index < sizeof(WORD_INITIAL_SYMBOLS) / sizeof(WORD_INITIAL_SYMBOLS[0]);
    index += 1
  ) {
    if (input->lexer.result_symbol == WORD_INITIAL_SYMBOLS[index]) {
      input->lexer.result_symbol =
        (TSSymbol)(WORD_INITIAL_LITERAL_BEGIN + index);
      return true;
    }
  }
  return false;
}

static bool classify_recovery_source(
  struct Scanner *scanner,
  struct LogicalLexer *input,
  const bool *valid
) {
  const struct LexicalPunctuation reserved_punctuation[] = {
    {'{', LEFT_BRACE},
    {'}', RIGHT_BRACE},
    {'!', PIPELINE_NEGATION},
  };
  for (
    size_t index = 0;
    index < sizeof(reserved_punctuation) / sizeof(reserved_punctuation[0]);
    index += 1
  ) {
    const struct LexicalPunctuation *entry = &reserved_punctuation[index];
    if (input->lexer.lookahead == entry->character && valid[entry->symbol]) {
      if (scan_delimited_character_token(&input->lexer, entry->symbol)) {
        return true;
      }
      logical_rewind(input);
      break;
    }
  }
  if (
    is_lowercase_letter(input->lexer.lookahead) &&
    scan_lowercase_dispatch(&input->lexer, valid)
  ) {
    return true;
  }
  logical_rewind(input);
  if (lexical_classify(input, valid)) {
    return true;
  }
  logical_rewind(input);
  return classify_word_start(scanner, input, valid);
}

static bool scan_source(
  struct Scanner *scanner,
  TSLexer *lexer,
  const bool *valid,
  bool recovering
) {
  if (!scanner_source_ready(scanner)) {
    return false;
  }
  if (scanner->emission.active) {
    return emit_physical_source(scanner, lexer, valid);
  }
  lexer->mark_end(lexer);
  const enum TokenType ends[] = {
    COMMAND_SUBSTITUTION_END,
    ARITHMETIC_EXPANSION_END,
  };
  for (size_t index = 0; index < sizeof(ends) / sizeof(ends[0]); index += 1) {
    if (valid[ends[index]]) {
      lexer->result_symbol = (TSSymbol)ends[index];
      return source_context_commit(scanner, ends[index]);
    }
  }
  if (scanner->active_count > 0) {
    enum TokenType body = scanner->active_documents[0].quoted
      ? QUOTED_HERE_DOCUMENT_BODY_START
      : HERE_DOCUMENT_BODY_START;
    if (valid[body]) {
      scanner->at_here_document_line_start = true;
      lexer->result_symbol = (TSSymbol)body;
      return source_context_push(scanner, body);
    }
  }
  struct NativeInput native = {.lexer = lexer};
  struct LogicalLexer input;
  if (!logical_init(&input, &native, &scanner->source)) {
    logical_clear(&input);
    native_input_clear(&native);
    return false;
  }
  size_t leading = input.result == SOURCE_END ? input.cursor.raw_count
    : input.character.origin.start > 0 ? (size_t)input.character.origin.start
                                       : 0;
  bool accepted = false;
  bool here_document_boundary =
    (scanner->sequence_end_pending && valid[HERE_DOCUMENT_SEQUENCE_END]) ||
    (scanner->active_count >
      0 &&
      ((valid[HERE_DOCUMENT_END_COMMIT] &&
         here_document_end_is_ready(scanner, &input.lexer) &&
         (scanner->here_document_end_line_consumed || leading == 0)) ||
        (scanner->at_here_document_line_start &&
          (valid[HERE_DOCUMENT_END_BEGIN] ||
            valid[QUOTED_HERE_DOCUMENT_END_BEGIN] ||
            valid[HERE_DOCUMENT_CONTENT_LINE_START]))));
  if (!here_document_boundary && leading > 0 && valid[SOURCE_BEGIN]) {
    scanner->emission = (struct LexicalEmission){
      .symbol = SOURCE_BEGIN,
      .remaining = leading,
      .active = true,
    };
    lexer->result_symbol = SOURCE_BEGIN;
    accepted = true;
  } else if (
    recovering ? classify_recovery_source(scanner, &input, valid)
               : classify_logical_source(scanner, &input, valid)
  ) {
    enum TokenType symbol = (enum TokenType)input.lexer.result_symbol;
    lexer->result_symbol = (TSSymbol)symbol;
    accepted = true;
    if (
      symbol ==
      HERE_DOCUMENT_END_COMMIT ||
      symbol ==
      WORD_BRACKET_FALLBACK_END ||
      symbol ==
      ASSIGNMENT_BRACKET_FALLBACK_END ||
      symbol == PARAMETER_BRACKET_FALLBACK_END
    ) {
      accepted = source_context_pop(scanner);
    }
    if (
      symbol <
      TOKEN_COUNT &&
      LEXICAL_SOURCES[symbol].kind != LEXICAL_SOURCE_NONE
    ) {
      accepted = begin_lexical_emission(scanner, &input, symbol, valid);
    }
  }
  accepted = accepted && !native.failed && !input.cursor.failed;
  logical_clear(&input);
  native_input_clear(&native);
  return accepted;
}

bool tree_sitter_sh_external_scanner_scan(
  void *payload,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  if (payload == NULL) {
    return false;
  }
  bool all_valid = true;
  for (size_t index = 0; index < TOKEN_COUNT; index += 1) {
    if (!valid_symbols[index]) {
      all_valid = false;
      break;
    }
  }
  if (all_valid) {
    static const bool recovery_symbols[TOKEN_COUNT] = {
      [LEFT_BRACE] = true,
      [PIPELINE_NEGATION] = true,
      [IF_KEYWORD] = true,
      [FOR_KEYWORD] = true,
      [IN_KEYWORD] = true,
      [CASE_KEYWORD] = true,
      [WHILE_KEYWORD] = true,
      [UNTIL_KEYWORD] = true,
      [RIGHT_BRACE] = true,
      [THEN_KEYWORD] = true,
      [ELIF_KEYWORD] = true,
      [ELSE_KEYWORD] = true,
      [FI_KEYWORD] = true,
      [DO_KEYWORD] = true,
      [DONE_KEYWORD] = true,
      [ESAC_KEYWORD] = true,
      [BACKQUOTE_END] = true,
      [PUNCT_LEFT_PARENTHESIS] = true,
      [PUNCT_RIGHT_PARENTHESIS] = true,
      [PUNCT_SEMICOLON] = true,
      [PUNCT_AMPERSAND] = true,
      [AND_IF_BEGIN] = true,
      [DSEMI_BEGIN] = true,
      [SEMI_AND_BEGIN] = true,
      [LOGICAL_NEWLINE_BEGIN] = true,
#define LEXICAL_RECOVERY_SYMBOL(begin, piece) [piece] = true,
      SH_LEXICAL_SOURCE_TOKENS(LEXICAL_RECOVERY_SYMBOL)
#undef LEXICAL_RECOVERY_SYMBOL
#define PHYSICAL_RECOVERY_SYMBOL(begin, prefix, character) \
  [prefix] = true, [character] = true,
        SH_PHYSICAL_SOURCE_TOKENS(PHYSICAL_RECOVERY_SYMBOL)
#undef PHYSICAL_RECOVERY_SYMBOL
#define WHOLE_RECOVERY_SYMBOL(begin, whole) [whole] = true,
          SH_WHOLE_SOURCE_TOKENS(WHOLE_RECOVERY_SYMBOL)
#undef WHOLE_RECOVERY_SYMBOL
            [LINE_CONTINUATION] = true,
      [REMOVED_NEWLINE] = true,
      [SOURCE_BEGIN] = true,
      [REMOVED_SOURCE] = true,
      [WORD_INITIAL_LITERAL_BEGIN] = true,
      [WORD_INITIAL_FALLBACK_LITERAL_BEGIN] = true,
      [WORD_INITIAL_PATTERN_BRACKET_OPEN] = true,
      [WORD_INITIAL_PATTERN_STAR_BEGIN] = true,
      [WORD_INITIAL_PATTERN_QUESTION_BEGIN] = true,
      [WORD_INITIAL_ESCAPED_CHARACTER_BEGIN] = true,
      [WORD_INITIAL_SQ_OPEN] = true,
      [WORD_INITIAL_DQ_OPEN] = true,
      [WORD_INITIAL_DOLLAR_SQ_DOLLAR] = true,
      [WORD_INITIAL_DOLLAR_EXPANSION_START] = true,
      [WORD_INITIAL_BACKQUOTE_START] = true,
      [WORD_TILDE_START] = true,
    };
    valid_symbols = recovery_symbols;
  }
  struct Scanner next;
  if (!scanner_copy(payload, &next)) {
    return false;
  }
  if (
    !scan_source(&next, lexer, valid_symbols, all_valid) ||
    lexer->result_symbol >=
    TOKEN_COUNT ||
    !valid_symbols[lexer->result_symbol] ||
    !scanner_state_fits(&next)
  ) {
    clear_scanner(&next);
    return false;
  }
  clear_scanner(payload);
  *(struct Scanner *)payload = next;
  return true;
}
