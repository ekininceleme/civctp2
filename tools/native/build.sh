#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
export PATH="$PWD/.native/deps/bin:/opt/homebrew/opt/libtool/libexec/gnubin:$PATH"
make -j8 \
 CPPFLAGS="-I$PWD/.native/deps/include -I$PWD/.native/deps/include/SDL2 -I/opt/homebrew/include" \
 CFLAGS='-O1 -g -std=gnu11 -fms-extensions -Wno-deprecated-declarations' \
 CXXFLAGS='-O1 -g -fms-extensions -Werror=non-pod-varargs -Wno-deprecated-declarations'
