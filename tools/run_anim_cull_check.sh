#!/usr/bin/env bash
# Linux unit check for the off-screen animation skip decision.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/anim_cull_test"
g++ -std=c++17 -Wall -Wextra -I"$ROOT" "$ROOT/tests/anim_cull_test.cpp" -o "$OUT"
"$OUT"
