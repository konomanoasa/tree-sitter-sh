import lexicalTokens from "./src/lexical-tokens.json" with { type: "json" };

if (Object.keys(lexicalTokens).length !== 2) {
  throw new Error("Source tokens must define only lexical and physical groups");
}
const sourceKinds = new Map();
const scannerSymbols = new Set();
const wholeSources = new Set();
for (const kind of ["lexical", "physical"]) {
  if (!Array.isArray(lexicalTokens[kind])) {
    throw new Error(`Missing ${kind} source tokens`);
  }
  for (const { begin, scanner, whole } of lexicalTokens[kind]) {
    if (
      typeof begin !== "string" ||
      !/^_[a-z][a-z0-9_]*$/.test(begin) ||
      typeof scanner !== "string" ||
      !/^[A-Z][A-Z0-9_]*$/.test(scanner) ||
      sourceKinds.has(begin) ||
      scannerSymbols.has(scanner) ||
      (whole !== undefined && (whole !== true || kind !== "lexical"))
    ) {
      throw new Error(`Invalid or duplicate source token ${begin}`);
    }
    sourceKinds.set(begin, kind);
    scannerSymbols.add(scanner);
    if (whole) wholeSources.add(begin);
  }
}
const sourceToken = ($, begin, kind, suffix) => {
  if (
    begin.type !== "SYMBOL" ||
    sourceKinds.get(begin.name) !== kind ||
    (suffix === "whole" && !wholeSources.has(begin.name))
  ) {
    throw new Error(`Expected ${kind} source token: ${begin.name}`);
  }
  return $[`${begin.name}_${suffix}`];
};
const sourceExternals = ($) => [
  ...lexicalTokens.lexical.map(({ begin }) => $[`${begin}_piece`]),
  ...lexicalTokens.physical.flatMap(({ begin }) => [
    $[`${begin}_prefix`],
    $[`${begin}_character`],
  ]),
  ...Array.from(wholeSources, (begin) => $[`${begin}_whole`]),
];

const ASSIGNMENT_WORD_PRECEDENCE = 3;

const ARITHMETIC_BINARY_LEVELS = [
  "logical_or",
  "logical_and",
  "bitwise_or",
  "bitwise_xor",
  "bitwise_and",
  "equality",
  "relational",
  "shift",
  "additive",
  "multiplicative",
];

const arithmeticBinaryLevelSymbols = ($, suffix) =>
  ARITHMETIC_BINARY_LEVELS.map((level) => $[`_arithmetic_${level}_${suffix}`]);

const PATTERN_PRECEDENCE = {
  expression: 2,
  specialElement: 4,
};

const lexical = ($, begin) =>
  seq(begin, repeat1(sourceToken($, begin, "lexical", "piece")));

// Direct aliases preserve MISSING tokens.
const wholeSource = ($, begin, lexeme, visible) =>
  choice(
    seq(
      optional(begin),
      alias(sourceToken($, begin, "lexical", "whole"), visible),
    ),
    alias(lexeme, visible),
  );

const KEYWORDS = [
  "if",
  "then",
  "elif",
  "else",
  "fi",
  "for",
  "in",
  "do",
  "done",
  "case",
  "esac",
  "while",
  "until",
];

const keywordRules = () =>
  Object.fromEntries(
    KEYWORDS.flatMap((name) => [
      [
        `_${name}_keyword_lexeme`,
        ($) => lexical($, $[`_${name}_keyword_begin`]),
      ],
      [
        `_${name}_keyword_source`,
        ($) =>
          wholeSource(
            $,
            $[`_${name}_keyword_begin`],
            $[`_${name}_keyword_lexeme`],
            $[`${name}_keyword`],
          ),
      ],
    ]),
  );

// Reduce delimiters to limit recovery stack depth.
const PHYSICAL_RULES = {
  _punct_left_parenthesis: "(",
  _punct_right_parenthesis: ")",
  _left_brace: "{",
  _right_brace: "}",
  _parameter_open: "{",
  _parameter_close: "}",
  _command_open: "(",
  _dollar_expansion_start: "$",
  _dq_open: '"',
  _dq_close: '"',
  _sq_open: "'",
  _sq_close: "'",
  _dollar_sq_dollar: "$",
  _dollar_sq_open: "'",
  _dollar_sq_close: "'",
  _backquote_start: "`",
  _backquote_end: "`",
  _double_quoted_backquote_start: "`",
  _punct_pipe: "|",
  _word_initial_sq_open: "'",
  _word_initial_dq_open: '"',
  _word_initial_dollar_sq_dollar: "$",
  _word_initial_dollar_expansion_start: "$",
  _word_initial_backquote_start: "`",
};
const physicalSource = ($, token, decorate) =>
  seq(
    token,
    repeat(sourceToken($, token, "physical", "prefix")),
    decorate(sourceToken($, token, "physical", "character")),
  );
const physical = ($, token, spelling, fieldName) => {
  const character = (source) => alias(source, spelling);
  if (fieldName !== undefined) {
    return physicalSource($, token, (source) =>
      field(fieldName, character(source)),
    );
  }
  if (Object.hasOwn(PHYSICAL_RULES, token.name)) {
    if (PHYSICAL_RULES[token.name] !== spelling) {
      throw new Error(`${token.name} is spelled ${PHYSICAL_RULES[token.name]}`);
    }
    return $[`${token.name}_physical`];
  }
  return physicalSource($, token, character);
};
const physicalRules = () =>
  Object.fromEntries(
    Object.entries(PHYSICAL_RULES).map(([name, spelling]) => [
      `${name}_physical`,
      ($) => physicalSource($, $[name], (source) => alias(source, spelling)),
    ]),
  );
const structuredSourceParts = ($) => [
  $.escaped_character,
  $.single_quoted,
  $.double_quoted,
  $.dollar_single_quoted,
  $.parameter_expansion,
  $.command_substitution,
  $.arithmetic_expansion,
  $.backquote_substitution,
];
const patternSpecialValue = ($, character) =>
  repeat1(field("value", choice(character, ...structuredSourceParts($))));
const bracketFallback = ($, first, literal, bracket, end) =>
  seq(
    alias(first, $.literal),
    repeat(
      choice(
        literal,
        bracket,
        $.pattern_star_source,
        $.pattern_question_source,
        $._word_structured_part,
      ),
    ),
    end,
  );
const wordPatternSpecialSources = ($) => [
  $.pattern_character_class_source,
  $.pattern_collating_symbol_source,
  $.pattern_equivalence_class_source,
];

const patternBracketPlainAtoms = ($, character) => [
  character,
  $._pattern_operator_bracket_character,
  $.pattern_bracket_hyphen_source,
  ...structuredSourceParts($),
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

const tildeExpansion = ($, start, user, end) =>
  prec.right(seq(physical($, start, "~"), optional(field("user", user)), end));
const wordSeparator = ($, marker) => seq(marker, optional($._blank));

const arithmeticBinaryExpression = ($, left, right, operatorSegment) =>
  prec.left(
    seq(
      field("left", left),
      operatorSegment,
      optional($._arithmetic_layout),
      field("right", right),
    ),
  );

const arithmeticBoundaryLayout = ($, boundary) =>
  seq(boundary, optional($._arithmetic_layout));

const arithmeticOperatorSegment = ($, boundary, operator) =>
  seq(arithmeticBoundaryLayout($, boundary), field("operator", operator));

const arithmeticBinaryLevelRules = () => {
  const rules = {};
  ARITHMETIC_BINARY_LEVELS.forEach((level, index) => {
    const following = ARITHMETIC_BINARY_LEVELS[index + 1];
    const next = ($) =>
      following === undefined
        ? $._arithmetic_unary_expression
        : $[`_arithmetic_${following}_expression`];
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
  for (const level of ARITHMETIC_BINARY_LEVELS) {
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
  for (const level of ARITHMETIC_BINARY_LEVELS) {
    rules[`_arithmetic_${level}_operator_lexeme`] = ($) =>
      lexical($, $[`_arithmetic_${level}_operator_begin`]);
    rules[`_arithmetic_${level}_operator`] = ($) =>
      alias($[`_arithmetic_${level}_operator_lexeme`], $.arithmetic_operator);
  }
  return rules;
};
const arithmeticClosingLayout = ($) =>
  seq($._arithmetic_closing_boundary, optional($._arithmetic_layout));

const parenthesizedArithmetic = ($, expression) =>
  seq(
    physical($, $._punct_left_parenthesis, "("),
    optional($._arithmetic_layout),
    field("expression", expression),
    arithmeticClosingLayout($),
    physical($, $._punct_right_parenthesis, ")"),
  );
const arithmeticExpansionStart = (
  $,
  marker,
  start = $._command_substitution_start,
) => seq(start, marker, physical($, $._punct_left_parenthesis, "("));
const arithmeticExpansion = ($, start, dynamicStart, sourceStart) =>
  choice(
    ...[
      [start, $._arithmetic_assignment_expression],
      [dynamicStart, $.arithmetic_dynamic_expression],
      [sourceStart, $.arithmetic_source],
    ].map(([opening, expression]) =>
      seq(
        opening,
        optional($._arithmetic_layout),
        field("expression", expression),
        arithmeticClosingLayout($),
        physical($, $._punct_right_parenthesis, ")"),
        physical($, $._punct_right_parenthesis, ")"),
        $._arithmetic_expansion_end,
      ),
    ),
  );

const arithmeticSourceTokens = ($) => [
  $.arithmetic_number,
  $.arithmetic_variable,
  $.arithmetic_source_fragment,
  $._arithmetic_source_operator,
];

const separatorOperatorLayout = ($, operator) =>
  seq(operator, optional(field("linebreak", $.linebreak)));

const newlineListElements = ($) => [
  $.here_document_sequence,
  $._layout_newline,
  $._blank_line,
  $._comment_line,
];
const ledNewlineList = ($, lead) =>
  prec.right(seq(lead, repeat(choice(...newlineListElements($)))));

const boundaryLineComment = ($, comment) =>
  seq($._comment_boundary, optional($._horizontal_layout), comment);

const trailingComment = ($) =>
  seq($._trailing_comment_boundary, optional($._horizontal_layout), $.comment);

const quotedExpansionParts = ($) => [
  alias($._double_quoted_parameter_expansion, $.parameter_expansion),
  $.command_substitution,
  $.arithmetic_expansion,
  alias($._double_quoted_backquote_substitution, $.backquote_substitution),
];
const singleQuoted = ($, opener) =>
  seq(
    physical($, opener, "'"),
    optional($.single_quote_content),
    physical($, $._sq_close, "'"),
  );
const doubleQuoted = ($, prefix) => seq(prefix, physical($, $._dq_close, '"'));
const doubleQuotedPrefix = ($, prefix, opener) =>
  choice(
    physical($, opener, '"'),
    seq(
      prefix,
      choice(
        $.double_quote_text,
        $.double_quote_escape,
        ...quotedExpansionParts($),
      ),
    ),
  );
const dollarSingleQuoted = ($, dollar) =>
  seq(
    physical($, dollar, "$"),
    physical($, $._dollar_sq_open, "'"),
    repeat(choice($.dollar_single_quote_text, $.dollar_single_quote_escape)),
    physical($, $._dollar_sq_close, "'"),
  );
const hereDocumentRedirect = ($, operator, end) =>
  seq(
    field("operator", operator),
    optional($._horizontal_layout),
    field("end", end),
  );
const hereEnd = ($, begin) =>
  seq(
    begin,
    field("word", alias($._here_end_source_word, $.word)),
    $._here_end_commit,
  );
const hereDocumentSequence = ($, document) =>
  seq(
    $._here_document_sequence_header,
    repeat1(field("document", document)),
    $._here_document_sequence_end,
  );
const hereDocumentVariant = ($, prefix) =>
  seq(prefix, choice(field("end", $.here_document_end), $._here_document_eof));
const hereDocumentPrefix = (start, body) =>
  seq(start, optional(field("body", body)));
const hereDocumentBody = ($, quoted) =>
  repeat1(
    choice(
      ...(quoted
        ? [
            alias(
              $._quoted_here_document_text_lexeme,
              $.quoted_here_document_text,
            ),
          ]
        : [
            $.here_document_text,
            $.here_document_escape,
            ...quotedExpansionParts($),
          ]),
      $._newline,
    ),
  );

const hereDocumentEndSource = (begin, text) => seq(begin, repeat(text));
const backquoteSubstitution = ($, start) =>
  seq(
    physical($, start, "`"),
    optional(field("body", $.backquote_substitution_body)),
    physical($, $._backquote_end, "`"),
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

const substitutionCommandsBody = ($, leadingLayout) =>
  choice(
    seq(leadingLayout, optional(completeCommandsTail($))),
    completeCommandsTail($),
    linebreakLedCommandsBody($),
    trailingComment($),
  );

const dollarExpansionStart = (
  $,
  token,
  spelling,
  start = $._dollar_expansion_start,
) => seq(physical($, start, "$"), physical($, token, spelling));
const commandSubstitution = ($, prefix) =>
  seq(
    prefix,
    optional(field("body", $.command_substitution_body)),
    physical($, $._punct_right_parenthesis, ")"),
    $._command_substitution_end,
  );
const patternSpecialStart = ($, marker) =>
  seq(physical($, $._pattern_special_left_bracket, "["), marker);
const patternSpecialEnd = ($, marker) =>
  seq(marker, physical($, $._punct_right_bracket, "]"));
const patternSpecialClassSource = ($, marker, characterSource) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.specialElement,
    seq(
      patternSpecialStart($, marker),
      patternSpecialValue($, characterSource),
      patternSpecialEnd($, marker),
    ),
  );

const patternCharacterClassBody = ($, content) =>
  choice(
    field("content", content),
    seq(
      optional(field("content", content)),
      repeat1(
        seq(
          field("content", choice(...structuredSourceParts($))),
          optional(field("content", content)),
        ),
      ),
    ),
  );

const patternCharacterClassSource = ($, content) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.specialElement,
    seq(
      patternSpecialStart($, physical($, $._punct_colon, ":")),
      patternCharacterClassBody($, content),
      patternSpecialEnd(
        $,
        physical($, $._pattern_character_class_end_colon, ":"),
      ),
    ),
  );

const parameterExpansion = (
  $,
  word,
  start = $._dollar_expansion_start,
  prefix = "",
) =>
  prec(
    1,
    choice(
      seq(physical($, start, "$"), field("parameter", $._unbraced_parameter)),
      bracedParameterExpansion($, word, prefix),
    ),
  );

const parameterHeaderRules = () =>
  Object.fromEntries(
    ["", "_word_initial"].flatMap((prefix) => {
      const name = `${prefix}_braced_parameter`;
      return [
        [
          `${name}_header`,
          ($) =>
            seq(
              $[`${name}_start`],
              choice(
                field("parameter", $._braced_parameter),
                field("parameter", $._special_parameter_hash),
              ),
            ),
        ],
        [
          `${name}_value_header`,
          ($) =>
            seq(
              $[`${name}_header`],
              field("operator", $._parameter_value_operator),
            ),
        ],
        [
          `${name}_pattern_header`,
          ($) =>
            seq(
              $[`${name}_header`],
              field("operator", $._parameter_pattern_operator),
            ),
        ],
        [
          `${name}_length_header`,
          ($) =>
            seq(
              $[`${name}_start`],
              field("operator", $.parameter_length_operator),
              field("parameter", $._length_parameter),
            ),
        ],
      ];
    }),
  );

const bracedParameterExpansion = ($, word, prefix) => {
  const name = `${prefix}_braced_parameter`;
  return choice(
    seq($[`${name}_header`], physical($, $._parameter_close, "}")),
    seq(
      $[`${name}_value_header`],
      optional(field("word", word)),
      physical($, $._parameter_close, "}"),
    ),
    seq(
      $[`${name}_pattern_header`],
      optional(field("pattern", $.parameter_pattern)),
      physical($, $._parameter_close, "}"),
    ),
    seq($[`${name}_length_header`], physical($, $._parameter_close, "}")),
  );
};

const parameterPatternSource = ($) =>
  choice(
    seq($._parameter_tilde_source, repeat($._parameter_pattern_part)),
    repeat1($._parameter_pattern_part),
  );
const caseClauseItems = ($) =>
  choice(
    $._esac_keyword_source,
    seq(field("items", $.case_list), $._esac_keyword_source),
    seq(field("items", $.case_list_ns), $._esac_keyword_source),
  );

const conditionalThenHeader = ($, keyword) =>
  seq(
    keyword,
    field("condition", $.compound_list),
    optional($._closing_layout),
    $._then_keyword_source,
  );

const conditionalThenBranch = ($, header, tail) =>
  seq(
    header,
    field("consequence", $.compound_list),
    optional($._closing_layout),
    tail,
  );

const ifClause = ($) =>
  conditionalThenBranch(
    $,
    $._if_then_header,
    choice(
      $._fi_keyword_source,
      seq(field("alternative", $.else_part), $._fi_keyword_source),
    ),
  );

const loopClause = ($, header) => seq(header, field("body", $.do_group));

const separatedForHeader = ($) =>
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
  );

const forHeaderTail = ($) =>
  choice(
    $._horizontal_layout,
    separatedForHeader($),
    seq(
      $._linebreak_layout,
      field("in", $.in),
      optional(seq($._horizontal_layout, field("words", $.wordlist))),
      separatedForHeader($),
    ),
  );

const commandRedirectContinuations = ($) => [
  field("redirect", alias($._io_redirect_without_descriptor, $.io_redirect)),
  seq($._redirect_separator, field("redirect", $.io_redirect)),
];

const namedCommand = ($, name, fieldName) =>
  choice(
    seq(field(fieldName, name), optional(field("suffix", $.cmd_suffix))),
    ...[
      [$._declaration_command_name, $._declaration_suffix],
      [$._forwarding_command_name, $._forwarding_suffix],
    ].map(([command, suffix]) =>
      seq(
        field(fieldName, alias(command, name)),
        optional(field("suffix", alias(suffix, $.cmd_suffix))),
      ),
    ),
  );

const redirectList = ($) =>
  seq(
    field("redirect", $.io_redirect),
    repeat(choice(...commandRedirectContinuations($))),
  );

const redirectableCompoundCommand = ($) =>
  prec.right(
    seq(
      field("body", $.compound_command),
      optional(
        seq(
          $._redirect_list_begin,
          optional($._horizontal_layout),
          field("redirects", $.redirect_list),
        ),
      ),
    ),
  );
const patternBracketExpression = ($, list, opener) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.expression,
    prec.right(
      2,
      seq(
        physical($, opener, "["),
        optional($._pattern_bracket_prefix),
        field("members", list),
        physical($, $._punct_right_bracket, "]"),
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
  seq(
    field("start", start),
    field("operator", $.pattern_bracket_range_operator_source),
    field(
      "end",
      choice(
        endpoint,
        alias(
          $._pattern_range_end_left_bracket,
          $.pattern_bracket_character_source,
        ),
      ),
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

  extras: ($) => [
    $._unmatchable,
    $.line_continuation,
    $._removed_newline,
    $._source_begin,
    $._removed_source,
    $._here_document_content_line_start,
  ],

  externals: ($) => {
    const tokens = [
      $._left_brace,
      $._right_brace,
      $._io_number_begin,
      $._bang_begin,
      $._if_keyword_begin,
      $._then_keyword_begin,
      $._elif_keyword_begin,
      $._else_keyword_begin,
      $._fi_keyword_begin,
      $._for_keyword_begin,
      $._in_keyword_begin,
      $._do_keyword_begin,
      $._done_keyword_begin,
      $._case_keyword_begin,
      $._esac_keyword_begin,
      $._while_keyword_begin,
      $._until_keyword_begin,
      $._here_end_begin,
      $._here_end_strip_begin,
      $._here_end_commit,
      $._here_document_line_end_begin,
      $._here_document_body_start,
      $._quoted_here_document_body_start,
      $._quoted_here_document_end_begin,
      $._quoted_here_document_end_text_begin,
      $._quoted_here_document_text_begin,
      $._here_document_end_begin,
      $._here_document_end_commit,
      $._here_document_eof,
      $._here_document_sequence_end,
      $._here_document_content_line_start,
      $._newline_begin,
      $._arithmetic_assignment_operator_boundary,
      $._arithmetic_question_operator_boundary,
      $._arithmetic_colon_operator_boundary,
      $._arithmetic_logical_or_operator_boundary,
      $._arithmetic_logical_and_operator_boundary,
      $._arithmetic_bitwise_or_operator_boundary,
      $._arithmetic_bitwise_xor_operator_boundary,
      $._arithmetic_bitwise_and_operator_boundary,
      $._arithmetic_equality_operator_boundary,
      $._arithmetic_relational_operator_boundary,
      $._arithmetic_shift_operator_boundary,
      $._arithmetic_additive_operator_boundary,
      $._arithmetic_multiplicative_operator_boundary,
      $._arithmetic_closing_boundary,
      $._arithmetic_left_parenthesis,
      $._arithmetic_dynamic_left_parenthesis,
      $._arithmetic_source_left_parenthesis,
      $._pattern_special_left_bracket,
      $._comment_boundary,
      $._trailing_comment_boundary,
      $._comment_text_begin,
      $._comment_line_end_begin,
      $._dollar_expansion_start,
      $._braced_zero_parameter_begin,
      $._braced_positional_parameter_begin,
      $._backquote_start,
      $._backquote_end,
      $._pattern_continuation,
      $._pattern_end,
      $._pipe_continuation,
      $._redirect_list_begin,
      $._case_item_end,
      $._function_body_continuation_boundary,
      $._command_substitution_body_begin,
      $._pattern_bracket_character_begin,
      $._parameter_pattern_bracket_character_begin,
      $._pattern_bracket_hyphen_begin,
      $._pattern_bracket_range_operator_begin,
      $._word_tilde_end,
      $._assignment_tilde_end,
      $._assignment_name_begin,
      $._fname_begin,
      $._declaration_word_start,
      $._forwarding_word_start,
      $._case_pattern_start,
      $._case_pattern_end,
      $._and_or_continuation,
      $._word_separator_begin,
      $._term_continuation,
      $._assignment_separator_begin,
      $._redirect_separator_begin,
      $._pre_newline_blank_begin,
      $._command_substitution_end,
      $._separator_newline,
      $._term_boundary,
      $._word_pattern_bracket_open,
      $._parameter_pattern_bracket_open,
      $._double_quoted_backquote_start,
      $._dollar_single_quote_escape_begin,
      $._pattern_character_class_end_colon,
      $.line_continuation,
      $._removed_newline,
      $._literal_begin,
      $._assignment_literal_begin,
      $._parameter_literal_begin,
      $._name_begin,
      $._variable_name_begin,
      $._escaped_character_begin,
      $._single_quote_content_begin,
      $._double_quote_text_begin,
      $._double_quote_escape_begin,
      $._dollar_single_quote_text_begin,
      $._double_quoted_parameter_text_begin,
      $._double_quoted_parameter_escape_begin,
      $._here_document_text_begin,
      $._here_document_escape_begin,
      $._here_document_end_text_begin,
      $._unbraced_positional_parameter_begin,
      $._special_parameter_begin,
      $._special_parameter_hash_begin,
      $._parameter_value_operator_begin,
      $._parameter_pattern_operator_begin,
      $._arithmetic_number_begin,
      $._arithmetic_variable_begin,
      $._arithmetic_unary_operator_begin,
      $._arithmetic_assignment_operator_begin,
      $._arithmetic_question_operator_begin,
      $._arithmetic_colon_operator_begin,
      $._arithmetic_logical_or_operator_begin,
      $._arithmetic_logical_and_operator_begin,
      $._arithmetic_bitwise_or_operator_begin,
      $._arithmetic_bitwise_xor_operator_begin,
      $._arithmetic_bitwise_and_operator_begin,
      $._arithmetic_equality_operator_begin,
      $._arithmetic_relational_operator_begin,
      $._arithmetic_shift_operator_begin,
      $._arithmetic_additive_operator_begin,
      $._arithmetic_multiplicative_operator_begin,
      $._and_if_begin,
      $._or_if_begin,
      $._dsemi_begin,
      $._semi_and_begin,
      $._lessand_begin,
      $._greatand_begin,
      $._dgreat_begin,
      $._lessgreat_begin,
      $._clobber_begin,
      $._dless_operator_begin,
      $._dlessdash_operator_begin,
      $._pattern_star_begin,
      $._pattern_question_begin,
      $._pattern_negation_begin,
      $._pattern_empty_quote_start,
      $._pattern_class_content_begin,
      $._parameter_pattern_class_content_begin,
      $._pattern_collating_character_begin,
      $._parameter_pattern_collating_character_begin,
      $._pattern_equivalence_character_begin,
      $._parameter_pattern_equivalence_character_begin,
      $._punct_left_parenthesis,
      $._punct_right_parenthesis,
      $._punct_semicolon,
      $._punct_ampersand,
      $._punct_pipe,
      $._punct_less,
      $._punct_greater,
      $._punct_equals,
      $._punct_right_bracket,
      $._punct_colon,
      $._punct_dot,
      $._sq_open,
      $._sq_close,
      $._dq_open,
      $._dq_close,
      $._dollar_sq_dollar,
      $._dollar_sq_open,
      $._dollar_sq_close,
      $._parameter_open,
      $._parameter_close,
      $._command_open,
      $._word_tilde_start,
      $._assignment_tilde_start,
      $._parameter_tilde_start,
      $._logical_newline_begin,
      $._logical_blank_begin,
      $._arithmetic_expansion_end,
      $._pattern_initial_right_bracket_begin,
      $._source_begin,
      $._removed_source,
      $._here_document_end_line_end_begin,
      $._word_fallback_literal_begin,
      $._assignment_fallback_literal_begin,
      $._parameter_fallback_literal_begin,
      $._word_bracket_fallback_end,
      $._assignment_bracket_fallback_end,
      $._parameter_bracket_fallback_end,
      $._word_initial_literal_begin,
      $._word_initial_fallback_literal_begin,
      $._word_initial_pattern_bracket_open,
      $._word_initial_pattern_star_begin,
      $._word_initial_pattern_question_begin,
      $._word_initial_escaped_character_begin,
      $._word_initial_sq_open,
      $._word_initial_dq_open,
      $._word_initial_dollar_sq_dollar,
      $._word_initial_dollar_expansion_start,
      $._word_initial_backquote_start,
      $._here_document_sequence_begin,
      $._parameter_hyphen_begin,
      $._parameter_question_begin,
      $._arithmetic_blank_begin,
      $._parameter_length_operator_begin,
      $._arithmetic_source_fragment_begin,
      $._pattern_range_end_left_bracket_begin,
      ...sourceExternals($),
    ];
    const names = new Set();
    for (const { name } of tokens) {
      if (names.has(name)) {
        throw new Error(`Duplicate external token ${name}`);
      }
      names.add(name);
    }
    for (const begin of sourceKinds.keys()) {
      if (!names.has(begin)) {
        throw new Error(`Missing external source token ${begin}`);
      }
    }
    return tokens;
  },

  conflicts: ($) => [
    [$.list],
    [$.term],
    [$.compound_list],
    [$.complete_commands],
    [$._sequential_newline_separator, $.linebreak],
    [$.case_list],
    [$.complete_command],
  ],

  rules: {
    program: ($) =>
      seq(
        optional(field("leading", $.linebreak)),
        optional($._horizontal_layout),
        choice(
          seq(
            field("commands", $.complete_commands),
            optional(field("trailing", $.linebreak)),
            optional($._free_trailing_layout),
          ),
          optional(trailingComment($)),
        ),
      ),

    _unmatchable: () => token(seq(/[\s\S]/, /[^\s\S]/)),

    _arithmetic_unary_operator_lexeme: ($) =>
      lexical($, $._arithmetic_unary_operator_begin),

    _and_if_lexeme: ($) => lexical($, $._and_if_begin),

    _or_if_lexeme: ($) => lexical($, $._or_if_begin),

    ...keywordRules(),

    _dsemi_lexeme: ($) => lexical($, $._dsemi_begin),

    _semi_and_lexeme: ($) => lexical($, $._semi_and_begin),

    _dgreat_lexeme: ($) => lexical($, $._dgreat_begin),

    _dless_operator_lexeme: ($) => lexical($, $._dless_operator_begin),

    _dlessdash_operator_lexeme: ($) => lexical($, $._dlessdash_operator_begin),

    _quoted_here_document_end_text_lexeme: ($) =>
      lexical($, $._quoted_here_document_end_text_begin),

    _quoted_here_document_text_lexeme: ($) =>
      lexical($, $._quoted_here_document_text_begin),

    _assignment_name_lexeme: ($) => lexical($, $._assignment_name_begin),

    _special_parameter_hash_lexeme: ($) =>
      lexical($, $._special_parameter_hash_begin),
    _parameter_hyphen_lexeme: ($) => lexical($, $._parameter_hyphen_begin),
    _parameter_question_lexeme: ($) => lexical($, $._parameter_question_begin),

    _special_parameter_lexeme: ($) => lexical($, $._special_parameter_begin),

    _parameter_value_operator_lexeme: ($) =>
      choice(
        lexical($, $._parameter_value_operator_begin),
        $._parameter_hyphen_lexeme,
        $._parameter_question_lexeme,
      ),

    _parameter_pattern_operator_lexeme: ($) =>
      choice(
        lexical($, $._parameter_pattern_operator_begin),
        prec.right(
          1,
          seq(
            $._special_parameter_hash_lexeme,
            optional($._special_parameter_hash_lexeme),
          ),
        ),
      ),

    _arithmetic_question_operator_lexeme: ($) =>
      lexical($, $._arithmetic_question_operator_begin),

    _arithmetic_colon_operator_lexeme: ($) =>
      lexical($, $._arithmetic_colon_operator_begin),

    _arithmetic_assignment_operator_lexeme: ($) =>
      lexical($, $._arithmetic_assignment_operator_begin),

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
            optional(
              seq($._and_or_continuation, optional($._horizontal_layout)),
            ),
            field("operator", choice($._and_if, $._or_if)),
            optional($._linebreak_layout),
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
            optional(seq($._pipe_continuation, optional($._horizontal_layout))),
            physical($, $._punct_pipe, "|"),
            optional($._linebreak_layout),
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

    separator_op: ($) =>
      choice(
        physical($, $._punct_ampersand, "&"),
        physical($, $._punct_semicolon, ";"),
      ),

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

    _sequential_operator_separator: ($) =>
      separatorOperatorLayout($, physical($, $._punct_semicolon, ";")),

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
                optional($._horizontal_layout),
                field("separator", alias($._operator_separator, $.separator)),
              ),
              seq(
                $._term_continuation,
                field("separator", alias($._newline_separator, $.separator)),
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
              optional($._horizontal_layout),
              field("terminator", alias($._operator_separator, $.separator)),
            ),
            field("terminator", alias($._newline_separator, $.separator)),
          ),
        ),
      ),

    _and_if: ($) => alias($._and_if_lexeme, $.and_if),
    _or_if: ($) => alias($._or_if_lexeme, $.or_if),
    bang: ($) => lexical($, $._bang_begin),
    function_definition: ($) =>
      seq($._function_definition_header, field("body", $.function_body)),

    _function_definition_header: ($) =>
      seq(
        field("name", $.fname),
        optional($._horizontal_layout),
        physical($, $._punct_left_parenthesis, "("),
        optional($._horizontal_layout),
        physical($, $._punct_right_parenthesis, ")"),
        $._function_body_continuation_boundary,
        optional(field("linebreak", $.linebreak)),
        optional($._horizontal_layout),
      ),

    function_body: ($) => $._redirectable_compound_command,

    fname: ($) => lexical($, $._fname_begin),
    name: ($) => lexical($, $._name_begin),
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
        physical($, $._left_brace, "{"),
        field("body", $.compound_list),
        optional($._closing_layout),
        physical($, $._right_brace, "}"),
      ),

    subshell: ($) =>
      seq(
        physical($, $._punct_left_parenthesis, "("),
        field("body", $.compound_list),
        optional($._closing_layout),
        physical($, $._punct_right_parenthesis, ")"),
      ),
    for_clause: ($) => seq($._for_clause_header, field("body", $.do_group)),

    _for_clause_header: ($) =>
      seq(
        $._for_keyword_source,
        $._word_separator,
        field("name", $.name),
        forHeaderTail($),
      ),

    in: ($) => $._in_keyword_source,

    wordlist: ($) =>
      prec.right(
        seq(
          field("word", $.word),
          repeat(seq($._word_separator, field("word", $.word))),
        ),
      ),
    do_group: ($) =>
      seq(
        $._do_keyword_source,
        field("body", $.compound_list),
        optional($._closing_layout),
        prec(20, $._done_keyword_source),
      ),

    _if_then_header: ($) => conditionalThenHeader($, $._if_keyword_source),

    _elif_then_header: ($) => conditionalThenHeader($, $._elif_keyword_source),

    if_clause: ($) => prec.right(ifClause($)),

    else_part: ($) =>
      prec.right(
        choice(
          conditionalThenBranch(
            $,
            $._elif_then_header,
            optional(field("alternative", $.else_part)),
          ),
          seq(
            $._else_keyword_source,
            field("body", $.compound_list),
            optional($._closing_layout),
          ),
        ),
      ),

    _loop_condition: ($) =>
      seq(field("condition", $.compound_list), optional($._closing_layout)),

    _while_clause_header: ($) =>
      seq($._while_keyword_source, $._loop_condition),

    while_clause: ($) => loopClause($, $._while_clause_header),

    _until_clause_header: ($) =>
      seq($._until_keyword_source, $._loop_condition),

    until_clause: ($) => loopClause($, $._until_clause_header),

    _case_selector_header: ($) =>
      seq($._case_keyword_source, $._word_separator, field("word", $.word)),

    _case_clause_header: ($) =>
      seq(
        $._case_selector_header,
        $._linebreak_layout,
        field("in", $.in),
        optional($._linebreak_layout),
      ),

    case_clause: ($) => seq($._case_clause_header, caseClauseItems($)),

    case_list_ns: ($) =>
      choice(
        field("item", $.case_item_ns),
        seq(field("terminated", $.case_list), field("item", $.case_item_ns)),
      ),

    case_list: ($) => repeat1(field("item", $.case_item)),

    _case_item_header: ($) =>
      seq(
        field("patterns", $.pattern_list),
        $._pattern_end,
        optional($._horizontal_layout),
        physical($, $._punct_right_parenthesis, ")"),
      ),

    case_item_ns: ($) =>
      seq(
        $._case_item_header,
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
        $._case_item_header,
        choice(
          optional($._linebreak_layout),
          prec.dynamic(10, field("body", $.compound_list)),
        ),
        seq($._case_item_end, optional($._horizontal_layout)),
        field("terminator", choice($._dsemi, $._semi_and)),
        optional($._linebreak_layout),
      ),

    pattern_list: ($) =>
      prec.dynamic(
        2,
        seq(
          optional(
            seq(
              physical($, $._punct_left_parenthesis, "("),
              optional($._horizontal_layout),
            ),
          ),
          seq(
            $._case_pattern_start,
            field("word", $.word),
            $._case_pattern_end,
          ),
          repeat(
            seq(
              $._pattern_continuation,
              optional($._horizontal_layout),
              physical($, $._punct_pipe, "|"),
              optional($._horizontal_layout),
              seq(
                $._case_pattern_start,
                field("word", $.word),
                $._case_pattern_end,
              ),
            ),
          ),
        ),
      ),
    _dsemi: ($) => alias($._dsemi_lexeme, $.dsemi),
    _semi_and: ($) => alias($._semi_and_lexeme, $.semi_and),
    simple_command: ($) =>
      choice(
        seq(
          field("prefix", $.cmd_prefix),
          optional(seq($._word_separator, namedCommand($, $.cmd_word, "word"))),
        ),
        namedCommand($, $.cmd_name, "name"),
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

    _declaration_command_name: ($) => alias($._declaration_word, $.word),

    _forwarding_command_name: ($) => alias($._forwarding_word, $.word),

    _declaration_word: ($) => seq($._declaration_word_start, $._source_word),

    _forwarding_word: ($) => seq($._forwarding_word_start, $._source_word),

    cmd_suffix: ($) => $._cmd_suffix,

    _cmd_suffix: ($) =>
      repeat1(
        choice(
          seq($._word_separator, field("word", $.word)),
          ...commandRedirectContinuations($),
        ),
      ),

    _declaration_suffix: ($) =>
      repeat1(
        choice(
          seq(
            $._word_separator,
            field("word", choice($.word, alias($.assignment_word, $.word))),
          ),
          ...commandRedirectContinuations($),
        ),
      ),

    _forwarding_suffix: ($) =>
      choice(
        seq(
          repeat1(choice(...commandRedirectContinuations($))),
          optional($._forwarding_argument),
        ),
        $._forwarding_argument,
      ),

    _forwarding_argument: ($) =>
      seq(
        $._word_separator,
        choice(
          seq(field("word", $.word), optional($._cmd_suffix)),
          seq(
            field("word", alias($._declaration_word, $.word)),
            optional($._declaration_suffix),
          ),
          seq(
            field("word", alias($._forwarding_word, $.word)),
            optional($._forwarding_suffix),
          ),
        ),
      ),

    io_redirect: ($) =>
      choice(
        seq(
          field("number", $.io_number),
          field("body", choice($.io_file, $.io_here)),
        ),
        field("body", choice($.io_file, $.io_here)),
      ),
    _io_redirect_without_descriptor: ($) =>
      field("body", choice($.io_file, $.io_here)),

    io_number: ($) => lexical($, $._io_number_begin),
    _io_file_prefix: ($) =>
      seq(
        choice(
          physical($, $._punct_less, "<", "operator"),
          physical($, $._punct_greater, ">", "operator"),
          field(
            "operator",
            choice($.lessand, $.greatand, $._dgreat, $.lessgreat, $.clobber),
          ),
        ),
        optional($._horizontal_layout),
      ),

    io_file: ($) => seq($._io_file_prefix, field("filename", $.filename)),

    filename: ($) => field("word", $.word),

    lessand: ($) => lexical($, $._lessand_begin),
    greatand: ($) => lexical($, $._greatand_begin),
    _dgreat: ($) => alias($._dgreat_lexeme, $.dgreat),
    lessgreat: ($) => lexical($, $._lessgreat_begin),
    clobber: ($) => lexical($, $._clobber_begin),
    io_here: ($) =>
      choice(
        hereDocumentRedirect(
          $,
          alias($._dless_operator_lexeme, $.dless),
          $.here_end,
        ),
        hereDocumentRedirect(
          $,
          alias($._dlessdash_operator_lexeme, $.dlessdash),
          alias($._here_end_strip, $.here_end),
        ),
      ),
    here_end: ($) => hereEnd($, $._here_end_begin),
    _here_end_strip: ($) => hereEnd($, $._here_end_strip_begin),

    _here_end_source_word: ($) =>
      seq($._word_initial_part, repeat($._word_part)),
    _here_document_sequence_header: ($) =>
      seq(
        $._here_document_sequence_begin,
        optional($._closing_layout),
        optional(boundaryLineComment($, field("comment", $.comment))),
        field("line_end", $.here_document_line_end),
      ),

    here_document_sequence: ($) => hereDocumentSequence($, $.here_document),
    _here_document_prefix: ($) =>
      hereDocumentPrefix($._here_document_body_start, $.here_document_body),
    _quoted_here_document_prefix: ($) =>
      hereDocumentPrefix(
        $._quoted_here_document_body_start,
        $.quoted_here_document_body,
      ),
    here_document: ($) =>
      choice(
        hereDocumentVariant($, $._here_document_prefix),
        hereDocumentVariant($, $._quoted_here_document_prefix),
      ),
    _separator_here_document_sequence: ($) =>
      hereDocumentSequence(
        $,
        alias($._separator_here_document, $.here_document),
      ),
    _separator_here_document_prefix: ($) =>
      hereDocumentPrefix(
        $._here_document_body_start,
        alias($._separator_here_document_body, $.here_document_body),
      ),
    _separator_here_document: ($) =>
      choice(
        hereDocumentVariant($, $._separator_here_document_prefix),
        hereDocumentVariant($, $._quoted_here_document_prefix),
      ),

    here_document_end: ($) =>
      seq(
        choice(
          hereDocumentEndSource(
            $._quoted_here_document_end_begin,
            alias(
              $._quoted_here_document_end_text_lexeme,
              $.here_document_end_text,
            ),
          ),
          hereDocumentEndSource(
            $._here_document_end_begin,
            $.here_document_end_text,
          ),
        ),
        optional($._here_document_end_line_end),
        $._here_document_end_commit,
      ),

    _here_document_end_line_end: ($) =>
      lexical($, $._here_document_end_line_end_begin),

    here_document_end_text: ($) => lexical($, $._here_document_end_text_begin),

    here_document_body: ($) => hereDocumentBody($, false),
    here_document_text: ($) => lexical($, $._here_document_text_begin),
    here_document_escape: ($) => lexical($, $._here_document_escape_begin),
    quoted_here_document_body: ($) => hereDocumentBody($, true),
    _separator_here_document_body: ($) => hereDocumentBody($, false),

    _assignment_word_header: ($) =>
      seq(
        field("name", alias($._assignment_name_lexeme, $.variable_name)),
        physical($, $._punct_equals, "="),
      ),

    assignment_word: ($) =>
      prec.dynamic(
        ASSIGNMENT_WORD_PRECEDENCE,
        seq(
          $._assignment_word_header,
          optional(field("value", $.assignment_value)),
        ),
      ),
    variable_name: ($) => lexical($, $._variable_name_begin),
    assignment_value: ($) => $._assignment_source_word,

    _assignment_source_word: ($) =>
      repeat1(
        choice(
          alias($._assignment_tilde_expansion, $.tilde_expansion),
          $._assignment_word_part,
        ),
      ),

    _assignment_word_part: ($) =>
      choice(
        $._assignment_bracket_fallback,
        $.pattern_bracket_source,
        alias($._assignment_literal, $.literal),
        $.pattern_star_source,
        $.pattern_question_source,
        $._word_structured_part,
      ),

    _assignment_literal: ($) =>
      prec.right(lexical($, $._assignment_literal_begin)),

    word: ($) => $._source_word,

    _source_word: ($) =>
      choice(
        $.tilde_expansion,
        $._word_initial_part,
        seq($._source_word, $._word_part),
      ),

    tilde_expansion: ($) =>
      tildeExpansion($, $._word_tilde_start, $.tilde_user, $._word_tilde_end),
    _assignment_tilde_expansion: ($) =>
      tildeExpansion(
        $,
        $._assignment_tilde_start,
        alias($._assignment_tilde_user, $.tilde_user),
        $._assignment_tilde_end,
      ),
    _parameter_tilde_expansion: ($) =>
      tildeExpansion(
        $,
        $._parameter_tilde_start,
        alias($._parameter_tilde_user, $.tilde_user),
        $._word_tilde_end,
      ),

    tilde_user: ($) => repeat1($._word_part),
    _assignment_tilde_user: ($) => repeat1($._assignment_word_part),
    _parameter_tilde_user: ($) => repeat1($._parameter_pattern_part),
    _word_part: ($) =>
      choice(
        $._word_bracket_fallback,
        $.pattern_bracket_source,
        $.literal,
        $.pattern_star_source,
        $.pattern_question_source,
        $._word_structured_part,
      ),

    _word_structured_part: ($) => choice(...structuredSourceParts($)),

    _word_initial_part: ($) =>
      choice(
        $._word_initial_bracket_fallback,
        alias($._word_initial_pattern_bracket, $.pattern_bracket_source),
        $._word_initial_literal,
        alias($._word_initial_pattern_star, $.pattern_star_source),
        alias($._word_initial_pattern_question, $.pattern_question_source),
        alias($._word_initial_escaped_character, $.escaped_character),
        alias($._word_initial_single_quoted, $.single_quoted),
        alias($._word_initial_double_quoted, $.double_quoted),
        alias($._word_initial_dollar_single_quoted, $.dollar_single_quoted),
        alias($._word_initial_parameter_expansion, $.parameter_expansion),
        alias($._word_initial_command_substitution, $.command_substitution),
        alias($._word_initial_arithmetic_expansion, $.arithmetic_expansion),
        alias($._word_initial_backquote_substitution, $.backquote_substitution),
      ),
    _word_initial_literal: ($) =>
      wholeSource(
        $,
        $._word_initial_literal_begin,
        $._word_initial_literal_lexeme,
        $.literal,
      ),
    _word_initial_literal_lexeme: ($) =>
      lexical($, $._word_initial_literal_begin),
    _word_initial_pattern_star: ($) =>
      lexical($, $._word_initial_pattern_star_begin),
    _word_initial_pattern_question: ($) =>
      lexical($, $._word_initial_pattern_question_begin),
    _word_initial_escaped_character: ($) =>
      lexical($, $._word_initial_escaped_character_begin),
    _word_initial_pattern_bracket: ($) =>
      patternBracketExpression(
        $,
        $.pattern_bracket_members_source,
        $._word_initial_pattern_bracket_open,
      ),
    _word_initial_fallback_literal: ($) =>
      lexical($, $._word_initial_fallback_literal_begin),
    _word_initial_bracket_fallback: ($) =>
      bracketFallback(
        $,
        $._word_initial_fallback_literal,
        $.literal,
        $.pattern_bracket_source,
        $._word_bracket_fallback_end,
      ),
    _word_initial_single_quoted: ($) =>
      singleQuoted($, $._word_initial_sq_open),
    _word_initial_double_quoted_prefix: ($) =>
      doubleQuotedPrefix(
        $,
        $._word_initial_double_quoted_prefix,
        $._word_initial_dq_open,
      ),
    _word_initial_double_quoted: ($) =>
      doubleQuoted($, $._word_initial_double_quoted_prefix),
    _word_initial_dollar_single_quoted: ($) =>
      dollarSingleQuoted($, $._word_initial_dollar_sq_dollar),
    _word_initial_braced_parameter_start: ($) =>
      dollarExpansionStart(
        $,
        $._parameter_open,
        "{",
        $._word_initial_dollar_expansion_start,
      ),
    _word_initial_parameter_expansion: ($) =>
      parameterExpansion(
        $,
        $.parameter_word,
        $._word_initial_dollar_expansion_start,
        "_word_initial",
      ),
    _word_initial_command_start: ($) =>
      dollarExpansionStart(
        $,
        $._command_open,
        "(",
        $._word_initial_dollar_expansion_start,
      ),
    _word_initial_command_substitution_prefix: ($) =>
      seq($._word_initial_command_start, $._command_substitution_body_begin),
    _word_initial_command_substitution: ($) =>
      commandSubstitution($, $._word_initial_command_substitution_prefix),
    _word_initial_arithmetic_start: ($) =>
      arithmeticExpansionStart(
        $,
        $._arithmetic_left_parenthesis,
        $._word_initial_command_start,
      ),
    _word_initial_arithmetic_dynamic_start: ($) =>
      arithmeticExpansionStart(
        $,
        $._arithmetic_dynamic_left_parenthesis,
        $._word_initial_command_start,
      ),
    _word_initial_arithmetic_source_start: ($) =>
      arithmeticExpansionStart(
        $,
        $._arithmetic_source_left_parenthesis,
        $._word_initial_command_start,
      ),
    _word_initial_arithmetic_expansion: ($) =>
      arithmeticExpansion(
        $,
        $._word_initial_arithmetic_start,
        $._word_initial_arithmetic_dynamic_start,
        $._word_initial_arithmetic_source_start,
      ),
    _word_initial_backquote_substitution: ($) =>
      backquoteSubstitution($, $._word_initial_backquote_start),

    literal: ($) => lexical($, $._literal_begin),

    ...physicalRules(),
    _word_fallback_literal: ($) => lexical($, $._word_fallback_literal_begin),
    _word_bracket_fallback: ($) =>
      bracketFallback(
        $,
        $._word_fallback_literal,
        $.literal,
        $.pattern_bracket_source,
        $._word_bracket_fallback_end,
      ),
    _assignment_fallback_literal: ($) =>
      lexical($, $._assignment_fallback_literal_begin),
    _assignment_bracket_fallback: ($) =>
      bracketFallback(
        $,
        $._assignment_fallback_literal,
        alias($._assignment_literal, $.literal),
        $.pattern_bracket_source,
        $._assignment_bracket_fallback_end,
      ),
    _parameter_fallback_literal: ($) =>
      lexical($, $._parameter_fallback_literal_begin),
    _parameter_bracket_fallback: ($) =>
      bracketFallback(
        $,
        $._parameter_fallback_literal,
        alias($._parameter_pattern_literal, $.literal),
        alias(
          $._parameter_pattern_bracket_expression,
          $.pattern_bracket_source,
        ),
        $._parameter_bracket_fallback_end,
      ),

    pattern_star_source: ($) => lexical($, $._pattern_star_begin),
    pattern_question_source: ($) => lexical($, $._pattern_question_begin),
    pattern_bracket_source: ($) =>
      patternBracketExpression(
        $,
        $.pattern_bracket_members_source,
        $._word_pattern_bracket_open,
      ),
    _parameter_pattern_bracket_expression: ($) =>
      patternBracketExpression(
        $,
        alias(
          $._parameter_pattern_bracket_list,
          $.pattern_bracket_members_source,
        ),
        $._parameter_pattern_bracket_open,
      ),
    _pattern_bracket_prefix: ($) =>
      choice(
        repeat1($._pattern_empty_quote),
        seq(
          repeat($._pattern_empty_quote),
          field("negation", $.pattern_bracket_negation_source),
          repeat($._pattern_empty_quote),
        ),
      ),
    _pattern_empty_quote: ($) =>
      seq(
        $._pattern_empty_quote_start,
        choice($.single_quoted, $.double_quoted),
      ),

    pattern_bracket_negation_source: ($) =>
      prec(1, lexical($, $._pattern_negation_begin)),
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
        ...patternBracketPlainAtoms($, $.pattern_bracket_character_source),
      ),

    _parameter_pattern_bracket_member: ($) =>
      choice(
        ...parameterPatternSpecialSources($),
        alias(
          $._parameter_pattern_bracket_range,
          $.pattern_bracket_range_source,
        ),
        ...patternBracketPlainAtoms(
          $,
          alias(
            $._parameter_pattern_bracket_character,
            $.pattern_bracket_character_source,
          ),
        ),
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
        ...patternBracketPlainAtoms($, $.pattern_bracket_character_source),
        $.pattern_collating_symbol_source,
      ),

    _parameter_pattern_bracket_range_endpoint: ($) =>
      choice(
        ...patternBracketPlainAtoms(
          $,
          alias(
            $._parameter_pattern_bracket_character,
            $.pattern_bracket_character_source,
          ),
        ),
        parameterPatternCollatingSymbolSource($),
      ),

    _pattern_range_end_left_bracket: ($) =>
      lexical($, $._pattern_range_end_left_bracket_begin),

    pattern_bracket_character_source: ($) =>
      lexical($, $._pattern_bracket_character_begin),
    _parameter_pattern_bracket_character: ($) =>
      lexical($, $._parameter_pattern_bracket_character_begin),
    _pattern_operator_bracket_character: ($) =>
      choice(
        alias($.pattern_star_source, $.pattern_bracket_character_source),
        alias($.pattern_question_source, $.pattern_bracket_character_source),
      ),

    pattern_bracket_range_operator_source: ($) =>
      lexical($, $._pattern_bracket_range_operator_begin),
    pattern_bracket_hyphen_source: ($) =>
      lexical($, $._pattern_bracket_hyphen_begin),
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
      lexical($, $._pattern_class_content_begin),
    _parameter_pattern_character_class_content_source: ($) =>
      lexical($, $._parameter_pattern_class_content_begin),

    pattern_collating_symbol_source: ($) =>
      patternSpecialClassSource(
        $,
        physical($, $._punct_dot, "."),
        $.pattern_collating_symbol_character_source,
      ),

    _parameter_pattern_collating_symbol_source: ($) =>
      patternSpecialClassSource(
        $,
        physical($, $._punct_dot, "."),
        alias(
          $._parameter_pattern_collating_symbol_character_source,
          $.pattern_collating_symbol_character_source,
        ),
      ),

    pattern_collating_symbol_character_source: ($) =>
      lexical($, $._pattern_collating_character_begin),
    _parameter_pattern_collating_symbol_character_source: ($) =>
      lexical($, $._parameter_pattern_collating_character_begin),

    pattern_equivalence_class_source: ($) =>
      patternSpecialClassSource(
        $,
        physical($, $._punct_equals, "="),
        $.pattern_equivalence_class_character_source,
      ),

    _parameter_pattern_equivalence_class_source: ($) =>
      patternSpecialClassSource(
        $,
        physical($, $._punct_equals, "="),
        alias(
          $._parameter_pattern_equivalence_class_character_source,
          $.pattern_equivalence_class_character_source,
        ),
      ),

    pattern_equivalence_class_character_source: ($) =>
      lexical($, $._pattern_equivalence_character_begin),
    _parameter_pattern_equivalence_class_character_source: ($) =>
      lexical($, $._parameter_pattern_equivalence_character_begin),

    escaped_character: ($) => lexical($, $._escaped_character_begin),
    single_quoted: ($) => singleQuoted($, $._sq_open),
    single_quote_content: ($) => lexical($, $._single_quote_content_begin),
    _double_quoted_prefix: ($) =>
      doubleQuotedPrefix($, $._double_quoted_prefix, $._dq_open),
    double_quoted: ($) => doubleQuoted($, $._double_quoted_prefix),

    double_quote_text: ($) => lexical($, $._double_quote_text_begin),

    double_quote_escape: ($) => lexical($, $._double_quote_escape_begin),
    dollar_single_quoted: ($) => dollarSingleQuoted($, $._dollar_sq_dollar),
    dollar_single_quote_text: ($) =>
      lexical($, $._dollar_single_quote_text_begin),
    dollar_single_quote_escape: ($) =>
      lexical($, $._dollar_single_quote_escape_begin),

    _braced_parameter_start: ($) =>
      dollarExpansionStart($, $._parameter_open, "{"),
    ...parameterHeaderRules(),
    parameter_expansion: ($) => parameterExpansion($, $.parameter_word),

    _double_quoted_parameter_expansion: ($) =>
      parameterExpansion(
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
        alias($._braced_zero_parameter, $.special_parameter),
        $.variable_name,
        $.positional_parameter,
        $._special_parameter_except_hash,
      ),

    _length_parameter: ($) =>
      choice($._braced_parameter, $._special_parameter_hash),

    _special_parameter_hash: ($) =>
      alias($._special_parameter_hash_lexeme, $.special_parameter),
    _special_parameter_except_hash: ($) =>
      choice(
        alias($._special_parameter_lexeme, $.special_parameter),
        alias($._parameter_hyphen_lexeme, $.special_parameter),
        alias($._parameter_question_lexeme, $.special_parameter),
      ),
    _unbraced_positional_parameter: ($) =>
      lexical($, $._unbraced_positional_parameter_begin),
    positional_parameter: ($) =>
      lexical($, $._braced_positional_parameter_begin),
    _braced_zero_parameter: ($) => lexical($, $._braced_zero_parameter_begin),
    parameter_length_operator: ($) =>
      lexical($, $._parameter_length_operator_begin),
    _parameter_value_operator: ($) =>
      wholeSource(
        $,
        $._parameter_value_operator_begin,
        $._parameter_value_operator_lexeme,
        $.parameter_value_operator,
      ),
    _parameter_pattern_operator: ($) =>
      wholeSource(
        $,
        $._parameter_pattern_operator_begin,
        $._parameter_pattern_operator_lexeme,
        $.parameter_pattern_operator,
      ),
    parameter_word: ($) => parameterPatternSource($),

    _double_quoted_parameter_word: ($) =>
      repeat1($._double_quoted_parameter_word_part),

    _double_quoted_parameter_word_part: ($) =>
      choice(
        alias($._double_quoted_parameter_text, $.double_quote_text),
        alias($._double_quoted_parameter_escape, $.double_quote_escape),
        $.double_quoted,
        ...quotedExpansionParts($),
      ),
    parameter_pattern: ($) => prec.right(1, parameterPatternSource($)),

    _parameter_tilde_source: ($) =>
      alias($._parameter_tilde_expansion, $.tilde_expansion),
    _parameter_pattern_part: ($) =>
      choice(
        $._parameter_bracket_fallback,
        alias(
          $._parameter_pattern_bracket_expression,
          $.pattern_bracket_source,
        ),
        alias($._parameter_pattern_literal, $.literal),
        $.pattern_star_source,
        $.pattern_question_source,
        $._word_structured_part,
      ),
    _parameter_pattern_literal: ($) =>
      prec.right(lexical($, $._parameter_literal_begin)),

    _double_quoted_parameter_text: ($) =>
      lexical($, $._double_quoted_parameter_text_begin),

    _double_quoted_parameter_escape: ($) =>
      lexical($, $._double_quoted_parameter_escape_begin),
    _command_substitution_prefix: ($) =>
      seq($._command_substitution_start, $._command_substitution_body_begin),
    command_substitution: ($) =>
      commandSubstitution($, $._command_substitution_prefix),

    _command_substitution_start: ($) =>
      dollarExpansionStart($, $._command_open, "("),
    command_substitution_body: ($) =>
      prec.left(substitutionCommandsBody($, $._closing_layout)),

    backquote_substitution: ($) => backquoteSubstitution($, $._backquote_start),
    _double_quoted_backquote_substitution: ($) =>
      backquoteSubstitution($, $._double_quoted_backquote_start),
    backquote_substitution_body: ($) => $._substitution_body,

    _substitution_body: ($) =>
      prec.left(substitutionCommandsBody($, $._horizontal_layout)),

    _arithmetic_expansion_start: ($) =>
      arithmeticExpansionStart($, $._arithmetic_left_parenthesis),

    _arithmetic_dynamic_expansion_start: ($) =>
      arithmeticExpansionStart($, $._arithmetic_dynamic_left_parenthesis),

    _arithmetic_source_expansion_start: ($) =>
      arithmeticExpansionStart($, $._arithmetic_source_left_parenthesis),

    arithmetic_expansion: ($) =>
      arithmeticExpansion(
        $,
        $._arithmetic_expansion_start,
        $._arithmetic_dynamic_expansion_start,
        $._arithmetic_source_expansion_start,
      ),

    arithmetic_source: ($) =>
      prec.right(
        repeat1(
          seq($._arithmetic_unstructured_part, optional($._arithmetic_layout)),
        ),
      ),

    _arithmetic_unstructured_part: ($) =>
      choice(
        ...arithmeticSourceTokens($),
        ...quotedExpansionParts($),
        alias(
          $._parenthesized_arithmetic_unstructured_source,
          $.parenthesized_arithmetic_source,
        ),
      ),

    _parenthesized_arithmetic_unstructured_source: ($) =>
      seq(
        physical($, $._punct_left_parenthesis, "("),
        optional($._arithmetic_layout),
        repeat(
          seq($._arithmetic_unstructured_part, optional($._arithmetic_layout)),
        ),
        physical($, $._punct_right_parenthesis, ")"),
      ),

    arithmetic_dynamic_expression: ($) =>
      prec.right(
        seq(
          repeat(
            seq($._arithmetic_source_part, optional($._arithmetic_layout)),
          ),
          field("runtime_fragment", $._arithmetic_runtime_fragment),
          repeat(
            seq(
              optional($._arithmetic_layout),
              choice($._arithmetic_source_part, $._arithmetic_runtime_fragment),
            ),
          ),
        ),
      ),

    _arithmetic_source_part: ($) =>
      choice(...arithmeticSourceTokens($), $.parenthesized_arithmetic_source),

    _arithmetic_runtime_fragment: ($) =>
      choice(
        ...quotedExpansionParts($),
        $.parenthesized_arithmetic_dynamic_source,
      ),

    _arithmetic_source_operator: ($) =>
      choice(
        $._arithmetic_assignment_operator,
        ...arithmeticBinaryLevelSymbols($, "operator"),
        alias($._arithmetic_unary_operator_lexeme, $.arithmetic_operator),
        alias($._arithmetic_question_operator_lexeme, $.arithmetic_operator),
        alias($._arithmetic_colon_operator_lexeme, $.arithmetic_operator),
      ),
    parenthesized_arithmetic_source: ($) =>
      seq(
        physical($, $._punct_left_parenthesis, "("),
        optional($._arithmetic_layout),
        repeat(seq($._arithmetic_source_part, optional($._arithmetic_layout))),
        physical($, $._punct_right_parenthesis, ")"),
      ),

    parenthesized_arithmetic_dynamic_source: ($) =>
      parenthesizedArithmetic($, $.arithmetic_dynamic_expression),

    _arithmetic_assignment_expression: ($) =>
      choice(
        $.arithmetic_assignment_expression,
        $._arithmetic_conditional_expression,
      ),

    arithmetic_assignment_expression: ($) =>
      prec.right(
        seq(
          field(
            "left",
            choice($.arithmetic_variable, $.parenthesized_arithmetic),
          ),
          $._arithmetic_assignment_operator_segment,
          optional($._arithmetic_layout),
          field(
            "right",
            choice(
              $.arithmetic_assignment_expression,
              $._arithmetic_conditional_expression,
            ),
          ),
        ),
      ),

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
          optional($._arithmetic_layout),
          field("consequence", $._arithmetic_assignment_expression),
          $._arithmetic_colon_operator_segment,
          optional($._arithmetic_layout),
          field("alternative", $._arithmetic_conditional_expression),
        ),
      ),

    ...arithmeticBinaryLevelRules(),

    _arithmetic_unary_expression: ($) =>
      choice($.arithmetic_unary_expression, $._arithmetic_primary_expression),

    arithmetic_unary_expression: ($) =>
      prec.right(
        seq(
          field(
            "operator",
            alias($._arithmetic_unary_operator_lexeme, $.arithmetic_operator),
          ),
          optional($._arithmetic_layout),
          field("operand", $._arithmetic_unary_expression),
        ),
      ),
    _arithmetic_primary_expression: ($) =>
      choice(
        $.arithmetic_number,
        $.arithmetic_variable,
        ...quotedExpansionParts($),
        $.parenthesized_arithmetic,
      ),

    parenthesized_arithmetic: ($) =>
      parenthesizedArithmetic($, $._arithmetic_assignment_expression),

    arithmetic_number: ($) => lexical($, $._arithmetic_number_begin),
    arithmetic_variable: ($) => lexical($, $._arithmetic_variable_begin),
    arithmetic_source_fragment: ($) =>
      lexical($, $._arithmetic_source_fragment_begin),
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
        alias($._arithmetic_question_operator_lexeme, $.arithmetic_operator),
      ),
    _arithmetic_colon_operator_segment: ($) =>
      arithmeticOperatorSegment(
        $,
        $._arithmetic_colon_operator_boundary,
        alias($._arithmetic_colon_operator_lexeme, $.arithmetic_operator),
      ),
    ...arithmeticBinaryOperatorSegmentRules(),

    _arithmetic_assignment_operator: ($) =>
      alias($._arithmetic_assignment_operator_lexeme, $.arithmetic_operator),
    ...arithmeticBinaryOperatorRules(),

    _arithmetic_layout: ($) =>
      repeat1(choice(lexical($, $._arithmetic_blank_begin), $._newline)),

    _pattern_initial_right_bracket: ($) =>
      lexical($, $._pattern_initial_right_bracket_begin),

    newline_list: ($) => prec.right(repeat1(choice(...newlineListElements($)))),

    _separator_led_newline_list: ($) =>
      ledNewlineList($, seq($._separator_newline, $._layout_newline)),

    _here_document_led_newline_list: ($) =>
      ledNewlineList(
        $,
        alias($._separator_here_document_sequence, $.here_document_sequence),
      ),

    linebreak: ($) => $.newline_list,

    _linebreak_layout: ($) =>
      choice(
        seq($.linebreak, optional($._horizontal_layout)),
        $._horizontal_layout,
      ),

    _horizontal_layout: ($) => $._blank,
    _closing_layout: ($) => $._blank,
    _word_separator: ($) => wordSeparator($, $._word_separator_begin),

    _assignment_separator: ($) =>
      wordSeparator($, $._assignment_separator_begin),

    _redirect_separator: ($) => wordSeparator($, $._redirect_separator_begin),

    _free_trailing_layout: ($) =>
      prec.right(1, choice($._closing_layout, trailingComment($))),

    comment: ($) => $.comment_text,
    comment_text: ($) => lexical($, $._comment_text_begin),

    _comment_line_end: ($) => physical($, $._comment_line_end_begin, "\n"),

    _pre_newline_blank: ($) => lexical($, $._pre_newline_blank_begin),

    here_document_line_end: ($) => lexical($, $._here_document_line_end_begin),

    _comment_line: ($) =>
      seq(boundaryLineComment($, $.comment), $._comment_line_end),

    _blank_line: ($) => seq($._pre_newline_blank, $._layout_newline),

    _layout_newline: ($) => physical($, $._logical_newline_begin, "\n"),

    _newline: ($) => lexical($, $._newline_begin),

    _blank: ($) => lexical($, $._logical_blank_begin),
  },
});
