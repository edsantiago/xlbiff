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

# Usage: test-scan-interface.sh [--logdir dirname] [--binary path_to_xlbiff]

. $(dirname "$0")/utilities.sh

parse_command_line "$@"

scan_binary=/usr/bin/mh/scan

if [[ ! -x "$scan_binary" ]]; then
    echo "$0: $scan_binary not found; is the nmh package installed?" >&2
    exit 2
fi

create_test_tmpdir

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

start_xlbiff_under_xvfb -file "$test_tmpdir/mailbox" -xrm "$xrm_scan"
start_test scan

scan_success_file_exists() {
    [[ -f "$test_tmpdir"/scan.success ]]
}

# wait for xlbiff to run "scan"

loop_for 25 scan_success_file_exists
end_test_with_status pass

kill_xvfb

exit 0
