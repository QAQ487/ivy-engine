#!/usr/bin/env bash
# Linux unit check for editor UI scale math (percent × DPI, no compounding).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/editor_ui_scale_math_test"
g++ -std=c++17 -Wall -Wextra -I"$ROOT" \
  "$ROOT/tests/editor_ui_scale_math_test.cpp" -o "$OUT"
"$OUT"
