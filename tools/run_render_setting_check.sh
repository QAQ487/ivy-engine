#!/usr/bin/env bash
# Linux checks for runtime render-setting access (name table, clamps, key-list agreement).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/render_setting_access_test"
g++ -std=c++17 -Wall -Wextra -I"$ROOT" "$ROOT/tests/render_setting_access_test.cpp" -o "$OUT"
"$OUT"
python3 "$ROOT/tools/check_render_setting_keys.py"
