#!/usr/bin/env bash
# Linux unit check for 2D orthographic view-rect sprite cull math.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/sprite_cull_check"
g++ -std=c++17 -I"$ROOT" -I"$ROOT/tools/linux_win32_shim" \
  "$ROOT/tools/sprite_cull_check.cpp" -o "$OUT"
"$OUT"
