#!/usr/bin/env bash
# Linux unit check for distance-based LOD selection math.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/lod_select_test"
g++ -std=c++17 -Wall -Wextra -I"$ROOT" "$ROOT/tests/lod_select_test.cpp" -o "$OUT"
"$OUT"
