#! /bin/bash
# Test that xlbiff can run "scan" from nmh.
# The "scan" program needs some setup; this tests that
# xlbiff does it if needed.
#
# This test needs the nmh package to be installed.
# This is a run-time requirement for xlbiff, but not
# a build-time requirement (except for this test).

# Depends: xvfb, xauth
# In Debian, xvfb merely recommends xauth, so we must include xauth dependency.
# In Ubuntu, xvfb depends on xauth.

# Usage: test-scan-interface.sh [--logdir dirname] [path_to_xlbiff]

logdir=

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
        -*)
            echo "$0: unknown option: $1" >&2
            exit 1
            ;;
        *)
            break
            ;;
    esac
    shift
done

xlbiff_to_test=$1
if [[ ! "$xlbiff_to_test" ]]; then
   xlbiff_to_test=./xlbiff
fi

scan_binary=/usr/bin/mh/scan

if [[ ! -x "$scan_binary" ]]; then
    echo "$0: $scan_binary not found; is the nmh package installed?" >&2
    exit 2
fi

# Create a tmp directory

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

# Create "scan" program wrapper

# This wrapper touches a file to let us know when scan runs successfully.
cat > "$test_tmpdir"/scan <<EOF
#! /bin/sh
HOME=
echo "\$@" > "$test_tmpdir"/scan.args
"$scan_binary" "\$@" > "$logdir/scan.\$\$.stdout" 2> "$logdir/scan.\$\$.stderr"
status=\$?
cat -- "$logdir/scan.\$\$.stdout"
cat -- "$logdir/scan.\$\$.stderr" >&2
if [ "\$status" -eq 0 ]; then
    touch -- "$test_tmpdir"/scan.success
fi 
exit "\$status"
EOF
chmod -- 755 "$test_tmpdir"/scan

# Change the "scan" program used in the installed X resources
# file to point to our wrapper instead.

xrm_scan=$(sed -n "/^[^:]*scanCommand:/s|$scan_binary|$test_tmpdir/scan|p" \
               ./XLbiff.ad)

# Create the mail file
cat > "$test_tmpdir"/mailbox <<EOF
From: Test Harness <tester@localhost>
Subject: your scan interface test
Date: Sat, 26 Sep 2020 21:10:55 -0700

The scan program should be called on this message.
EOF

# run xlbiff (under Xvfb)

xvfb_run_script=/usr/bin/xvfb-run
if [[ ! -x "$xvfb_run_script" ]]; then
    echo "$0: script not available: $xvfb_run_script" >&2
    echo "$0: package xvfb is probably not installed" >&2
    echo "$0: (not required for build, only for this test)" >&2
    exit 2
fi

MH=
XFILESEARCHPATH=
XUSERFILESEARCHPATH=
XAPPLRESDIR=
# xvfb-run uses $TMPDIR for .Xauthority file
export TMPDIR="$test_tmpdir"
PATH=$test_tmpdir:$PATH
# The xvfb-run script needs job control (-m) so that it will catch the
# "kill" below and kill its Xvfb subprocess.  But this causes it to
# warn that its stderr is not a tty, so we capture stderr lest spurious
# output fail the test.
# Note that this assumes xvfb-run is implemented as a shell script.
bash -m -- "$xvfb_run_script" -n 20 --auto-servernum -- \
     "$xlbiff_to_test" -file "$test_tmpdir"/mailbox -xrm "$xrm_scan" \
     2> "$logdir/xvfb-run.stderr" &
xvfb_pid=$!

# wait for xlbiff to run "scan"

wait_count=25
child_alive=1
exit_status=1
while sleep 0.2; do
    if ((--wait_count <= 0)); then
        echo "$0: timed out waiting for scan to succeed" >&2
        break
    fi
    if [[ -z "$(jobs -r -p)" ]]; then
        child_alive=0
    fi
    if [[ -f "$test_tmpdir"/scan.success ]]; then
        echo "$0: Success"
        exit_status=0
        break
    fi
    if [[ "$child_alive" = 0 ]]; then
        echo "$0: child process has died" >&2
        exit 1
    fi
done

# Killing xvfb-run causes it to kill Xvfb, which kills xlbiff,
# and all subprocesses are thus cleaned up.
kill -TERM "$xvfb_pid"
wait
exit "$exit_status"
