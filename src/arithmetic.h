#ifndef TREE_SITTER_SH_ARITHMETIC_H_
#define TREE_SITTER_SH_ARITHMETIC_H_

#include "tree_sitter/alloc.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum ArithmeticValidation {
  ARITHMETIC_VALIDATION_INVALID,
  ARITHMETIC_VALIDATION_INCOMPLETE,
  ARITHMETIC_VALIDATION_VALID,
  ARITHMETIC_VALIDATION_RESOURCE_FAILURE,
};

enum DynamicArithmeticToken {
  DYNAMIC_ARITHMETIC_NAME,
  DYNAMIC_ARITHMETIC_NUMBER,
  DYNAMIC_ARITHMETIC_LEFT,
  DYNAMIC_ARITHMETIC_RIGHT,
  DYNAMIC_ARITHMETIC_UNARY_OPERATOR,
  DYNAMIC_ARITHMETIC_BINARY_OPERATOR,
  DYNAMIC_ARITHMETIC_ASSIGNMENT_OPERATOR,
  DYNAMIC_ARITHMETIC_QUESTION,
  DYNAMIC_ARITHMETIC_COLON,
  DYNAMIC_ARITHMETIC_TOKEN_COUNT,
  DYNAMIC_ARITHMETIC_START = DYNAMIC_ARITHMETIC_TOKEN_COUNT,
  DYNAMIC_ARITHMETIC_ASSIGNMENT,
  DYNAMIC_ARITHMETIC_CONDITIONAL,
  DYNAMIC_ARITHMETIC_BINARY,
  DYNAMIC_ARITHMETIC_UNARY,
  DYNAMIC_ARITHMETIC_PRIMARY,
  DYNAMIC_ARITHMETIC_LVALUE,
  DYNAMIC_ARITHMETIC_SYMBOL_COUNT,
};

enum DynamicArithmeticLexState {
  DYNAMIC_LEX_START,
  DYNAMIC_LEX_NAME,
  DYNAMIC_LEX_ZERO,
  DYNAMIC_LEX_OCTAL,
  DYNAMIC_LEX_DECIMAL,
  DYNAMIC_LEX_HEX_PREFIX,
  DYNAMIC_LEX_HEX,
  DYNAMIC_LEX_PLUS,
  DYNAMIC_LEX_MINUS,
  DYNAMIC_LEX_BANG,
  DYNAMIC_LEX_TILDE,
  DYNAMIC_LEX_STAR,
  DYNAMIC_LEX_SLASH,
  DYNAMIC_LEX_PERCENT,
  DYNAMIC_LEX_EQUALS,
  DYNAMIC_LEX_LESS,
  DYNAMIC_LEX_GREATER,
  DYNAMIC_LEX_AMPERSAND,
  DYNAMIC_LEX_PIPE,
  DYNAMIC_LEX_CARET,
  DYNAMIC_LEX_BINARY,
  DYNAMIC_LEX_LEFT_SHIFT,
  DYNAMIC_LEX_RIGHT_SHIFT,
  DYNAMIC_LEX_ASSIGNMENT,
  DYNAMIC_LEX_LEFT,
  DYNAMIC_LEX_RIGHT,
  DYNAMIC_LEX_QUESTION,
  DYNAMIC_LEX_COLON,
};

enum {
  DYNAMIC_LEX_STOP = -1,
  DYNAMIC_LEX_INVALID = -2,
};

struct DynamicArithmeticVertex {
  size_t position;
  size_t edge;
};

struct DynamicArithmeticEdge {
  size_t target;
  size_t next;
  uint16_t tokens;
};

struct DynamicArithmeticLexItem {
  size_t position;
  uint8_t state;
};

struct DynamicArithmeticGraph {
  const int32_t *source;
  size_t length;
  size_t required_end;
  struct DynamicArithmeticVertex *vertices;
  size_t vertex_count;
  size_t vertex_capacity;
  struct DynamicArithmeticEdge *edges;
  size_t edge_count;
  size_t edge_capacity;
  size_t *vertex_ids;
  uint64_t *visited;
  uint16_t *emitted;
  size_t *endpoints;
  size_t endpoint_count;
  size_t endpoint_capacity;
  struct DynamicArithmeticLexItem *pending;
  size_t pending_count;
  size_t pending_capacity;
  bool valid;
};

static void *dynamic_arithmetic_grow(
  void *buffer,
  size_t *capacity,
  size_t length,
  size_t width
) {
  if (length <= *capacity) {
    return buffer;
  }
  size_t maximum = SIZE_MAX / width;
  if (length > maximum) {
    return NULL;
  }
  size_t next = *capacity == 0 ? 16 : *capacity;
  while (next < length) {
    next = next > maximum / 2 ? maximum : next * 2;
  }
  void *grown = ts_realloc(buffer, next * width);
  if (grown == NULL) {
    return NULL;
  }
  *capacity = next;
  return grown;
}

static bool dynamic_arithmetic_blank(int32_t character) {
  return character ==
    ' ' ||
    character ==
    '\t' ||
    character ==
    '\n' ||
    character ==
    '\r' ||
    character ==
    '\v' ||
    character == '\f';
}

static bool dynamic_arithmetic_name_start(int32_t character) {
  return (character >= 'A' && character <= 'Z') ||
    (character >= 'a' && character <= 'z') ||
    character == '_';
}

static bool dynamic_arithmetic_digit(int32_t character) {
  return character >= '0' && character <= '9';
}

static bool dynamic_arithmetic_hex(int32_t character) {
  return dynamic_arithmetic_digit(character) ||
    (character >= 'a' && character <= 'f') ||
    (character >= 'A' && character <= 'F');
}

static int dynamic_arithmetic_lex(uint8_t state, int32_t character) {
  bool name = dynamic_arithmetic_name_start(character);
  bool digit = dynamic_arithmetic_digit(character);
  switch (state) {
  case DYNAMIC_LEX_START:
    if (dynamic_arithmetic_blank(character)) {
      return DYNAMIC_LEX_START;
    }
    if (name) {
      return DYNAMIC_LEX_NAME;
    }
    if (digit) {
      return character == '0' ? DYNAMIC_LEX_ZERO : DYNAMIC_LEX_DECIMAL;
    }
    switch (character) {
    case '+':
      return DYNAMIC_LEX_PLUS;
    case '-':
      return DYNAMIC_LEX_MINUS;
    case '!':
      return DYNAMIC_LEX_BANG;
    case '~':
      return DYNAMIC_LEX_TILDE;
    case '*':
      return DYNAMIC_LEX_STAR;
    case '/':
      return DYNAMIC_LEX_SLASH;
    case '%':
      return DYNAMIC_LEX_PERCENT;
    case '=':
      return DYNAMIC_LEX_EQUALS;
    case '<':
      return DYNAMIC_LEX_LESS;
    case '>':
      return DYNAMIC_LEX_GREATER;
    case '&':
      return DYNAMIC_LEX_AMPERSAND;
    case '|':
      return DYNAMIC_LEX_PIPE;
    case '^':
      return DYNAMIC_LEX_CARET;
    case '(':
      return DYNAMIC_LEX_LEFT;
    case ')':
      return DYNAMIC_LEX_RIGHT;
    case '?':
      return DYNAMIC_LEX_QUESTION;
    case ':':
      return DYNAMIC_LEX_COLON;
    default:
      return DYNAMIC_LEX_INVALID;
    }
  case DYNAMIC_LEX_NAME:
    return name || digit ? DYNAMIC_LEX_NAME : DYNAMIC_LEX_STOP;
  case DYNAMIC_LEX_ZERO:
    if (character == 'x' || character == 'X') {
      return DYNAMIC_LEX_HEX_PREFIX;
    }
    if (digit && character <= '7') {
      return DYNAMIC_LEX_OCTAL;
    }
    return name || digit ? DYNAMIC_LEX_INVALID : DYNAMIC_LEX_STOP;
  case DYNAMIC_LEX_OCTAL:
    if (digit && character <= '7') {
      return DYNAMIC_LEX_OCTAL;
    }
    return name || digit ? DYNAMIC_LEX_INVALID : DYNAMIC_LEX_STOP;
  case DYNAMIC_LEX_DECIMAL:
    if (digit) {
      return DYNAMIC_LEX_DECIMAL;
    }
    return name ? DYNAMIC_LEX_INVALID : DYNAMIC_LEX_STOP;
  case DYNAMIC_LEX_HEX_PREFIX:
    return dynamic_arithmetic_hex(character) ? DYNAMIC_LEX_HEX
                                             : DYNAMIC_LEX_INVALID;
  case DYNAMIC_LEX_HEX:
    if (dynamic_arithmetic_hex(character)) {
      return DYNAMIC_LEX_HEX;
    }
    return name || digit ? DYNAMIC_LEX_INVALID : DYNAMIC_LEX_STOP;
  case DYNAMIC_LEX_PLUS:
  case DYNAMIC_LEX_MINUS:
    if (character == (state == DYNAMIC_LEX_PLUS ? '+' : '-')) {
      return DYNAMIC_LEX_INVALID;
    }
    return character == '=' ? DYNAMIC_LEX_ASSIGNMENT : DYNAMIC_LEX_STOP;
  case DYNAMIC_LEX_BANG:
  case DYNAMIC_LEX_EQUALS:
    return character == '=' ? DYNAMIC_LEX_BINARY : DYNAMIC_LEX_STOP;
  case DYNAMIC_LEX_STAR:
  case DYNAMIC_LEX_SLASH:
  case DYNAMIC_LEX_PERCENT:
  case DYNAMIC_LEX_CARET:
  case DYNAMIC_LEX_LEFT_SHIFT:
  case DYNAMIC_LEX_RIGHT_SHIFT:
    return character == '=' ? DYNAMIC_LEX_ASSIGNMENT : DYNAMIC_LEX_STOP;
  case DYNAMIC_LEX_LESS:
  case DYNAMIC_LEX_GREATER:
    if (character == '=') {
      return DYNAMIC_LEX_BINARY;
    }
    if (character == (state == DYNAMIC_LEX_LESS ? '<' : '>')) {
      return state == DYNAMIC_LEX_LESS ? DYNAMIC_LEX_LEFT_SHIFT
                                       : DYNAMIC_LEX_RIGHT_SHIFT;
    }
    return DYNAMIC_LEX_STOP;
  case DYNAMIC_LEX_AMPERSAND:
  case DYNAMIC_LEX_PIPE:
    if (character == '=') {
      return DYNAMIC_LEX_ASSIGNMENT;
    }
    return character == (state == DYNAMIC_LEX_AMPERSAND ? '&' : '|')
      ? DYNAMIC_LEX_BINARY
      : DYNAMIC_LEX_STOP;
  default:
    return DYNAMIC_LEX_STOP;
  }
}

static uint16_t dynamic_arithmetic_token(uint8_t state) {
  unsigned token;
  switch (state) {
  case DYNAMIC_LEX_NAME:
    token = DYNAMIC_ARITHMETIC_NAME;
    break;
  case DYNAMIC_LEX_ZERO:
  case DYNAMIC_LEX_OCTAL:
  case DYNAMIC_LEX_DECIMAL:
  case DYNAMIC_LEX_HEX:
    token = DYNAMIC_ARITHMETIC_NUMBER;
    break;
  case DYNAMIC_LEX_PLUS:
  case DYNAMIC_LEX_MINUS:
    return (uint16_t)((1u << DYNAMIC_ARITHMETIC_UNARY_OPERATOR) |
      (1u << DYNAMIC_ARITHMETIC_BINARY_OPERATOR));
  case DYNAMIC_LEX_BANG:
  case DYNAMIC_LEX_TILDE:
    token = DYNAMIC_ARITHMETIC_UNARY_OPERATOR;
    break;
  case DYNAMIC_LEX_EQUALS:
  case DYNAMIC_LEX_ASSIGNMENT:
    token = DYNAMIC_ARITHMETIC_ASSIGNMENT_OPERATOR;
    break;
  case DYNAMIC_LEX_STAR:
  case DYNAMIC_LEX_SLASH:
  case DYNAMIC_LEX_PERCENT:
  case DYNAMIC_LEX_LESS:
  case DYNAMIC_LEX_GREATER:
  case DYNAMIC_LEX_AMPERSAND:
  case DYNAMIC_LEX_PIPE:
  case DYNAMIC_LEX_CARET:
  case DYNAMIC_LEX_BINARY:
  case DYNAMIC_LEX_LEFT_SHIFT:
  case DYNAMIC_LEX_RIGHT_SHIFT:
    token = DYNAMIC_ARITHMETIC_BINARY_OPERATOR;
    break;
  case DYNAMIC_LEX_LEFT:
    token = DYNAMIC_ARITHMETIC_LEFT;
    break;
  case DYNAMIC_LEX_RIGHT:
    token = DYNAMIC_ARITHMETIC_RIGHT;
    break;
  case DYNAMIC_LEX_QUESTION:
    token = DYNAMIC_ARITHMETIC_QUESTION;
    break;
  case DYNAMIC_LEX_COLON:
    token = DYNAMIC_ARITHMETIC_COLON;
    break;
  default:
    return 0;
  }
  return (uint16_t)(1u << token);
}

static size_t dynamic_arithmetic_boundary(
  const struct DynamicArithmeticGraph *graph,
  size_t position
) {
  while (
    position <
    graph->length &&
    dynamic_arithmetic_blank(graph->source[position])
  ) {
    position += 1;
  }
  return position;
}

static size_t dynamic_arithmetic_after_hole(
  const struct DynamicArithmeticGraph *graph,
  size_t position
) {
  do {
    position += 1;
  } while (position < graph->length && graph->source[position] == -1);
  return position;
}

static bool dynamic_arithmetic_vertex(
  struct DynamicArithmeticGraph *graph,
  size_t position,
  size_t *id
) {
  position = dynamic_arithmetic_boundary(graph, position);
  if (graph->vertex_ids[position] == SIZE_MAX) {
    if (graph->vertex_count == SIZE_MAX) {
      return false;
    }
    struct DynamicArithmeticVertex *vertices = dynamic_arithmetic_grow(
      graph->vertices,
      &graph->vertex_capacity,
      graph->vertex_count + 1,
      sizeof(*graph->vertices)
    );
    if (vertices == NULL) {
      return false;
    }
    graph->vertices = vertices;
    graph->vertex_ids[position] = graph->vertex_count;
    graph->vertices[graph->vertex_count++] =
      (struct DynamicArithmeticVertex){position, SIZE_MAX};
  }
  *id = graph->vertex_ids[position];
  return true;
}

static bool dynamic_arithmetic_edge(
  struct DynamicArithmeticGraph *graph,
  size_t origin,
  size_t position,
  uint16_t tokens
) {
  size_t target;
  if (
    !dynamic_arithmetic_vertex(graph, position, &target) ||
    graph->edge_count == SIZE_MAX
  ) {
    return false;
  }
  struct DynamicArithmeticEdge *edges = dynamic_arithmetic_grow(
    graph->edges,
    &graph->edge_capacity,
    graph->edge_count + 1,
    sizeof(*graph->edges)
  );
  if (edges == NULL) {
    return false;
  }
  graph->edges = edges;
  graph->edges[graph->edge_count] = (struct DynamicArithmeticEdge){
    target,
    graph->vertices[origin].edge,
    tokens,
  };
  graph->vertices[origin].edge = graph->edge_count++;
  return true;
}

static bool dynamic_arithmetic_enqueue(
  struct DynamicArithmeticGraph *graph,
  size_t position,
  uint8_t state
) {
  if (state == DYNAMIC_LEX_START) {
    position = dynamic_arithmetic_boundary(graph, position);
  }
  uint64_t bit = UINT64_C(1) << state;
  if ((graph->visited[position] & bit) != 0) {
    return true;
  }
  if (graph->pending_count == SIZE_MAX) {
    return false;
  }
  struct DynamicArithmeticLexItem *pending = dynamic_arithmetic_grow(
    graph->pending,
    &graph->pending_capacity,
    graph->pending_count + 1,
    sizeof(*graph->pending)
  );
  if (pending == NULL) {
    return false;
  }
  graph->pending = pending;
  graph->visited[position] |= bit;
  graph->pending[graph->pending_count++] =
    (struct DynamicArithmeticLexItem){position, state};
  return true;
}

static bool dynamic_arithmetic_emit(
  struct DynamicArithmeticGraph *graph,
  size_t origin,
  size_t position,
  uint8_t state
) {
  uint16_t tokens = dynamic_arithmetic_token(state);
  if (tokens == 0) {
    return true;
  }
  position = dynamic_arithmetic_boundary(graph, position);
  if (
    origin ==
    0 &&
    position >=
    graph->required_end &&
    (tokens &
      ((1u << DYNAMIC_ARITHMETIC_NAME) | (1u << DYNAMIC_ARITHMETIC_NUMBER))) !=
    0
  ) {
    graph->valid = true;
    return true;
  }
  if (graph->emitted[position] == 0) {
    if (graph->endpoint_count == SIZE_MAX) {
      return false;
    }
    size_t *endpoints = dynamic_arithmetic_grow(
      graph->endpoints,
      &graph->endpoint_capacity,
      graph->endpoint_count + 1,
      sizeof(*graph->endpoints)
    );
    if (endpoints == NULL) {
      return false;
    }
    graph->endpoints = endpoints;
    graph->endpoints[graph->endpoint_count++] = position;
  }
  graph->emitted[position] |= tokens;
  return true;
}

static bool
dynamic_arithmetic_build_graph(struct DynamicArithmeticGraph *graph) {
  size_t initial;
  if (!dynamic_arithmetic_vertex(graph, 0, &initial)) {
    return false;
  }
  for (size_t origin = 0; origin < graph->vertex_count; origin += 1) {
    size_t position = graph->vertices[origin].position;
    if (
      position <
      graph->length &&
      graph->source[position] ==
      -1 &&
      !dynamic_arithmetic_edge(
        graph,
        origin,
        dynamic_arithmetic_after_hole(graph, position),
        0
      )
    ) {
      return false;
    }
    memset(graph->visited, 0, (graph->length + 1) * sizeof(*graph->visited));
    graph->pending_count = 0;
    graph->endpoint_count = 0;
    if (!dynamic_arithmetic_enqueue(graph, position, DYNAMIC_LEX_START)) {
      return false;
    }
    for (size_t index = 0; index < graph->pending_count; index += 1) {
      struct DynamicArithmeticLexItem item = graph->pending[index];
      if (item.position == graph->length) {
        if (!dynamic_arithmetic_emit(
              graph,
              origin,
              item.position,
              item.state
            )) {
          return false;
        }
      } else if (graph->source[item.position] == -1) {
        if (!dynamic_arithmetic_emit(
              graph,
              origin,
              item.position,
              item.state
            )) {
          return false;
        }
        if (
          item.state !=
          DYNAMIC_LEX_START &&
          !dynamic_arithmetic_enqueue(
            graph,
            dynamic_arithmetic_after_hole(graph, item.position),
            item.state
          )
        ) {
          return false;
        }
        for (int32_t character = 1; character < 128; character += 1) {
          int next = dynamic_arithmetic_lex(item.state, character);
          if (
            next >=
            0 &&
            !dynamic_arithmetic_enqueue(graph, item.position, (uint8_t)next)
          ) {
            return false;
          }
        }
      } else {
        int next =
          dynamic_arithmetic_lex(item.state, graph->source[item.position]);
        if (next == DYNAMIC_LEX_STOP) {
          if (!dynamic_arithmetic_emit(
                graph,
                origin,
                item.position,
                item.state
              )) {
            return false;
          }
        } else if (
          next >=
          0 &&
          !dynamic_arithmetic_enqueue(graph, item.position + 1, (uint8_t)next)
        ) {
          return false;
        }
      }
      if (graph->valid) {
        return true;
      }
    }
    for (size_t index = 0; index < graph->endpoint_count; index += 1) {
      size_t endpoint = graph->endpoints[index];
      uint16_t tokens = graph->emitted[endpoint];
      graph->emitted[endpoint] = 0;
      if (!dynamic_arithmetic_edge(graph, origin, endpoint, tokens)) {
        return false;
      }
    }
  }
  return true;
}

struct DynamicArithmeticRule {
  uint8_t left;
  uint8_t length;
  uint8_t right[5];
};

static const struct DynamicArithmeticRule dynamic_arithmetic_rules[] = {
  {DYNAMIC_ARITHMETIC_START, 1, {DYNAMIC_ARITHMETIC_ASSIGNMENT}},
  {DYNAMIC_ARITHMETIC_ASSIGNMENT, 1, {DYNAMIC_ARITHMETIC_CONDITIONAL}},
  {DYNAMIC_ARITHMETIC_ASSIGNMENT,
    3,
    {DYNAMIC_ARITHMETIC_LVALUE,
      DYNAMIC_ARITHMETIC_ASSIGNMENT_OPERATOR,
      DYNAMIC_ARITHMETIC_ASSIGNMENT}},
  {DYNAMIC_ARITHMETIC_CONDITIONAL, 1, {DYNAMIC_ARITHMETIC_BINARY}},
  {DYNAMIC_ARITHMETIC_CONDITIONAL,
    5,
    {DYNAMIC_ARITHMETIC_BINARY,
      DYNAMIC_ARITHMETIC_QUESTION,
      DYNAMIC_ARITHMETIC_ASSIGNMENT,
      DYNAMIC_ARITHMETIC_COLON,
      DYNAMIC_ARITHMETIC_CONDITIONAL}},
  {DYNAMIC_ARITHMETIC_BINARY, 1, {DYNAMIC_ARITHMETIC_UNARY}},
  {DYNAMIC_ARITHMETIC_BINARY,
    3,
    {DYNAMIC_ARITHMETIC_UNARY,
      DYNAMIC_ARITHMETIC_BINARY_OPERATOR,
      DYNAMIC_ARITHMETIC_BINARY}},
  {DYNAMIC_ARITHMETIC_UNARY, 1, {DYNAMIC_ARITHMETIC_PRIMARY}},
  {DYNAMIC_ARITHMETIC_UNARY,
    2,
    {DYNAMIC_ARITHMETIC_UNARY_OPERATOR, DYNAMIC_ARITHMETIC_UNARY}},
  {DYNAMIC_ARITHMETIC_PRIMARY, 1, {DYNAMIC_ARITHMETIC_NUMBER}},
  {DYNAMIC_ARITHMETIC_PRIMARY, 1, {DYNAMIC_ARITHMETIC_NAME}},
  {DYNAMIC_ARITHMETIC_PRIMARY,
    3,
    {DYNAMIC_ARITHMETIC_LEFT,
      DYNAMIC_ARITHMETIC_ASSIGNMENT,
      DYNAMIC_ARITHMETIC_RIGHT}},
  {DYNAMIC_ARITHMETIC_LVALUE, 1, {DYNAMIC_ARITHMETIC_NAME}},
  {DYNAMIC_ARITHMETIC_LVALUE,
    3,
    {DYNAMIC_ARITHMETIC_LEFT,
      DYNAMIC_ARITHMETIC_LVALUE,
      DYNAMIC_ARITHMETIC_RIGHT}},
};

static uint8_t dynamic_arithmetic_symbol(
  const struct DynamicArithmeticRule *rule,
  size_t dot
) {
  return rule->right[rule->length - dot - 1];
}

struct DynamicArithmeticProjection {
  uint8_t *visited;
  size_t *pending;
  size_t count;
  size_t capacity;
};

static bool dynamic_arithmetic_project_enqueue(
  struct DynamicArithmeticProjection *projection,
  size_t state
) {
  if (projection->visited[state] != 0) {
    return true;
  }
  if (projection->count == SIZE_MAX) {
    return false;
  }
  size_t *pending = dynamic_arithmetic_grow(
    projection->pending,
    &projection->capacity,
    projection->count + 1,
    sizeof(*projection->pending)
  );
  if (pending == NULL) {
    return false;
  }
  projection->pending = pending;
  projection->visited[state] = 1;
  projection->pending[projection->count++] = state;
  return true;
}

static enum ArithmeticValidation
dynamic_arithmetic_project_graph(const struct DynamicArithmeticGraph *graph) {
  enum {
    rule_count = sizeof(dynamic_arithmetic_rules) /
      sizeof(dynamic_arithmetic_rules[0]),
    maximum_states = rule_count *
      (sizeof(dynamic_arithmetic_rules[0].right) + 1),
  };
  size_t starts[rule_count];
  uint8_t rules[maximum_states];
  uint8_t dots[maximum_states];
  size_t state_count = 0;
  for (size_t rule = 0; rule < rule_count; rule += 1) {
    starts[rule] = state_count;
    for (
      uint8_t dot = 0; dot <= dynamic_arithmetic_rules[rule].length; dot += 1
    ) {
      rules[state_count] = (uint8_t)rule;
      dots[state_count++] = dot;
    }
  }
  struct DynamicArithmeticProjection projection = {0};
  enum ArithmeticValidation result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
  if (graph->vertex_count > SIZE_MAX / state_count) {
    return result;
  }
  projection.visited = ts_calloc(graph->vertex_count * state_count, 1);
  if (
    projection.visited ==
    NULL ||
    !dynamic_arithmetic_project_enqueue(&projection, 0)
  ) {
    goto finish;
  }
  // Ignoring the return stack gives a regular superset of the full grammar.
  for (size_t index = 0; index < projection.count; index += 1) {
    size_t vertex = projection.pending[index] / state_count;
    size_t state = projection.pending[index] % state_count;
    const struct DynamicArithmeticRule *rule =
      &dynamic_arithmetic_rules[rules[state]];
    uint8_t dot = dots[state];
    if (
      state ==
      dynamic_arithmetic_rules[0].length &&
      graph->vertices[vertex].position >= graph->required_end
    ) {
      result = ARITHMETIC_VALIDATION_VALID;
      goto finish;
    }
    bool complete = dot == rule->length;
    uint8_t symbol = complete ? DYNAMIC_ARITHMETIC_SYMBOL_COUNT
                              : dynamic_arithmetic_symbol(rule, dot);
    for (
      size_t edge = graph->vertices[vertex].edge; edge != SIZE_MAX;
      edge = graph->edges[edge].next
    ) {
      struct DynamicArithmeticEdge transition = graph->edges[edge];
      if (transition.tokens == 0) {
        if (!dynamic_arithmetic_project_enqueue(
              &projection,
              transition.target * state_count + state
            )) {
          goto finish;
        }
      } else if (
        symbol <
        DYNAMIC_ARITHMETIC_TOKEN_COUNT &&
        (transition.tokens & (1u << symbol)) !=
        0 &&
        !dynamic_arithmetic_project_enqueue(
          &projection,
          transition.target * state_count + state + 1
        )
      ) {
        goto finish;
      }
    }
    for (size_t candidate = 0; candidate < rule_count; candidate += 1) {
      const struct DynamicArithmeticRule *target =
        &dynamic_arithmetic_rules[candidate];
      if (complete) {
        for (size_t position = 0; position < target->length; position += 1) {
          if (
            dynamic_arithmetic_symbol(target, position) ==
            rule->left &&
            !dynamic_arithmetic_project_enqueue(
              &projection,
              vertex * state_count + starts[candidate] + position + 1
            )
          ) {
            goto finish;
          }
        }
      } else if (
        symbol ==
        target->left &&
        !dynamic_arithmetic_project_enqueue(
          &projection,
          vertex * state_count + starts[candidate]
        )
      ) {
        goto finish;
      }
    }
  }
  result = ARITHMETIC_VALIDATION_INVALID;
finish:
  ts_free(projection.visited);
  ts_free(projection.pending);
  return result;
}

struct DynamicArithmeticItem {
  size_t vertex;
  size_t origin;
  size_t next;
  uint8_t rule;
  uint8_t dot;
};

struct DynamicArithmeticCompletion {
  size_t vertex;
  size_t origin;
  size_t next;
  uint8_t symbol;
};

struct DynamicArithmeticChart {
  const struct DynamicArithmeticGraph *graph;
  struct DynamicArithmeticItem *items;
  size_t item_count;
  size_t item_capacity;
  size_t *heads;
  size_t *hash;
  size_t hash_capacity;
  struct DynamicArithmeticCompletion *completions;
  size_t completion_count;
  size_t completion_capacity;
  size_t *completion_hash;
  size_t completion_hash_capacity;
  size_t *completed;
  size_t *pending;
  size_t pending_count;
  size_t pending_capacity;
};

static bool dynamic_arithmetic_precedes(
  const struct DynamicArithmeticChart *chart,
  size_t first,
  size_t second
) {
  size_t left = chart->graph->vertices[chart->items[first].vertex].position;
  size_t right = chart->graph->vertices[chart->items[second].vertex].position;
  return left > right || (left == right && first < second);
}

static size_t dynamic_arithmetic_next(struct DynamicArithmeticChart *chart) {
  size_t result = chart->pending[0];
  size_t replacement = chart->pending[--chart->pending_count];
  size_t position = 0;
  while (position < chart->pending_count / 2) {
    size_t child = position * 2 + 1;
    if (
      child +
      1 <
      chart->pending_count &&
      dynamic_arithmetic_precedes(
        chart,
        chart->pending[child + 1],
        chart->pending[child]
      )
    ) {
      child += 1;
    }
    if (
      dynamic_arithmetic_precedes(chart, replacement, chart->pending[child])
    ) {
      break;
    }
    chart->pending[position] = chart->pending[child];
    position = child;
  }
  if (chart->pending_count > 0) {
    chart->pending[position] = replacement;
  }
  return result;
}

static size_t dynamic_arithmetic_hash_key(
  size_t vertex,
  size_t origin,
  uint8_t rule,
  uint8_t dot
) {
  uint64_t hash = (uint64_t)vertex * UINT64_C(0x9e3779b97f4a7c15);
  hash ^= (uint64_t)origin * UINT64_C(0xbf58476d1ce4e5b9);
  hash ^= (((uint64_t)rule << 8) | dot) * UINT64_C(0x94d049bb133111eb);
  hash = (hash ^ (hash >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  hash = (hash ^ (hash >> 27)) * UINT64_C(0x94d049bb133111eb);
  return (size_t)(hash ^ (hash >> 31));
}

static bool dynamic_arithmetic_add_item(
  struct DynamicArithmeticChart *chart,
  size_t vertex,
  size_t origin,
  uint8_t rule,
  uint8_t dot
) {
  if (chart->item_count >= chart->hash_capacity / 2) {
    size_t capacity =
      chart->hash_capacity == 0 ? 256 : chart->hash_capacity * 2;
    if (
      capacity < chart->hash_capacity || capacity > SIZE_MAX / sizeof(size_t)
    ) {
      return false;
    }
    size_t *hash = ts_calloc(capacity, sizeof(*hash));
    if (hash == NULL) {
      return false;
    }
    for (size_t index = 0; index < chart->item_count; index += 1) {
      struct DynamicArithmeticItem item = chart->items[index];
      size_t slot = dynamic_arithmetic_hash_key(
                      item.vertex,
                      item.origin,
                      item.rule,
                      item.dot
                    ) &
        (capacity - 1);
      while (hash[slot] != 0) {
        slot = (slot + 1) & (capacity - 1);
      }
      hash[slot] = index + 1;
    }
    ts_free(chart->hash);
    chart->hash = hash;
    chart->hash_capacity = capacity;
  }
  const struct DynamicArithmeticRule *production =
    &dynamic_arithmetic_rules[rule];
  uint8_t symbol = dot < production->length
    ? dynamic_arithmetic_symbol(production, dot)
    : DYNAMIC_ARITHMETIC_SYMBOL_COUNT;
  size_t head = vertex * DYNAMIC_ARITHMETIC_SYMBOL_COUNT + symbol;
  bool waiting = symbol >=
    DYNAMIC_ARITHMETIC_TOKEN_COUNT &&
    symbol < DYNAMIC_ARITHMETIC_SYMBOL_COUNT;
  struct DynamicArithmeticItem item =
    {vertex, origin, waiting ? chart->heads[head] : SIZE_MAX, rule, dot};
  size_t slot = dynamic_arithmetic_hash_key(vertex, origin, rule, dot) &
    (chart->hash_capacity - 1);
  while (chart->hash[slot] != 0) {
    struct DynamicArithmeticItem existing = chart->items[chart->hash[slot] - 1];
    if (
      existing.vertex ==
      vertex &&
      existing.origin ==
      origin &&
      existing.rule ==
      rule &&
      existing.dot == dot
    ) {
      return true;
    }
    slot = (slot + 1) & (chart->hash_capacity - 1);
  }
  if (chart->item_count == SIZE_MAX) {
    return false;
  }
  struct DynamicArithmeticItem *items = dynamic_arithmetic_grow(
    chart->items,
    &chart->item_capacity,
    chart->item_count + 1,
    sizeof(*chart->items)
  );
  if (items == NULL) {
    return false;
  }
  chart->items = items;
  size_t *pending = dynamic_arithmetic_grow(
    chart->pending,
    &chart->pending_capacity,
    chart->pending_count + 1,
    sizeof(*chart->pending)
  );
  if (pending == NULL) {
    return false;
  }
  chart->pending = pending;
  chart->items[chart->item_count] = item;
  if (waiting) {
    chart->heads[head] = chart->item_count;
  }
  chart->hash[slot] = ++chart->item_count;
  size_t position = chart->pending_count++;
  while (position > 0) {
    size_t parent = (position - 1) / 2;
    if (
      dynamic_arithmetic_precedes(
        chart,
        chart->pending[parent],
        chart->item_count - 1
      )
    ) {
      break;
    }
    chart->pending[position] = chart->pending[parent];
    position = parent;
  }
  chart->pending[position] = chart->item_count - 1;
  return true;
}

static bool dynamic_arithmetic_complete(
  struct DynamicArithmeticChart *chart,
  struct DynamicArithmeticItem item
) {
  uint8_t symbol = dynamic_arithmetic_rules[item.rule].left;
  size_t head = item.origin * DYNAMIC_ARITHMETIC_SYMBOL_COUNT + symbol;
  if (chart->completion_count >= chart->completion_hash_capacity / 2) {
    size_t capacity = chart->completion_hash_capacity == 0
      ? 256
      : chart->completion_hash_capacity * 2;
    if (
      capacity <
      chart->completion_hash_capacity ||
      capacity >
      SIZE_MAX /
      sizeof(size_t)
    ) {
      return false;
    }
    size_t *hash = ts_calloc(capacity, sizeof(*hash));
    if (hash == NULL) {
      return false;
    }
    for (size_t index = 0; index < chart->completion_count; index += 1) {
      struct DynamicArithmeticCompletion completion = chart->completions[index];
      size_t slot = dynamic_arithmetic_hash_key(
                      completion.vertex,
                      completion.origin,
                      completion.symbol,
                      0
                    ) &
        (capacity - 1);
      while (hash[slot] != 0) {
        slot = (slot + 1) & (capacity - 1);
      }
      hash[slot] = index + 1;
    }
    ts_free(chart->completion_hash);
    chart->completion_hash = hash;
    chart->completion_hash_capacity = capacity;
  }
  size_t slot =
    dynamic_arithmetic_hash_key(item.vertex, item.origin, symbol, 0) &
    (chart->completion_hash_capacity - 1);
  while (chart->completion_hash[slot] != 0) {
    struct DynamicArithmeticCompletion completion =
      chart->completions[chart->completion_hash[slot] - 1];
    if (
      completion.vertex ==
      item.vertex &&
      completion.origin ==
      item.origin &&
      completion.symbol == symbol
    ) {
      return true;
    }
    slot = (slot + 1) & (chart->completion_hash_capacity - 1);
  }
  if (chart->completion_count == SIZE_MAX) {
    return false;
  }
  struct DynamicArithmeticCompletion *completions = dynamic_arithmetic_grow(
    chart->completions,
    &chart->completion_capacity,
    chart->completion_count + 1,
    sizeof(*chart->completions)
  );
  if (completions == NULL) {
    return false;
  }
  chart->completions = completions;
  chart->completions[chart->completion_count] =
    (struct DynamicArithmeticCompletion){
      item.vertex,
      item.origin,
      chart->completed[head],
      symbol
  };
  chart->completed[head] = chart->completion_count;
  chart->completion_hash[slot] = ++chart->completion_count;
  for (size_t index = chart->heads[head]; index != SIZE_MAX;) {
    struct DynamicArithmeticItem parent = chart->items[index];
    index = parent.next;
    if (!dynamic_arithmetic_add_item(
          chart,
          item.vertex,
          parent.origin,
          parent.rule,
          (uint8_t)(parent.dot + 1)
        )) {
      return false;
    }
  }
  return true;
}

static enum ArithmeticValidation
dynamic_arithmetic_parse_graph(const struct DynamicArithmeticGraph *graph) {
  struct DynamicArithmeticChart chart = {.graph = graph};
  enum ArithmeticValidation result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
  if (
    graph->vertex_count >
    SIZE_MAX /
    sizeof(size_t) /
    DYNAMIC_ARITHMETIC_SYMBOL_COUNT
  ) {
    return result;
  }
  chart.heads = ts_malloc(
    graph->vertex_count * DYNAMIC_ARITHMETIC_SYMBOL_COUNT * sizeof(*chart.heads)
  );
  chart.completed = ts_malloc(
    graph->vertex_count *
    DYNAMIC_ARITHMETIC_SYMBOL_COUNT *
    sizeof(*chart.completed)
  );
  if (chart.heads == NULL || chart.completed == NULL) {
    goto finish;
  }
  for (
    size_t index = 0;
    index < graph->vertex_count * DYNAMIC_ARITHMETIC_SYMBOL_COUNT;
    index += 1
  ) {
    chart.heads[index] = SIZE_MAX;
    chart.completed[index] = SIZE_MAX;
  }
  if (!dynamic_arithmetic_add_item(&chart, 0, 0, 0, 0)) {
    goto finish;
  }
  while (chart.pending_count > 0) {
    struct DynamicArithmeticItem item =
      chart.items[dynamic_arithmetic_next(&chart)];
    const struct DynamicArithmeticRule *rule =
      &dynamic_arithmetic_rules[item.rule];
    bool complete = item.dot == rule->length;
    if (
      complete &&
      rule->left ==
      DYNAMIC_ARITHMETIC_START &&
      item.origin ==
      0 &&
      graph->vertices[item.vertex].position >= graph->required_end
    ) {
      result = ARITHMETIC_VALIDATION_VALID;
      goto finish;
    }
    uint8_t symbol = complete ? DYNAMIC_ARITHMETIC_SYMBOL_COUNT
                              : dynamic_arithmetic_symbol(rule, item.dot);
    for (
      size_t edge = graph->vertices[item.vertex].edge; edge != SIZE_MAX;
      edge = graph->edges[edge].next
    ) {
      struct DynamicArithmeticEdge transition = graph->edges[edge];
      if (transition.tokens == 0) {
        if (!dynamic_arithmetic_add_item(
              &chart,
              transition.target,
              item.origin,
              item.rule,
              item.dot
            )) {
          goto finish;
        }
      } else if (
        symbol <
        DYNAMIC_ARITHMETIC_TOKEN_COUNT &&
        (transition.tokens & (1u << symbol)) !=
        0 &&
        !dynamic_arithmetic_add_item(
          &chart,
          transition.target,
          item.origin,
          item.rule,
          (uint8_t)(item.dot + 1)
        )
      ) {
        goto finish;
      }
    }
    if (complete) {
      if (!dynamic_arithmetic_complete(&chart, item)) {
        goto finish;
      }
    } else if (symbol >= DYNAMIC_ARITHMETIC_TOKEN_COUNT) {
      size_t count =
        sizeof(dynamic_arithmetic_rules) / sizeof(dynamic_arithmetic_rules[0]);
      for (size_t candidate = 0; candidate < count; candidate += 1) {
        if (
          dynamic_arithmetic_rules[candidate].left ==
          symbol &&
          !dynamic_arithmetic_add_item(
            &chart,
            item.vertex,
            item.vertex,
            (uint8_t)candidate,
            0
          )
        ) {
          goto finish;
        }
      }
      size_t head = item.vertex * DYNAMIC_ARITHMETIC_SYMBOL_COUNT + symbol;
      for (size_t completion = chart.completed[head]; completion != SIZE_MAX;) {
        struct DynamicArithmeticCompletion matched =
          chart.completions[completion];
        completion = matched.next;
        if (!dynamic_arithmetic_add_item(
              &chart,
              matched.vertex,
              item.origin,
              item.rule,
              (uint8_t)(item.dot + 1)
            )) {
          goto finish;
        }
      }
    }
  }
  result = ARITHMETIC_VALIDATION_INVALID;
finish:
  ts_free(chart.items);
  ts_free(chart.heads);
  ts_free(chart.hash);
  ts_free(chart.completions);
  ts_free(chart.completion_hash);
  ts_free(chart.completed);
  ts_free(chart.pending);
  return result;
}

static size_t
dynamic_arithmetic_reverse_vertex(size_t vertex, size_t endpoint) {
  return vertex == 0 ? endpoint : vertex == endpoint ? 0 : vertex;
}

static enum ArithmeticValidation
dynamic_arithmetic_reverse_graph(struct DynamicArithmeticGraph *graph) {
  size_t endpoint = graph->vertex_ids[graph->length];
  if (endpoint == SIZE_MAX) {
    return ARITHMETIC_VALIDATION_INVALID;
  }
  struct DynamicArithmeticVertex *vertices =
    ts_malloc(graph->vertex_count * sizeof(*vertices));
  if (vertices == NULL) {
    return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
  }
  for (size_t index = 0; index < graph->vertex_count; index += 1) {
    size_t reversed = dynamic_arithmetic_reverse_vertex(index, endpoint);
    vertices[reversed] = (struct DynamicArithmeticVertex){
      graph->length - graph->vertices[index].position,
      SIZE_MAX
    };
  }
  for (size_t origin = 0; origin < graph->vertex_count; origin += 1) {
    size_t target = dynamic_arithmetic_reverse_vertex(origin, endpoint);
    for (size_t edge = graph->vertices[origin].edge; edge != SIZE_MAX;) {
      struct DynamicArithmeticEdge transition = graph->edges[edge];
      size_t reversed =
        dynamic_arithmetic_reverse_vertex(transition.target, endpoint);
      graph->edges[edge] = (struct DynamicArithmeticEdge){
        target,
        vertices[reversed].edge,
        transition.tokens
      };
      vertices[reversed].edge = edge;
      edge = transition.next;
    }
  }
  graph->required_end = vertices[endpoint].position;
  ts_free(graph->vertices);
  graph->vertices = vertices;
  graph->vertex_capacity = graph->vertex_count;
  return ARITHMETIC_VALIDATION_VALID;
}

static inline enum ArithmeticValidation
validate_dynamic_arithmetic(const int32_t *source, size_t length) {
  struct DynamicArithmeticGraph graph = {.source = source, .length = length};
  enum ArithmeticValidation result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
  if (
    length ==
    SIZE_MAX ||
    length +
    1 >
    SIZE_MAX /
    sizeof(uint64_t) ||
    length +
    1 >
    SIZE_MAX /
    sizeof(size_t)
  ) {
    return result;
  }
  graph.vertex_ids = ts_malloc((length + 1) * sizeof(*graph.vertex_ids));
  graph.visited = ts_calloc(length + 1, sizeof(*graph.visited));
  graph.emitted = ts_calloc(length + 1, sizeof(*graph.emitted));
  if (
    graph.vertex_ids == NULL || graph.visited == NULL || graph.emitted == NULL
  ) {
    goto finish;
  }
  for (size_t index = 0; index <= length; index += 1) {
    graph.vertex_ids[index] = SIZE_MAX;
    if (
      index <
      length &&
      source[index] !=
      -1 &&
      !dynamic_arithmetic_blank(source[index])
    ) {
      graph.required_end = index + 1;
    }
  }
  if (!dynamic_arithmetic_build_graph(&graph)) {
    goto finish;
  }
  if (graph.valid) {
    result = ARITHMETIC_VALIDATION_VALID;
    goto finish;
  }
  // Reading backward exposes assignment constraints before ambiguous prefixes.
  result = dynamic_arithmetic_reverse_graph(&graph);
  if (result != ARITHMETIC_VALIDATION_VALID) {
    goto finish;
  }
  result = dynamic_arithmetic_project_graph(&graph);
  if (result == ARITHMETIC_VALIDATION_VALID) {
    result = dynamic_arithmetic_parse_graph(&graph);
  }
finish:
  ts_free(graph.vertices);
  ts_free(graph.edges);
  ts_free(graph.vertex_ids);
  ts_free(graph.visited);
  ts_free(graph.emitted);
  ts_free(graph.endpoints);
  ts_free(graph.pending);
  return result;
}

#endif
