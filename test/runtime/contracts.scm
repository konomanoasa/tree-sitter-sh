(line_continuation) @line.continuation

(function_definition
  name: (fname) @function)

(if_clause
  recovery: (compound_command_recovery) @compound.recovery) @compound.owner

(do_group
  recovery: (compound_command_recovery) @compound.recovery) @compound.owner

(case_clause
  recovery: (compound_command_recovery) @compound.recovery) @compound.owner

(for_clause
  recovery: (compound_command_recovery) @compound.recovery) @compound.owner

(while_clause
  recovery: (compound_command_recovery) @compound.recovery) @compound.owner

(until_clause
  recovery: (compound_command_recovery) @compound.recovery) @compound.owner

(brace_group
  recovery: (compound_command_recovery) @group.recovery) @group.owner

(subshell
  recovery: (compound_command_recovery) @group.recovery) @group.owner

(parameter_expansion
  recovery: (parameter_expansion_recovery) @parameter.recovery) @parameter.owner
