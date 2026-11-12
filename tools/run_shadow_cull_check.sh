#!/usr/bin/env bash
# Linux unit check for shadow-cascade light-ortho / open-near AABB cull math.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/shadow_cull_check"
g++ -std=c++17 -I"$ROOT" -I"$ROOT/tools/linux_win32_shim" \
  "$ROOT/tools/shadow_cull_check.cpp" -o "$OUT"
"$OUT"
