#!/bin/sh
# Conservative classifier; automatic coverage is not hardware verification.
exec python3 "$(dirname "$0")/verify-check.py" "$@"
