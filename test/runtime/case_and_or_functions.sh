sample_log() {
  cat <<'SAMPLE'
run: 2026-08-01T15:24:00Z
suite: checkout-api
check: creates order | 142ms | PASS
check: rejects expired card | 87ms | FAIL
failure: rejects expired card: expected 422, got 500
SAMPLE
}

summarize() {
  run_value=
  suite_value=
  pass_count=0
  fail_count=0
  total_ms=0

  printf '%s\n' 'CI test summary'

  while IFS= read -r line || [ -n "$line" ]; do
    case $line in
    'run: '*)
      [ -z "$run_value" ] || invalid_input 'duplicate run'
      run_value=$(printf '%s\n' "$line" | sed 's/^run: //')
      [ -n "$run_value" ] || invalid_input 'empty run'
      printf 'run: %s\n' "$run_value"
      ;;
    'suite: '*)
      [ -n "$run_value" ] || invalid_input 'suite before run'
      [ -z "$suite_value" ] || invalid_input 'duplicate suite'
      suite_value=$(printf '%s\n' "$line" | sed 's/^suite: //')
      [ -n "$suite_value" ] || invalid_input 'empty suite'
      printf 'suite: %s\n' "$suite_value"
      ;;
    'check: '*)
      [ -n "$suite_value" ] || invalid_input 'check before suite'
      check_data=$(printf '%s\n' "$line" | sed 's/^check: //')
      IFS='|' read -r check_name check_duration check_status <<EOF
$check_data
EOF
      check_name=$(printf '%s\n' "$check_name" | trim)
      check_duration=$(printf '%s\n' "$check_duration" | trim)
      check_status=$(printf '%s\n' "$check_status" | trim)
      [ "$check_data" = "$check_name | $check_duration | $check_status" ] || invalid_input 'malformed check'
      case $check_duration in
      *ms) duration_value=$(printf '%s\n' "$check_duration" | sed 's/ms$//') ;;
      *) invalid_input 'check duration must end in ms' ;;
      esac
      case $duration_value in
      '' | *[!0-9]*) invalid_input 'check duration must be an integer' ;;
      esac
      case $check_status in
      PASS)
        pass_count=$((pass_count + 1))
        ;;
      FAIL)
        fail_count=$((fail_count + 1))
        ;;
      *)
        invalid_input 'check status must be PASS or FAIL'
        ;;
      esac
      total_ms=$((total_ms + duration_value))
      printf '%s %s (%sms)\n' "$check_status" "$check_name" "$duration_value"
      ;;
    'failure: '*)
      [ "$fail_count" -gt 0 ] || invalid_input 'failure without a failed check'
      failure_data=$(printf '%s\n' "$line" | sed 's/^failure: //')
      IFS=: read -r failure_name failure_message <<EOF
$failure_data
EOF
      failure_name=$(printf '%s\n' "$failure_name" | trim)
      failure_message=$(printf '%s\n' "$failure_message" | trim)
      [ "$failure_data" = "$failure_name: $failure_message" ] || invalid_input 'malformed failure'
      [ -n "$failure_name" ] && [ -n "$failure_message" ] || invalid_input 'empty failure'
      printf '  failure: %s: %s\n' "$failure_name" "$failure_message"
      ;;
    '')
      ;;
    *)
      invalid_input 'unknown record'
      ;;
    esac
  done

  [ -n "$run_value" ] || invalid_input 'missing run'
  [ -n "$suite_value" ] || invalid_input 'missing suite'
  [ "$pass_count" -gt 0 ] || [ "$fail_count" -gt 0 ] || invalid_input 'missing check'
  printf 'result: %s passed, %s failed, %sms total\n' "$pass_count" "$fail_count" "$total_ms"

  [ "$fail_count" -eq 0 ]
}
