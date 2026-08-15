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
const PARAMETER_DEFERRED_EXTRA_CHARACTER_PATTERN = /[ \t\n;&|<>()]/;
const ASSIGNMENT_WORD_PRECEDENCE = 3;

// One row per POSIX binary-operator precedence level, from lowest to highest
// binding strength. Each row derives the level's expression, binary
// expression, operator segment, and operator token rules, plus its external
// operator boundary reference.
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
  negation: 1,
  expression: 2,
  range: 3,
  specialElement: 4,
};

// The quote, escape, expansion, and substitution parts a word can contain, in
// the one order every containing choice uses.
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
  );

const parameterIncompleteBracketLiteralAtom = ($) =>
  choice(
    "$",
    $._parameter_pattern_text_token,
    $._literal_tilde,
    $._literal_right_bracket,
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
  continuation,
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
    continuation,
    ...structuredSourceParts($),
  );

const reservedWord = ($, marker) => seq(marker, $._reserved_word_source);

const lineContinuationRun = ($) => prec.right(1, repeat1($.line_continuation));

const boundaryLineContinuationRun = ($) =>
  prec.right(
    1,
    seq(
      alias($._boundary_line_continuation, $.line_continuation),
      repeat($.line_continuation),
    ),
  );

// A here-document body reaching its end synchronizes every construct that is
// still open inside the body at that boundary.
const hereDocumentBoundaryRecovery = ($) =>
  alias($._here_document_boundary, $.here_document_end_recovery);

const parameterBraceClose = ($) => choice("}", hereDocumentBoundaryRecovery($));

const continuedWordSeparator = ($) =>
  seq(
    alias($._word_separator_line_continuation, $.line_continuation),
    repeat($.line_continuation),
    $._blank,
    repeat(choice($._blank, $.line_continuation)),
  );

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
  seq(
    repeat($._blank),
    $.line_continuation,
    repeat(choice($._blank, $.line_continuation)),
  );

const arithmeticExpansionEnd = ($) =>
  choice(
    seq(")", $._arithmetic_second_right_parenthesis),
    hereDocumentBoundaryRecovery($),
  );

const arithmeticExpansionStart = ($, marker) =>
  seq($._command_or_arithmetic_substitution_start, alias(marker, "("));

// The structured reading closes through the scanner's closing boundary, which
// settles the reduce-versus-shift decision across layout. The flat readings
// have no such decision: layout units are consumed directly and the closer is
// recognized at its own character.
const closedArithmeticExpansion = ($, start, expression, closing) =>
  seq(
    start,
    optional($._arithmetic_layout),
    choice(
      seq(field("expression", expression), closing, arithmeticExpansionEnd($)),
      hereDocumentBoundaryRecovery($),
    ),
  );

const linebreakLayout = ($) =>
  seq(optional($.linebreak), optional($._horizontal_layout));

const continuedLinebreakLayout = ($, continuation) =>
  choice(
    prec.right(
      2,
      seq(
        optional($._blank),
        repeat1(alias(continuation, $.line_continuation)),
        optional($._horizontal_layout),
        optional(seq($.linebreak, optional($._horizontal_layout))),
      ),
    ),
    continuedWordSeparator($),
    seq(
      optional($._horizontal_layout),
      optional(seq($.linebreak, optional($._horizontal_layout))),
    ),
  );

const operatorContinuedLinebreakLayout = ($) =>
  continuedLinebreakLayout($, $._operator_line_continuation);

const boundaryContinuedLinebreakLayout = ($) =>
  continuedLinebreakLayout($, $._boundary_line_continuation);

const reservedWordLinebreak = ($) =>
  choice(seq($.linebreak, optional($._horizontal_layout)), $._word_separator);

const continuationBoundaryLayout = ($, marker) =>
  prec.right(
    1,
    seq(
      optional(lineContinuationRun($)),
      marker,
      optional($._horizontal_layout),
    ),
  );

const ownedContinuationBoundaryLayout = ($, marker) =>
  prec.right(
    1,
    seq(
      optional(boundaryLineContinuationRun($)),
      marker,
      optional($._horizontal_layout),
    ),
  );

const commandBoundaryLayout = ($) =>
  continuationBoundaryLayout($, $._command_continuation);

const patternBoundaryLayout = ($) =>
  continuationBoundaryLayout($, $._pattern_continuation);

const patternClosingLayout = ($) =>
  continuationBoundaryLayout($, $._pattern_end);

const caseItemFollowingLayout = ($) =>
  prec.right(
    1,
    choice(
      continuedWordSeparator($),
      seq($.linebreak, optional($._horizontal_layout)),
      $._horizontal_layout,
    ),
  );

const lineComment = ($, comment) =>
  seq(continuationBoundaryLayout($, $._comment_boundary), comment);

const doubleQuotedPart = ($) =>
  choice(
    $.double_quote_text,
    $.double_quote_escape,
    $.line_continuation,
    alias($._newline, $.double_quote_text),
    alias($._double_quoted_parameter_expansion, $.parameter_expansion),
    $.command_substitution,
    $.arithmetic_expansion,
    $.backquote_substitution,
  );

const completeCommandsBody = ($, leading) =>
  seq(
    leading,
    $.complete_commands,
    optional($.linebreak),
    optional($._free_trailing_layout),
  );

const commandSequenceBody = ($) =>
  choice(
    completeCommandsBody(
      $,
      seq(optional($.linebreak), optional($._horizontal_layout)),
    ),
    seq($.linebreak, optional($._free_trailing_layout)),
    $._free_trailing_layout,
  );

// A leading line continuation has to remain visible until the next byte is
// known: `$(\\\n(` can begin either an arithmetic expansion or a command
// substitution whose first command is a subshell.  Raw linebreaks already
// settle that choice, so only the initial, no-linebreak layout is narrowed to
// blanks here. A continuation-prefixed start is factored as one complete run
// below so the next byte, rather than an arbitrary prefix of the run, chooses
// arithmetic expansion or command substitution.
const commandSubstitutionSequenceBody = ($) =>
  choice(
    completeCommandsBody(
      $,
      choice(
        optional($._blank),
        seq($.linebreak, optional($._horizontal_layout)),
      ),
    ),
    seq($.linebreak, optional($._free_trailing_layout)),
    prec(2, $._blank),
  );

const closedCommandBoundaryLayout = ($) =>
  ownedContinuationBoundaryLayout($, $._closed_command_end);

const backquoteDollar = ($) =>
  seq(alias($._backquote_dollar_prefix, "\\"), token.immediate("$"));

const backquoteDelimiter = (plain, prefix) =>
  choice(alias(plain, "`"), seq(alias(prefix, "\\"), token.immediate("`")));

const commandSubstitutionEnd = ($) =>
  choice(
    seq(closedCommandBoundaryLayout($), ")"),
    hereDocumentBoundaryRecovery($),
  );

const commandSubstitution = ($, start) =>
  seq(
    start,
    optional(field("body", $.command_substitution_body)),
    commandSubstitutionEnd($),
  );

const patternSpecialStart = ($, marker) =>
  seq(
    alias($._pattern_special_left_bracket, "["),
    repeat($.line_continuation),
    marker,
  );

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

const patternCharacterClassBody = ($) =>
  choice(
    field("content", $.pattern_character_class_content_source),
    seq(
      optional(field("content", $.pattern_character_class_content_source)),
      repeat1(
        seq(
          field("content", patternCharacterClassStructuredContent($)),
          optional(field("content", $.pattern_character_class_content_source)),
        ),
      ),
    ),
  );

const patternSpecialInitialRange = ($, endpoint) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.range,
    seq(
      field(
        "start",
        alias(
          $._pattern_special_marker_character,
          $.pattern_bracket_character_source,
        ),
      ),
      repeat($.line_continuation),
      field("operator", $.pattern_bracket_range_operator_source),
      repeat($.line_continuation),
      field("end", endpoint),
    ),
  );

const patternSpecialPrefixedList = ($, member, initialRange) =>
  prec.right(
    choice(
      seq(
        field("member", alias(initialRange, $.pattern_bracket_range_source)),
        repeat(seq(repeat($.line_continuation), field("member", member))),
        optional(lineContinuationRun($)),
      ),
      seq(
        field(
          "member",
          alias(
            $._pattern_special_marker_character,
            $.pattern_bracket_character_source,
          ),
        ),
        repeat(seq(repeat($.line_continuation), field("member", member))),
        optional(lineContinuationRun($)),
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

const patternDeferredBracketRangeEndpoint = ($, character) =>
  choice(
    alias(character, $.pattern_bracket_character_source),
    $.pattern_collating_symbol_source,
    $.pattern_bracket_hyphen_source,
    ...structuredSourceParts($),
  );

const patternDeferredBracketMember = ($, character, range) =>
  choice(
    $.pattern_character_class_source,
    $.pattern_collating_symbol_source,
    $.pattern_equivalence_class_source,
    alias(range, $.pattern_bracket_range_source),
    alias(character, $.pattern_bracket_character_source),
    $.pattern_bracket_hyphen_source,
    ...structuredSourceParts($),
  );

const bracedParameterStart = ($) => choice("${", seq(backquoteDollar($), "{"));

const parameterExpansion = ($, bracedExpansion) =>
  prec(
    1,
    choice(
      seq(
        choice(seq($._unbraced_parameter_start, "$"), backquoteDollar($)),
        field("parameter", $._unbraced_parameter),
      ),
      seq(
        bracedParameterStart($),
        repeat($.line_continuation),
        choice(
          hereDocumentBoundaryRecovery($),
          bracedExpansion,
          prec.dynamic(
            -20,
            seq(
              field(
                "recovery",
                alias(
                  $._parameter_missing_recovery,
                  $.parameter_expansion_recovery,
                ),
              ),
              optional("}"),
            ),
          ),
        ),
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
    prec.dynamic(
      3,
      seq(
        field("operator", $.parameter_length_operator),
        repeat($.line_continuation),
        field("recovery", $.parameter_expansion_recovery),
      ),
    ),
    prec.dynamic(
      -20,
      seq(
        field("operator", $.parameter_length_operator),
        repeat($.line_continuation),
        bracedParameterSource($, $._length_parameter),
        repeat($.line_continuation),
        field(
          "recovery",
          parameterTailRecovery($, $._parameter_tail_recovery_boundary),
        ),
        optional("}"),
      ),
    ),
  );

const parameterExpansionTail = ($, word) =>
  choice(
    parameterBraceClose($),
    seq(
      field("operator", $.parameter_value_operator),
      repeat($.line_continuation),
      optional(field("word", word)),
      parameterBraceClose($),
    ),
    seq(
      field("operator", $.parameter_pattern_operator),
      repeat($.line_continuation),
      optional(field("pattern", $.parameter_pattern)),
      parameterBraceClose($),
    ),
  );

const parameterTailRecovery = ($, boundary) =>
  alias(boundary, $.parameter_expansion_recovery);

// Directly after the parameter only a closer, an operator, or a line
// continuation may follow, so layout and control characters there prove the
// expansion invalid and the full tail boundary applies. Behind an operator
// the word absorbs every character up to the matching right brace, so
// recovery waits for a real terminator: the end of input or the closing
// backquote of an enclosing substitution.
const recoveringParameterExpansionTail = ($, word, wordRecoveryBoundary) =>
  prec.dynamic(
    -20,
    choice(
      seq(
        field(
          "recovery",
          choice(
            parameterTailRecovery($, $._parameter_tail_recovery_boundary),
            alias(
              $._parameter_operator_recovery,
              $.parameter_expansion_recovery,
            ),
          ),
        ),
        optional("}"),
      ),
      seq(
        field("operator", $.parameter_value_operator),
        repeat($.line_continuation),
        optional(field("word", word)),
        field("recovery", parameterTailRecovery($, wordRecoveryBoundary)),
        optional("}"),
      ),
      seq(
        field("operator", $.parameter_pattern_operator),
        repeat($.line_continuation),
        optional(field("pattern", $.parameter_pattern)),
        field("recovery", parameterTailRecovery($, wordRecoveryBoundary)),
        optional("}"),
      ),
    ),
  );

// Every recovery reading of a command position wraps one payload in the
// formal pipeline hierarchy. This generates the hidden chain rules for a
// prefix from the given top level down to the command payload.
const RECOVERY_COMMAND_CHAIN = [
  ["and_or", "pipeline", "pipeline"],
  ["pipeline", "pipe_sequence", "sequence"],
  ["pipe_sequence", "command", "command"],
];

const recoveryCommandChainRules = (prefix, top, command) => {
  const rules = { [`_${prefix}_command`]: command };
  let reached = false;
  for (const [level, child, fieldName] of RECOVERY_COMMAND_CHAIN) {
    reached = reached || level === top;
    if (!reached) {
      continue;
    }
    rules[`_${prefix}_${level}`] = ($) =>
      field(fieldName, alias($[`_${prefix}_${child}`], $[child]));
  }
  return rules;
};

const recoveryField = ($) => field("recovery", $.compound_command_recovery);

const directRecoveryField = ($) =>
  field(
    "recovery",
    alias($._direct_recovery_boundary, $.compound_command_recovery),
  );

const invalidCommandRecoveryField = (
  $,
  source = $._invalid_command_recovery_source,
) => field("recovery", alias(source, $.command_recovery));

const headerRecoveryField = ($) =>
  field(
    "recovery",
    alias($._header_recovery_boundary, $.compound_command_recovery),
  );

const forTailRecoveryField = ($) =>
  field(
    "recovery",
    alias($._for_tail_recovery_boundary, $.compound_command_recovery),
  );

const caseItemsRecoveryField = ($) =>
  field(
    "recovery",
    alias($._case_items_recovery_boundary, $.compound_command_recovery),
  );

const subshellRecoveryField = ($) =>
  field(
    "recovery",
    alias($._subshell_recovery_boundary, $.compound_command_recovery),
  );

const caseClauseItems = ($) =>
  choice(
    $.esac_keyword,
    caseItemsRecoveryField($),
    field("items", alias($._recovering_case_list_ns, $.case_list_ns)),
    seq(
      field("items", choice($.case_list, $.case_list_ns)),
      optional($._horizontal_layout),
      choice($.esac_keyword, recoveryField($)),
    ),
  );

const compoundListField = ($, name) =>
  field(name, alias($._reserved_compound_list, $.compound_list));

const terminatedCompoundList = ($) =>
  seq(
    field("body", $.term),
    optional($._separator_boundary_layout),
    optional($._comment_boundary),
    field("terminator", $.separator),
    $._compound_list_boundary,
  );

const emptyCompoundListField = ($, name) =>
  field(name, alias($._recovering_empty_compound_list, $.compound_list));

const recoverableCompoundListField = ($, name) =>
  field(
    name,
    choice(
      alias($._reserved_compound_list, $.compound_list),
      alias($._recovering_empty_compound_list, $.compound_list),
    ),
  );

const conditionalThenBranch = ($, keyword, tail) =>
  seq(
    keyword,
    choice(
      recoveryField($),
      seq(
        recoverableCompoundListField($, "condition"),
        optional($._horizontal_layout),
        choice(
          recoveryField($),
          seq(
            $.then_keyword,
            recoverableCompoundListField($, "consequence"),
            optional($._horizontal_layout),
            tail,
          ),
        ),
      ),
    ),
  );

const ifClause = ($) =>
  conditionalThenBranch(
    $,
    $.if_keyword,
    choice(
      $.fi_keyword,
      recoveryField($),
      seq(
        field("alternative", $.else_part),
        optional($._horizontal_layout),
        choice($.fi_keyword, recoveryField($)),
      ),
    ),
  );

const loopClause = ($, keyword) =>
  seq(
    keyword,
    choice(
      recoveryField($),
      seq(
        recoverableCompoundListField($, "condition"),
        optional($._horizontal_layout),
        choice(recoveryField($), field("body", $.do_group)),
      ),
    ),
  );

const separatedForBody = ($) =>
  seq(
    optional($._separator_boundary_layout),
    field("separator", $.sequential_sep),
    optional($._horizontal_layout),
    choice(
      field("recovery", $.compound_command_recovery),
      field("body", $.do_group),
    ),
  );

const recoveringForTail = ($) =>
  choice(
    seq(optional($._horizontal_layout), forTailRecoveryField($)),
    seq($._word_separator, field("body", $.do_group)),
    separatedForBody($),
    seq(
      reservedWordLinebreak($),
      field("in", $.in),
      optional(seq($._word_separator, field("words", $.wordlist))),
      choice(
        seq(optional($._horizontal_layout), forTailRecoveryField($)),
        separatedForBody($),
      ),
    ),
  );

// In a list, a completed and_or directly followed by source that cannot begin
// a command recovers as a zero-width separator_recovery and a command_recovery
// that owns only that source, per the list recovery contract. The
// stray-parenthesis variant reads a right parenthesis that has no enclosing
// closer to serve; the case-terminator variant starts from a marker the
// scanner emits only at a real ";;" or ";&", keeping the terminator one
// token. Terms do not take these tails: inside a case item the terminator
// must stay reachable for its formal owner through the enclosing recovery
// cascade.
const strayParenthesisTail = ($) =>
  prec.dynamic(
    -50,
    seq(
      optional($._separator_boundary_layout),
      field("separator", prec(100, $.separator_recovery)),
      field("and_or", alias($._invalid_list_and_or, $.and_or)),
    ),
  );

const caseTerminatorTail = ($) =>
  prec.dynamic(
    -50,
    seq(
      optional($._separator_boundary_layout),
      field(
        "separator",
        alias($._invalid_case_terminator_start, $.separator_recovery),
      ),
      field("and_or", alias($._case_terminator_and_or, $.and_or)),
    ),
  );

// A redirection continues a simple command directly after the previous
// element, after line continuations alone, or after a word separator that
// lets it carry its own descriptor.
const commandRedirectContinuations = ($) => [
  field("redirect", alias($._io_redirect_without_descriptor, $.io_redirect)),
  seq(
    repeat1($.line_continuation),
    field("redirect", alias($._io_redirect_without_descriptor, $.io_redirect)),
  ),
  seq($._word_separator, field("redirect", $.io_redirect)),
];

const redirectList = ($) =>
  seq(
    field("redirect", $.io_redirect),
    repeat(
      choice(
        field(
          "redirect",
          alias($._io_redirect_without_descriptor, $.io_redirect),
        ),
        prec.dynamic(
          3,
          seq(
            repeat1($.line_continuation),
            field(
              "redirect",
              alias($._io_redirect_without_descriptor, $.io_redirect),
            ),
          ),
        ),
        seq($._word_separator, field("redirect", $.io_redirect)),
      ),
    ),
  );

const redirectableCompoundCommand = ($, end = null) =>
  prec.right(
    seq(
      field("body", $.compound_command),
      end === null
        ? optional(
            seq(
              $._redirect_list_begin,
              optional($._horizontal_layout),
              field("redirects", $.redirect_list),
            ),
          )
        : seq(
            choice(
              seq(optional($._horizontal_layout), end),
              seq(
                $._redirect_list_begin,
                optional($._horizontal_layout),
                field("redirects", $.redirect_list),
                optional($._horizontal_layout),
                end,
              ),
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

const recoveringFunctionDefinition = ($, end = null) =>
  prec.dynamic(
    -50,
    seq(
      functionDefinitionHeader($),
      field("body", alias($._recovering_function_body, $.function_body)),
      ...(end === null ? [] : [optional($._horizontal_layout), end]),
    ),
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
        repeat(seq(repeat($.line_continuation), field("member", member))),
        optional(lineContinuationRun($)),
      ),
      seq(
        field("member", member),
        repeat(seq(repeat($.line_continuation), field("member", member))),
        optional(lineContinuationRun($)),
      ),
    ),
  );

const patternBracketRange = ($, endpoint) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.range,
    seq(
      field("start", endpoint),
      repeat($.line_continuation),
      field("operator", $.pattern_bracket_range_operator_source),
      repeat($.line_continuation),
      field("end", endpoint),
    ),
  );

const patternInitialBracketRange = ($, endpoint) =>
  prec.dynamic(
    PATTERN_PRECEDENCE.range,
    seq(
      field(
        "start",
        alias(
          $._pattern_initial_right_bracket,
          $.pattern_bracket_character_source,
        ),
      ),
      repeat($.line_continuation),
      field("operator", $.pattern_bracket_range_operator_source),
      repeat($.line_continuation),
      field("end", endpoint),
    ),
  );

module.exports = grammar({
  name: "posix_sh",

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
    $._missing_here_document_delimiter,
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
    $._boundary_line_continuation,
    $._word_separator_line_continuation,
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
    $._arithmetic_incomplete_left_parenthesis,
    $._pattern_special_left_bracket,
    $._literal_hash,
    $._comment_boundary,
    $.comment,
    $._here_document_boundary,
    $._unbraced_parameter_start,
    $._braced_parameter_number_start,
    $._braced_positional_parameter_start,
    $._backquote_start,
    $._backquote_start_prefix,
    $._backquote_dollar_prefix,
    $._backquote_end,
    $._backquote_end_prefix,
    $._pattern_continuation,
    $._pattern_end,
    $._command_continuation,
    $._redirect_list_begin,
    $._closed_command_end,
    $._closed_simple_command_end,
    $._case_item_end,
    $._case_item_ns_boundary,
    $._compound_command_recovery_boundary,
    $._subshell_recovery_boundary,
    $._direct_recovery_boundary,
    $._header_recovery_boundary,
    $._for_tail_recovery_boundary,
    $._case_items_recovery_boundary,
    $._compound_list_boundary,
    $._function_body_continuation_boundary,
    $._function_body_recovery_boundary,
    $._parameter_missing_recovery_boundary,
    $._parameter_operator_recovery_boundary,
    $._parameter_tail_recovery_boundary,
    $._double_quoted_parameter_tail_recovery_boundary,
    $._input_end_recovery,
    $._boundary_command_recovery,
    $._missing_command_recovery_boundary,
    $._invalid_reserved_command_start,
    $._invalid_case_terminator_start,
    $._invalid_command_character_source,
    $.separator_recovery,
    $.redirection_target_recovery,
    $.here_document_end_recovery,
    $.backquote_end_recovery,
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
    $._operator_line_continuation,
  ],

  extras: ($) => [$._here_document_content_line_start],

  conflicts: ($) => [
    [$.simple_command],
    [$.cmd_prefix],
    [$.cmd_suffix],
    [$.fname, $.literal],
    [$._horizontal_layout, $._blank_line],
    [$._horizontal_layout, $._continued_blank_line],
    [$._separator_boundary_layout, $.here_document_sequence],
    [$.pipe_sequence],
    [$.pipe_sequence, $._closed_pipe_sequence],
    [$.pipe_sequence, $._closed_pipe_sequence, $._recoverable_pipe_sequence],
    [$.and_or],
    [$.and_or, $._closed_and_or],
    [
      $.and_or,
      $._closed_and_or,
      $._recoverable_and_or,
      $._boundary_recovered_and_or,
    ],
    [$.complete_commands],
    [$.complete_command],
    [$.newline_list],
    [$.here_document_sequence],
    [$.list],
    [$.compound_list],
    [$.compound_list, $._case_item_ns_compound_list],
    [$.term],
    [$.term, $._closed_term],
    [$.term, $._closed_term, $._recoverable_term, $._boundary_recovered_term],
    [$.separator],
    [$.sequential_sep],
    [$.sequential_sep, $.linebreak],
    [$.case_list],
    [$.wordlist],
    [$.redirect_list],
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
      $.pattern_character_class_source,
      $._pattern_special_literal_left,
    ],
    [
      $._parameter_special_prefixed_bracket_source,
      $.pattern_collating_symbol_source,
      $._pattern_special_literal_left,
    ],
    [
      $._parameter_special_prefixed_bracket_source,
      $.pattern_equivalence_class_source,
      $._pattern_special_literal_left,
    ],
    [
      $._word_special_prefixed_bracket_source,
      $.pattern_character_class_source,
      $.pattern_collating_symbol_source,
      $.pattern_equivalence_class_source,
      $._pattern_special_literal_left,
    ],
    [
      $._parameter_special_prefixed_bracket_source,
      $.pattern_character_class_source,
      $.pattern_collating_symbol_source,
      $.pattern_equivalence_class_source,
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
    [$.parameter_expansion],
    [$._double_quoted_parameter_expansion],
    [$._parameter_expansion_tail],
    [$._double_quoted_parameter_expansion_tail],
    [$._braced_parameter_expansion],
    [$._double_quoted_braced_parameter_expansion],
    [$.case_item],
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
          optional(field("trailing", $.linebreak)),
          optional($._free_trailing_layout),
        ),
        seq(
          optional(field("leading", $.linebreak)),
          optional($._free_trailing_layout),
        ),
      ),

    complete_commands: ($) =>
      seq(
        field("command", $.complete_command),
        repeat(
          seq(
            field("separator", $.newline_list),
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
            optional($._separator_boundary_layout),
            field("terminator", $.separator_op),
          ),
        ),
      ),

    list: ($) =>
      seq(
        field("and_or", $.and_or),
        repeat(
          seq(
            optional($._separator_boundary_layout),
            field("separator", $.separator_op),
            optional($._horizontal_layout),
            field("and_or", $.and_or),
          ),
        ),
        optional(choice(strayParenthesisTail($), caseTerminatorTail($))),
      ),

    ...recoveryCommandChainRules("invalid_list", "and_or", ($) =>
      invalidCommandRecoveryField($, $._stray_right_parenthesis),
    ),

    ...recoveryCommandChainRules("case_terminator", "and_or", ($) =>
      invalidCommandRecoveryField($, $._case_terminator_operator),
    ),

    _case_terminator_operator: ($) => choice($.dsemi, $.semi_and),

    and_or: ($) =>
      seq(
        field("pipeline", $.pipeline),
        repeat(
          seq(
            commandBoundaryLayout($),
            field("operator", choice($.and_if, $.or_if)),
            operatorContinuedLinebreakLayout($),
            field(
              "pipeline",
              choice($.pipeline, alias($._recovering_pipeline, $.pipeline)),
            ),
          ),
        ),
      ),

    pipeline: ($) =>
      choice(
        seq(
          optional(
            seq(field("negation", $.bang), optional($._horizontal_layout)),
          ),
          field("sequence", $.pipe_sequence),
        ),
        prec.dynamic(
          -20,
          seq(
            field("negation", $.bang),
            optional($._horizontal_layout),
            field(
              "sequence",
              alias($._recovering_pipe_sequence, $.pipe_sequence),
            ),
          ),
        ),
      ),

    pipe_sequence: ($) =>
      seq(
        field("command", $.command),
        repeat(
          seq(
            commandBoundaryLayout($),
            "|",
            operatorContinuedLinebreakLayout($),
            field(
              "command",
              choice($.command, alias($._recovering_command, $.command)),
            ),
          ),
        ),
      ),

    command: ($) =>
      choice(
        field("body", $.simple_command),
        $._redirectable_compound_command,
        field("body", $.function_definition),
        prec.dynamic(-1000, invalidCommandRecoveryField($)),
      ),

    _redirectable_compound_command: ($) => redirectableCompoundCommand($),

    _closed_redirectable_compound_command: ($) =>
      redirectableCompoundCommand($, $._closed_command_end),

    command_recovery: ($) => $._missing_command_recovery_boundary,

    _invalid_command_recovery_source: ($) =>
      choice(
        seq(
          $._invalid_reserved_command_start,
          choice(
            $.then_keyword,
            $.elif_keyword,
            $.else_keyword,
            $.fi_keyword,
            $.in_keyword,
            $.do_keyword,
            $.done_keyword,
            $.esac_keyword,
          ),
        ),
        seq($._invalid_case_terminator_start, $._case_terminator_operator),
        $._invalid_command_character_source,
      ),

    _stray_right_parenthesis: (_) => token(prec(-1, ")")),

    _reserved_word_source: (_) =>
      choice(
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
      ),

    ...recoveryCommandChainRules("recovering", "pipeline", ($) =>
      field("recovery", $.command_recovery),
    ),

    ...recoveryCommandChainRules("boundary_recovering", "pipeline", ($) =>
      field(
        "recovery",
        alias($._boundary_command_recovery, $.command_recovery),
      ),
    ),

    separator_op: (_) => choice("&", ";"),

    _separator_boundary_layout: ($) => $._horizontal_layout,

    separator: ($) =>
      choice(
        seq(
          field("operator", $.separator_op),
          optional(
            seq(
              optional($._horizontal_layout),
              field("linebreak", $.linebreak),
            ),
          ),
        ),
        field("newlines", $.newline_list),
      ),

    sequential_sep: ($) =>
      choice(
        seq(
          ";",
          optional(
            seq(
              optional($._horizontal_layout),
              field("linebreak", $.linebreak),
            ),
          ),
        ),
        field("newlines", $.newline_list),
      ),

    term: ($) =>
      seq(
        field("and_or", $.and_or),
        repeat(
          seq(
            optional($._separator_boundary_layout),
            field("separator", $.separator),
            optional($._horizontal_layout),
            field("and_or", $.and_or),
          ),
        ),
      ),

    compound_list: ($) =>
      choice(
        prec.dynamic(
          50,
          seq(
            field("leading", $.linebreak),
            optional($._horizontal_layout),
            terminatedCompoundList($),
          ),
        ),
        seq(
          field("leading", $.linebreak),
          optional($._horizontal_layout),
          field("body", $.term),
        ),
        prec.dynamic(
          50,
          seq(optional($._horizontal_layout), terminatedCompoundList($)),
        ),
        seq(optional($._horizontal_layout), field("body", $.term)),
        prec.dynamic(
          10,
          seq(
            field("leading", $.linebreak),
            optional($._horizontal_layout),
            field("body", alias($._closed_term, $.term)),
          ),
        ),
        prec.dynamic(
          10,
          seq(
            optional($._horizontal_layout),
            field("body", alias($._closed_term, $.term)),
          ),
        ),
      ),

    _reserved_compound_list: ($) =>
      prec.dynamic(
        20,
        seq(
          optional(field("leading", $.linebreak)),
          optional($._horizontal_layout),
          choice(
            terminatedCompoundList($),
            prec.dynamic(300, field("body", alias($._closed_term, $.term))),
            prec.dynamic(
              200,
              field("body", alias($._boundary_recovered_term, $.term)),
            ),
            prec.dynamic(
              100,
              seq(
                field("body", alias($._recoverable_term, $.term)),
                optional($._horizontal_layout),
                field("terminator", prec(100, $.separator_recovery)),
              ),
            ),
          ),
        ),
      ),

    _recovering_empty_compound_list: ($) =>
      prec.dynamic(
        -20,
        prec.right(
          seq(
            optional(field("leading", $.linebreak)),
            $._compound_list_boundary,
            optional($._horizontal_layout),
            recoveryField($),
          ),
        ),
      ),

    _closed_term: ($) =>
      seq(
        optional($._term_prefix),
        field("and_or", alias($._closed_and_or, $.and_or)),
      ),

    _term_prefix: ($) =>
      repeat1(
        seq(
          field("and_or", $.and_or),
          optional($._separator_boundary_layout),
          field("separator", $.separator),
          optional($._horizontal_layout),
        ),
      ),

    _recoverable_term: ($) =>
      seq(
        optional($._term_prefix),
        field("and_or", alias($._recoverable_and_or, $.and_or)),
      ),

    _boundary_recovered_term: ($) =>
      seq(
        optional($._term_prefix),
        field("and_or", alias($._boundary_recovered_and_or, $.and_or)),
      ),

    _closed_and_or: ($) =>
      seq(
        optional($._and_or_prefix),
        field("pipeline", alias($._closed_pipeline, $.pipeline)),
      ),

    _recoverable_and_or: ($) =>
      seq(
        optional($._and_or_prefix),
        field("pipeline", alias($._recoverable_pipeline, $.pipeline)),
      ),

    _and_or_prefix: ($) =>
      repeat1(
        seq(
          field("pipeline", $.pipeline),
          commandBoundaryLayout($),
          field("operator", choice($.and_if, $.or_if)),
          boundaryContinuedLinebreakLayout($),
        ),
      ),

    _boundary_recovered_and_or: ($) =>
      seq(
        $._and_or_prefix,
        field("pipeline", alias($._boundary_recovering_pipeline, $.pipeline)),
      ),

    _closed_pipeline: ($) =>
      seq(
        optional(
          seq(field("negation", $.bang), optional($._horizontal_layout)),
        ),
        field("sequence", alias($._closed_pipe_sequence, $.pipe_sequence)),
      ),

    _recoverable_pipeline: ($) =>
      seq(
        optional(
          seq(field("negation", $.bang), optional($._horizontal_layout)),
        ),
        field("sequence", alias($._recoverable_pipe_sequence, $.pipe_sequence)),
      ),

    _closed_pipe_sequence: ($) =>
      seq(
        optional($._pipe_sequence_prefix),
        field("command", alias($._closed_command, $.command)),
      ),

    _recoverable_pipe_sequence: ($) =>
      seq(
        optional($._pipe_sequence_prefix),
        field("command", alias($._recoverable_command, $.command)),
      ),

    _pipe_sequence_prefix: ($) =>
      repeat1(
        seq(
          field("command", $.command),
          commandBoundaryLayout($),
          "|",
          boundaryContinuedLinebreakLayout($),
        ),
      ),

    _closed_command: ($) =>
      prec.dynamic(
        5,
        choice(
          $._closed_redirectable_compound_command,
          field(
            "body",
            alias($._closed_function_definition, $.function_definition),
          ),
        ),
      ),

    _recoverable_command: ($) =>
      prec.dynamic(
        -50,
        seq(
          field("body", $.simple_command),
          ownedContinuationBoundaryLayout($, $._closed_simple_command_end),
        ),
      ),

    and_if: (_) => "&&",

    or_if: (_) => "||",

    bang: ($) => $._bang_token,

    if_keyword: ($) => reservedWord($, $._if_keyword),

    then_keyword: ($) => reservedWord($, $._then_keyword),

    elif_keyword: ($) => reservedWord($, $._elif_keyword),

    else_keyword: ($) => reservedWord($, $._else_keyword),

    fi_keyword: ($) => reservedWord($, $._fi_keyword),

    for_keyword: ($) => reservedWord($, $._for_keyword),

    in_keyword: ($) => reservedWord($, $._in_keyword),

    do_keyword: ($) => reservedWord($, $._do_keyword),

    done_keyword: ($) => reservedWord($, $._done_keyword),

    case_keyword: ($) => reservedWord($, $._case_keyword),

    esac_keyword: ($) => reservedWord($, $._esac_keyword),

    while_keyword: ($) => reservedWord($, $._while_keyword),

    until_keyword: ($) => reservedWord($, $._until_keyword),

    function_definition: ($) =>
      choice(
        functionDefinitionWithBody($, $.function_body),
        recoveringFunctionDefinition($),
      ),

    _closed_function_definition: ($) =>
      choice(
        functionDefinitionWithBody(
          $,
          alias($._closed_function_body, $.function_body),
        ),
        prec.right(1, recoveringFunctionDefinition($, $._closed_command_end)),
      ),

    function_body: ($) => $._redirectable_compound_command,

    _closed_function_body: ($) => $._closed_redirectable_compound_command,

    _recovering_function_body: ($) =>
      field(
        "recovery",
        alias($._function_body_recovery_boundary, $.compound_command_recovery),
      ),

    fname: ($) => $._name_token,

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

    compound_command_recovery: ($) =>
      prec.dynamic(-50, $._compound_command_recovery_boundary),

    redirect_list: ($) => redirectList($),

    brace_group: ($) =>
      seq(
        alias($._left_brace, "{"),
        choice(
          prec(
            100,
            seq(
              field("body", alias($._reserved_compound_list, $.compound_list)),
              optional($._horizontal_layout),
              alias($._right_brace, "}"),
            ),
          ),
          prec.dynamic(
            -50,
            choice(
              prec.right(
                20,
                seq(
                  emptyCompoundListField($, "body"),
                  choice(alias($._right_brace, "}"), recoveryField($)),
                ),
              ),
              seq(
                compoundListField($, "body"),
                optional($._horizontal_layout),
                recoveryField($),
              ),
            ),
          ),
        ),
      ),

    subshell: ($) =>
      seq(
        "(",
        choice(
          prec(
            100,
            seq(
              field("body", $.compound_list),
              optional($._horizontal_layout),
              ")",
            ),
          ),
          prec.dynamic(
            -50,
            prec.right(
              choice(
                prec.right(
                  20,
                  seq(
                    emptyCompoundListField($, "body"),
                    choice(")", subshellRecoveryField($)),
                  ),
                ),
                seq(
                  field("body", $.compound_list),
                  optional($._horizontal_layout),
                  subshellRecoveryField($),
                ),
              ),
            ),
          ),
        ),
      ),

    for_clause: ($) =>
      seq(
        $.for_keyword,
        choice(
          headerRecoveryField($),
          seq(
            $._word_separator,
            choice(
              seq(field("name", $.name), recoveringForTail($)),
              headerRecoveryField($),
            ),
          ),
        ),
      ),

    in: ($) => $.in_keyword,

    wordlist: ($) =>
      seq(
        field("word", $.word),
        repeat(seq($._word_separator, field("word", $.word))),
      ),

    do_group: ($) =>
      seq(
        $.do_keyword,
        choice(
          prec.dynamic(
            -50,
            prec.right(
              20,
              seq(
                emptyCompoundListField($, "body"),
                choice($.done_keyword, recoveryField($)),
              ),
            ),
          ),
          seq(
            compoundListField($, "body"),
            optional($._horizontal_layout),
            choice(
              prec(20, $.done_keyword),
              prec.dynamic(-50, recoveryField($)),
            ),
          ),
        ),
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
          seq(
            $.else_keyword,
            choice(recoveryField($), recoverableCompoundListField($, "body")),
          ),
        ),
      ),

    while_clause: ($) => loopClause($, $.while_keyword),

    until_clause: ($) => loopClause($, $.until_keyword),

    case_clause: ($) =>
      seq(
        $.case_keyword,
        choice(
          headerRecoveryField($),
          seq(
            $._word_separator,
            choice(
              headerRecoveryField($),
              seq(
                field("word", $.word),
                choice(
                  seq(optional($._horizontal_layout), recoveryField($)),
                  seq(
                    reservedWordLinebreak($),
                    choice(
                      recoveryField($),
                      seq(
                        field("in", $.in),
                        linebreakLayout($),
                        caseClauseItems($),
                      ),
                    ),
                  ),
                ),
              ),
            ),
          ),
        ),
      ),

    case_list_ns: ($) =>
      choice(
        field("item", $.case_item_ns),
        seq(field("terminated", $.case_list), field("item", $.case_item_ns)),
      ),

    _recovering_case_list_ns: ($) =>
      field("item", alias($._recovering_case_item_ns, $.case_item_ns)),

    case_list: ($) => repeat1(field("item", $.case_item)),

    case_item_ns: ($) =>
      seq(
        field("patterns", $.pattern_list),
        patternClosingLayout($),
        ")",
        choice(
          seq(
            optional(seq(optional($._horizontal_layout), $.linebreak)),
            $._case_item_ns_boundary,
          ),
          prec.dynamic(
            10,
            seq(
              field(
                "body",
                alias($._case_item_ns_compound_list, $.compound_list),
              ),
              $._case_item_ns_boundary,
            ),
          ),
        ),
      ),

    _case_item_ns_compound_list: ($) =>
      prec.dynamic(
        20,
        seq(
          optional(field("leading", $.linebreak)),
          optional($._horizontal_layout),
          choice(
            terminatedCompoundList($),
            prec.dynamic(300, field("body", alias($._closed_term, $.term))),
          ),
        ),
      ),

    _recovering_case_item_ns: ($) =>
      seq(
        field("patterns", $.pattern_list),
        choice(
          seq(optional($._horizontal_layout), directRecoveryField($)),
          seq(
            patternClosingLayout($),
            ")",
            choice(
              seq(linebreakLayout($), directRecoveryField($)),
              prec.dynamic(
                10,
                seq(field("body", $.compound_list), directRecoveryField($)),
              ),
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
        continuationBoundaryLayout($, $._case_item_end),
        field("terminator", choice($.dsemi, $.semi_and)),
        prec.right(1, optional(caseItemFollowingLayout($))),
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
            seq($._word_separator, field("assignment", $.assignment_word)),
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

    filename: ($) =>
      choice(
        field("word", $.word),
        field("recovery", $.redirection_target_recovery),
      ),

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
      choice(
        seq(
          $._here_end_begin,
          field("word", alias($._here_end_source_word, $.word)),
          $._here_end_commit,
        ),
        field("recovery", $.missing_here_end),
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

    missing_here_end: ($) => $._missing_here_document_delimiter,

    here_document_sequence: ($) =>
      seq(
        optional(
          choice(
            lineComment($, field("comment", $.comment)),
            $._horizontal_layout,
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
          field(
            "end",
            choice($.here_document_end, $.here_document_end_recovery),
          ),
        ),
        seq(
          $._quoted_here_document_body_start,
          optional(field("body", $.quoted_here_document_body)),
          field(
            "end",
            choice($.here_document_end, $.here_document_end_recovery),
          ),
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
        choice(
          $.pattern_bracket_source,
          alias(
            $._word_special_prefixed_bracket_source,
            $.pattern_bracket_source,
          ),
        ),
        $._word_incomplete_bracket_literal_run,
        [
          $._pattern_special_literal_start,
          alias($._word_incomplete_bracket_literal_text, $.literal),
        ],
        $.line_continuation,
      ),

    _word_special_prefixed_bracket_source: ($) =>
      patternSpecialPrefixedExpression($, $._pattern_special_prefixed_members),

    _word_incomplete_bracket_literal_text: ($) =>
      prec.right(1, repeat1(wordIncompleteBracketLiteralAtom($))),

    _word_incomplete_bracket_literal_run: ($) =>
      prec.dynamic(
        PATTERN_PRECEDENCE.literalFallback,
        prec.right(
          1,
          seq(
            $._pattern_bracket_left,
            repeat(
              choice(
                $._pattern_bracket_left,
                wordIncompleteBracketLiteralAtom($),
              ),
            ),
          ),
        ),
      ),

    pattern_star_source: (_) => token(prec(-1, "*")),

    pattern_question_source: (_) => token(prec(-1, "?")),

    _word_bracket_literal_fallback: ($) =>
      prec.dynamic(
        PATTERN_PRECEDENCE.literalFallback,
        prec.right(
          1,
          seq(
            alias($._literal_left_bracket, $.literal),
            optional(alias($._pattern_initial_right_bracket, $.literal)),
            repeat1($._word_bracket_literal_fallback_part),
            choice(
              $._word_bracket_fallback_end,
              alias($._literal_right_bracket, $.literal),
            ),
          ),
        ),
      ),

    _word_bracket_literal_fallback_part: ($) =>
      prec.right(
        incompleteBracketLiteralPart(
          $,
          choice(
            $.pattern_bracket_source,
            alias(
              $._word_special_prefixed_bracket_source,
              $.pattern_bracket_source,
            ),
          ),
          $._word_bracket_literal_run,
          [$._pattern_special_literal_start],
          $.line_continuation,
        ),
      ),

    _word_bracket_literal_run: ($) =>
      repeat1(
        choice(
          $._pattern_bracket_character_token,
          $._pattern_bracket_hyphen_token,
          $._pattern_bracket_left,
          $._pattern_bracket_exclamation,
          $._pattern_character_class_colon,
          $._pattern_collating_dot,
          $._pattern_equivalence_equals,
        ),
      ),

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
        $.pattern_character_class_source,
        $.pattern_collating_symbol_source,
        $.pattern_equivalence_class_source,
        $.pattern_bracket_range_source,
        $.pattern_bracket_character_source,
        $._pattern_operator_bracket_character,
        $.pattern_bracket_hyphen_source,
        ...structuredSourceParts($),
      ),

    _parameter_pattern_bracket_member: ($) =>
      choice(
        $.pattern_character_class_source,
        $.pattern_collating_symbol_source,
        $.pattern_equivalence_class_source,
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
        ...structuredSourceParts($),
      ),

    _parameter_pattern_bracket_range_endpoint: ($) =>
      choice(
        alias(
          $._parameter_pattern_bracket_character,
          $.pattern_bracket_character_source,
        ),
        $._pattern_operator_bracket_character,
        $.pattern_collating_symbol_source,
        $.pattern_bracket_hyphen_source,
        ...structuredSourceParts($),
      ),

    pattern_bracket_character_source: ($) =>
      choice(
        $._pattern_bracket_character_token,
        $._pattern_bracket_left,
        $._pattern_bracket_exclamation,
        $._pattern_character_class_colon,
        $._pattern_collating_dot,
        $._pattern_equivalence_equals,
      ),

    _parameter_pattern_bracket_character: ($) =>
      choice(
        $._parameter_pattern_bracket_character_token,
        $._pattern_bracket_left,
        $._pattern_bracket_exclamation,
        $._pattern_character_class_colon,
        $._pattern_collating_dot,
        $._pattern_equivalence_equals,
      ),

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
      ),

    _pattern_special_prefixed_members: ($) =>
      patternSpecialPrefixedList(
        $,
        $._pattern_deferred_member,
        $._pattern_deferred_initial_range,
      ),

    _parameter_pattern_deferred_bracket_character: ($) =>
      choice(
        $._pattern_deferred_bracket_character,
        $._parameter_deferred_extra_character,
      ),

    _parameter_pattern_deferred_range_endpoint: ($) =>
      patternDeferredBracketRangeEndpoint(
        $,
        $._parameter_pattern_deferred_bracket_character,
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
      prec.dynamic(
        PATTERN_PRECEDENCE.specialElement,
        seq(
          patternSpecialStart($, $._pattern_character_class_colon),
          patternCharacterClassBody($),
          patternSpecialEnd($, $._pattern_character_class_colon),
        ),
      ),

    pattern_character_class_content_source: ($) =>
      prec.right(
        1,
        repeat1(
          choice(
            $._pattern_special_plain_character,
            $._pattern_special_marker_character,
            $._pattern_bracket_hyphen_token,
            $._pattern_special_left_bracket,
          ),
        ),
      ),

    _pattern_character_class_colon: (_) => token.immediate(prec(-3, ":")),

    pattern_collating_symbol_source: ($) =>
      patternSpecialClassSource(
        $,
        $._pattern_collating_dot,
        $.pattern_collating_symbol_character_source,
      ),

    pattern_collating_symbol_character_source: ($) =>
      choice(
        $._pattern_special_plain_character,
        $._pattern_special_marker_character,
        $._pattern_bracket_hyphen_token,
        $._pattern_special_left_bracket,
        $._literal_right_bracket,
      ),

    _pattern_collating_dot: (_) => token.immediate(prec(-2, ".")),

    pattern_equivalence_class_source: ($) =>
      patternSpecialClassSource(
        $,
        $._pattern_equivalence_equals,
        $.pattern_equivalence_class_character_source,
      ),

    pattern_equivalence_class_character_source: ($) =>
      choice(
        $._pattern_special_plain_character,
        $._pattern_special_marker_character,
        $._pattern_bracket_hyphen_token,
        $._pattern_special_left_bracket,
      ),

    _pattern_equivalence_equals: (_) => token.immediate(prec(-2, "=")),

    escaped_character: (_) => token(seq("\\", /[^\n]/)),

    single_quoted: ($) =>
      seq(
        "'",
        optional($.single_quote_content),
        choice("'", hereDocumentBoundaryRecovery($)),
      ),

    single_quote_content: ($) =>
      repeat1(choice(token.immediate(/[^'\n]+/), $._newline)),

    double_quoted: ($) =>
      seq(
        '"',
        repeat(doubleQuotedPart($)),
        choice('"', hereDocumentBoundaryRecovery($)),
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
          choice("'", hereDocumentBoundaryRecovery($)),
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
      choice(
        parameterExpansionTail($, $.parameter_word),
        recoveringParameterExpansionTail(
          $,
          $.parameter_word,
          $._input_end_recovery,
        ),
      ),

    _double_quoted_parameter_expansion_tail: ($) =>
      choice(
        parameterExpansionTail(
          $,
          alias($._double_quoted_parameter_word, $.parameter_word),
        ),
        recoveringParameterExpansionTail(
          $,
          alias($._double_quoted_parameter_word, $.parameter_word),
          $._double_quoted_parameter_tail_recovery_boundary,
        ),
      ),

    parameter_expansion_recovery: ($) => $._input_end_recovery,

    _parameter_missing_recovery: ($) => $._parameter_missing_recovery_boundary,

    _parameter_operator_recovery: ($) =>
      seq(
        ":",
        repeat($.line_continuation),
        $._parameter_operator_recovery_boundary,
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

    parameter_word: ($) =>
      choice(
        $._parameter_tilde_source,
        seq($._parameter_pattern_part, optional($._parameter_source_tail)),
      ),

    _double_quoted_parameter_word: ($) =>
      seq(
        $._double_quoted_parameter_word_part,
        repeat($._double_quoted_parameter_word_part),
      ),

    // POSIX 2.6.2: a '}' within a quoted string is not examined when finding
    // the matching '}', so a double-quote inside the word opens a nested
    // quoted region even when the expansion itself is double-quoted.
    _double_quoted_parameter_word_part: ($) =>
      choice(
        alias($._double_quoted_parameter_text, $.double_quote_text),
        alias($._double_quoted_parameter_escape, $.double_quote_escape),
        alias($._newline, $.double_quote_text),
        $.double_quoted,
        alias($._double_quoted_parameter_expansion, $.parameter_expansion),
        $.command_substitution,
        $.arithmetic_expansion,
        $.backquote_substitution,
      ),

    parameter_pattern: ($) =>
      prec.right(
        1,
        choice(
          $._parameter_tilde_source,
          seq($._parameter_pattern_part, optional($._parameter_source_tail)),
        ),
      ),

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
        choice(
          alias(
            $._parameter_pattern_bracket_expression,
            $.pattern_bracket_source,
          ),
          alias(
            $._parameter_special_prefixed_bracket_source,
            $.pattern_bracket_source,
          ),
        ),
        $._parameter_incomplete_bracket_literal_run,
        [
          $._pattern_special_literal_start,
          alias($._parameter_incomplete_bracket_literal_text, $.literal),
        ],
        $.line_continuation,
      ),

    _parameter_special_prefixed_bracket_source: ($) =>
      patternSpecialPrefixedExpression(
        $,
        $._parameter_pattern_special_prefixed_members,
      ),

    _parameter_incomplete_bracket_literal_text: ($) =>
      prec.right(1, repeat1(parameterIncompleteBracketLiteralAtom($))),

    _parameter_incomplete_bracket_literal_run: ($) =>
      prec.dynamic(
        PATTERN_PRECEDENCE.literalFallback,
        prec.right(
          1,
          seq(
            $._pattern_bracket_left,
            repeat(
              choice(
                $._pattern_bracket_left,
                parameterIncompleteBracketLiteralAtom($),
              ),
            ),
          ),
        ),
      ),

    _parameter_bracket_literal_fallback: ($) =>
      prec.dynamic(
        PATTERN_PRECEDENCE.literalFallback,
        prec.right(
          1,
          seq(
            alias($._literal_left_bracket, $.literal),
            optional(alias($._pattern_initial_right_bracket, $.literal)),
            repeat1($._parameter_bracket_literal_fallback_part),
            choice(
              $._parameter_bracket_fallback_end,
              alias($._literal_right_bracket, $.literal),
            ),
          ),
        ),
      ),

    _parameter_bracket_literal_fallback_part: ($) =>
      prec.right(
        incompleteBracketLiteralPart(
          $,
          choice(
            alias(
              $._parameter_pattern_bracket_expression,
              $.pattern_bracket_source,
            ),
            alias(
              $._parameter_special_prefixed_bracket_source,
              $.pattern_bracket_source,
            ),
          ),
          $._parameter_bracket_literal_run,
          [$._pattern_special_literal_start],
          $.line_continuation,
        ),
      ),

    _parameter_bracket_literal_run: ($) =>
      repeat1(
        choice(
          $._parameter_pattern_bracket_character_token,
          $._pattern_bracket_hyphen_token,
          $._pattern_bracket_left,
          $._pattern_bracket_exclamation,
          $._pattern_character_class_colon,
          $._pattern_collating_dot,
          $._pattern_equivalence_equals,
        ),
      ),

    _parameter_pattern_literal: ($) => prec.right(parameterPlainChunk($)),

    _double_quoted_parameter_text: ($) =>
      prec.right(choice("$", "\\", $._double_quoted_parameter_text_chunk)),

    _double_quoted_parameter_text_chunk: (_) =>
      token.immediate(prec(-1, /[^}"$`\\\n]+/)),

    _double_quoted_parameter_escape: (_) =>
      token.immediate(seq("\\", choice("$", "`", '"', "\\", "}"))),

    command_substitution: ($) =>
      commandSubstitution($, $._command_or_arithmetic_substitution_start),

    _command_substitution_start: ($) =>
      choice("$(", seq(backquoteDollar($), "(")),

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
      prec.left(commandSubstitutionSequenceBody($)),

    backquote_substitution: ($) =>
      seq(
        backquoteDelimiter($._backquote_start, $._backquote_start_prefix),
        optional(field("body", $.backquote_substitution_body)),
        choice(
          seq(
            closedCommandBoundaryLayout($),
            backquoteDelimiter($._backquote_end, $._backquote_end_prefix),
          ),
          $.backquote_end_recovery,
        ),
      ),

    backquote_substitution_body: ($) => $._substitution_body,

    _substitution_body: ($) => prec.left(commandSequenceBody($)),

    _arithmetic_expansion_start: ($) =>
      arithmeticExpansionStart($, $._arithmetic_left_parenthesis),

    _arithmetic_dynamic_expansion_start: ($) =>
      arithmeticExpansionStart($, $._arithmetic_dynamic_left_parenthesis),

    _arithmetic_incomplete_expansion_start: ($) =>
      arithmeticExpansionStart($, $._arithmetic_incomplete_left_parenthesis),

    // The external scanner settles the arithmetic reading at the second left
    // parenthesis, so these variants are mutually exclusive: a structured
    // expression, a flat source run around runtime fragments, and an
    // incomplete expansion recovering at the end of its input or at the
    // boundary of an enclosing here-document. Racing the readings instead
    // would let an edited tree keep flat-reading subtrees that an incremental
    // reparse of the restored source reuses over the structured reading.
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
        seq(
          $._arithmetic_incomplete_expansion_start,
          optional($._arithmetic_layout),
          repeat(
            seq(
              choice($._arithmetic_source_part, $._arithmetic_runtime_fragment),
              optional($._arithmetic_layout),
            ),
          ),
          choice($._input_end_recovery, hereDocumentBoundaryRecovery($)),
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

    arithmetic_variable: ($) =>
      seq(optional($._name_equals_begin), $._name_token),

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
        repeat($.line_continuation),
        alias(
          choice(
            $._pattern_character_class_colon,
            $._pattern_collating_dot,
            $._pattern_equivalence_equals,
          ),
          $.literal,
        ),
      ),

    _pattern_special_literal_left: ($) =>
      alias($._pattern_special_left_bracket, $.literal),

    _pattern_special_plain_character: (_) =>
      token.immediate(prec(-2, PATTERN_SPECIAL_PLAIN_CHARACTER_PATTERN)),

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
      repeat1(
        choice(
          $.here_document_sequence,
          $._newline,
          $._blank_line,
          $._continued_blank_line,
          $._comment_line,
        ),
      ),

    linebreak: ($) => $.newline_list,

    _horizontal_layout: ($) =>
      prec.right(1, repeat1(choice($._blank, prec(2, $.line_continuation)))),

    _word_separator: ($) =>
      prec.right(
        2,
        choice(
          seq($._blank, repeat(choice($._blank, $.line_continuation))),
          continuedWordSeparator($),
        ),
      ),

    _free_trailing_layout: ($) =>
      prec.right(1, choice($._horizontal_layout, $._free_comment)),

    _comment_line: ($) => seq($._free_comment, $._newline),

    _continued_blank_line: ($) =>
      prec.dynamic(2, seq(continuedBlankLineLayout($), $._newline)),

    _blank_line: ($) => seq($._blank, $._newline),

    _free_comment: ($) => prec.right(1, lineComment($, $.comment)),

    _blank: (_) => /[ \t]+/,
  },
});
