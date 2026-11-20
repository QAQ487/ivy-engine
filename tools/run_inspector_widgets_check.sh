#!/usr/bin/env bash
# Linux headless check: Properties-panel widgets never overflow the panel width.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/inspector_widgets_test"
g++ -std=c++17 -Wall -Wextra -DIMGUI_DISABLE_WIN32_FUNCTIONS \
  -I"$ROOT" -I"$ROOT/libraries" \
  "$ROOT/tests/inspector_widgets_test.cpp" \
  "$ROOT/engine/editor/inspector_widgets.cpp" \
  "$ROOT/engine/editor/editor_icon_button.cpp" \
  "$ROOT/libraries/imgui/imgui.cpp" \
  "$ROOT/libraries/imgui/imgui_draw.cpp" \
  "$ROOT/libraries/imgui/imgui_tables.cpp" \
  "$ROOT/libraries/imgui/imgui_widgets.cpp" \
  -o "$OUT"
"$OUT"
