#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
work="$root/.native/checks/work/sanitize"
if [ ! -x "$work/ctp2_code/ctp/ctp2" ]; then
    echo 'Build first: python3 tools/native-checks/check.py sanitize' >&2
    exit 1
fi
# Separate writable data and saves from the playable build.
mkdir -p "$work/ctp2_data" "$work/Scenarios" "$root/.native/checks/user"
rsync -a "$root/ctp2_data/" "$work/ctp2_data/"
rsync -a "$root/Scenarios/" "$work/Scenarios/"
export CTP2_USER_DIR="$root/.native/checks/user"
export DYLD_FALLBACK_LIBRARY_PATH=/opt/homebrew/lib:/usr/local/lib:/usr/lib
export UBSAN_OPTIONS=print_stacktrace=1
cd "$work/ctp2_code/ctp"
exec ./ctp2 nointromovie "$@"
