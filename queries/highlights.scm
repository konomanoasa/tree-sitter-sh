[
  (literal)
  (single_quote_content)
  (double_quote_text)
  (dollar_single_quote_text)
  (here_document_text)
  (quoted_here_document_text)
] @string

[
  (escaped_character)
  (double_quote_escape)
  (dollar_single_quote_escape)
  (here_document_escape)
] @string.escape

(comment) @comment

(line_continuation) @punctuation.special

[
  (arithmetic_number)
  (io_number)
] @number

[
  (arithmetic_variable)
  (variable_name)
  (name)
  (io_location)
] @variable

(positional_parameter) @variable.parameter

(special_parameter) @variable.builtin

(fname) @function

[
  (if_keyword)
  (then_keyword)
  (elif_keyword)
  (else_keyword)
  (fi_keyword)
  (for_keyword)
  (in_keyword)
  (do_keyword)
  (done_keyword)
  (case_keyword)
  (esac_keyword)
  (while_keyword)
  (until_keyword)
] @keyword

[
  (arithmetic_operator)
  (parameter_length_operator)
  (parameter_value_operator)
  (parameter_pattern_operator)
  (and_if)
  (or_if)
  (bang)
  (dsemi)
  (semi_and)
] @operator

[
  (lessand)
  (greatand)
  (dgreat)
  (lessgreat)
  (clobber)
  (dless)
  (dlessdash)
] @operator

(io_file
  operator: [
    "<"
    ">"
  ] @operator)

(assignment_word
  "=" @operator)

[
  "&"
  "|"
] @operator

";" @punctuation.delimiter

[
  "("
  ")"
  "{"
  "}"
  "$("
  "${"
] @punctuation.bracket

(parameter_expansion
  "$" @constant)

(single_quoted
  "'" @punctuation.delimiter)

(double_quoted
  "\"" @punctuation.delimiter)

(dollar_single_quoted
  [
    "$'"
    "'"
  ] @punctuation.delimiter)

(backquote_substitution
  "`" @punctuation.delimiter)

(tilde_expansion
  "~" @string.special.path)

(tilde_expansion
  user: (tilde_user
    (literal) @string.special.path))

(here_end
  word: (word
    (literal) @label))

(here_end
  word: (word
    (single_quoted
      (single_quote_content) @label)))

(here_end
  word: (word
    (double_quoted
      (double_quote_text) @label)))

(here_document_end) @label

(cmd_name
  (word
    (literal) @function.call))

(cmd_word
  (word
    (literal) @function.call))

(parameter_pattern
  [
    (literal) @string.regexp
    (pattern_star_source) @character.special
    (pattern_question_source) @character.special
  ])

(pattern_list
  (word
    [
      (literal) @string.regexp
      (pattern_star_source) @character.special
      (pattern_question_source) @character.special
    ]))

(cmd_name
  (word
    [
      (pattern_star_source) @character.special
      (pattern_question_source) @character.special
    ]))

(cmd_word
  (word
    [
      (pattern_star_source) @character.special
      (pattern_question_source) @character.special
    ]))

(cmd_suffix
  word: (word
    [
      (pattern_star_source) @character.special
      (pattern_question_source) @character.special
    ]))

(wordlist
  word: (word
    [
      (pattern_star_source) @character.special
      (pattern_question_source) @character.special
    ]))

(filename
  word: (word
    [
      (pattern_star_source) @character.special
      (pattern_question_source) @character.special
    ]))

[
  (cmd_name
    (word
      (literal) @string.regexp
      [
        (pattern_star_source)
        (pattern_question_source)
        (pattern_bracket_source)
      ]))
  (cmd_name
    (word
      [
        (pattern_star_source)
        (pattern_question_source)
        (pattern_bracket_source)
      ]
      (literal) @string.regexp))
  (cmd_word
    (word
      (literal) @string.regexp
      [
        (pattern_star_source)
        (pattern_question_source)
        (pattern_bracket_source)
      ]))
  (cmd_word
    (word
      [
        (pattern_star_source)
        (pattern_question_source)
        (pattern_bracket_source)
      ]
      (literal) @string.regexp))
  (cmd_suffix
    word: (word
      (literal) @string.regexp
      [
        (pattern_star_source)
        (pattern_question_source)
        (pattern_bracket_source)
      ]))
  (cmd_suffix
    word: (word
      [
        (pattern_star_source)
        (pattern_question_source)
        (pattern_bracket_source)
      ]
      (literal) @string.regexp))
  (wordlist
    word: (word
      (literal) @string.regexp
      [
        (pattern_star_source)
        (pattern_question_source)
        (pattern_bracket_source)
      ]))
  (wordlist
    word: (word
      [
        (pattern_star_source)
        (pattern_question_source)
        (pattern_bracket_source)
      ]
      (literal) @string.regexp))
  (filename
    word: (word
      (literal) @string.regexp
      [
        (pattern_star_source)
        (pattern_question_source)
        (pattern_bracket_source)
      ]))
  (filename
    word: (word
      [
        (pattern_star_source)
        (pattern_question_source)
        (pattern_bracket_source)
      ]
      (literal) @string.regexp))
]

(pattern_list
  (word
    (pattern_bracket_source
      "[" @punctuation.bracket
      (pattern_bracket_negation_source)? @punctuation.special
      (pattern_bracket_members_source
        [
          (pattern_bracket_character_source) @character
          (pattern_bracket_hyphen_source) @character
          (pattern_bracket_range_source
            start: [
              (pattern_bracket_character_source)
              (pattern_bracket_hyphen_source)
            ] @character
            operator: (pattern_bracket_range_operator_source) @punctuation.special
            end: [
              (pattern_bracket_character_source)
              (pattern_bracket_hyphen_source)
            ] @character)
          (pattern_character_class_source
            "[" @punctuation.bracket
            ":" @punctuation.bracket
            content: (pattern_character_class_content_source) @character.special
            ":" @punctuation.bracket
            "]" @punctuation.bracket)
          (pattern_collating_symbol_source) @character.special
          (pattern_equivalence_class_source) @character.special
        ])
      "]" @punctuation.bracket)))

(parameter_pattern
  (pattern_bracket_source
    "[" @punctuation.bracket
    (pattern_bracket_negation_source)? @punctuation.special
    (pattern_bracket_members_source
      [
        (pattern_bracket_character_source) @character
        (pattern_bracket_hyphen_source) @character
        (pattern_bracket_range_source
          start: [
            (pattern_bracket_character_source)
            (pattern_bracket_hyphen_source)
          ] @character
          operator: (pattern_bracket_range_operator_source) @punctuation.special
          end: [
            (pattern_bracket_character_source)
            (pattern_bracket_hyphen_source)
          ] @character)
        (pattern_character_class_source
          "[" @punctuation.bracket
          ":" @punctuation.bracket
          content: (pattern_character_class_content_source) @character.special
          ":" @punctuation.bracket
          "]" @punctuation.bracket)
        (pattern_collating_symbol_source) @character.special
        (pattern_equivalence_class_source) @character.special
      ])
    "]" @punctuation.bracket))

(cmd_name
  (word
    (pattern_bracket_source
      "[" @punctuation.bracket
      (pattern_bracket_negation_source)? @punctuation.special
      (pattern_bracket_members_source
        [
          (pattern_bracket_character_source) @character
          (pattern_bracket_hyphen_source) @character
          (pattern_bracket_range_source
            start: [
              (pattern_bracket_character_source)
              (pattern_bracket_hyphen_source)
            ] @character
            operator: (pattern_bracket_range_operator_source) @punctuation.special
            end: [
              (pattern_bracket_character_source)
              (pattern_bracket_hyphen_source)
            ] @character)
          (pattern_character_class_source
            "[" @punctuation.bracket
            ":" @punctuation.bracket
            content: (pattern_character_class_content_source) @character.special
            ":" @punctuation.bracket
            "]" @punctuation.bracket)
          (pattern_collating_symbol_source) @character.special
          (pattern_equivalence_class_source) @character.special
        ])
      "]" @punctuation.bracket)))

(cmd_word
  (word
    (pattern_bracket_source
      "[" @punctuation.bracket
      (pattern_bracket_negation_source)? @punctuation.special
      (pattern_bracket_members_source
        [
          (pattern_bracket_character_source) @character
          (pattern_bracket_hyphen_source) @character
          (pattern_bracket_range_source
            start: [
              (pattern_bracket_character_source)
              (pattern_bracket_hyphen_source)
            ] @character
            operator: (pattern_bracket_range_operator_source) @punctuation.special
            end: [
              (pattern_bracket_character_source)
              (pattern_bracket_hyphen_source)
            ] @character)
          (pattern_character_class_source
            "[" @punctuation.bracket
            ":" @punctuation.bracket
            content: (pattern_character_class_content_source) @character.special
            ":" @punctuation.bracket
            "]" @punctuation.bracket)
          (pattern_collating_symbol_source) @character.special
          (pattern_equivalence_class_source) @character.special
        ])
      "]" @punctuation.bracket)))

(cmd_suffix
  word: (word
    (pattern_bracket_source
      "[" @punctuation.bracket
      (pattern_bracket_negation_source)? @punctuation.special
      (pattern_bracket_members_source
        [
          (pattern_bracket_character_source) @character
          (pattern_bracket_hyphen_source) @character
          (pattern_bracket_range_source
            start: [
              (pattern_bracket_character_source)
              (pattern_bracket_hyphen_source)
            ] @character
            operator: (pattern_bracket_range_operator_source) @punctuation.special
            end: [
              (pattern_bracket_character_source)
              (pattern_bracket_hyphen_source)
            ] @character)
          (pattern_character_class_source
            "[" @punctuation.bracket
            ":" @punctuation.bracket
            content: (pattern_character_class_content_source) @character.special
            ":" @punctuation.bracket
            "]" @punctuation.bracket)
          (pattern_collating_symbol_source) @character.special
          (pattern_equivalence_class_source) @character.special
        ])
      "]" @punctuation.bracket)))

(wordlist
  word: (word
    (pattern_bracket_source
      "[" @punctuation.bracket
      (pattern_bracket_negation_source)? @punctuation.special
      (pattern_bracket_members_source
        [
          (pattern_bracket_character_source) @character
          (pattern_bracket_hyphen_source) @character
          (pattern_bracket_range_source
            start: [
              (pattern_bracket_character_source)
              (pattern_bracket_hyphen_source)
            ] @character
            operator: (pattern_bracket_range_operator_source) @punctuation.special
            end: [
              (pattern_bracket_character_source)
              (pattern_bracket_hyphen_source)
            ] @character)
          (pattern_character_class_source
            "[" @punctuation.bracket
            ":" @punctuation.bracket
            content: (pattern_character_class_content_source) @character.special
            ":" @punctuation.bracket
            "]" @punctuation.bracket)
          (pattern_collating_symbol_source) @character.special
          (pattern_equivalence_class_source) @character.special
        ])
      "]" @punctuation.bracket)))

(filename
  word: (word
    (pattern_bracket_source
      "[" @punctuation.bracket
      (pattern_bracket_negation_source)? @punctuation.special
      (pattern_bracket_members_source
        [
          (pattern_bracket_character_source) @character
          (pattern_bracket_hyphen_source) @character
          (pattern_bracket_range_source
            start: [
              (pattern_bracket_character_source)
              (pattern_bracket_hyphen_source)
            ] @character
            operator: (pattern_bracket_range_operator_source) @punctuation.special
            end: [
              (pattern_bracket_character_source)
              (pattern_bracket_hyphen_source)
            ] @character)
          (pattern_character_class_source
            "[" @punctuation.bracket
            ":" @punctuation.bracket
            content: (pattern_character_class_content_source) @character.special
            ":" @punctuation.bracket
            "]" @punctuation.bracket)
          (pattern_collating_symbol_source) @character.special
          (pattern_equivalence_class_source) @character.special
        ])
      "]" @punctuation.bracket)))
