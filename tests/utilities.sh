# /bin/bash
# Functions used by test scripts.

# Side effect: sets variables logdir and xlbiff_to_test
parse_command_line() {
    cd $(dirname "$0")/.. || exit 1
    logdir=
    xlbiff_to_test=./xlbiff
    while [[ $# -gt 0 ]]; do
        case $1 in
            --logdir)
                if [[ $# -lt 2 ]]; then
                    echo "$0: --logdir requires an argument" >&2
                    exit 1
                fi
                logdir=$2
                shift
                ;;
            --binary)
                if [[ $# -lt 2 ]]; then
                    echo "$0: --binary requires an argument" >&2
                    exit 1
                fi
                xlbiff_to_test=$2
                shift
                ;;
            -*)
                echo "$0: unknown option: $1" >&2
                exit 1
                ;;
            *)
                echo "$0: unknown argument: $1" >&2
                exit 1
                ;;
        esac
        shift
    done
}

# Creates a temp directory and sets an exit trap to remove it.
# Side effects:
# Sets variable test_tmpdir.
# Sets variable atexit_tmpdir.
# May update logdir.
create_test_tmpdir() {
    if [[ -d "$TEST_TMPDIR" ]]; then
        test_tmpdir=$TEST_TMPDIR
        atexit_tmpdir=':'
    else
        test_tmpdir=$(mktemp -d -t xlbiff-test-si-XXXXXXXXX)
        atexit_tmpdir='rm -rf -- "$test_tmpdir"'
    fi
    trap "$atexit_tmpdir" EXIT
    if [[ ! "$logdir" ]]; then
        logdir=$test_tmpdir
    fi
}

# Arguments are passed to xlbiff.
# Global xlbiff_to_test is the executable to run.
# Runs under xvfb unless envvar NO_XVFB is set.
# Starts a window manager if envvar USE_WM is set.
# Side effects: sets variable xvfb_pid
#   and envvars XAUTHORITY and DISPLAY
start_xlbiff_under_xvfb() {
    MH=
    XFILESEARCHPATH=
    XUSERFILESEARCHPATH=
    XAPPLRESDIR=
    # xvfb-run uses $TMPDIR for .Xauthority file
    export TMPDIR="$test_tmpdir"
    PATH=$test_tmpdir:$PATH

    if [[ -n "$NO_XVFB" ]]; then
        timeout 60 "$xlbiff_to_test" "$@" &
        xvfb_pid=$!
        return
    fi

    local xvfb_run_script=/usr/bin/xvfb-run
    if [[ ! -x "$xvfb_run_script" ]]; then
        echo "$0: script not available: $xvfb_run_script" >&2
        echo "$0: package xvfb is probably not installed" >&2
        echo "$0: (not required for build, only for this test)" >&2
        exit 2
    fi
    # Some tests are stricter with a window manager.
    local -a xvfb_client_cmd
    if [[ -n "$USE_WM" ]]; then
        # We need a dummy client that won't write to stderr.
        # We will start the WM later, with stderr redirected.
        xvfb_client_cmd=(xmessage dummy)
    else
        xvfb_client_cmd=("$xlbiff_to_test" "$@")
    fi
    export XAUTHORITY="$test_tmpdir/Xauthority"
    echo -n > "$XAUTHORITY"
    # The xvfb-run script needs job control (-m) so that it will catch the
    # "kill" below and kill its Xvfb subprocess.  But this causes it to
    # warn that its stderr is not a tty, so we capture stderr lest spurious
    # output fail the test.
    # Note that this assumes xvfb-run is implemented as a shell script.
    timeout 60 bash -m -- "$xvfb_run_script" -n 20 -a -f "$XAUTHORITY" -- \
         "${xvfb_client_cmd[@]}" 2> "$logdir/xvfb-run.stderr.log" &
    xvfb_pid=$!
    loop_for 50 util_get_display
    if [[ -n "$USE_WM" ]]; then
        # Choice of WM is arbitrary; metacity is simple to run
        TEST_WM="${TEST_WM:-metacity}"
        [[ -n "$VERBOSE" ]] && echo "$0: starting window manager $TEST_WM"
        "$TEST_WM" 2> "$logdir/xvfb-run.stderr.log" &
        loop_for 20 util_window_manager_has_connected
        "$xlbiff_to_test" "$@" &
        xlbiff_pid=$!
    else
        xlbiff_pid=
    fi
}

# exports DISPLAY
util_get_display() {
    # we want the last line in the Xauthority file, in case xvfb-run
    # had to loop through several server numbers
    DISPLAY="$(xauth -q -i -n list | sed -E -n '$s/^[^ ]*(:[0-9]*) .*/\1/p')"
    export DISPLAY
    [[ -n "$DISPLAY" ]] && xwininfo -root >/dev/null 2>&1
}

# returns true if a window has been created on the X server
util_window_manager_has_connected() {
    xwininfo -root -children 2>&1 | egrep -q -i "Sawfish|Metacity|$TEST_WM"
}

# Returns success if test with this name should be run.
# TEST_SELECTION is a comma-separated list of tests to run
# or empty to run all tests
should_run_test() {
    local test_name="$1"
    [[ -z "$TEST_SELECTION" ]] || [[ ",$TEST_SELECTION," = *,$test_name,* ]]
}

maybe_start_test() {
    should_run_test "$1" && start_test "$@"
}

# sets current_test_name, increments num_tests_run
start_test() {
    local test_name="$1"
    if [[ -n "$current_test_name" ]]; then
        echo "$0: start_test called again within test $current_test_name" >&2
    fi
    current_test_name=$test_name
    ((num_tests_run++))
    [[ -n "$VERBOSE" ]] && echo "$0: starting test $current_test_name"
    return 0
}

# uses current_test_name
end_test_with_status() {
    local pass_fail="$1"
    if [[ -z "$current_test_name" ]]; then
        echo "$0: end_test_with_status called outside a test" >&2
    fi
    if [[ "$pass_fail" = pass ]]; then
        echo "$0: Passed: $current_test_name"
        ((num_tests_passed++))
    else
        echo "$0: FAILED: $current_test_name" >&2
    fi
    current_test_name=
}

# loops until success or timeout
loop_for() {
    local loop_count="$1"
    local success_function="$2"
    local context_msg="$3"

    local child_alive=1
    while sleep 0.1; do
        if ((--loop_count <= 0)); then
            echo "$0: timed out waiting for $success_function" \
                 "in test $current_test_name $context_msg" >&2
            xauth -v -i -n list >&2
            xwininfo -root -tree >&2
            kill_xvfb
            exit 1
        fi
        if [[ -z "$(jobs -r -p)" ]]; then
            child_alive=0
        fi
        if $success_function; then
            [[ -n "$VERBOSE" ]] && echo "$0: $success_function true"
            break
        fi
        if [[ "$child_alive" = 0 ]]; then
            echo "$0: child process has died waiting for $success_function" \
                 "in test $current_test_name $context_msg" >&2
            exit 1
        fi
        [[ -n "$VERBOSE" ]] && echo "$0: $success_function false"
    done
}

# Returns status 0, so suitable as the last thing in a successful test.
kill_xvfb() {
    if [[ -n "$xlbiff_pid" ]]; then
        # kill xlbiff so that it doesn't output "X connection broken"
        kill -TERM "$xlbiff_pid"
    fi
    if [[ -n "$xvfb_pid" ]]; then
        # Killing xvfb-run causes it to kill Xvfb, which kills any X clients,
        # and all subprocesses are thus cleaned up.
        kill -TERM "$xvfb_pid"
        wait
    fi
    return 0
}
