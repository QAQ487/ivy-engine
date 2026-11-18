#!/usr/bin/env bash
# Linux unit check for scene light slot selection.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/light_select_test"
g++ -std=c++17 -Wall -Wextra -I"$ROOT" "$ROOT/tests/light_select_test.cpp" -o "$OUT"
"$OUT"
