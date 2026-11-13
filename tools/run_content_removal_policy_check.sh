#!/usr/bin/env bash
# Linux unit check for editor content-removal light policy.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/content_removal_policy_test"
g++ -std=c++17 -Wall -Wextra -I"$ROOT" \
  "$ROOT/tests/content_removal_policy_test.cpp" -o "$OUT"
"$OUT"
