#! /bin/bash
# Test that xlbiff can run "scan" from nmh.
# The "scan" program needs some setup; this tests that
# xlbiff does it if needed.

# Usage: test-scan-interface.sh [--logdir dirname] [--as-installed]

. "$(dirname "$0")"/utilities.sh

parse_command_line "$@"

scan_binary=/usr/bin/mh/scan

util_check_dependencies "$scan_binary" nmh

create_test_tmpdir

# Create "scan" program wrapper

# This wrapper touches a file to let us know when scan runs successfully.
cat > "$test_tmpdir"/scan <<EOF
#! /bin/sh
HOME=/xlbiff-scan-test-nonexistent
echo "\$@" > "$logdir"/scan.\$\$.args
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

printf -v xrm_sed_sub 's|mailbox-preview|& --scanproc %q/scan|p' "$test_tmpdir"
xrm_scan=$(sed -n "/^[^:!]*scanCommand:/$xrm_sed_sub" "$resource_file")

# Create the mail file
cat > "$test_tmpdir"/mailbox <<EOF
From: Test Harness <tester@localhost>
Subject: your scan interface test
Date: Sat, 26 Sep 2020 21:10:55 -0700

The scan program should be called on this message.
EOF

start_test scan
util_logv "$("$scan_binary" -version 2>&1)"
XLBIFF_DEBUG=1
start_xlbiff_under_xvfb -file "$test_tmpdir/mailbox" -xrm "$xrm_scan"

scan_success_file_exists() {
    [[ -f "$test_tmpdir"/scan.success ]]
}

# wait for xlbiff to run "scan"
loop_for 1000 scan_success_file_exists

end_test_with_status pass

kill_xvfb

exit 0
