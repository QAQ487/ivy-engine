#!/usr/bin/env bash
# Linux unit check for billboard / particle / effect culling math.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/fx_cull_test"
g++ -std=c++17 -Wall -Wextra -I"$ROOT" -I"$ROOT/tools/linux_win32_shim" \
  "$ROOT/tests/fx_cull_test.cpp" -o "$OUT"
"$OUT"
