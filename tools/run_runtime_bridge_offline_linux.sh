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
build_one runtime_bridge_observe_pose_check "$ROOT/tools/runtime_bridge_observe_pose_check.cpp"
build_one runtime_bridge_editor_play_check "$ROOT/tools/runtime_bridge_editor_play_check.cpp"

echo "==> run runtime_bridge_world_manip_check"
"$OUT/runtime_bridge_world_manip_check"
echo "==> run runtime_bridge_phase6_check"
"$OUT/runtime_bridge_phase6_check"
echo "==> run runtime_bridge_protocol_check"
"$OUT/runtime_bridge_protocol_check"
echo "==> run runtime_bridge_observe_pose_check"
"$OUT/runtime_bridge_observe_pose_check"
echo "==> run runtime_bridge_editor_play_check"
"$OUT/runtime_bridge_editor_play_check"

# Editor Debug host compile probe (no GAME_RUNTIME) — catches declaration/include gate drift.
echo "==> building runtime_bridge_editor_host_compile_check (Editor Debug defines)"
"$CXX" -std=c++20 -Wall -Wextra -Wno-unused-parameter \
  -D_DEBUG -DNOMINMAX \
  -I"$SHIM" -I"$ROOT" \
  "$ROOT/tools/runtime_bridge_editor_host_compile_check.cpp" \
  "$SHIM/win32_api_stubs.cpp" \
  -pthread -o "$OUT/runtime_bridge_editor_host_compile_check"
echo "==> run runtime_bridge_editor_host_compile_check"
"$OUT/runtime_bridge_editor_host_compile_check"

echo "==> Editor host gate sync (grep/static)"
python3 "$ROOT/tools/runtime_bridge_editor_host_gate_check.py"

echo "==> tools/*.cmd echo redirect check"
python3 "$ROOT/tools/runtime_bridge_cmd_echo_redirect_check.py"

echo "==> python keyboard key-table sync + validation"
python3 "$ROOT/tools/runtime_bridge_keyboard_keys_check.py" -v

echo "==> python mouse absolute client payload validation"
( cd "$ROOT/tools" && python3 -m unittest runtime_test_client_check.ClientChecks.test_mouse_client_validation -v )

echo "==> python client bridge rejection identity handling"
( cd "$ROOT/tools" && python3 -m unittest runtime_test_client_check.ClientChecks.test_request_surfaces_bridge_error_when_identity_echoed -v )

echo "==> python client editor play helpers"
( cd "$ROOT/tools" && python3 -m unittest   runtime_test_client_check.ClientChecks.test_editor_play_client_validation -v )

echo "==> python client capture path + sequential connection helpers"
( cd "$ROOT/tools" && python3 -m unittest \
  runtime_test_client_check.ClientChecks.test_resolve_capture_path_helper \
  runtime_test_client_check.ClientChecks.test_sequential_completion_helpers_keep_one_connection -v )

echo "==> python connection acceptance unit tests"
python3 "$ROOT/tools/runtime_bridge_connection_acceptance_check.py" -v

echo "==> python angular acceptance unit tests"
python3 "$ROOT/tools/runtime_bridge_angular_acceptance_check.py" -v

echo "==> python scenario runner unit tests"
python3 "$ROOT/tools/runtime_bridge_scenario_check.py" -v

echo "==> runtime host asset coverage"
python3 "$ROOT/tools/check_runtime_host_assets.py"

echo "PASS Linux offline runtime bridge protocol checks"
