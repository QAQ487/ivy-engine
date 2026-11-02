#!/usr/bin/env bash
# Compile and run Protocol-level runtime bridge checks on Linux (no named pipes).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/x64/linux-bridge-offline}"
CXX="${CXX:-g++}"
mkdir -p "$OUT"

SHIM="$ROOT/tools/linux_win32_shim"
COMMON=(
  -std=c++20 -Wall -Wextra -Wno-unused-parameter
  -D_DEBUG -DGAME_RUNTIME -DNOMINMAX
  -I"$SHIM" -I"$ROOT"
  "$ROOT/engine/testing/runtime_test_bridge.cpp"
  "$SHIM/win32_api_stubs.cpp"
  "$SHIM/linux_bridge_input_stubs.cpp"
  "$ROOT/tools/test_renderer_stubs.cpp"
)

build_one() {
  local name="$1"
  local src="$2"
  echo "==> building $name"
  "$CXX" "${COMMON[@]}" "$src" -pthread -o "$OUT/$name"
}

build_one runtime_bridge_world_manip_check "$ROOT/tools/runtime_bridge_world_manip_check.cpp"
build_one runtime_bridge_phase6_check "$ROOT/tools/runtime_bridge_phase6_check.cpp"
build_one runtime_bridge_protocol_check "$ROOT/tools/runtime_bridge_protocol_check.cpp"

echo "==> run runtime_bridge_world_manip_check"
"$OUT/runtime_bridge_world_manip_check"
echo "==> run runtime_bridge_phase6_check"
"$OUT/runtime_bridge_phase6_check"
echo "==> run runtime_bridge_protocol_check"
"$OUT/runtime_bridge_protocol_check"

echo "==> python angular acceptance unit tests"
python3 "$ROOT/tools/runtime_bridge_angular_acceptance_check.py" -v

echo "==> python scenario runner unit tests"
python3 "$ROOT/tools/runtime_bridge_scenario_check.py" -v

echo "PASS Linux offline runtime bridge protocol checks"
