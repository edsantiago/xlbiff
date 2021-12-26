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

# programs used:

# if $NO_XVFB is not set
xvfb_dependencies=(
    Xvfb xvfb
    xauth xauth
    mcookie util-linux
    xwininfo x11-utils
    xmessage x11-utils
)

# if $USE_WM is set
# Choice of WM is arbitrary; metacity is simple to run
wm_dependency=(
    metacity metacity
)

util_check_dependencies() {
    # arguments are alternating binary and package
    local binary package
    while (($# > 0)); do
        binary=$1
        package=$2
        shift; shift
        if [[ "$binary" != /* ]]; then
            binary=/usr/bin/$binary
        fi
        if [[ ! -x "$binary" ]]; then
            echo "$0: program not available: $binary" >&2
            echo "$0: package \"$package\" is probably not installed" >&2
            echo "$0: (not required for build, only for this test)" >&2
            exit 2
        fi
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
    # we use $TMPDIR for .Xauthority file
    export TMPDIR="$test_tmpdir"
    PATH=$test_tmpdir:$PATH

    if [[ -n "$NO_XVFB" ]]; then
        timeout 60 "$xlbiff_to_test" "$@" &
        xvfb_pid=$!
        return
    fi

    util_check_dependencies "${xvfb_dependencies[@]}"
    local -i display_num=19
    # loop looking for an available display number
    while ((++display_num < 64)); do
        kill_xvfb
        local xauth_cookie=$(mcookie)
        export XAUTHORITY="$test_tmpdir/Xauthority"
        echo -n > "$XAUTHORITY"
        export DISPLAY=":$display_num"
        xauth add "$DISPLAY" MIT-MAGIC-COOKIE-1 "$xauth_cookie"

        # Xvfb will send us SIGUSR1 when and if ready;
        # we don't want it to kill us.
        trap : USR1
        # Pass SIGUSR1 as ignored, so Xvfb will signal its parent when ready.
        # See xserver(1).
        (trap '' USR1; exec Xvfb "$DISPLAY" -auth "$XAUTHORITY" \
                 2> "$logdir/xvfb.$current_test_name.log") &
        xvfb_pid=$!

        # Wait for Xvfb to to either signal us (if ready) or die (if
        # it cannot use the this display number).
        wait
        if ! kill -0 "$xvfb_pid" 2> /dev/null; then
            # Xvfb died, probably because this display number is unavailable
            xvfb_pid=
            continue
        fi

        # Start the first client
        if [[ -n "$USE_WM" ]]; then
            if [[ -n "$TEST_WM" ]]; then
                local probable_wm_pkg_name="$(basename "$TEST_WM")"
                util_check_dependencies "$TEST_WM" "$probable_wm_pkg_name"
            else
                util_check_dependencies "${wm_dependency[@]}"
                TEST_WM="${wm_dependency[0]}"
            fi
            [[ -n "$VERBOSE" ]] && echo "$0: starting window manager $TEST_WM"
            "$TEST_WM" 2>> "$logdir/xvfb.$current_test_name.log" &
            util_wait_for_client_has_connected "Sawfish|Metacity|$TEST_WM" \
                || continue
        else
            # We need an initial client that will start by creating a window,
            # to check that the X server is actually usable.
            xmessage dummy 2>> "$logdir/xvfb.$current_test_name.log" &
            util_wait_for_client_has_connected "xmessage" || continue
        fi

        # everything started up successfully
        "$xlbiff_to_test" "$@" &
        xlbiff_pid=$!
        return
    done
}

# Returns on failure
util_wait_for_client_has_connected() {
    # global variable
    util_window_name_pattern="$1"
    loop_for_or_return 50 util_client_has_connected initial_client
}

util_client_has_connected() {
    xwininfo -root -children 2>&1 | egrep -q -i "$util_window_name_pattern"
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

# loops until success, or returns non-0 on timeout
loop_for_or_return() {
    loop_for "$1" "$2" "$3" return_on_failure
}

# loops until success or timeout
loop_for() {
    local loop_count="$1"
    local success_function="$2"
    local context_msg="$3"
    local return_on_failure="$4"

    local child_alive=1
    while sleep 0.1; do
        if ((--loop_count <= 0)); then
            echo "$0: timed out waiting for $success_function" \
                 "in test $current_test_name $context_msg" >&2
            xauth -v -i -n list >&2
            xwininfo -root -tree >&2
            kill_xvfb
            [[ -n "$return_on_failure" ]] && return 1
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
            [[ -n "$return_on_failure" ]] && return 1
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
        xlbiff_pid=
    fi
    if [[ -n "$xvfb_pid" ]]; then
        # Kill Xvfb, which kills any X clients,
        # and all subprocesses are thus cleaned up.
        kill -TERM "$xvfb_pid"
        wait
        xvfb_pid=
    fi
    return 0
}
