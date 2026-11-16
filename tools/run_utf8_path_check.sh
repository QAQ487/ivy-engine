#!/usr/bin/env bash
# Linux unit check for UTF-8 path helpers and CJK content scan.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/utf8_path_test"
g++ -std=c++20 -Wall -Wextra -I"$ROOT" \
  "$ROOT/tests/utf8_path_test.cpp" -o "$OUT"
"$OUT"
