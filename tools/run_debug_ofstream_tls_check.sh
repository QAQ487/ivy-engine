#!/usr/bin/env bash
# Linux check for thread_local dout flush / no UAF on streambuf destruction.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SHIM="$ROOT/tools/linux_win32_shim"
OUT="${TMPDIR:-/tmp}/debug_ofstream_tls_test"
CXX="${CXX:-g++}"
ASAN_FLAGS=()
if "$CXX" -fsanitize=address -x c++ - -o /dev/null >/dev/null 2>&1 <<< 'int main(){return 0;}'; then
  ASAN_FLAGS=(-fsanitize=address -fno-omit-frame-pointer)
  echo "==> building with AddressSanitizer"
fi
"$CXX" -std=c++17 -Wall -Wextra "${ASAN_FLAGS[@]}" \
  -I"$SHIM" -I"$ROOT" \
  "$ROOT/tests/debug_ofstream_tls_test.cpp" \
  "$ROOT/engine/debug_draw/debug_ofstream.cpp" \
  "$SHIM/win32_api_stubs.cpp" \
  -pthread -o "$OUT"
"$OUT"
