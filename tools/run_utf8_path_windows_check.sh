#!/usr/bin/env bash
# Builds tests/utf8_path_windows_check.cpp. On Linux the binary prints SKIP and exits 0.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/utf8_path_windows_check"
g++ -std=c++20 -Wall -Wextra -I"$ROOT" \
  "$ROOT/tests/utf8_path_windows_check.cpp" -o "$OUT"
"$OUT"
