// Tree-sitter's regex dialect requires an escaped left bracket inside a
// character class, which Biome strips from regex literals, so these two
// patterns stay strings.
const LITERAL_TOKEN_PATTERN_SOURCE = "[^ \\t\\n;&|<>()/\\\\'\"$`*?\\[\\]~:#=]+";
const PARAMETER_PATTERN_TEXT_PATTERN_SOURCE = "[^}\\n/'\"$`\\\\*?\\[\\]~]+";
const LITERAL_TOKEN_PATTERN = RegExp(LITERAL_TOKEN_PATTERN_SOURCE);
const PARAMETER_PATTERN_TEXT_PATTERN = RegExp(
  PARAMETER_PATTERN_TEXT_PATTERN_SOURCE,
);

const PATTERN_SPECIAL_PLAIN_CHARACTER_PATTERN = /[^ \t\n;&|<>()\\'"$`:.=\]-]/;
const PARAMETER_PATTERN_SPECIAL_PLAIN_CHARACTER_PATTERN =
  /[^ \t\n;&|<>()\\'"$`:.=\]}-]/;
const PARAMETER_DEFERRED_EXTRA_CHARACTER_PATTERN = /[ \t\n;&|<>()]/;
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

const patternRangeEndpointStructuredSourceParts = ($) =>
  structuredSourcePartsWithBackquoteRuns($, [
    $._backquote_single_escaped_pair_run,
  ]);

// The markers keep a run atomic; consuming one pair first can change how the
// remaining suffix folds through the enclosing backquotes.
const backquoteContentEscapeRun = ($, escapeName, dollar) =>
  seq(
    $._backquote_content_run_begin,
    repeat(alias($._backquote_escaped_pair, escapeName)),
    choice(alias($._backquote_escaped_tail, escapeName), dollar),
  );

const backquoteEscapedPairRun = ($, pair) =>
  seq(
    $._backquote_pair_run_begin,
    repeat1(alias($._backquote_escaped_pair, pair)),
    $._backquote_pair_run_end,
  );

const backquoteSingleEscapedPairRun = ($, pair) =>
  prec(
    1,
    seq(
      $._backquote_pair_run_begin,
      alias($._backquote_escaped_pair, pair),
      $._backquote_pair_run_end,
    ),
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

const bracketLiteralRun = ($, characterToken) =>
  repeat1(
    choice(
      characterToken,
      $._pattern_bracket_hyphen_token,
      $._pattern_bracket_left,
      $._pattern_bracket_exclamation,
      $._pattern_character_class_colon,
      $._pattern_collating_dot,
      $._pattern_equivalence_equals,
    ),
  );

const bracketLiteralFallback = ($, part, end) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.literalFallback,
    prec.right(
      1,
      seq(
        alias($._literal_left_bracket, $.literal),
        optional(alias($._pattern_initial_right_bracket, $.literal)),
        repeat1(part),
        choice(end, alias($._literal_right_bracket, $.literal)),
      ),
    ),
  );

const patternBracketCharacter = ($, characterToken) =>
  choice(
    characterToken,
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
    "$",
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
    "$",
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
    $.line_continuation,
    ...structuredSourceParts($),
  );

const wordSeparator = ($, marker) =>
  prec.right(2, seq(marker, repeat(choice($._blank, $.line_continuation))));

const lineContinuationRun = ($) => prec.right(1, repeat1($.line_continuation));

const parameterBraceClose = ($) => choice("}", $._here_document_boundary);

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
  seq(
    arithmeticBoundaryLayout($, boundary),
    field("operator", alias(operator, $.arithmetic_operator)),
  );

const arithmeticOperandLayout = (
  $,
  boundary = $._arithmetic_operand_boundary,
) => arithmeticBoundaryLayout($, boundary);

const arithmeticUnaryExpression = (
  $,
  operator,
  boundary = $._arithmetic_operand_boundary,
) =>
  prec.right(
    seq(
      field("operator", alias(operator, $.arithmetic_operator)),
      arithmeticOperandLayout($, boundary),
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
    rules[`_arithmetic_${level}_operator`] = (_) =>
      operators.length === 1 ? operators[0] : choice(...operators);
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

const continuedBlankLineLayout = ($) =>
  seq(optional($._pre_newline_blank), $._continuation_led_run);

const arithmeticExpansionEnd = ($) =>
  choice(
    seq(")", $._arithmetic_second_right_parenthesis),
    $._here_document_boundary,
  );

const arithmeticExpansionStart = ($, marker) =>
  seq($._command_or_arithmetic_substitution_start, marker, "(");

// The scanner boundary prevents layout from choosing a flat reading during
// the structured reading's final reduce-versus-shift decision.
const closedArithmeticExpansion = ($, start, expression, closing) =>
  seq(
    start,
    optional($._arithmetic_layout),
    choice(
      seq(field("expression", expression), closing, arithmeticExpansionEnd($)),
      $._here_document_boundary,
    ),
  );

const linebreakLayout = ($) =>
  seq(optional($.linebreak), optional($._horizontal_layout));

const separatorOperatorLayout = ($, operator) =>
  seq(
    operator,
    repeat(prec(2, $.line_continuation)),
    optional(field("linebreak", $.linebreak)),
  );

const reservedWordLinebreak = ($) =>
  choice(
    seq($.linebreak, optional($._horizontal_layout)),
    $._horizontal_layout,
  );

const continuationBoundaryLayout = ($, marker) =>
  prec.right(
    1,
    seq(
      optional(lineContinuationRun($)),
      marker,
      optional($._horizontal_layout),
    ),
  );

const patternBoundaryLayout = ($) =>
  continuationBoundaryLayout($, $._pattern_continuation);

const patternClosingLayout = ($) =>
  continuationBoundaryLayout($, $._pattern_end);

const lineComment = ($, comment) =>
  seq(continuationBoundaryLayout($, $._comment_boundary), comment);

const newlineListElements = ($) => [
  $.here_document_sequence,
  $._layout_newline,
  $._blank_line,
  $._continued_blank_line,
  $._comment_line,
];

const ledNewlineList = ($, lead) =>
  prec.right(
    seq(
      lead,
      repeat(choice(...newlineListElements($))),
      optional($._continuation_led_run),
    ),
  );

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
    $.line_continuation,
    alias($._newline, $.double_quote_text),
    alias($._double_quoted_parameter_expansion, $.parameter_expansion),
    $.command_substitution,
    $.arithmetic_expansion,
    $.backquote_substitution,
  );

const completeCommandsTail = ($) =>
  seq(
    $.complete_commands,
    optional(alias($._trailing_linebreak, $.linebreak)),
    optional($._free_trailing_layout),
  );

// Delay the body-versus-closer decision until after the blank run.
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

const dollarExpansionPrefix = ($) =>
  choice(seq($._dollar_expansion_start, "$"), backquoteDollar($));

const dollarExpansionStart = ($, delimiter) =>
  seq(dollarExpansionPrefix($), delimiter);

const backquoteDelimiter = (plain, prefix) =>
  choice(alias(plain, "`"), seq(alias(prefix, "\\"), token.immediate("`")));

// The marker commits the `$(` reading and opens its scanner nesting level.
const commandSubstitution = ($, start) =>
  seq(
    start,
    $._command_substitution_body_begin,
    optional(field("body", $.command_substitution_body)),
    choice(
      alias($._command_substitution_close, ")"),
      $._here_document_boundary,
    ),
  );

const patternSpecialStart = ($, marker) =>
  seq(alias($._pattern_special_left_bracket, "["), marker);

const patternSpecialEnd = ($, marker) =>
  seq(marker, repeat($.line_continuation), $._literal_right_bracket);

const patternSpecialClassSource = ($, marker, characterSource) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.specialElement,
    seq(
      patternSpecialStart($, marker),
      repeat1(
        field(
          "value",
          choice(
            characterSource,
            $.line_continuation,
            ...structuredSourceParts($),
          ),
        ),
      ),
      patternSpecialEnd($, marker),
    ),
  );

const patternCharacterClassStructuredContent = ($) =>
  choice($.line_continuation, ...structuredSourceParts($));

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
      patternSpecialStart($, $._pattern_character_class_colon),
      patternCharacterClassBody($, content),
      patternSpecialEnd($, $._pattern_character_class_colon),
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

const patternBracketListTail = ($, member) =>
  seq(
    repeat(seq(repeat($.line_continuation), field("member", member))),
    optional(lineContinuationRun($)),
  );

const patternSpecialPrefixedList = ($, member, initialRange) =>
  prec.right(
    choice(
      seq(
        field("member", alias(initialRange, $.pattern_bracket_range_source)),
        patternBracketListTail($, member),
      ),
      seq(
        field(
          "member",
          alias(
            $._pattern_special_marker_character,
            $.pattern_bracket_character_source,
          ),
        ),
        patternBracketListTail($, member),
      ),
    ),
  );

const patternSpecialPrefixedExpression = ($, list) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.expression,
    seq(
      alias($._pattern_special_left_bracket, "["),
      repeat($.line_continuation),
      field("members", alias(list, $.pattern_bracket_members_source)),
      $._literal_right_bracket,
    ),
  );

const patternDeferredBracketRangeEndpoint = ($, character, collatingSymbol) =>
  choice(
    alias(character, $.pattern_bracket_character_source),
    collatingSymbol,
    $.pattern_bracket_hyphen_source,
    ...patternRangeEndpointStructuredSourceParts($),
  );

const patternDeferredBracketMember = ($, character, range, specialSources) =>
  choice(
    ...specialSources,
    alias(range, $.pattern_bracket_range_source),
    alias(character, $.pattern_bracket_character_source),
    $.pattern_bracket_hyphen_source,
    ...structuredSourceParts($),
  );

const parameterExpansion = ($, bracedExpansion) =>
  prec(
    1,
    choice(
      seq(dollarExpansionPrefix($), field("parameter", $._unbraced_parameter)),
      seq(
        dollarExpansionStart($, "{"),
        repeat($.line_continuation),
        choice($._here_document_boundary, bracedExpansion),
      ),
    ),
  );

const bracedParameterSource = ($, classifiedParameter) =>
  choice(
    field("parameter", classifiedParameter),
    $._unclassified_numeric_parameter_source,
  );

const bracedParameterExpansion = ($, tail) =>
  choice(
    seq(
      bracedParameterSource($, $._braced_parameter),
      repeat($.line_continuation),
      tail,
    ),
    prec.dynamic(
      2,
      seq(
        field(
          "parameter",
          alias($._special_parameter_hash, $.special_parameter),
        ),
        repeat($.line_continuation),
        tail,
      ),
    ),
    prec.dynamic(
      3,
      seq(
        field("operator", $.parameter_length_operator),
        repeat($.line_continuation),
        bracedParameterSource($, $._length_parameter),
        repeat($.line_continuation),
        parameterBraceClose($),
      ),
    ),
  );

const parameterTailWordSlot = ($, word, wordTrailingContinuations) =>
  optional(
    wordTrailingContinuations
      ? seq(field("word", word), repeat($.line_continuation))
      : field("word", word),
  );

const parameterTailPatternSlot = ($) =>
  optional(
    seq(field("pattern", $.parameter_pattern), repeat($.line_continuation)),
  );

const parameterOperatorTail = ($, word, wordTrailingContinuations) =>
  choice(
    seq(
      field("operator", $.parameter_value_operator),
      repeat($.line_continuation),
      parameterTailWordSlot($, word, wordTrailingContinuations),
      parameterBraceClose($),
    ),
    seq(
      field("operator", $.parameter_pattern_operator),
      repeat($.line_continuation),
      parameterTailPatternSlot($),
      parameterBraceClose($),
    ),
  );

const parameterExpansionTail = ($, operatorTail) =>
  choice(parameterBraceClose($), operatorTail);

const parameterPatternSource = ($) =>
  choice(
    $._parameter_tilde_source,
    seq($._parameter_pattern_part, optional($._parameter_source_tail)),
  );

const caseClauseItems = ($) =>
  choice(
    $.esac_keyword,
    seq(field("items", $.case_list), $.esac_keyword),
    seq(field("items", $.case_list_ns), $.esac_keyword),
  );

const compoundListField = ($, name) => field(name, $.compound_list);

const conditionalThenBranch = ($, keyword, tail) =>
  seq(
    keyword,
    compoundListField($, "condition"),
    optional($._closing_layout),
    $.then_keyword,
    compoundListField($, "consequence"),
    optional($._closing_layout),
    tail,
  );

const ifClause = ($) =>
  conditionalThenBranch(
    $,
    $.if_keyword,
    choice(
      $.fi_keyword,
      seq(
        field("alternative", $.else_part),
        optional($._closing_layout),
        $.fi_keyword,
      ),
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
      repeat($.line_continuation),
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
      $._name_token,
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
      "$",
      $._name_token,
      $._literal_token,
      $._literal_right_bracket,
      $._literal_equals,
    ),
  );

const parameterPlainChunk = ($) =>
  plainChunk(
    choice(
      "$",
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

const patternBracketExpression = ($, list, opener = $._literal_left_bracket) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.expression,
    prec.right(
      2,
      seq(
        opener,
        repeat($.line_continuation),
        choice(
          seq(
            field("negation", $.pattern_bracket_negation_source),
            repeat($.line_continuation),
            field("members", list),
          ),
          field("members", list),
        ),
        $._literal_right_bracket,
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
        patternBracketListTail($, member),
      ),
      seq(field("member", member), patternBracketListTail($, member)),
    ),
  );

const patternBracketRange = ($, endpoint, start = endpoint) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.range,
    seq(
      field("start", start),
      repeat($.line_continuation),
      field("operator", $.pattern_bracket_range_operator_source),
      repeat($.line_continuation),
      field("end", endpoint),
    ),
  );

const patternInitialBracketRange = ($, endpoint) =>
  patternBracketRange(
    $,
    endpoint,
    alias($._pattern_initial_right_bracket, $.pattern_bracket_character_source),
  );

module.exports = grammar({
  name: "sh",

  extras: ($) => [$._here_document_content_line_start],

  externals: ($) => [
    $._left_brace,
    $._right_brace,
    $._io_number_token,
    $._io_location_token,
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
    $._quoted_here_document_end,
    $._here_document_end_begin,
    $._here_document_end_commit,
    $._here_document_sequence_end,
    $._here_document_content_line_start,
    $._newline,
    $.line_continuation,
    $._arithmetic_assignment_operator_boundary,
    $._arithmetic_question_operator_boundary,
    $._arithmetic_colon_operator_boundary,
    ...arithmeticBinaryLevelSymbols($, "operator_boundary"),
    $._arithmetic_plus_operand_boundary,
    $._arithmetic_minus_operand_boundary,
    $._arithmetic_operand_boundary,
    $._arithmetic_closing_boundary,
    $._arithmetic_left_parenthesis,
    $._arithmetic_dynamic_left_parenthesis,
    $._pattern_special_left_bracket,
    $._literal_hash,
    $._comment_boundary,
    $._trailing_comment_boundary,
    $.comment,
    $._comment_line_end,
    $._here_document_boundary,
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
    $._case_item_ns_boundary,
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
    $._name_equals_begin,
    $._fname_begin,
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
  ],

  conflicts: ($) => [
    [$.term],
    [$.compound_list],
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
    [
      $._word_bracket_literal_fallback_part,
      $._pattern_operator_bracket_character,
    ],
    [
      $._parameter_bracket_literal_fallback_part,
      $._pattern_operator_bracket_character,
    ],
    [$.pattern_character_class_source, $._pattern_special_marker_character],
    [$.pattern_collating_symbol_source, $._pattern_special_marker_character],
    [$.pattern_equivalence_class_source, $._pattern_special_marker_character],
    [
      $._parameter_pattern_character_class_source,
      $._pattern_special_marker_character,
    ],
    [
      $._parameter_pattern_collating_symbol_source,
      $._pattern_special_marker_character,
    ],
    [
      $._parameter_pattern_equivalence_class_source,
      $._pattern_special_marker_character,
    ],
    [$._word_special_prefixed_bracket_source, $._pattern_special_literal_left],
    [
      $._parameter_special_prefixed_bracket_source,
      $._pattern_special_literal_left,
    ],
    [
      $._word_special_prefixed_bracket_source,
      $.pattern_character_class_source,
      $._pattern_special_literal_left,
    ],
    [
      $._word_special_prefixed_bracket_source,
      $.pattern_collating_symbol_source,
      $._pattern_special_literal_left,
    ],
    [
      $._word_special_prefixed_bracket_source,
      $.pattern_equivalence_class_source,
      $._pattern_special_literal_left,
    ],
    [
      $._parameter_special_prefixed_bracket_source,
      $._parameter_pattern_character_class_source,
      $._pattern_special_literal_left,
    ],
    [
      $._parameter_special_prefixed_bracket_source,
      $._parameter_pattern_collating_symbol_source,
      $._pattern_special_literal_left,
    ],
    [
      $._parameter_special_prefixed_bracket_source,
      $._parameter_pattern_equivalence_class_source,
      $._pattern_special_literal_left,
    ],
    [$._word_bracket_literal_fallback_part, $.pattern_bracket_source],
    [$._word_bracket_literal_fallback_part, $.pattern_bracket_character_source],
    [$._word_bracket_literal_fallback_part, $.pattern_bracket_hyphen_source],
    [
      $._word_bracket_literal_fallback_part,
      $.pattern_bracket_range_operator_source,
      $.pattern_bracket_hyphen_source,
    ],
    [$._word_bracket_literal_fallback_part, $._pattern_bracket_member],
    [
      $._word_bracket_literal_fallback_part,
      $.pattern_bracket_members_source,
      $._pattern_initial_bracket_range,
    ],
    [
      $._word_bracket_literal_fallback_part,
      $._pattern_bracket_member,
      $._pattern_bracket_range_endpoint,
    ],
    [
      $._parameter_pattern_bracket_member,
      $._parameter_pattern_bracket_range_endpoint,
    ],
    [
      $._parameter_bracket_literal_fallback_part,
      $._parameter_pattern_bracket_expression,
    ],
    [
      $._parameter_bracket_literal_fallback_part,
      $._parameter_pattern_bracket_character,
    ],
    [
      $._parameter_bracket_literal_fallback_part,
      $.pattern_bracket_hyphen_source,
    ],
    [
      $._parameter_bracket_literal_fallback_part,
      $.pattern_bracket_range_operator_source,
      $.pattern_bracket_hyphen_source,
    ],
    [
      $._parameter_bracket_literal_fallback_part,
      $._parameter_pattern_bracket_member,
    ],
    [
      $._parameter_pattern_bracket_list,
      $._parameter_pattern_initial_bracket_range,
      $._parameter_bracket_literal_fallback_part,
    ],
    [
      $._parameter_bracket_literal_fallback_part,
      $._parameter_pattern_bracket_member,
      $._parameter_pattern_bracket_range_endpoint,
    ],
    [$.pattern_bracket_negation_source, $.pattern_bracket_character_source],
    [
      $._word_bracket_literal_fallback_part,
      $.pattern_bracket_negation_source,
      $.pattern_bracket_character_source,
    ],
    [$.pattern_bracket_negation_source, $._parameter_pattern_bracket_character],
    [
      $._parameter_bracket_literal_fallback_part,
      $.pattern_bracket_negation_source,
      $._parameter_pattern_bracket_character,
    ],
    [$.pattern_bracket_range_operator_source, $.pattern_bracket_hyphen_source],
    [$._special_parameter_hash, $.parameter_length_operator],
    [$._parenthesized_arithmetic_lvalue, $._arithmetic_primary_expression],
    [$.arithmetic_dynamic_expression],
  ],

  rules: {
    program: ($) =>
      choice(
        seq(
          optional(field("leading", $.linebreak)),
          optional($._horizontal_layout),
          field("commands", $.complete_commands),
          optional(
            field("trailing", alias($._trailing_linebreak, $.linebreak)),
          ),
          optional($._free_trailing_layout),
        ),
        seq(
          optional(field("leading", $.linebreak)),
          optional($._horizontal_layout),
          optional(trailingComment($)),
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
      ),

    complete_command: ($) =>
      seq(
        field("body", $.list),
        optional(
          seq(
            $._terminator_ahead,
            optional($._horizontal_layout),
            field("terminator", $.separator_op),
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
            field("operator", choice($.and_if, $.or_if)),
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

    _trailing_newline_separator: ($) =>
      field("newlines", alias($._trailing_newline_list, $.newline_list)),

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

    // No position derives these two rules directly: every use aliases a
    // variant-specific hidden rule to them, and the DSL requires an alias
    // target to be a defined rule.
    sequential_sep: ($) =>
      choice($._sequential_operator_separator, $._sequential_newline_separator),

    separator: ($) => choice($._operator_separator, $._newline_separator),

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
      ),

    compound_list: ($) =>
      seq(
        optional(field("leading", $.linebreak)),
        optional($._horizontal_layout),
        field("body", $.term),
        optional(
          choice(
            seq(
              $._terminator_ahead,
              optional($._horizontal_layout),
              field("terminator", alias($._operator_separator, $.separator)),
            ),
            seq(
              optional($._closing_layout),
              field(
                "terminator",
                alias($._here_document_led_operator_separator, $.separator),
              ),
            ),
            field(
              "terminator",
              alias($._trailing_newline_separator, $.separator),
            ),
          ),
        ),
      ),

    and_if: (_) => "&&",

    or_if: (_) => "||",

    bang: ($) => $._bang_token,

    if_keyword: ($) => seq($._if_keyword, "if"),

    then_keyword: ($) => seq($._then_keyword, "then"),

    elif_keyword: ($) => seq($._elif_keyword, "elif"),

    else_keyword: ($) => seq($._else_keyword, "else"),

    fi_keyword: ($) => seq($._fi_keyword, "fi"),

    for_keyword: ($) => seq($._for_keyword, "for"),

    in_keyword: ($) => seq($._in_keyword, "in"),

    do_keyword: ($) => seq($._do_keyword, "do"),

    done_keyword: ($) => seq($._done_keyword, "done"),

    case_keyword: ($) => seq($._case_keyword, "case"),

    esac_keyword: ($) => seq($._esac_keyword, "esac"),

    while_keyword: ($) => seq($._while_keyword, "while"),

    until_keyword: ($) => seq($._until_keyword, "until"),

    function_definition: ($) => functionDefinitionWithBody($, $.function_body),

    function_body: ($) => $._redirectable_compound_command,

    fname: ($) => seq($._fname_begin, $._name_token),

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
        alias($._subshell_close, ")"),
      ),

    for_clause: ($) =>
      seq($.for_keyword, $._word_separator, field("name", $.name), forTail($)),

    in: ($) => $.in_keyword,

    wordlist: ($) =>
      prec.right(
        seq(
          field("word", $.word),
          repeat(
            choice(
              seq($._word_separator, field("word", $.word)),
              prec(1, $.line_continuation),
            ),
          ),
        ),
      ),

    do_group: ($) =>
      seq(
        $.do_keyword,
        compoundListField($, "body"),
        optional($._closing_layout),
        prec(20, $.done_keyword),
      ),

    if_clause: ($) => prec.right(ifClause($)),

    else_part: ($) =>
      prec.right(
        choice(
          conditionalThenBranch(
            $,
            $.elif_keyword,
            optional(field("alternative", $.else_part)),
          ),
          seq($.else_keyword, compoundListField($, "body")),
        ),
      ),

    while_clause: ($) => loopClause($, $.while_keyword),

    until_clause: ($) => loopClause($, $.until_keyword),

    case_clause: ($) =>
      seq(
        $.case_keyword,
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
        patternClosingLayout($),
        ")",
        choice(
          seq(
            optional($.linebreak),
            optional($._closing_layout),
            $._case_item_ns_boundary,
          ),
          prec.dynamic(
            10,
            seq(
              field("body", $.compound_list),
              optional($._closing_layout),
              $._case_item_ns_boundary,
            ),
          ),
        ),
      ),

    case_item: ($) =>
      seq(
        field("patterns", $.pattern_list),
        patternClosingLayout($),
        ")",
        choice(
          linebreakLayout($),
          prec.dynamic(10, field("body", $.compound_list)),
        ),
        seq($._case_item_end, optional($._horizontal_layout)),
        field("terminator", choice($.dsemi, $.semi_and)),
        optional(reservedWordLinebreak($)),
      ),

    pattern_list: ($) =>
      prec.dynamic(
        2,
        prec.right(
          1,
          seq(
            optional(seq("(", optional($._horizontal_layout))),
            field("word", $.word),
            repeat(
              seq(
                patternBoundaryLayout($),
                "|",
                optional($._horizontal_layout),
                field("word", $.word),
              ),
            ),
          ),
        ),
      ),

    dsemi: (_) => prec(10, ";;"),

    semi_and: (_) => prec(10, ";&"),

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
            choice(
              field("number", $.io_number),
              field("location", $.io_location),
            ),
            repeat($.line_continuation),
            field("body", choice($.io_file, $.io_here)),
          ),
        ),
        field("body", choice($.io_file, $.io_here)),
      ),

    _io_redirect_without_descriptor: ($) =>
      field("body", choice($.io_file, $.io_here)),

    io_number: ($) => seq($._io_number_token, /[0-9]+/),

    io_location: ($) => $._io_location_token,

    io_file: ($) =>
      seq(
        field(
          "operator",
          choice(
            "<",
            ">",
            $.lessand,
            $.greatand,
            $.dgreat,
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

    dgreat: (_) => ">>",

    lessgreat: (_) => "<>",

    clobber: (_) => ">|",

    io_here: ($) =>
      seq(
        field("operator", choice($.dless, $.dlessdash)),
        optional($._horizontal_layout),
        field("end", $.here_end),
      ),

    dless: ($) => seq("<<", $._dless_commit),

    dlessdash: ($) => seq("<<-", $._dlessdash_commit),

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
            seq($._pre_newline_blank, optional($._continuation_led_run)),
            seq($._trailing_continuation_begin, $._continuation_led_run),
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
      choice(
        $._quoted_here_document_end,
        seq(
          $._here_document_end_begin,
          repeat1(choice($._here_document_end_text, $.line_continuation)),
          $._here_document_end_commit,
        ),
      ),

    _here_document_end_text: (_) => token.immediate(/[^\\\n]+/),

    here_document_body: ($) =>
      repeat1(
        choice(
          $.here_document_text,
          alias($._here_document_dollar, $.here_document_text),
          alias($._here_document_backslash, $.here_document_text),
          $.here_document_escape,
          $.line_continuation,
          $.parameter_expansion,
          $.command_substitution,
          $.arithmetic_expansion,
          $.backquote_substitution,
          $._newline,
        ),
      ),

    here_document_text: (_) => token.immediate(prec(-1, /[^$`\\\n]+/)),

    _here_document_dollar: (_) => token.immediate(prec(-2, "$")),

    _here_document_backslash: (_) => token.immediate(prec(-2, "\\")),

    here_document_escape: (_) =>
      token.immediate(seq("\\", choice("$", "`", "\\"))),

    quoted_here_document_body: ($) =>
      repeat1(choice($.quoted_here_document_text, $._newline)),

    quoted_here_document_text: (_) => token.immediate(prec(-1, /[^\n]+/)),

    assignment_word: ($) =>
      prec.dynamic(
        ASSIGNMENT_WORD_PRECEDENCE,
        prec.right(
          1,
          seq(
            $._name_equals_begin,
            field("name", $.variable_name),
            repeat($.line_continuation),
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
        $._word_bracket_part,
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
      seq(
        choice(alias($._literal_hash, $.literal), $._word_non_slash_part),
        optional($._source_word_tail),
      ),

    _assignment_tilde_user: ($) =>
      seq(
        $._assignment_non_delimiter_part,
        optional($._assignment_tilde_user_tail),
      ),

    _assignment_tilde_user_tail: ($) =>
      repeat1($._assignment_non_delimiter_part),

    _parameter_tilde_user: ($) =>
      seq($._parameter_non_slash_part, optional($._parameter_source_tail)),

    _word_non_slash_part: ($) =>
      choice(
        $._word_bracket_part,
        $.literal,
        $.pattern_star_source,
        $.pattern_question_source,
        $._word_structured_part,
      ),

    _word_part: ($) =>
      choice(
        $._word_non_slash_part,
        prec(-1, alias($._literal_slash, $.literal)),
      ),

    _word_structured_part: ($) => choice(...structuredSourceParts($)),

    literal: ($) => prec.right(choice("$", wordPlainChunk($))),

    _word_bracket_part: ($) =>
      choice(
        $.pattern_bracket_source,
        $._word_bracket_literal_fallback,
        $._word_incomplete_bracket_literal,
      ),

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

    _word_bracket_literal_fallback: ($) =>
      bracketLiteralFallback(
        $,
        $._word_bracket_literal_fallback_part,
        $._word_bracket_fallback_end,
      ),

    _word_bracket_literal_fallback_part: ($) =>
      prec.right(
        incompleteBracketLiteralPart(
          $,
          wordPatternBracketSources($),
          $._word_bracket_literal_run,
          [$._pattern_special_literal_start],
        ),
      ),

    _word_bracket_literal_run: ($) =>
      bracketLiteralRun($, $._pattern_bracket_character_token),

    pattern_bracket_source: ($) =>
      patternBracketExpression($, $.pattern_bracket_members_source),

    _parameter_pattern_bracket_expression: ($) =>
      patternBracketExpression(
        $,
        alias(
          $._parameter_pattern_bracket_list,
          $.pattern_bracket_members_source,
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
        ...structuredSourceParts($),
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
        ...structuredSourceParts($),
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
        ...patternRangeEndpointStructuredSourceParts($),
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
        ...patternRangeEndpointStructuredSourceParts($),
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
        $._pattern_special_marker_character,
        $._parameter_deferred_extra_character,
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

    _pattern_character_class_colon: (_) => token.immediate(prec(-3, ":")),

    pattern_collating_symbol_source: ($) =>
      patternSpecialClassSource(
        $,
        $._pattern_collating_dot,
        $.pattern_collating_symbol_character_source,
      ),

    _parameter_pattern_collating_symbol_source: ($) =>
      patternSpecialClassSource(
        $,
        $._pattern_collating_dot,
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

    _pattern_collating_dot: (_) => token.immediate(prec(-2, ".")),

    pattern_equivalence_class_source: ($) =>
      patternSpecialClassSource(
        $,
        $._pattern_equivalence_equals,
        $.pattern_equivalence_class_character_source,
      ),

    _parameter_pattern_equivalence_class_source: ($) =>
      patternSpecialClassSource(
        $,
        $._pattern_equivalence_equals,
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

    _pattern_equivalence_equals: (_) => token.immediate(prec(-2, "=")),

    escaped_character: (_) => token(seq("\\", /[^\n]/)),

    _backquote_content_escape_run: ($) =>
      backquoteContentEscapeRun($, $.escaped_character, alias("$", $.literal)),

    _backquote_escaped_pair_run: ($) =>
      backquoteEscapedPairRun($, $.escaped_character),

    _backquote_single_escaped_pair_run: ($) =>
      backquoteSingleEscapedPairRun($, $.escaped_character),

    _backquote_double_quote_content_escape_run: ($) =>
      backquoteContentEscapeRun(
        $,
        $.double_quote_escape,
        alias("$", $.double_quote_text),
      ),

    _backquote_double_quote_escaped_pair_run: ($) =>
      backquoteEscapedPairRun($, $.double_quote_escape),

    _backquote_escaped_pair: (_) => "\\\\",

    _backquote_escaped_tail: (_) => token(prec(1, seq("\\", /[^\\\n]/))),

    single_quoted: ($) =>
      seq(
        "'",
        optional($.single_quote_content),
        choice("'", $._here_document_boundary),
      ),

    single_quote_content: ($) =>
      repeat1(choice(token.immediate(/[^'\n]+/), $._newline)),

    double_quoted: ($) =>
      seq(
        '"',
        repeat(doubleQuotedPart($)),
        choice('"', $._here_document_boundary),
      ),

    double_quote_text: ($) =>
      prec.right(choice("$", $._double_quote_text_chunk)),

    _double_quote_text_chunk: (_) =>
      prec.right(
        repeat1(choice(token.immediate(prec(-1, /[^"$`\\\n]+/)), "\\")),
      ),

    double_quote_escape: (_) =>
      token.immediate(seq("\\", choice("$", "`", '"', "\\"))),

    dollar_single_quoted: ($) =>
      prec(
        2,
        seq(
          "$'",
          repeat(
            choice(
              $.dollar_single_quote_text,
              $.dollar_single_quote_escape,
              alias($._newline, $.dollar_single_quote_text),
            ),
          ),
          choice("'", $._here_document_boundary),
        ),
      ),

    dollar_single_quote_text: (_) => token.immediate(prec(-1, /[^'\\\n]+/)),

    dollar_single_quote_escape: (_) =>
      token.immediate(
        seq(
          "\\",
          choice(
            seq("x", /[0-9A-Fa-f]+/),
            /[0-7]{1,3}/,
            seq("c", choice(seq("\\", "\\"), /[^\\\n]/)),
            /[^\n]/,
            "\n",
          ),
        ),
      ),

    parameter_expansion: ($) =>
      parameterExpansion($, $._braced_parameter_expansion),

    _double_quoted_parameter_expansion: ($) =>
      parameterExpansion($, $._double_quoted_braced_parameter_expansion),

    _braced_parameter_expansion: ($) =>
      bracedParameterExpansion($, $._parameter_expansion_tail),

    _double_quoted_braced_parameter_expansion: ($) =>
      bracedParameterExpansion($, $._double_quoted_parameter_expansion_tail),

    _parameter_expansion_tail: ($) =>
      parameterExpansionTail($, $._parameter_operator_expansion_tail),

    _parameter_operator_expansion_tail: ($) =>
      parameterOperatorTail($, $.parameter_word, true),

    _double_quoted_parameter_expansion_tail: ($) =>
      parameterExpansionTail(
        $,
        $._double_quoted_parameter_operator_expansion_tail,
      ),

    _double_quoted_parameter_operator_expansion_tail: ($) =>
      parameterOperatorTail(
        $,
        alias($._double_quoted_parameter_word, $.parameter_word),
        false,
      ),

    _unbraced_parameter: ($) =>
      choice(
        $.variable_name,
        alias($._unbraced_positional_parameter, $.positional_parameter),
        $.special_parameter,
      ),

    _braced_parameter: ($) =>
      choice(
        $.variable_name,
        $.positional_parameter,
        alias($._special_parameter_except_hash, $.special_parameter),
      ),

    _length_parameter: ($) =>
      choice(
        $._braced_parameter,
        alias($._special_parameter_hash, $.special_parameter),
      ),

    _special_parameter_hash: (_) => "#",

    // Spelled as string tokens: a regex token here loses the lexical
    // preference contest against the operator strings in states where both
    // are valid, which discards the string-length reading of "${#-}".
    _special_parameter_except_hash: (_) =>
      choice("0", "*", "@", "?", "$", "!", "-"),

    special_parameter: ($) =>
      choice($._special_parameter_except_hash, $._special_parameter_hash),

    _unbraced_positional_parameter: (_) => /[1-9]/,

    positional_parameter: ($) =>
      seq($._braced_positional_parameter_start, $._braced_numeric_parameter),

    _unclassified_numeric_parameter_source: ($) =>
      seq($._braced_parameter_number_start, $._braced_numeric_parameter),

    _braced_numeric_parameter: (_) => /[0-9]+/,

    parameter_length_operator: (_) => "#",

    parameter_value_operator: (_) =>
      choice(":-", ":=", ":?", ":+", "-", "=", "?", "+"),

    parameter_pattern_operator: (_) => prec(2, choice("%%", "##", "%", "#")),

    parameter_word: ($) => parameterPatternSource($),

    _double_quoted_parameter_word: ($) =>
      seq(
        $._double_quoted_parameter_word_lead_part,
        repeat($._double_quoted_parameter_word_part),
      ),

    _double_quoted_parameter_word_lead_part: ($) =>
      choice(
        alias($._double_quoted_parameter_text, $.double_quote_text),
        alias($._double_quoted_parameter_escape, $.double_quote_escape),
        $._backquote_double_quote_content_escape_run,
        $._backquote_double_quote_escaped_pair_run,
        alias($._newline, $.double_quote_text),
        $.double_quoted,
        alias($._double_quoted_parameter_expansion, $.parameter_expansion),
        $.command_substitution,
        $.arithmetic_expansion,
        $.backquote_substitution,
      ),

    _double_quoted_parameter_word_part: ($) =>
      choice($._double_quoted_parameter_word_lead_part, $.line_continuation),

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
        $._parameter_pattern_bracket_part,
        alias($._parameter_pattern_literal, $.literal),
        alias($._literal_tilde, $.literal),
        $.pattern_star_source,
        $.pattern_question_source,
        $._word_structured_part,
      ),

    _parameter_pattern_part: ($) =>
      choice(
        $._parameter_non_slash_part,
        prec(-1, alias($._literal_slash, $.literal)),
      ),

    _parameter_pattern_bracket_part: ($) =>
      choice(
        prec(
          PATTERN_PRECEDENCE.specialElement,
          alias(
            $._parameter_pattern_bracket_expression,
            $.pattern_bracket_source,
          ),
        ),
        $._parameter_bracket_literal_fallback,
        $._parameter_incomplete_bracket_literal,
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

    _parameter_bracket_literal_fallback: ($) =>
      bracketLiteralFallback(
        $,
        $._parameter_bracket_literal_fallback_part,
        $._parameter_bracket_fallback_end,
      ),

    _parameter_bracket_literal_fallback_part: ($) =>
      prec.right(
        incompleteBracketLiteralPart(
          $,
          parameterPatternBracketSources($),
          $._parameter_bracket_literal_run,
          [$._pattern_special_literal_start],
        ),
      ),

    _parameter_bracket_literal_run: ($) =>
      bracketLiteralRun($, $._parameter_pattern_bracket_character_token),

    _parameter_pattern_literal: ($) => prec.right(parameterPlainChunk($)),

    _double_quoted_parameter_text: ($) =>
      prec.right(choice("$", "\\", $._double_quoted_parameter_text_chunk)),

    _double_quoted_parameter_text_chunk: (_) =>
      token.immediate(prec(-1, /[^}"$`\\\n]+/)),

    _double_quoted_parameter_escape: (_) =>
      token.immediate(seq("\\", choice("$", "`", '"', "\\", "}"))),

    command_substitution: ($) =>
      commandSubstitution($, $._command_or_arithmetic_substitution_start),

    _command_substitution_start: ($) => dollarExpansionStart($, "("),

    _command_or_arithmetic_substitution_start: ($) =>
      prec.right(
        1,
        seq(
          $._command_substitution_start,
          optional($._command_substitution_initial_continuations),
        ),
      ),

    _command_substitution_initial_continuations: ($) =>
      prec.right(
        1,
        seq(
          $.line_continuation,
          optional($._command_substitution_initial_continuations),
        ),
      ),

    command_substitution_body: ($) =>
      prec.left(substitutionCommandsBody($, $._closing_layout)),

    backquote_substitution: ($) =>
      seq(
        backquoteDelimiter($._backquote_start, $._backquote_start_prefix),
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
        alias($._arithmetic_source_operator, $.arithmetic_operator),
        $.parenthesized_arithmetic_source,
      ),

    _arithmetic_runtime_fragment: ($) =>
      choice(
        alias($._double_quoted_parameter_expansion, $.parameter_expansion),
        $.command_substitution,
        $.arithmetic_expansion,
        $.backquote_substitution,
        $.parenthesized_arithmetic_dynamic_source,
      ),

    _arithmetic_source_operator: ($) =>
      choice(
        $._arithmetic_assignment_operator,
        ...arithmeticBinaryLevelSymbols($, "operator"),
        "!",
        "~",
        "?",
        ":",
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

    _arithmetic_second_right_parenthesis: ($) =>
      choice(")", seq(repeat1($.line_continuation), token.immediate(")"))),

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
        arithmeticUnaryExpression($, "+", $._arithmetic_plus_operand_boundary),
        arithmeticUnaryExpression($, "-", $._arithmetic_minus_operand_boundary),
        arithmeticUnaryExpression($, choice("!", $._bang_token, "~")),
      ),

    _arithmetic_primary_expression: ($) =>
      choice(
        $.arithmetic_number,
        $.arithmetic_variable,
        alias($._double_quoted_parameter_expansion, $.parameter_expansion),
        $.command_substitution,
        $.arithmetic_expansion,
        $.backquote_substitution,
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
        "?",
      ),

    _arithmetic_colon_operator_segment: ($) =>
      arithmeticOperatorSegment($, $._arithmetic_colon_operator_boundary, ":"),

    ...arithmeticBinaryOperatorSegmentRules(),

    _arithmetic_assignment_operator: (_) =>
      choice("<<=", ">>=", "*=", "/=", "%=", "+=", "-=", "&=", "^=", "|=", "="),

    ...arithmeticBinaryOperatorRules(),

    _arithmetic_layout: ($) =>
      repeat1(choice($.line_continuation, $._blank, $._newline)),

    _literal_token: (_) => token(prec(-1, LITERAL_TOKEN_PATTERN)),

    _literal_left_bracket: (_) => "[",

    _literal_right_bracket: (_) => "]",

    _literal_tilde: (_) => "~",

    _literal_slash: (_) => "/",

    _literal_colon: (_) => ":",

    _literal_equals: (_) => "=",

    _parameter_pattern_text_token: (_) =>
      token.immediate(prec(-1, PARAMETER_PATTERN_TEXT_PATTERN)),

    _pattern_initial_right_bracket: (_) => token.immediate("]"),

    _pattern_bracket_left: (_) => token.immediate("["),

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

    _parameter_deferred_extra_character: (_) =>
      token.immediate(prec(-2, PARAMETER_DEFERRED_EXTRA_CHARACTER_PATTERN)),

    _pattern_special_marker_character: ($) =>
      choice(
        $._pattern_character_class_colon,
        $._pattern_collating_dot,
        $._pattern_equivalence_equals,
      ),

    _pattern_bracket_exclamation: (_) => token.immediate("!"),

    _name_token: (_) => token(prec(1, /[A-Za-z_][A-Za-z0-9_]*/)),

    newline_list: ($) =>
      prec.right(
        seq(
          repeat1(choice(...newlineListElements($))),
          optional($._continuation_led_run),
        ),
      ),

    _separator_led_newline_list: ($) =>
      ledNewlineList($, seq($._separator_newline, $._layout_newline)),

    _trailing_newline_list: ($) =>
      ledNewlineList(
        $,
        choice(
          $.here_document_sequence,
          $._layout_newline,
          $._blank_line,
          $._trailing_continued_blank_line,
          seq(boundaryLineComment($, $.comment), $._comment_line_end),
        ),
      ),

    _trailing_continued_blank_line: ($) =>
      seq(
        choice($._pre_newline_blank, $._trailing_continuation_begin),
        $._continuation_led_run,
        $._layout_newline,
      ),

    _trailing_linebreak: ($) => alias($._trailing_newline_list, $.newline_list),

    _here_document_led_newline_list: ($) =>
      ledNewlineList($, $.here_document_sequence),

    linebreak: ($) => $.newline_list,

    _horizontal_layout: ($) =>
      prec.right(1, repeat1(choice($._blank, prec(2, $.line_continuation)))),

    _closing_layout: ($) =>
      prec.right(
        1,
        choice(
          seq($._blank, repeat(choice($._blank, prec(2, $.line_continuation)))),
          seq($._trailing_continuation_begin, $._continuation_led_run),
        ),
      ),

    _word_separator: ($) => wordSeparator($, $._word_separator_begin),

    _assignment_separator: ($) =>
      wordSeparator($, $._assignment_separator_begin),

    _redirect_separator: ($) => wordSeparator($, $._redirect_separator_begin),

    _free_trailing_layout: ($) =>
      prec.right(1, choice($._closing_layout, trailingComment($))),

    // Disjoint lookahead links each comment to the next one or the run horizon.
    _comment_line: ($) => seq($._free_comment, $._comment_line_end),

    _continued_blank_line: ($) =>
      prec.dynamic(2, seq(continuedBlankLineLayout($), $._layout_newline)),

    _blank_line: ($) => seq($._pre_newline_blank, $._layout_newline),

    _free_comment: ($) => prec.right(1, lineComment($, $.comment)),

    _layout_newline: (_) => "\n",

    _continuation_led_run: ($) =>
      prec.right(
        1,
        seq($.line_continuation, repeat(choice($._blank, $.line_continuation))),
      ),

    _blank: (_) => /[ \t]+/,
  },
});
