#!/usr/bin/env bash
# Linux unit check for 3D frustum / AABB math.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/frustum_cull_check"
g++ -std=c++17 -I"$ROOT" -I"$ROOT/tools/linux_win32_shim" \
  "$ROOT/tools/frustum_cull_check.cpp" -o "$OUT"
"$OUT"
