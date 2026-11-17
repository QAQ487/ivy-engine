#!/usr/bin/env bash
# Linux check for the editor's New Project agent-docs export (engine/editor/agent_docs_export.cpp):
# unit tests, then a parity diff against tools/export_agent_docs.py on the real engine docs.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/agent_docs_export_check"
rm -rf "$OUT"
mkdir -p "$OUT/cpp" "$OUT/py"

g++ -std=c++20 -Wall -Wextra -I"$ROOT" \
  "$ROOT/tests/agent_docs_export_test.cpp" "$ROOT/engine/editor/agent_docs_export.cpp" \
  -o "$OUT/agent_docs_export_test"
"$OUT/agent_docs_export_test"

"$OUT/agent_docs_export_test" --export "$OUT/cpp"
python3 "$ROOT/tools/export_agent_docs.py" --game-repo "$OUT/py" >/dev/null
if ! diff -r "$OUT/cpp" "$OUT/py"; then
  echo "FAIL: C++ New Project export differs from tools/export_agent_docs.py" >&2
  exit 1
fi
echo "agent docs export parity: C++ == Python"
