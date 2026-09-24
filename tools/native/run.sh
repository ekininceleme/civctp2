#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
mkdir -p .native/user
export CTP2_USER_DIR="$PWD/.native/user"
cd ctp2_code/ctp
exec ./ctp2 nointromovie "$@"
