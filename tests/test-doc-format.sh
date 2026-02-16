#! /bin/bash
# Test that the manual pages examples format correctly.

# Tests a variety of "man page" formatters,
# samples their output, and verifies that they emit no warnings.

. "$(dirname "$0")"/utilities.sh

doc_dependencies=(
    groff groff
    man man-db
    mandoc mandoc
    man2html man2html-base
)

util_check_dependencies "${doc_dependencies[@]}"

create_test_tmpdir

exit_status=0

main() {
    local xlbiff_man_out='<Button1Press>:  popdown()$'
    local preview_man_out='^ *mailbox-preview imap.example.com$'

    run_man_test 'man' "$xlbiff_man_out" "$preview_man_out"
    run_man_test 'nroff -man' "$xlbiff_man_out" "$preview_man_out"
    run_man_test 'groff -man -Tpdf' '/Courier'
    run_man_test 'groff -man -Thtml' '<pre[ >]'
    run_man_test 'mandoc -T lint' ''
    run_man_test 'mandoc -T html' '<pre[ >]'
    run_man_test 'man2html' '<pre[ >]'
    # man2texi does not have a Debian package; test only if we find the binary.
    if hash man2texi 2> /dev/null; then
        run_man_test 'man2texi' '@display'
    fi
}


# Run all manual pages through one formatter
run_man_test() {
    local formatting_cmd="$1"
    local expect_in_xlbiff_out="$2"
    local expect_in_preview_out="$3"

    printf '%q: testing %s\n' "$0" "$formatting_cmd"
    local pre='./' post=".man"
    if [[ "$formatting_cmd" = man ]]; then
        pre=''
        post=''
    fi
    if [[ -z "$expect_in_preview_out" ]]; then
        expect_in_preview_out=$expect_in_xlbiff_out
    fi
    run_man_test_1 "${pre}xlbiff${post}" "$formatting_cmd" "$expect_in_xlbiff_out"
    run_man_test_1 "${pre}mailbox-preview${post}" "$formatting_cmd" \
                   "$expect_in_preview_out"
}

# Run 1 manual page through one formatter
run_man_test_1() {
    local input_file="$1"
    local formatting_command="$2"
    local expected_in_output="$3"

    local command_parts=($formatting_command) # not quoted, want word expand
    "${command_parts[@]}" "$input_file" \
                          > "$test_tmpdir"/stdout 2> "$test_tmpdir"/stderr
    # test is case insensitive:  <PRE> == <pre>
    if ! grep -i -- "$expected_in_output" "$test_tmpdir"/stdout > /dev/null
    then
        printf '%q: text "%s" not found in output of %s %s\n' \
               "$0" "$expected_in_output" "$formatting_command" "$input_file"
        exit_status=1
    fi
    if [[ "$formatting_command" =~ lint ]]; then
        # mandoc lint incorrectly/uselessly flags
        # "WARNING: skipping paragraph macro: IP empty"
        grep -v 'IP empty' "$test_tmpdir/stdout" >> "$test_tmpdir/stderr"
    fi
    if [[ -s "$test_tmpdir/stderr" ]]; then
        printf '%q: unexpected error output running %s %s\n' \
               "$0" "$formatting_command" "$input_file"
        cat "$test_tmpdir/stderr"
        exit_status=1
    fi
}

main "$@"
exit "$exit_status"
