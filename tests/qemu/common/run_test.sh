#!/bin/sh
# QEMU tests park after printing their verdict, so timeout (124) is expected.
# Optional environment: EXPECT — a substring the output must additionally
# contain, for suites whose per-case expected output is stronger than the
# generic final-OK check (tests/asm uses this).
set -u
seconds=$1
shift
output=$(mktemp) || exit 1
trap 'rm -f "$output"' EXIT HUP INT TERM

if timeout --foreground "$seconds" "$@" >"$output" 2>&1; then
    status=0
else
    status=$?
fi
cat "$output"
if [ "$status" -ne 0 ] && [ "$status" -ne 124 ]; then
    echo "QEMU exited with status $status" >&2
    exit 1
fi
if grep -q 'FAIL' "$output" || ! grep -Eq '(^|[[:space:]])OK([[:space:]]|$)' "$output"; then
    echo "QEMU test did not report a clean OK" >&2
    exit 1
fi
if [ -n "${EXPECT:-}" ] && ! grep -Fq -- "$EXPECT" "$output"; then
    echo "Expected output fragment not found: $EXPECT" >&2
    exit 1
fi
