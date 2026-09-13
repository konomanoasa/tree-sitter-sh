// Keep these regexes as strings: Biome removes the bracket escape that
// Tree-sitter requires inside character classes.
const LITERAL_TOKEN_PATTERN_SOURCE =
  "[^A-Za-z_ \\t\\n;&|<>()/\\\\'\"$`*?\\[\\]~:#=]+";
const PARAMETER_PATTERN_TEXT_PATTERN_SOURCE = "[^}\\n/'\"$`\\\\*?\\[\\]~]+";
const LITERAL_TOKEN_PATTERN = RegExp(LITERAL_TOKEN_PATTERN_SOURCE);
const PARAMETER_PATTERN_TEXT_PATTERN = RegExp(
  PARAMETER_PATTERN_TEXT_PATTERN_SOURCE,
);

const PATTERN_SPECIAL_PLAIN_CHARACTER_PATTERN = /[^ \t\n;&|<>()\\'"$`:.=\]-]/;
const PARAMETER_PATTERN_SPECIAL_PLAIN_CHARACTER_PATTERN = /[^\\'"$`:.=\]}-]/;
const ASSIGNMENT_WORD_PRECEDENCE = 3;

const ARITHMETIC_BINARY_LEVELS = [
  ["logical_or", ["||"]],
  ["logical_and", ["&&"]],
  ["bitwise_or", ["|"]],
  ["bitwise_xor", ["^"]],
  ["bitwise_and", ["&"]],
  ["equality", ["==", "!="]],
  ["relational", ["<=", ">=", "<", ">"]],
  ["shift", ["<<", ">>"]],
  ["additive", ["+", "-"]],
  ["multiplicative", ["*", "/", "%"]],
];

const arithmeticBinaryLevelSymbols = ($, suffix) =>
  ARITHMETIC_BINARY_LEVELS.map(
    ([level]) => $[`_arithmetic_${level}_${suffix}`],
  );

const PATTERN_PRECEDENCE = {
  literalFallback: -2,
  expression: 2,
  range: 3,
  specialElement: 4,
  negation: 5,
};

const structuredSourcePartsWithBackquoteRuns = ($, backquoteRuns) => [
  $.escaped_character,
  ...backquoteRuns,
  $.single_quoted,
  $.double_quoted,
  $.dollar_single_quoted,
  $.parameter_expansion,
  $.command_substitution,
  $.arithmetic_expansion,
  $.backquote_substitution,
];

const structuredSourceParts = ($) =>
  structuredSourcePartsWithBackquoteRuns($, [
    $._backquote_content_escape_run,
    $._backquote_escaped_pair_run,
  ]);

const patternBracketStructuredSourceParts = ($) =>
  structuredSourcePartsWithBackquoteRuns($, [
    alias($._backquote_pattern_escape, $.escaped_character),
  ]);

const backquoteContentEscapeRun = (
  $,
  escapeName,
  literalName,
  tail = alias($._backquote_escaped_tail, escapeName),
) =>
  seq(
    $._backquote_content_run_begin,
    repeat(alias($._backquote_escaped_pair, escapeName)),
    choice(
      $._line_continuation,
      tail,
      alias($._literal_dollar, literalName),
      alias($._backquote_escaped_ordinary, literalName),
    ),
  );

const backquoteEscapedPairRun = ($, pair) =>
  seq(
    $._backquote_pair_run_begin,
    repeat1(alias($._backquote_escaped_pair, pair)),
    optional("\\"),
    $._backquote_pair_run_end,
  );

const wordPatternSpecialSources = ($) => [
  $.pattern_character_class_source,
  $.pattern_collating_symbol_source,
  $.pattern_equivalence_class_source,
];

const parameterPatternCollatingSymbolSource = ($) =>
  alias(
    $._parameter_pattern_collating_symbol_source,
    $.pattern_collating_symbol_source,
  );

const parameterPatternSpecialSources = ($) => [
  alias(
    $._parameter_pattern_character_class_source,
    $.pattern_character_class_source,
  ),
  parameterPatternCollatingSymbolSource($),
  alias(
    $._parameter_pattern_equivalence_class_source,
    $.pattern_equivalence_class_source,
  ),
];

const patternSpecialContentCharacters = ($, plain) => [
  plain,
  $._literal_dollar,
  $._pattern_special_marker_character,
  $._pattern_bracket_hyphen_token,
  $._pattern_special_left_bracket,
];

const patternCharacterClassContent = ($, plain) =>
  prec.right(1, repeat1(choice(...patternSpecialContentCharacters($, plain))));

const wordPatternBracketSources = ($) =>
  choice(
    $.pattern_bracket_source,
    alias($._word_special_prefixed_bracket_source, $.pattern_bracket_source),
  );

const parameterPatternBracketSources = ($) =>
  choice(
    alias($._parameter_pattern_bracket_expression, $.pattern_bracket_source),
    alias(
      $._parameter_special_prefixed_bracket_source,
      $.pattern_bracket_source,
    ),
  );

const incompleteBracketLiteralRun = ($, atom) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.literalFallback,
    prec.right(
      1,
      seq(
        $._pattern_bracket_left,
        repeat(choice($._pattern_bracket_left, atom)),
      ),
    ),
  );

const patternBracketCharacter = ($, characterToken) =>
  choice(
    characterToken,
    $._literal_dollar,
    $._pattern_bracket_left,
    $._pattern_bracket_exclamation,
    $._pattern_character_class_colon,
    $._pattern_collating_dot,
    $._pattern_equivalence_equals,
  );

const incompleteBracketLiteral = ($, start, part, end) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.literalFallback,
    prec.right(1, seq(alias(start, $.literal), repeat(part), end)),
  );

const wordIncompleteBracketLiteralAtom = ($) =>
  choice(
    $._literal_dollar,
    $._name_token,
    $._literal_token,
    $._literal_right_bracket,
    $._literal_tilde,
    $._literal_colon,
    $._literal_equals,
    $._literal_hash,
    $._literal_slash,
  );

const parameterIncompleteBracketLiteralAtom = ($) =>
  choice(
    $._literal_dollar,
    $._parameter_pattern_text_token,
    $._literal_tilde,
    $._literal_right_bracket,
    $._literal_slash,
    $._newline,
  );

const tildeExpansion = (user, end = null) =>
  choice(
    prec(2, "~"),
    prec.dynamic(
      2,
      prec.right(
        3,
        seq("~", field("user", user), ...(end === null ? [] : [end])),
      ),
    ),
  );

const incompleteBracketLiteralPart = (
  $,
  bracketSource,
  bracketLiteralRun,
  literalSource,
) =>
  choice(
    bracketSource,
    prec.dynamic(
      PATTERN_PRECEDENCE.literalFallback,
      alias(bracketLiteralRun, $.literal),
    ),
    ...literalSource,
    $.pattern_star_source,
    $.pattern_question_source,
    $._line_continuation,
    ...structuredSourceParts($),
  );

const wordSeparator = ($, marker) =>
  prec.right(2, seq(marker, repeat(choice($._blank, $._line_continuation))));

const lineContinuationRun = ($) => prec.right(1, repeat1($._line_continuation));

const arithmeticBinaryExpression = ($, left, right, operatorSegment) =>
  prec.left(
    seq(
      field("left", left),
      operatorSegment,
      arithmeticOperandLayout($),
      field("right", right),
    ),
  );

const arithmeticBoundaryLayout = ($, boundary) =>
  seq(boundary, optional($._arithmetic_layout));

const arithmeticOperatorSegment = ($, boundary, operator) =>
  seq(arithmeticBoundaryLayout($, boundary), field("operator", operator));

const arithmeticOperandLayout = ($) => optional($._arithmetic_layout);

const arithmeticUnaryExpression = ($, operator) =>
  prec.right(
    seq(
      field("operator", alias(operator, $.arithmetic_operator)),
      arithmeticOperandLayout($),
      field("operand", $._arithmetic_unary_expression),
    ),
  );

const arithmeticBinaryLevelRules = () => {
  const rules = {};
  ARITHMETIC_BINARY_LEVELS.forEach(([level], index) => {
    const following = ARITHMETIC_BINARY_LEVELS[index + 1];
    const next = ($) =>
      following === undefined
        ? $._arithmetic_unary_expression
        : $[`_arithmetic_${following[0]}_expression`];
    rules[`_arithmetic_${level}_expression`] = ($) =>
      choice(
        alias(
          $[`_arithmetic_${level}_binary_expression`],
          $.arithmetic_binary_expression,
        ),
        next($),
      );
    rules[`_arithmetic_${level}_binary_expression`] = ($) =>
      arithmeticBinaryExpression(
        $,
        $[`_arithmetic_${level}_expression`],
        next($),
        $[`_arithmetic_${level}_operator_segment`],
      );
  });
  return rules;
};

const arithmeticBinaryOperatorSegmentRules = () => {
  const rules = {};
  for (const [level] of ARITHMETIC_BINARY_LEVELS) {
    rules[`_arithmetic_${level}_operator_segment`] = ($) =>
      arithmeticOperatorSegment(
        $,
        $[`_arithmetic_${level}_operator_boundary`],
        $[`_arithmetic_${level}_operator`],
      );
  }
  return rules;
};

const arithmeticBinaryOperatorRules = () => {
  const rules = {};
  for (const [level, operators] of ARITHMETIC_BINARY_LEVELS) {
    rules[`_arithmetic_${level}_operator`] = ($) =>
      alias(
        operators.length === 1 ? operators[0] : choice(...operators),
        $.arithmetic_operator,
      );
  }
  return rules;
};

const arithmeticClosingLayout = ($) =>
  seq($._arithmetic_closing_boundary, optional($._arithmetic_layout));

const arithmeticLvalue = ($) =>
  choice(
    $.arithmetic_variable,
    alias($._parenthesized_arithmetic_lvalue, $.parenthesized_arithmetic),
  );

const parenthesizedArithmetic = ($, expression) =>
  seq(
    "(",
    optional($._arithmetic_layout),
    field("expression", expression),
    arithmeticClosingLayout($),
    ")",
  );

const arithmeticExpansionStart = ($, marker) =>
  seq($._command_or_arithmetic_substitution_start, marker, "(");

// The scanner boundary prevents layout from choosing a flat reading during
// the structured reading's final reduce-versus-shift decision.
const closedArithmeticExpansion = ($, start, expression, closing) =>
  seq(
    start,
    optional($._arithmetic_layout),
    field("expression", expression),
    closing,
    ")",
    ")",
  );

const linebreakLayout = ($) =>
  seq(optional($.linebreak), optional($._horizontal_layout));

const separatorOperatorLayout = ($, operator) =>
  seq(
    operator,
    repeat(prec(2, $._line_continuation)),
    optional(field("linebreak", $.linebreak)),
  );

const reservedWordLinebreak = ($) =>
  choice(
    seq($.linebreak, optional($._horizontal_layout)),
    $._horizontal_layout,
  );

const newlineListElements = ($) => [
  $.here_document_sequence,
  $._layout_newline,
  $._blank_line,
  $._continued_blank_line,
  $._comment_line,
];

const ledNewlineList = ($, lead) =>
  prec.right(seq(lead, repeat(choice(...newlineListElements($)))));

const boundaryLineComment = ($, comment) =>
  seq($._comment_boundary, optional($._horizontal_layout), comment);

const trailingComment = ($) =>
  seq($._trailing_comment_boundary, optional($._horizontal_layout), $.comment);

const doubleQuotedPart = ($) =>
  choice(
    $.double_quote_text,
    $.double_quote_escape,
    $._backquote_double_quote_content_escape_run,
    $._backquote_double_quote_escaped_pair_run,
    alias($._newline, $.double_quote_text),
    alias($._double_quoted_parameter_expansion, $.parameter_expansion),
    $.command_substitution,
    $.arithmetic_expansion,
    alias($._double_quoted_backquote_substitution, $.backquote_substitution),
  );

const completeCommandsTail = ($) =>
  seq(
    $.complete_commands,
    optional($.linebreak),
    optional($._free_trailing_layout),
  );

const linebreakLedCommandsBody = ($) =>
  seq(
    $.linebreak,
    optional($._horizontal_layout),
    optional(choice(completeCommandsTail($), trailingComment($))),
  );

// Keep leading continuations outside the body while `$(` can still become an
// arithmetic expansion; backquotes pass their continuation-led layout here.
const substitutionCommandsBody = ($, leadingLayout) =>
  choice(
    seq(leadingLayout, optional(completeCommandsTail($))),
    completeCommandsTail($),
    linebreakLedCommandsBody($),
    trailingComment($),
  );

const backquoteDollar = ($) =>
  seq(alias($._backquote_dollar_prefix, "\\"), token.immediate("$"));

// Recovery drops zero-width markers. Let the scanner own '$' so a literal
// fallback cannot remain reusable when an edit turns it into an expansion.
const dollarExpansionPrefix = ($) =>
  choice(alias($._dollar_expansion_start, "$"), backquoteDollar($));

const dollarExpansionStart = ($, delimiter) =>
  seq(dollarExpansionPrefix($), delimiter);

const backquoteDelimiter = (plain, prefix) =>
  choice(alias(plain, "`"), seq(alias(prefix, "\\"), token.immediate("`")));

const commandSubstitution = ($, start) =>
  seq(
    start,
    $._command_substitution_body_begin,
    optional(field("body", $.command_substitution_body)),
    alias($._command_substitution_close, ")"),
  );

const patternSpecialStart = ($, marker) =>
  seq(alias($._pattern_special_left_bracket, "["), marker);

const patternSpecialEnd = ($, marker) =>
  seq(marker, alias($._literal_right_bracket, "]"));

const patternSpecialClassSource = ($, marker, characterSource) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.specialElement,
    seq(
      patternSpecialStart($, marker),
      repeat1(
        field("value", choice(characterSource, ...structuredSourceParts($))),
      ),
      patternSpecialEnd($, marker),
    ),
  );

const patternCharacterClassStructuredContent = ($) =>
  choice(...structuredSourceParts($));

const patternCharacterClassBody = ($, content) =>
  choice(
    field("content", content),
    seq(
      optional(field("content", content)),
      repeat1(
        seq(
          field("content", patternCharacterClassStructuredContent($)),
          optional(field("content", content)),
        ),
      ),
    ),
  );

const patternCharacterClassSource = ($, content) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.specialElement,
    seq(
      patternSpecialStart($, alias($._pattern_character_class_colon, ":")),
      patternCharacterClassBody($, content),
      patternSpecialEnd($, alias($._pattern_character_class_end_colon, ":")),
    ),
  );

const patternSpecialInitialRange = ($, endpoint) =>
  patternBracketRange(
    $,
    endpoint,
    alias(
      $._pattern_special_marker_character,
      $.pattern_bracket_character_source,
    ),
  );

const patternSpecialPrefixedList = ($, member, initialRange) =>
  prec.right(
    choice(
      seq(
        field("member", alias(initialRange, $.pattern_bracket_range_source)),
        repeat(field("member", member)),
      ),
      seq(
        field(
          "member",
          alias(
            $._pattern_special_marker_character,
            $.pattern_bracket_character_source,
          ),
        ),
        repeat(field("member", member)),
      ),
    ),
  );

const patternSpecialPrefixedExpression = ($, list) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.expression,
    seq(
      alias($._pattern_special_left_bracket, "["),
      field("members", alias(list, $.pattern_bracket_members_source)),
      alias($._literal_right_bracket, "]"),
    ),
  );

const patternDeferredBracketRangeEndpoint = ($, character, collatingSymbol) =>
  choice(
    alias(character, $.pattern_bracket_character_source),
    collatingSymbol,
    $.pattern_bracket_hyphen_source,
    ...patternBracketStructuredSourceParts($),
  );

const patternDeferredBracketMember = ($, character, range, specialSources) =>
  choice(
    ...specialSources,
    alias(range, $.pattern_bracket_range_source),
    alias(character, $.pattern_bracket_character_source),
    $.pattern_bracket_hyphen_source,
    ...patternBracketStructuredSourceParts($),
  );

const parameterExpansion = ($, bracedExpansion) =>
  prec(
    1,
    choice(
      seq(dollarExpansionPrefix($), field("parameter", $._unbraced_parameter)),
      seq(dollarExpansionStart($, "{"), bracedExpansion),
    ),
  );

const bracedParameterSource = ($, classifiedParameter) =>
  choice(
    field("parameter", classifiedParameter),
    $._unclassified_numeric_parameter_source,
  );

const bracedParameterExpansion = ($, tail) =>
  choice(
    seq(bracedParameterSource($, $._braced_parameter), tail),
    prec.dynamic(2, seq(field("parameter", $._special_parameter_hash), tail)),
    prec.dynamic(
      3,
      seq(
        field("operator", $._parameter_length_operator),
        bracedParameterSource($, $._length_parameter),
        "}",
      ),
    ),
  );

const parameterOperatorTail = ($, word) =>
  choice(
    seq(
      field("operator", $._parameter_value_operator),
      optional(field("word", word)),
      "}",
    ),
    seq(
      field("operator", $._parameter_pattern_operator),
      optional(field("pattern", $.parameter_pattern)),
      "}",
    ),
  );

const parameterPatternSource = ($) =>
  choice(
    $._parameter_tilde_source,
    seq($._parameter_pattern_part, optional($._parameter_source_tail)),
  );

const caseClauseItems = ($) =>
  choice(
    $._esac_keyword_source,
    seq(field("items", $.case_list), $._esac_keyword_source),
    seq(field("items", $.case_list_ns), $._esac_keyword_source),
  );

const compoundListField = ($, name) => field(name, $.compound_list);

const conditionalThenBranch = ($, keyword, tail) =>
  seq(
    keyword,
    compoundListField($, "condition"),
    optional($._closing_layout),
    $._then_keyword_source,
    compoundListField($, "consequence"),
    optional($._closing_layout),
    tail,
  );

// Only the consequence owns closing layout; a competing owner before fi
// lets elif reduce its empty alternative before reaching else.
const ifClause = ($) =>
  conditionalThenBranch(
    $,
    $._if_keyword_source,
    choice(
      $._fi_keyword_source,
      seq(field("alternative", $.else_part), $._fi_keyword_source),
    ),
  );

const loopClause = ($, keyword) =>
  seq(
    keyword,
    compoundListField($, "condition"),
    optional($._closing_layout),
    field("body", $.do_group),
  );

const separatedForBody = ($) =>
  seq(
    choice(
      seq(
        optional($._horizontal_layout),
        field(
          "separator",
          alias($._sequential_operator_separator, $.sequential_sep),
        ),
      ),
      field(
        "separator",
        alias($._sequential_newline_separator, $.sequential_sep),
      ),
    ),
    optional($._horizontal_layout),
    field("body", $.do_group),
  );

const forTail = ($) =>
  choice(
    seq($._horizontal_layout, field("body", $.do_group)),
    separatedForBody($),
    seq(
      reservedWordLinebreak($),
      field("in", $.in),
      optional(seq($._horizontal_layout, field("words", $.wordlist))),
      separatedForBody($),
    ),
  );

const commandRedirectContinuations = ($) => [
  field("redirect", alias($._io_redirect_without_descriptor, $.io_redirect)),
  seq($._redirect_separator, field("redirect", $.io_redirect)),
];

const redirectList = ($) =>
  seq(
    field("redirect", $.io_redirect),
    repeat(choice(...commandRedirectContinuations($))),
  );

const redirectableCompoundCommand = ($) =>
  prec.right(
    seq(
      field("body", $.compound_command),
      repeat($._line_continuation),
      optional(
        seq(
          $._redirect_list_begin,
          optional($._horizontal_layout),
          field("redirects", $.redirect_list),
        ),
      ),
    ),
  );

const plainChunk = (part) => prec.right(repeat1(part));

const wordPlainChunk = ($) =>
  plainChunk(
    choice(
      $._word_name_token,
      $._literal_token,
      $._literal_right_bracket,
      $._literal_tilde,
      $._literal_colon,
      $._literal_equals,
    ),
  );

const assignmentPlainChunk = ($) =>
  plainChunk(
    choice(
      $._literal_dollar,
      $._name_token,
      $._literal_token,
      $._literal_right_bracket,
      $._literal_equals,
    ),
  );

const parameterPlainChunk = ($) =>
  plainChunk(
    choice(
      $._literal_dollar,
      $._parameter_pattern_text_token,
      $._literal_right_bracket,
      $._newline,
    ),
  );

const functionDefinitionHeader = ($) =>
  seq(
    field("name", $.fname),
    optional($._horizontal_layout),
    "(",
    optional($._horizontal_layout),
    ")",
  );

const functionDefinitionWithBody = ($, body) =>
  seq(
    functionDefinitionHeader($),
    $._function_body_continuation_boundary,
    optional(field("linebreak", $.linebreak)),
    optional($._horizontal_layout),
    field("body", body),
  );

const patternBracketExpression = (
  $,
  list,
  opener = alias($._literal_left_bracket, "["),
) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.expression,
    prec.right(
      2,
      seq(
        opener,
        choice(
          seq(
            field("negation", $.pattern_bracket_negation_source),
            field("members", list),
          ),
          field("members", list),
        ),
        alias($._literal_right_bracket, "]"),
      ),
    ),
  );

const patternBracketList = ($, member, initialRange) =>
  prec.right(
    choice(
      seq(
        field(
          "member",
          choice(
            alias(
              $._pattern_initial_right_bracket,
              $.pattern_bracket_character_source,
            ),
            initialRange,
          ),
        ),
        repeat(field("member", member)),
      ),
      repeat1(field("member", member)),
    ),
  );

const patternBracketRange = ($, endpoint, start = endpoint) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.range,
    seq(
      field("start", start),
      field("operator", $.pattern_bracket_range_operator_source),
      field("end", endpoint),
    ),
  );

const patternInitialBracketRange = ($, endpoint) =>
  patternBracketRange(
    $,
    endpoint,
    alias($._pattern_initial_right_bracket, $.pattern_bracket_character_source),
  );

export default grammar({
  name: "sh",

  extras: ($) => [$._here_document_content_line_start],

  externals: ($) => [
    $._left_brace,
    $._right_brace,
    $._io_number_token,
    $._bang_token,
    $._if_keyword,
    $._then_keyword,
    $._elif_keyword,
    $._else_keyword,
    $._fi_keyword,
    $._for_keyword,
    $._in_keyword,
    $._do_keyword,
    $._done_keyword,
    $._case_keyword,
    $._esac_keyword,
    $._while_keyword,
    $._until_keyword,
    $._dless_commit,
    $._dlessdash_commit,
    $._here_end_begin,
    $._here_end_commit,
    $.here_document_line_end,
    $._here_document_body_start,
    $._quoted_here_document_body_start,
    $._quoted_here_document_end_begin,
    $._quoted_here_document_end_text,
    $._here_document_end_begin,
    $._here_document_end_leading_tabs,
    $._here_document_end_commit,
    $._here_document_sequence_end,
    $._here_document_content_line_start,
    $._newline,
    $.line_continuation,
    $._arithmetic_assignment_operator_boundary,
    $._arithmetic_question_operator_boundary,
    $._arithmetic_colon_operator_boundary,
    ...arithmeticBinaryLevelSymbols($, "operator_boundary"),
    $._arithmetic_closing_boundary,
    $._arithmetic_left_parenthesis,
    $._arithmetic_dynamic_left_parenthesis,
    $._pattern_special_left_bracket,
    $._literal_hash,
    $._comment_boundary,
    $._trailing_comment_boundary,
    $.comment,
    $._comment_line_end,
    $._dollar_expansion_start,
    $._braced_parameter_number_start,
    $._braced_positional_parameter_start,
    $._backquote_start,
    $._backquote_start_prefix,
    $._backquote_dollar_prefix,
    $._backquote_end,
    $._backquote_end_prefix,
    $._backquote_content_run_begin,
    $._backquote_pair_run_begin,
    $._backquote_pair_run_end,
    $._pattern_continuation,
    $._pattern_end,
    $._pipe_continuation,
    $._redirect_list_begin,
    $._case_item_end,
    $._function_body_continuation_boundary,
    $._command_substitution_body_begin,
    $._subshell_close,
    $._word_bracket_literal_start,
    $._parameter_bracket_literal_start,
    $._word_bracket_fallback_end,
    $._parameter_bracket_fallback_end,
    $._pattern_bracket_character_token,
    $._parameter_pattern_bracket_character_token,
    $._pattern_bracket_hyphen_token,
    $._word_tilde_end,
    $._assignment_tilde_end,
    $._tilde_bracket_literal_start,
    $._assignment_tilde_bracket_literal_start,
    $._assignment_name_token,
    $._fname_token,
    $._word_name_token,
    $._and_or_continuation,
    $._word_separator_begin,
    $._list_continuation,
    $._term_continuation,
    $._terminator_ahead,
    $._assignment_separator_begin,
    $._redirect_separator_begin,
    $._pre_newline_blank,
    $._trailing_continuation_begin,
    $._command_substitution_close,
    $._separator_newline,
    $._layout_begin,
    $._term_boundary,
    $._word_pattern_bracket_open,
    $._parameter_pattern_bracket_open,
    $._double_quoted_backquote_start,
    $._double_quoted_backquote_start_prefix,
    $._backquote_quote_prefix,
    $._backquote_continuation_begin,
    $.dollar_single_quote_escape,
    $._backquote_dollar_single_quote_text,
    $._backquote_dollar_single_quote_prefix,
    $._backquote_pattern_escape,
    $._pattern_character_class_end_colon,
  ],

  conflicts: ($) => [
    [$.term],
    [$.complete_commands],
    [$._operator_separator],
    [$._sequential_operator_separator],
    [$._sequential_newline_separator, $.linebreak],
    [$.case_list],
    [$._pattern_bracket_member, $._pattern_bracket_range_endpoint],
    [$._pattern_deferred_member, $._pattern_deferred_range_endpoint],
    [
      $._parameter_pattern_deferred_member,
      $._parameter_pattern_deferred_range_endpoint,
    ],
    [$.pattern_collating_symbol_source, $._pattern_special_marker_character],
    [
      $._parameter_pattern_collating_symbol_source,
      $._pattern_special_marker_character,
    ],
    [$._word_special_prefixed_bracket_source, $._pattern_special_literal_left],
    [
      $._parameter_special_prefixed_bracket_source,
      $._pattern_special_literal_left,
    ],
    [
      $._parameter_pattern_bracket_member,
      $._parameter_pattern_bracket_range_endpoint,
    ],
    [$.pattern_bracket_negation_source, $.pattern_bracket_character_source],
    [$.pattern_bracket_negation_source, $._parameter_pattern_bracket_character],
    [$.pattern_bracket_range_operator_source, $.pattern_bracket_hyphen_source],
    [$._special_parameter_hash, $._parameter_length_operator],
    [$._parenthesized_arithmetic_lvalue, $._arithmetic_primary_expression],
    [$.arithmetic_dynamic_expression],
    [$.complete_command],
  ],

  rules: {
    program: ($) =>
      choice(
        seq(
          optional(field("leading", $.linebreak)),
          optional($._horizontal_layout),
          field("commands", $.complete_commands),
          optional(field("trailing", $.linebreak)),
          optional($._free_trailing_layout),
        ),
        seq(
          optional(field("leading", $.linebreak)),
          optional($._horizontal_layout),
          optional(trailingComment($)),
        ),
      ),

    _line_continuation: ($) =>
      choice(
        $.line_continuation,
        seq(
          $._backquote_continuation_begin,
          repeat1("\\"),
          $.line_continuation,
        ),
      ),

    complete_commands: ($) =>
      seq(
        field("command", $.complete_command),
        repeat(
          seq(
            choice(
              field(
                "separator",
                alias($._separator_led_newline_list, $.newline_list),
              ),
              seq($._term_continuation, field("separator", $.newline_list)),
              field(
                "separator",
                alias($._here_document_led_newline_list, $.newline_list),
              ),
            ),
            optional($._horizontal_layout),
            field("command", $.complete_command),
          ),
        ),
        optional($._term_boundary),
      ),

    complete_command: ($) =>
      seq(
        field("body", $.list),
        optional(
          choice(
            seq(
              $._terminator_ahead,
              optional($._horizontal_layout),
              field("terminator", $.separator_op),
            ),
            $._term_boundary,
          ),
        ),
      ),

    list: ($) =>
      seq(
        field("and_or", $.and_or),
        repeat(
          seq(
            $._list_continuation,
            optional($._horizontal_layout),
            field("separator", $.separator_op),
            optional($._horizontal_layout),
            field("and_or", $.and_or),
          ),
        ),
      ),

    and_or: ($) =>
      seq(
        field("pipeline", $.pipeline),
        repeat(
          seq(
            $._and_or_continuation,
            optional($._horizontal_layout),
            field("operator", choice($._and_if, $._or_if)),
            linebreakLayout($),
            field("pipeline", $.pipeline),
          ),
        ),
      ),

    pipeline: ($) =>
      seq(
        optional(
          seq(field("negation", $.bang), optional($._horizontal_layout)),
        ),
        field("sequence", $.pipe_sequence),
      ),

    pipe_sequence: ($) =>
      seq(
        field("command", $.command),
        repeat(
          seq(
            $._pipe_continuation,
            optional($._horizontal_layout),
            "|",
            linebreakLayout($),
            field("command", $.command),
          ),
        ),
      ),

    command: ($) =>
      choice(
        field("body", $.simple_command),
        $._redirectable_compound_command,
        field("body", $.function_definition),
      ),

    _redirectable_compound_command: ($) => redirectableCompoundCommand($),

    separator_op: (_) => choice("&", ";"),

    _operator_separator: ($) =>
      separatorOperatorLayout($, field("operator", $.separator_op)),

    _newline_separator: ($) => field("newlines", $.newline_list),

    _separator_led_newline_separator: ($) =>
      field("newlines", alias($._separator_led_newline_list, $.newline_list)),

    _here_document_led_separator: ($) =>
      field(
        "newlines",
        alias($._here_document_led_newline_list, $.newline_list),
      ),

    _here_document_led_linebreak: ($) =>
      alias($._here_document_led_newline_list, $.newline_list),

    _here_document_led_operator_separator: ($) =>
      seq(
        field("operator", $.separator_op),
        field("linebreak", alias($._here_document_led_linebreak, $.linebreak)),
      ),

    _sequential_operator_separator: ($) => separatorOperatorLayout($, ";"),

    _sequential_newline_separator: ($) => field("newlines", $.newline_list),

    term: ($) =>
      seq(
        field("and_or", $.and_or),
        repeat(
          seq(
            choice(
              field(
                "separator",
                alias($._separator_led_newline_separator, $.separator),
              ),
              seq(
                $._term_continuation,
                choice(
                  seq(
                    optional($._horizontal_layout),
                    field(
                      "separator",
                      alias($._operator_separator, $.separator),
                    ),
                  ),
                  field("separator", alias($._newline_separator, $.separator)),
                ),
              ),
              seq(
                optional($._closing_layout),
                field(
                  "separator",
                  alias($._here_document_led_operator_separator, $.separator),
                ),
              ),
              field(
                "separator",
                alias($._here_document_led_separator, $.separator),
              ),
            ),
            optional($._horizontal_layout),
            field("and_or", $.and_or),
          ),
        ),
        optional($._term_boundary),
      ),

    compound_list: ($) =>
      seq(
        optional(field("leading", $.linebreak)),
        optional($._horizontal_layout),
        field("body", $.term),
        optional(
          choice(
            seq(
              optional(
                seq($._terminator_ahead, optional($._horizontal_layout)),
              ),
              field("terminator", alias($._operator_separator, $.separator)),
            ),
            field("terminator", alias($._newline_separator, $.separator)),
          ),
        ),
      ),

    _and_if: ($) => alias("&&", $.and_if),

    _or_if: ($) => alias("||", $.or_if),

    bang: ($) => $._bang_token,

    _if_keyword_source: ($) => alias($._if_keyword, $.if_keyword),

    _then_keyword_source: ($) => alias($._then_keyword, $.then_keyword),

    _elif_keyword_source: ($) => alias($._elif_keyword, $.elif_keyword),

    _else_keyword_source: ($) => alias($._else_keyword, $.else_keyword),

    _fi_keyword_source: ($) => alias($._fi_keyword, $.fi_keyword),

    _for_keyword_source: ($) => alias($._for_keyword, $.for_keyword),

    _in_keyword_source: ($) => alias($._in_keyword, $.in_keyword),

    _do_keyword_source: ($) => alias($._do_keyword, $.do_keyword),

    _done_keyword_source: ($) => alias($._done_keyword, $.done_keyword),

    _case_keyword_source: ($) => alias($._case_keyword, $.case_keyword),

    _esac_keyword_source: ($) => alias($._esac_keyword, $.esac_keyword),

    _while_keyword_source: ($) => alias($._while_keyword, $.while_keyword),

    _until_keyword_source: ($) => alias($._until_keyword, $.until_keyword),

    function_definition: ($) => functionDefinitionWithBody($, $.function_body),

    function_body: ($) => $._redirectable_compound_command,

    fname: ($) => $._fname_token,

    name: ($) => $._name_token,

    compound_command: ($) =>
      choice(
        $.brace_group,
        $.subshell,
        $.for_clause,
        $.case_clause,
        $.if_clause,
        $.while_clause,
        $.until_clause,
      ),

    redirect_list: ($) => redirectList($),

    brace_group: ($) =>
      seq(
        alias($._left_brace, "{"),
        compoundListField($, "body"),
        optional($._closing_layout),
        alias($._right_brace, "}"),
      ),

    subshell: ($) =>
      seq(
        "(",
        field("body", $.compound_list),
        optional($._closing_layout),
        optional($._subshell_close),
        ")",
      ),

    for_clause: ($) =>
      seq(
        $._for_keyword_source,
        $._word_separator,
        field("name", $.name),
        forTail($),
      ),

    in: ($) => $._in_keyword_source,

    wordlist: ($) =>
      prec.right(
        seq(
          field("word", $.word),
          repeat(
            choice(
              seq($._word_separator, field("word", $.word)),
              prec(1, $._line_continuation),
            ),
          ),
        ),
      ),

    do_group: ($) =>
      seq(
        $._do_keyword_source,
        compoundListField($, "body"),
        optional($._closing_layout),
        prec(20, $._done_keyword_source),
      ),

    if_clause: ($) => prec.right(ifClause($)),

    else_part: ($) =>
      prec.right(
        choice(
          conditionalThenBranch(
            $,
            $._elif_keyword_source,
            optional(field("alternative", $.else_part)),
          ),
          seq(
            $._else_keyword_source,
            compoundListField($, "body"),
            optional($._closing_layout),
          ),
        ),
      ),

    while_clause: ($) => loopClause($, $._while_keyword_source),

    until_clause: ($) => loopClause($, $._until_keyword_source),

    case_clause: ($) =>
      seq(
        $._case_keyword_source,
        $._word_separator,
        field("word", $.word),
        reservedWordLinebreak($),
        field("in", $.in),
        linebreakLayout($),
        caseClauseItems($),
      ),

    case_list_ns: ($) =>
      choice(
        field("item", $.case_item_ns),
        seq(field("terminated", $.case_list), field("item", $.case_item_ns)),
      ),

    case_list: ($) => repeat1(field("item", $.case_item)),

    case_item_ns: ($) =>
      seq(
        field("patterns", $.pattern_list),
        $._pattern_end,
        optional($._horizontal_layout),
        ")",
        choice(
          seq(optional($.linebreak), optional($._horizontal_layout)),
          prec.dynamic(
            10,
            seq(field("body", $.compound_list), optional($._closing_layout)),
          ),
        ),
      ),

    case_item: ($) =>
      seq(
        field("patterns", $.pattern_list),
        $._pattern_end,
        optional($._horizontal_layout),
        ")",
        choice(
          linebreakLayout($),
          prec.dynamic(10, field("body", $.compound_list)),
        ),
        seq($._case_item_end, optional($._horizontal_layout)),
        field("terminator", choice($._dsemi, $._semi_and)),
        optional(reservedWordLinebreak($)),
      ),

    pattern_list: ($) =>
      prec.dynamic(
        2,
        seq(
          optional(seq("(", optional($._horizontal_layout))),
          field("word", $.word),
          optional(lineContinuationRun($)),
          repeat(
            seq(
              $._pattern_continuation,
              optional($._horizontal_layout),
              "|",
              optional($._horizontal_layout),
              field("word", $.word),
              optional(lineContinuationRun($)),
            ),
          ),
        ),
      ),

    _dsemi: ($) => prec(10, alias(";;", $.dsemi)),

    _semi_and: ($) => prec(10, alias(";&", $.semi_and)),

    simple_command: ($) =>
      choice(
        seq(
          field("prefix", $.cmd_prefix),
          optional(
            seq(
              $._word_separator,
              field("word", $.cmd_word),
              optional(field("suffix", $.cmd_suffix)),
            ),
          ),
        ),
        seq(field("name", $.cmd_name), optional(field("suffix", $.cmd_suffix))),
      ),

    cmd_prefix: ($) =>
      seq(
        choice(
          field("assignment", $.assignment_word),
          field("redirect", $.io_redirect),
        ),
        repeat(
          choice(
            seq(
              $._assignment_separator,
              field("assignment", $.assignment_word),
            ),
            ...commandRedirectContinuations($),
          ),
        ),
      ),

    cmd_name: ($) => $.word,

    cmd_word: ($) => $.word,

    cmd_suffix: ($) =>
      repeat1(
        choice(
          seq($._word_separator, field("word", $.word)),
          ...commandRedirectContinuations($),
        ),
      ),

    io_redirect: ($) =>
      choice(
        prec.dynamic(
          2,
          seq(
            field("number", $.io_number),
            repeat($._line_continuation),
            field("body", choice($.io_file, $.io_here)),
          ),
        ),
        field("body", choice($.io_file, $.io_here)),
      ),

    _io_redirect_without_descriptor: ($) =>
      field("body", choice($.io_file, $.io_here)),

    io_number: ($) => seq($._io_number_token, /[0-9]+/),

    io_file: ($) =>
      seq(
        field(
          "operator",
          choice(
            "<",
            ">",
            $.lessand,
            $.greatand,
            $._dgreat,
            $.lessgreat,
            $.clobber,
          ),
        ),
        optional($._horizontal_layout),
        field("filename", $.filename),
      ),

    filename: ($) => field("word", $.word),

    lessand: (_) => "<&",

    greatand: (_) => ">&",

    _dgreat: ($) => alias(">>", $.dgreat),

    lessgreat: (_) => "<>",

    clobber: (_) => ">|",

    io_here: ($) =>
      seq(
        field("operator", choice($._dless, $._dlessdash)),
        optional($._horizontal_layout),
        field("end", $.here_end),
      ),

    _dless: ($) => seq(alias("<<", $.dless), $._dless_commit),

    _dlessdash: ($) => seq(alias("<<-", $.dlessdash), $._dlessdash_commit),

    here_end: ($) =>
      seq(
        $._here_end_begin,
        field("word", alias($._here_end_source_word, $.word)),
        $._here_end_commit,
      ),

    _here_end_source_word: ($) =>
      prec(
        1,
        seq(
          $._word_part,
          repeat(choice(alias($._literal_hash, $.literal), $._word_part)),
          optional(lineContinuationRun($)),
        ),
      ),

    here_document_sequence: ($) =>
      seq(
        optional(
          choice(
            boundaryLineComment($, field("comment", $.comment)),
            seq(
              $._pre_newline_blank,
              optional(seq(optional($._blank), $._continuation_led_run)),
            ),
          ),
        ),
        field("line_end", $.here_document_line_end),
        repeat1(field("document", $.here_document)),
        $._here_document_sequence_end,
      ),

    here_document: ($) =>
      choice(
        seq(
          $._here_document_body_start,
          optional(field("body", $.here_document_body)),
          field("end", $.here_document_end),
        ),
        seq(
          $._quoted_here_document_body_start,
          optional(field("body", $.quoted_here_document_body)),
          field("end", $.here_document_end),
        ),
      ),

    here_document_end: ($) =>
      seq(
        choice(
          seq(
            $._quoted_here_document_end_begin,
            optional($._here_document_end_leading_tabs),
            optional(
              alias($._quoted_here_document_end_text, $.here_document_end_text),
            ),
          ),
          seq(
            $._here_document_end_begin,
            repeat(
              choice($._here_document_end_leading_tabs, $._line_continuation),
            ),
            optional(
              seq(
                $._here_document_end_part,
                repeat(choice($._here_document_end_part, $._line_continuation)),
              ),
            ),
          ),
        ),
        $._here_document_end_commit,
      ),

    _here_document_end_part: ($) =>
      choice(
        $.here_document_end_text,
        alias($._here_document_backslash, $.here_document_end_text),
        alias($._here_document_end_backquote, $.here_document_end_text),
      ),

    here_document_end_text: (_) => token.immediate(/[^\\\n`]+/),

    _here_document_end_backquote: (_) =>
      token.immediate(prec(-2, seq(repeat("\\"), "`"))),

    here_document_body: ($) =>
      repeat1(
        choice(
          $.here_document_text,
          alias($._here_document_dollar, $.here_document_text),
          alias($._here_document_backslash, $.here_document_text),
          $.here_document_escape,
          $._backquote_here_document_content_escape_run,
          $._backquote_here_document_escaped_pair_run,
          $._line_continuation,
          $.parameter_expansion,
          $.command_substitution,
          $.arithmetic_expansion,
          $.backquote_substitution,
          $._newline,
        ),
      ),

    here_document_text: (_) => token.immediate(prec(-1, /[^$`\\\n]+/)),

    _here_document_dollar: (_) => token.immediate(prec(-2, /\$/)),

    _here_document_backslash: (_) => token.immediate(prec(-2, /\\/)),

    here_document_escape: (_) =>
      token.immediate(seq("\\", choice("$", "`", "\\"))),

    _backquote_here_document_content_escape_run: ($) =>
      backquoteContentEscapeRun(
        $,
        $.here_document_escape,
        $.here_document_text,
        choice(
          alias($._backquote_here_document_escape_tail, $.here_document_escape),
          alias($._backquote_here_document_text_tail, $.here_document_text),
        ),
      ),

    _backquote_here_document_escaped_pair_run: ($) =>
      backquoteEscapedPairRun($, $.here_document_escape),

    _backquote_here_document_escape_tail: (_) =>
      token.immediate(seq("\\", choice("$", "`"))),

    _backquote_here_document_text_tail: (_) =>
      token.immediate(seq("\\", /[^\\\n$`]/)),

    quoted_here_document_body: ($) =>
      repeat1(choice($.quoted_here_document_text, $._newline)),

    quoted_here_document_text: (_) => token.immediate(prec(-1, /[^\n]+/)),

    assignment_word: ($) =>
      prec.dynamic(
        ASSIGNMENT_WORD_PRECEDENCE,
        prec.right(
          1,
          seq(
            field("name", alias($._assignment_name_token, $.variable_name)),
            "=",
            optional(field("value", $.assignment_value)),
            optional(lineContinuationRun($)),
          ),
        ),
      ),

    variable_name: ($) => $._name_token,

    assignment_value: ($) => $._assignment_source_word,

    _assignment_source_word: ($) =>
      choice(
        prec.right(
          seq(
            alias($._assignment_tilde_expansion, $.tilde_expansion),
            optional($._assignment_source_word_tail),
          ),
        ),
        prec.right(
          seq(
            choice($._assignment_colon_part, $._assignment_word_part),
            optional($._assignment_source_word_tail),
          ),
        ),
      ),

    _assignment_source_word_tail: ($) =>
      repeat1(choice($._assignment_colon_part, $._assignment_word_part)),

    _assignment_non_delimiter_part: ($) =>
      choice(
        $.pattern_bracket_source,
        alias($._literal_hash, $.literal),
        alias($._assignment_literal, $.literal),
        alias($._literal_tilde, $.literal),
        $.pattern_star_source,
        $.pattern_question_source,
        $._word_structured_part,
      ),

    _assignment_word_part: ($) =>
      choice(
        $._assignment_non_delimiter_part,
        $._word_incomplete_bracket_literal,
        prec(-1, alias($._literal_slash, $.literal)),
      ),

    _assignment_literal: ($) => prec.right(assignmentPlainChunk($)),

    _assignment_colon_part: ($) =>
      prec.right(
        seq(
          alias($._literal_colon, $.literal),
          optional(
            prec.dynamic(
              3,
              alias($._assignment_tilde_expansion, $.tilde_expansion),
            ),
          ),
        ),
      ),

    word: ($) => $._source_word,

    _source_word: ($) =>
      choice(
        prec.right(seq($.tilde_expansion, optional($._source_word_tail))),
        prec.right(seq($._word_part, optional($._source_word_tail))),
      ),

    _source_word_tail: ($) =>
      repeat1(choice(alias($._literal_hash, $.literal), $._word_part)),

    tilde_expansion: ($) => tildeExpansion($.tilde_user, $._word_tilde_end),

    _assignment_tilde_expansion: ($) =>
      tildeExpansion(
        alias($._assignment_tilde_user, $.tilde_user),
        $._assignment_tilde_end,
      ),

    _parameter_tilde_expansion: ($) =>
      tildeExpansion(
        alias($._parameter_tilde_user, $.tilde_user),
        $._word_tilde_end,
      ),

    _parameter_terminal_tilde_expansion: ($) =>
      tildeExpansion(alias($._parameter_tilde_user, $.tilde_user), null),

    tilde_user: ($) =>
      repeat1(
        choice(
          alias($._tilde_bracket_literal_start, $.literal),
          alias($._literal_hash, $.literal),
          $._word_non_slash_part,
        ),
      ),

    _assignment_tilde_user: ($) =>
      repeat1(
        choice(
          alias($._assignment_tilde_bracket_literal_start, $.literal),
          $._assignment_non_delimiter_part,
        ),
      ),

    _parameter_tilde_user: ($) =>
      repeat1(
        choice(
          alias($._tilde_bracket_literal_start, $.literal),
          $._parameter_non_slash_part,
        ),
      ),

    _word_non_slash_part: ($) =>
      choice(
        $.pattern_bracket_source,
        $.literal,
        $.pattern_star_source,
        $.pattern_question_source,
        $._word_structured_part,
      ),

    _word_part: ($) =>
      choice(
        $._word_non_slash_part,
        $._word_incomplete_bracket_literal,
        prec(-1, alias($._literal_slash, $.literal)),
      ),

    _word_structured_part: ($) => choice(...structuredSourceParts($)),

    literal: ($) => prec.right(choice($._literal_dollar, wordPlainChunk($))),

    _word_incomplete_bracket_literal: ($) =>
      incompleteBracketLiteral(
        $,
        $._word_bracket_literal_start,
        $._word_incomplete_bracket_literal_part,
        $._word_bracket_fallback_end,
      ),

    _word_incomplete_bracket_literal_part: ($) =>
      incompleteBracketLiteralPart(
        $,
        wordPatternBracketSources($),
        $._word_incomplete_bracket_literal_run,
        [
          $._pattern_special_literal_start,
          alias($._word_incomplete_bracket_literal_text, $.literal),
        ],
      ),

    _word_special_prefixed_bracket_source: ($) =>
      patternSpecialPrefixedExpression($, $._pattern_special_prefixed_members),

    _word_incomplete_bracket_literal_text: ($) =>
      prec.right(1, repeat1(wordIncompleteBracketLiteralAtom($))),

    _word_incomplete_bracket_literal_run: ($) =>
      incompleteBracketLiteralRun($, wordIncompleteBracketLiteralAtom($)),

    pattern_star_source: (_) => token(prec(-1, "*")),

    pattern_question_source: (_) => token(prec(-1, "?")),

    pattern_bracket_source: ($) =>
      patternBracketExpression(
        $,
        $.pattern_bracket_members_source,
        choice(
          alias($._word_pattern_bracket_open, "["),
          alias($._literal_left_bracket, "["),
        ),
      ),

    _parameter_pattern_bracket_expression: ($) =>
      patternBracketExpression(
        $,
        alias(
          $._parameter_pattern_bracket_list,
          $.pattern_bracket_members_source,
        ),
        choice(
          alias($._parameter_pattern_bracket_open, "["),
          alias($._literal_left_bracket, "["),
        ),
      ),

    pattern_bracket_negation_source: ($) =>
      prec.dynamic(PATTERN_PRECEDENCE.negation, $._pattern_bracket_exclamation),

    pattern_bracket_members_source: ($) =>
      patternBracketList(
        $,
        $._pattern_bracket_member,
        alias($._pattern_initial_bracket_range, $.pattern_bracket_range_source),
      ),

    _parameter_pattern_bracket_list: ($) =>
      patternBracketList(
        $,
        $._parameter_pattern_bracket_member,
        alias(
          $._parameter_pattern_initial_bracket_range,
          $.pattern_bracket_range_source,
        ),
      ),

    _pattern_bracket_member: ($) =>
      choice(
        ...wordPatternSpecialSources($),
        $.pattern_bracket_range_source,
        $.pattern_bracket_character_source,
        $._pattern_operator_bracket_character,
        $.pattern_bracket_hyphen_source,
        ...patternBracketStructuredSourceParts($),
      ),

    _parameter_pattern_bracket_member: ($) =>
      choice(
        ...parameterPatternSpecialSources($),
        alias(
          $._parameter_pattern_bracket_range,
          $.pattern_bracket_range_source,
        ),
        alias(
          $._parameter_pattern_bracket_character,
          $.pattern_bracket_character_source,
        ),
        $._pattern_operator_bracket_character,
        $.pattern_bracket_hyphen_source,
        ...patternBracketStructuredSourceParts($),
      ),

    pattern_bracket_range_source: ($) =>
      patternBracketRange($, $._pattern_bracket_range_endpoint),

    _parameter_pattern_bracket_range: ($) =>
      patternBracketRange($, $._parameter_pattern_bracket_range_endpoint),

    _pattern_initial_bracket_range: ($) =>
      patternInitialBracketRange($, $._pattern_bracket_range_endpoint),

    _parameter_pattern_initial_bracket_range: ($) =>
      patternInitialBracketRange(
        $,
        $._parameter_pattern_bracket_range_endpoint,
      ),

    _pattern_bracket_range_endpoint: ($) =>
      choice(
        $.pattern_bracket_character_source,
        $._pattern_operator_bracket_character,
        $.pattern_collating_symbol_source,
        $.pattern_bracket_hyphen_source,
        ...patternBracketStructuredSourceParts($),
      ),

    _parameter_pattern_bracket_range_endpoint: ($) =>
      choice(
        alias(
          $._parameter_pattern_bracket_character,
          $.pattern_bracket_character_source,
        ),
        $._pattern_operator_bracket_character,
        parameterPatternCollatingSymbolSource($),
        $.pattern_bracket_hyphen_source,
        ...patternBracketStructuredSourceParts($),
      ),

    pattern_bracket_character_source: ($) =>
      patternBracketCharacter($, $._pattern_bracket_character_token),

    _parameter_pattern_bracket_character: ($) =>
      patternBracketCharacter($, $._parameter_pattern_bracket_character_token),

    _pattern_operator_bracket_character: ($) =>
      choice(
        alias($.pattern_star_source, $.pattern_bracket_character_source),
        alias($.pattern_question_source, $.pattern_bracket_character_source),
      ),

    _pattern_deferred_bracket_character: ($) =>
      choice(
        $._pattern_special_plain_character,
        $._literal_dollar,
        $._pattern_special_marker_character,
      ),

    _pattern_deferred_range_endpoint: ($) =>
      patternDeferredBracketRangeEndpoint(
        $,
        $._pattern_deferred_bracket_character,
        $.pattern_collating_symbol_source,
      ),

    _pattern_deferred_range: ($) =>
      patternBracketRange($, $._pattern_deferred_range_endpoint),

    _pattern_deferred_initial_range: ($) =>
      patternSpecialInitialRange($, $._pattern_deferred_range_endpoint),

    _pattern_deferred_member: ($) =>
      patternDeferredBracketMember(
        $,
        $._pattern_deferred_bracket_character,
        $._pattern_deferred_range,
        wordPatternSpecialSources($),
      ),

    _pattern_special_prefixed_members: ($) =>
      patternSpecialPrefixedList(
        $,
        $._pattern_deferred_member,
        $._pattern_deferred_initial_range,
      ),

    _parameter_pattern_deferred_bracket_character: ($) =>
      choice(
        $._parameter_pattern_special_plain_character,
        $._literal_dollar,
        $._pattern_special_marker_character,
      ),

    _parameter_pattern_deferred_range_endpoint: ($) =>
      patternDeferredBracketRangeEndpoint(
        $,
        $._parameter_pattern_deferred_bracket_character,
        parameterPatternCollatingSymbolSource($),
      ),

    _parameter_pattern_deferred_range: ($) =>
      patternBracketRange($, $._parameter_pattern_deferred_range_endpoint),

    _parameter_pattern_deferred_initial_range: ($) =>
      patternSpecialInitialRange(
        $,
        $._parameter_pattern_deferred_range_endpoint,
      ),

    _parameter_pattern_deferred_member: ($) =>
      patternDeferredBracketMember(
        $,
        $._parameter_pattern_deferred_bracket_character,
        $._parameter_pattern_deferred_range,
        parameterPatternSpecialSources($),
      ),

    _parameter_pattern_special_prefixed_members: ($) =>
      patternSpecialPrefixedList(
        $,
        $._parameter_pattern_deferred_member,
        $._parameter_pattern_deferred_initial_range,
      ),

    pattern_bracket_range_operator_source: ($) =>
      $._pattern_bracket_hyphen_token,

    pattern_bracket_hyphen_source: ($) => $._pattern_bracket_hyphen_token,

    pattern_character_class_source: ($) =>
      patternCharacterClassSource($, $.pattern_character_class_content_source),

    _parameter_pattern_character_class_source: ($) =>
      patternCharacterClassSource(
        $,
        alias(
          $._parameter_pattern_character_class_content_source,
          $.pattern_character_class_content_source,
        ),
      ),

    pattern_character_class_content_source: ($) =>
      patternCharacterClassContent($, $._pattern_special_plain_character),

    _parameter_pattern_character_class_content_source: ($) =>
      patternCharacterClassContent(
        $,
        $._parameter_pattern_special_plain_character,
      ),

    _pattern_character_class_colon: (_) => token.immediate(prec(-3, /:/)),

    pattern_collating_symbol_source: ($) =>
      patternSpecialClassSource(
        $,
        alias($._pattern_collating_dot, "."),
        $.pattern_collating_symbol_character_source,
      ),

    _parameter_pattern_collating_symbol_source: ($) =>
      patternSpecialClassSource(
        $,
        alias($._pattern_collating_dot, "."),
        alias(
          $._parameter_pattern_collating_symbol_character_source,
          $.pattern_collating_symbol_character_source,
        ),
      ),

    pattern_collating_symbol_character_source: ($) =>
      choice(
        ...patternSpecialContentCharacters(
          $,
          $._pattern_special_plain_character,
        ),
        $._literal_right_bracket,
      ),

    _parameter_pattern_collating_symbol_character_source: ($) =>
      choice(
        ...patternSpecialContentCharacters(
          $,
          $._parameter_pattern_special_plain_character,
        ),
        $._literal_right_bracket,
      ),

    _pattern_collating_dot: (_) => token.immediate(prec(-2, /\./)),

    pattern_equivalence_class_source: ($) =>
      patternSpecialClassSource(
        $,
        alias($._pattern_equivalence_equals, "="),
        $.pattern_equivalence_class_character_source,
      ),

    _parameter_pattern_equivalence_class_source: ($) =>
      patternSpecialClassSource(
        $,
        alias($._pattern_equivalence_equals, "="),
        alias(
          $._parameter_pattern_equivalence_class_character_source,
          $.pattern_equivalence_class_character_source,
        ),
      ),

    pattern_equivalence_class_character_source: ($) =>
      choice(
        ...patternSpecialContentCharacters(
          $,
          $._pattern_special_plain_character,
        ),
      ),

    _parameter_pattern_equivalence_class_character_source: ($) =>
      choice(
        ...patternSpecialContentCharacters(
          $,
          $._parameter_pattern_special_plain_character,
        ),
      ),

    _pattern_equivalence_equals: (_) => token.immediate(prec(-2, /=/)),

    escaped_character: (_) => token(seq("\\", /[^\n]/)),

    _backquote_content_escape_run: ($) =>
      backquoteContentEscapeRun($, $.escaped_character, $.literal),

    _backquote_escaped_pair_run: ($) =>
      backquoteEscapedPairRun($, $.escaped_character),

    _backquote_double_quote_content_escape_run: ($) =>
      backquoteContentEscapeRun(
        $,
        $.double_quote_escape,
        $.double_quote_text,
        choice(
          alias($._backquote_double_quote_escape_tail, $.double_quote_escape),
          alias($._backquote_double_quote_text_tail, $.double_quote_text),
        ),
      ),

    _backquote_double_quote_escape_tail: (_) =>
      token.immediate(seq("\\", choice("$", "`", '"'))),

    _backquote_double_quote_text_tail: (_) =>
      token.immediate(seq("\\", /[^\\\n$`"]/)),

    _backquote_double_quote_escaped_pair_run: ($) =>
      backquoteEscapedPairRun($, $.double_quote_escape),

    _backquote_escaped_pair: (_) => /\\\\/,

    _backquote_escaped_tail: (_) => token(prec(1, seq("\\", /[^\\\n]/))),

    _backquote_escaped_ordinary: (_) => token.immediate(/[^\\\n$`]/),

    single_quoted: ($) => seq("'", optional($.single_quote_content), "'"),

    single_quote_content: ($) =>
      repeat1(choice(token.immediate(/[^'\n]+/), $._newline)),

    double_quoted: ($) =>
      seq(
        $._double_quote_delimiter,
        repeat(doubleQuotedPart($)),
        $._double_quote_delimiter,
      ),

    _double_quote_delimiter: ($) =>
      seq(optional(alias($._backquote_quote_prefix, "\\")), '"'),

    double_quote_text: ($) =>
      prec.right(choice($._literal_dollar, $._double_quote_text_chunk)),

    _double_quote_text_chunk: ($) =>
      prec.right(
        repeat1(
          choice(
            token.immediate(prec(-1, /[^"$`\\\n]+/)),
            $._literal_backslash,
          ),
        ),
      ),

    double_quote_escape: (_) =>
      token.immediate(seq("\\", choice("$", "`", '"', "\\"))),

    dollar_single_quoted: ($) =>
      prec(
        2,
        seq(
          choice(
            "$'",
            seq(
              alias($._backquote_dollar_single_quote_prefix, "\\"),
              token.immediate("$'"),
            ),
          ),
          repeat(
            choice(
              $.dollar_single_quote_text,
              $.dollar_single_quote_escape,
              alias(
                $._backquote_dollar_single_quote_text,
                $.dollar_single_quote_text,
              ),
              alias($._newline, $.dollar_single_quote_text),
            ),
          ),
          "'",
        ),
      ),

    dollar_single_quote_text: (_) => token.immediate(prec(-1, /[^'\\\n]+/)),

    parameter_expansion: ($) =>
      parameterExpansion($, $._braced_parameter_expansion),

    _double_quoted_parameter_expansion: ($) =>
      parameterExpansion($, $._double_quoted_braced_parameter_expansion),

    _braced_parameter_expansion: ($) =>
      bracedParameterExpansion($, $._parameter_expansion_tail),

    _double_quoted_braced_parameter_expansion: ($) =>
      bracedParameterExpansion($, $._double_quoted_parameter_expansion_tail),

    _parameter_expansion_tail: ($) =>
      choice("}", $._parameter_operator_expansion_tail),

    _parameter_operator_expansion_tail: ($) =>
      parameterOperatorTail($, $.parameter_word),

    _double_quoted_parameter_expansion_tail: ($) =>
      choice("}", $._double_quoted_parameter_operator_expansion_tail),

    _double_quoted_parameter_operator_expansion_tail: ($) =>
      parameterOperatorTail(
        $,
        alias($._double_quoted_parameter_word, $.parameter_word),
      ),

    _unbraced_parameter: ($) =>
      choice(
        $.variable_name,
        alias($._unbraced_positional_parameter, $.positional_parameter),
        $._special_parameter_except_hash,
        $._special_parameter_hash,
      ),

    _braced_parameter: ($) =>
      choice(
        $.variable_name,
        $.positional_parameter,
        $._special_parameter_except_hash,
      ),

    _length_parameter: ($) =>
      choice($._braced_parameter, $._special_parameter_hash),

    _special_parameter_hash: ($) => alias("#", $.special_parameter),

    // Regex tokens lose to operator strings here, breaking the '${#-}' reading.
    _special_parameter_except_hash: ($) =>
      alias(choice("0", "*", "@", "?", "$", "!", "-"), $.special_parameter),

    _unbraced_positional_parameter: (_) => /[1-9]/,

    positional_parameter: ($) =>
      seq($._braced_positional_parameter_start, $._braced_numeric_parameter),

    _unclassified_numeric_parameter_source: ($) =>
      seq(
        $._braced_parameter_number_start,
        alias($._braced_numeric_parameter, "numeric_parameter_source"),
      ),

    _braced_numeric_parameter: (_) => /[0-9]+/,

    _parameter_length_operator: ($) => alias("#", $.parameter_length_operator),

    _parameter_value_operator: ($) =>
      alias(
        choice(":-", ":=", ":?", ":+", "-", "=", "?", "+"),
        $.parameter_value_operator,
      ),

    _parameter_pattern_operator: ($) =>
      prec(
        2,
        alias(choice("%%", "##", "%", "#"), $.parameter_pattern_operator),
      ),

    parameter_word: ($) => parameterPatternSource($),

    _double_quoted_parameter_word: ($) =>
      repeat1($._double_quoted_parameter_word_part),

    _double_quoted_parameter_word_part: ($) =>
      choice(
        alias($._double_quoted_parameter_text, $.double_quote_text),
        alias($._double_quoted_parameter_escape, $.double_quote_escape),
        $._backquote_double_quoted_parameter_content_escape_run,
        $._backquote_double_quote_escaped_pair_run,
        alias($._newline, $.double_quote_text),
        $.double_quoted,
        alias($._double_quoted_parameter_expansion, $.parameter_expansion),
        $.command_substitution,
        $.arithmetic_expansion,
        alias(
          $._double_quoted_backquote_substitution,
          $.backquote_substitution,
        ),
      ),

    parameter_pattern: ($) => prec.right(1, parameterPatternSource($)),

    _parameter_source_tail: ($) => repeat1($._parameter_pattern_part),

    _parameter_tilde_source: ($) =>
      prec.right(
        choice(
          seq(
            alias($._parameter_tilde_expansion, $.tilde_expansion),
            prec(2, alias($._literal_slash, $.literal)),
            repeat($._parameter_pattern_part),
          ),
          alias($._parameter_terminal_tilde_expansion, $.tilde_expansion),
        ),
      ),

    _parameter_non_slash_part: ($) =>
      choice(
        prec(
          PATTERN_PRECEDENCE.specialElement,
          alias(
            $._parameter_pattern_bracket_expression,
            $.pattern_bracket_source,
          ),
        ),
        alias($._parameter_pattern_literal, $.literal),
        alias($._literal_tilde, $.literal),
        $.pattern_star_source,
        $.pattern_question_source,
        $._word_structured_part,
      ),

    _parameter_pattern_part: ($) =>
      choice(
        $._parameter_non_slash_part,
        $._parameter_incomplete_bracket_literal,
        prec(-1, alias($._literal_slash, $.literal)),
      ),

    _parameter_incomplete_bracket_literal: ($) =>
      incompleteBracketLiteral(
        $,
        $._parameter_bracket_literal_start,
        $._parameter_incomplete_bracket_literal_part,
        $._parameter_bracket_fallback_end,
      ),

    _parameter_incomplete_bracket_literal_part: ($) =>
      incompleteBracketLiteralPart(
        $,
        parameterPatternBracketSources($),
        $._parameter_incomplete_bracket_literal_run,
        [
          $._pattern_special_literal_start,
          alias($._parameter_incomplete_bracket_literal_text, $.literal),
        ],
      ),

    _parameter_special_prefixed_bracket_source: ($) =>
      patternSpecialPrefixedExpression(
        $,
        $._parameter_pattern_special_prefixed_members,
      ),

    _parameter_incomplete_bracket_literal_text: ($) =>
      prec.right(1, repeat1(parameterIncompleteBracketLiteralAtom($))),

    _parameter_incomplete_bracket_literal_run: ($) =>
      incompleteBracketLiteralRun($, parameterIncompleteBracketLiteralAtom($)),

    _parameter_pattern_literal: ($) => prec.right(parameterPlainChunk($)),

    _double_quoted_parameter_text: ($) =>
      prec.right(
        choice(
          $._literal_dollar,
          $._literal_backslash,
          $._double_quoted_parameter_text_chunk,
        ),
      ),

    _double_quoted_parameter_text_chunk: (_) =>
      token.immediate(prec(-1, /[^}"$`\\\n]+/)),

    _double_quoted_parameter_escape: (_) =>
      token.immediate(seq("\\", choice("$", "`", '"', "\\", "}"))),

    _backquote_double_quoted_parameter_content_escape_run: ($) =>
      backquoteContentEscapeRun(
        $,
        $.double_quote_escape,
        $.double_quote_text,
        choice(
          alias(
            $._backquote_double_quoted_parameter_escape_tail,
            $.double_quote_escape,
          ),
          alias(
            $._backquote_double_quoted_parameter_text_tail,
            $.double_quote_text,
          ),
        ),
      ),

    _backquote_double_quoted_parameter_escape_tail: (_) =>
      token.immediate(seq("\\", choice("$", "`", '"', "}"))),

    _backquote_double_quoted_parameter_text_tail: (_) =>
      token.immediate(seq("\\", /[^\\\n$`"}]/)),

    command_substitution: ($) =>
      commandSubstitution($, $._command_or_arithmetic_substitution_start),

    _command_substitution_start: ($) => dollarExpansionStart($, "("),

    _command_or_arithmetic_substitution_start: ($) =>
      prec.right(
        1,
        seq($._command_substitution_start, optional(lineContinuationRun($))),
      ),

    command_substitution_body: ($) =>
      prec.left(substitutionCommandsBody($, $._closing_layout)),

    backquote_substitution: ($) =>
      seq(
        backquoteDelimiter($._backquote_start, $._backquote_start_prefix),
        optional(field("body", $.backquote_substitution_body)),
        backquoteDelimiter($._backquote_end, $._backquote_end_prefix),
      ),

    _double_quoted_backquote_substitution: ($) =>
      seq(
        backquoteDelimiter(
          $._double_quoted_backquote_start,
          $._double_quoted_backquote_start_prefix,
        ),
        optional(field("body", $.backquote_substitution_body)),
        backquoteDelimiter($._backquote_end, $._backquote_end_prefix),
      ),

    backquote_substitution_body: ($) => $._substitution_body,

    _substitution_body: ($) =>
      prec.left(substitutionCommandsBody($, $._horizontal_layout)),

    _arithmetic_expansion_start: ($) =>
      arithmeticExpansionStart($, $._arithmetic_left_parenthesis),

    _arithmetic_dynamic_expansion_start: ($) =>
      arithmeticExpansionStart($, $._arithmetic_dynamic_left_parenthesis),

    arithmetic_expansion: ($) =>
      choice(
        closedArithmeticExpansion(
          $,
          $._arithmetic_expansion_start,
          $._arithmetic_assignment_expression,
          arithmeticClosingLayout($),
        ),
        closedArithmeticExpansion(
          $,
          $._arithmetic_dynamic_expansion_start,
          $.arithmetic_dynamic_expression,
          optional($._arithmetic_layout),
        ),
      ),

    arithmetic_dynamic_expression: ($) =>
      seq(
        repeat(seq($._arithmetic_source_part, optional($._arithmetic_layout))),
        field("runtime_fragment", $._arithmetic_runtime_fragment),
        repeat(
          seq(
            optional($._arithmetic_layout),
            choice($._arithmetic_source_part, $._arithmetic_runtime_fragment),
          ),
        ),
      ),

    _arithmetic_source_part: ($) =>
      choice(
        $.arithmetic_number,
        $.arithmetic_variable,
        $._arithmetic_source_operator,
        $.parenthesized_arithmetic_source,
      ),

    _arithmetic_runtime_fragment: ($) =>
      choice(
        alias($._double_quoted_parameter_expansion, $.parameter_expansion),
        $.command_substitution,
        $.arithmetic_expansion,
        alias(
          $._double_quoted_backquote_substitution,
          $.backquote_substitution,
        ),
        $.parenthesized_arithmetic_dynamic_source,
      ),

    _arithmetic_source_operator: ($) =>
      choice(
        $._arithmetic_assignment_operator,
        ...arithmeticBinaryLevelSymbols($, "operator"),
        alias(choice("!", "~", "?", ":"), $.arithmetic_operator),
      ),

    parenthesized_arithmetic_source: ($) =>
      seq(
        "(",
        optional($._arithmetic_layout),
        repeat(seq($._arithmetic_source_part, optional($._arithmetic_layout))),
        ")",
      ),

    parenthesized_arithmetic_dynamic_source: ($) =>
      seq(
        "(",
        optional($._arithmetic_layout),
        field("expression", $.arithmetic_dynamic_expression),
        optional($._arithmetic_layout),
        ")",
      ),

    _arithmetic_assignment_expression: ($) =>
      choice(
        $.arithmetic_assignment_expression,
        $._arithmetic_conditional_expression,
      ),

    arithmetic_assignment_expression: ($) =>
      prec.right(
        seq(
          field("left", arithmeticLvalue($)),
          $._arithmetic_assignment_operator_segment,
          arithmeticOperandLayout($),
          field(
            "right",
            choice(
              $.arithmetic_assignment_expression,
              $._arithmetic_conditional_expression,
            ),
          ),
        ),
      ),

    _parenthesized_arithmetic_lvalue: ($) =>
      parenthesizedArithmetic($, arithmeticLvalue($)),

    _arithmetic_conditional_expression: ($) =>
      prec.right(
        choice(
          $.arithmetic_conditional_expression,
          $._arithmetic_logical_or_expression,
        ),
      ),

    arithmetic_conditional_expression: ($) =>
      prec.right(
        seq(
          field("condition", $._arithmetic_logical_or_expression),
          $._arithmetic_question_operator_segment,
          arithmeticOperandLayout($),
          field("consequence", $._arithmetic_assignment_expression),
          $._arithmetic_colon_operator_segment,
          arithmeticOperandLayout($),
          field("alternative", $._arithmetic_conditional_expression),
        ),
      ),

    ...arithmeticBinaryLevelRules(),

    _arithmetic_unary_expression: ($) =>
      choice($.arithmetic_unary_expression, $._arithmetic_primary_expression),

    arithmetic_unary_expression: ($) =>
      choice(
        arithmeticUnaryExpression($, "+"),
        arithmeticUnaryExpression($, "-"),
        arithmeticUnaryExpression($, choice("!", "~")),
      ),

    _arithmetic_primary_expression: ($) =>
      choice(
        $.arithmetic_number,
        $.arithmetic_variable,
        alias($._double_quoted_parameter_expansion, $.parameter_expansion),
        $.command_substitution,
        $.arithmetic_expansion,
        alias(
          $._double_quoted_backquote_substitution,
          $.backquote_substitution,
        ),
        $.parenthesized_arithmetic,
      ),

    parenthesized_arithmetic: ($) =>
      parenthesizedArithmetic($, $._arithmetic_assignment_expression),

    arithmetic_number: (_) =>
      choice(
        token(prec(2, /0[xX][0-9A-Fa-f]+/)),
        token(/[1-9][0-9]*/),
        token(/0[0-7]*/),
      ),

    arithmetic_variable: ($) => $._name_token,

    _arithmetic_assignment_operator_segment: ($) =>
      arithmeticOperatorSegment(
        $,
        $._arithmetic_assignment_operator_boundary,
        $._arithmetic_assignment_operator,
      ),

    _arithmetic_question_operator_segment: ($) =>
      arithmeticOperatorSegment(
        $,
        $._arithmetic_question_operator_boundary,
        alias("?", $.arithmetic_operator),
      ),

    _arithmetic_colon_operator_segment: ($) =>
      arithmeticOperatorSegment(
        $,
        $._arithmetic_colon_operator_boundary,
        alias(":", $.arithmetic_operator),
      ),

    ...arithmeticBinaryOperatorSegmentRules(),

    _arithmetic_assignment_operator: ($) =>
      alias(
        choice(
          "<<=",
          ">>=",
          "*=",
          "/=",
          "%=",
          "+=",
          "-=",
          "&=",
          "^=",
          "|=",
          "=",
        ),
        $.arithmetic_operator,
      ),

    ...arithmeticBinaryOperatorRules(),

    _arithmetic_layout: ($) => repeat1(choice($._blank, $._newline)),

    _literal_dollar: (_) => /\$/,

    _literal_backslash: (_) => /\\/,

    _literal_token: (_) => token(prec(-1, LITERAL_TOKEN_PATTERN)),

    _literal_left_bracket: (_) => /\[/,

    _literal_right_bracket: (_) => /\]/,

    _literal_tilde: (_) => /~/,

    _literal_slash: (_) => /\//,

    _literal_colon: (_) => /:/,

    _literal_equals: (_) => /=/,

    _parameter_pattern_text_token: (_) =>
      token.immediate(prec(-1, PARAMETER_PATTERN_TEXT_PATTERN)),

    _pattern_initial_right_bracket: (_) => token.immediate(/\]/),

    _pattern_bracket_left: (_) => token.immediate(/\[/),

    _pattern_special_literal_start: ($) =>
      seq(
        $._pattern_special_literal_left,
        alias($._pattern_special_marker_character, $.literal),
      ),

    _pattern_special_literal_left: ($) =>
      alias($._pattern_special_left_bracket, $.literal),

    _pattern_special_plain_character: (_) =>
      token.immediate(prec(-2, PATTERN_SPECIAL_PLAIN_CHARACTER_PATTERN)),

    _parameter_pattern_special_plain_character: (_) =>
      token.immediate(
        prec(-2, PARAMETER_PATTERN_SPECIAL_PLAIN_CHARACTER_PATTERN),
      ),

    _pattern_special_marker_character: ($) =>
      choice(
        $._pattern_character_class_colon,
        $._pattern_collating_dot,
        $._pattern_equivalence_equals,
      ),

    _pattern_bracket_exclamation: (_) => token.immediate(/!/),

    _name_token: (_) => token(prec(1, /[A-Za-z_][A-Za-z0-9_]*/)),

    newline_list: ($) => prec.right(repeat1(choice(...newlineListElements($)))),

    _separator_led_newline_list: ($) =>
      ledNewlineList($, seq($._separator_newline, $._layout_newline)),

    _here_document_led_newline_list: ($) =>
      ledNewlineList($, $.here_document_sequence),

    linebreak: ($) => $.newline_list,

    _horizontal_layout: ($) =>
      prec.right(
        1,
        seq(
          optional($._layout_begin),
          repeat1(choice($._blank, prec(2, $._line_continuation))),
        ),
      ),

    _closing_layout: ($) =>
      prec.right(
        1,
        choice(
          seq(
            $._blank,
            repeat(choice($._blank, prec(2, $._line_continuation))),
          ),
          seq($._trailing_continuation_begin, $._continuation_led_run),
        ),
      ),

    _word_separator: ($) => wordSeparator($, $._word_separator_begin),

    _assignment_separator: ($) =>
      wordSeparator($, $._assignment_separator_begin),

    _redirect_separator: ($) => wordSeparator($, $._redirect_separator_begin),

    _free_trailing_layout: ($) =>
      prec.right(1, choice($._closing_layout, trailingComment($))),

    _comment_line: ($) =>
      seq(boundaryLineComment($, $.comment), $._comment_line_end),

    _continued_blank_line: ($) =>
      prec.dynamic(
        2,
        seq(
          $._pre_newline_blank,
          optional($._blank),
          $._continuation_led_run,
          $._layout_newline,
        ),
      ),

    _blank_line: ($) => seq($._pre_newline_blank, $._layout_newline),

    _layout_newline: (_) => "\n",

    _continuation_led_run: ($) =>
      prec.right(
        1,
        seq(
          $._line_continuation,
          repeat(choice($._blank, $._line_continuation)),
        ),
      ),

    _blank: (_) => /[ \t]+/,
  },
});
