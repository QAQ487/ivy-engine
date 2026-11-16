#!/usr/bin/env bash
# Linux unit check for editor Dear ImGui theme presets / color schemes.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/editor_theme_test"
g++ -std=c++17 -Wall -Wextra -DIMGUI_DISABLE_WIN32_FUNCTIONS \
  -I"$ROOT" -I"$ROOT/libraries" \
  "$ROOT/tests/editor_theme_test.cpp" \
  "$ROOT/engine/editor/editor_theme.cpp" \
  "$ROOT/engine/core/content_path.cpp" \
  "$ROOT/libraries/imgui/imgui.cpp" \
  "$ROOT/libraries/imgui/imgui_draw.cpp" \
  "$ROOT/libraries/imgui/imgui_tables.cpp" \
  "$ROOT/libraries/imgui/imgui_widgets.cpp" \
  -o "$OUT"
"$OUT"
