#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
export PATH="$PWD/.native/deps/bin:/opt/homebrew/opt/libtool/libexec/gnubin:$PATH"
./autogen.sh
# Sandbox blocks sysctl kern.argmax; use a conservative known-safe limit.
lt_cv_sys_max_cmd_len=262144 ./configure \
 --prefix="$PWD/.native/install" --enable-silent-rules --without-x \
 CPPFLAGS="-I$PWD/.native/deps/include -I$PWD/.native/deps/include/SDL2 -I/opt/homebrew/include" \
 LDFLAGS="-L$PWD/.native/deps/lib -L/opt/homebrew/lib" \
 CFLAGS='-O1 -g -std=gnu11 -fms-extensions -Wno-deprecated-declarations' \
 CXXFLAGS='-O1 -g -fms-extensions -Werror=non-pod-varargs -Wno-deprecated-declarations'
