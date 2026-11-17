#!/usr/bin/env bash
# Linux unit check for editor console ring buffer / filter logic.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/editor_console_log_test"
g++ -std=c++17 -Wall -Wextra -pthread -I"$ROOT" \
  "$ROOT/tests/editor_console_log_test.cpp" -o "$OUT"
"$OUT"
