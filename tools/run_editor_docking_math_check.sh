#!/usr/bin/env bash
# Linux unit check for editor docking layout math / ini path / rect conversion.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/editor_docking_math_test"
g++ -std=c++20 -Wall -Wextra -I"$ROOT" \
  "$ROOT/tests/editor_docking_math_test.cpp" -o "$OUT"
"$OUT"
