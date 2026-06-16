#! /bin/bash
# Test that the manual pages examples format correctly.

# Tests a variety of "man page" formatters,
# samples their output, and verifies that they emit no warnings.

. "$(dirname "$0")"/utilities.sh

parse_command_line "$@"

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

    ensure_file_exists xlbiff.man
    ensure_file_exists mailbox-preview.man

    run_man_test man 'man' "$xlbiff_man_out" "$preview_man_out"
    run_man_test nroff 'nroff -man' "$xlbiff_man_out" "$preview_man_out"
    run_man_test groff 'groff -man -ww -z -rCHECKSTYLE=3' ''
    run_man_test mandoc-lint 'mandoc -T lint' ''
    run_man_test mandoc-html 'mandoc -T html' '<pre[ >]'
}


# Run all manual pages through one formatter
run_man_test() {
    local test_name="$1"
    local formatting_cmd="$2"
    local expect_in_xlbiff_out="$3"
    local expect_in_preview_out="$4"

    printf '%q: testing %s\n' "$0" "$formatting_cmd"
    local pre='./' post=".man"
    if [[ "$formatting_cmd" = man ]]; then
        pre=''
        post=''
    fi
    if [[ -z "$expect_in_preview_out" ]]; then
        expect_in_preview_out=$expect_in_xlbiff_out
    fi
    run_man_test_1 "$test_name" "${pre}xlbiff${post}" "$formatting_cmd" \
                   "$expect_in_xlbiff_out"
    run_man_test_1 "$test_name" "${pre}mailbox-preview${post}" \
                   "$formatting_cmd" "$expect_in_preview_out"
}

# Run 1 manual page through one formatter
run_man_test_1() {
    local test_name="$1"
    local input_file="$2"
    local formatting_command="$3"
    local expected_in_output="$4"

    local outbase="$logdir/doc-$test_name"

    local command_parts=($formatting_command) # not quoted, want word expand
    "${command_parts[@]}" "$input_file" \
                          > "$outbase.stdout" 2> "$outbase.stderr"
    # test is case insensitive:  <PRE> == <pre>
    if [[ "$expected_in_output" ]] && \
           ! grep -i -- "$expected_in_output" "$outbase.stdout" > /dev/null
    then
        printf '%q: text "%s" not found in output of %s %s\n' \
               "$0" "$expected_in_output" "$formatting_command" "$input_file"
        exit_status=1
    fi
    if [[ "$formatting_command" =~ lint ]]; then
        # mandoc lint incorrectly/uselessly flags
        # "WARNING: skipping paragraph macro: IP empty"
        grep -v 'IP empty' "$outbase.stdout" >> "$outbase.stderr"
    fi
    if [[ "$formatting_command" =~ CHECKSTYLE ]]; then
        # groff -rCHECKSTYLE=3 flags
        #   "style: use of deprecated macro: .PD"
        #   We understand that "paragraph distance" is not translated
        #   into HTML, but it improves the PDF appearance of lists so
        #   much that we contintue to use it.
        sed -i '/style: use of deprecated macro: .PD/d' "$outbase.stderr"
    fi
    if [[ -s "$outbase.stderr" ]]; then
        printf '%q: unexpected error output running %s %s\n' \
               "$0" "$formatting_command" "$input_file"
        cat "$outbase.stderr"
        exit_status=1
    fi
}

ensure_file_exists() {
    local filename="$1"

    test ! -f "$filename" || return
    # omit version-customization of the man page; good enough for this test
    printf '%q: sed s/MANUAL_VERSION/xlbiff/g -- %q.in > %q\n' \
           "$0" "$filename" "$filename"
    sed 's/MANUAL_VERSION/xlbiff/g' -- "$filename".in > "$filename"
}

main "$@"
exit "$exit_status"
