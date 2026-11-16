#!/usr/bin/env bash
# Linux unit check for the Project Settings / Preferences category registry.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/editor_settings_categories_test"
g++ -std=c++20 -Wall -Wextra -I"$ROOT" \
  "$ROOT/tests/editor_settings_categories_test.cpp" -o "$OUT"
"$OUT"
