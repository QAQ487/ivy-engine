#!/usr/bin/env bash
# Runs every Linux-runnable engine check in one go (used by CI, also handy locally):
#   1. tools/run_*_check.sh / run_*_linux.sh scripts
#   2. each tests/*_test.cpp that documents a "//   g++ ..." build line in its header
#   3. tools/content_path_check.cpp
#   4. portable Python checks and unit tests
# Keeps going after a failure and exits non-zero if anything failed.
# None of this is a Windows (MSVC / D3D11 / named pipe) pass.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
OUT="${IVY_CHECK_OUT:-${TMPDIR:-/tmp}/ivy-linux-checks}"
mkdir -p "$OUT"
export TMPDIR="$OUT"

passed=()
failed=()

run() {
  local label="$1"
  shift
  local log="$OUT/$(echo "$label" | tr '/ ' '__').log"
  if "$@" >"$log" 2>&1; then
    passed+=("$label")
    echo "PASS $label"
  else
    failed+=("$label")
    echo "FAIL $label"
    tail -n 30 "$log" | sed 's/^/    /'
  fi
}

# 1. Shell check scripts.
for script in tools/run_*.sh; do
  case "$script" in
    tools/run_linux_checks.sh) continue ;;
  esac
  run "$script" bash "$script" "$OUT/$(basename "$script" .sh)"
done

# 2. Single-file unit tests with a documented build line. The build line's
#    /tmp/<name> output is redirected into $OUT.
build_and_run_test() {
  local src="$1"
  local line="$2"
  local name
  name="$(basename "$src" .cpp)"
  local -a args
  read -r -a args <<<"${line//\/tmp\//$OUT/}"
  "${args[@]}" && "$OUT/$name"
}

for src in tests/*_test.cpp; do
  line="$(grep -m1 -E '^//[[:space:]]+g\+\+ ' "$src" | sed -E 's#^//[[:space:]]+##')"
  if [ -z "$line" ]; then
    # Covered by a tools/run_*.sh script instead.
    continue
  fi
  run "$src" build_and_run_test "$src" "$line"
done

# 3. Content path rules.
run tools/content_path_check.cpp bash -c \
  "g++ -std=c++17 -I. tools/content_path_check.cpp -o '$OUT/content_path_check' && '$OUT/content_path_check'"

# 4. Python checks that do not need Windows (no ctypes.WinDLL, no live bridge harness).
for py in \
  tools/check_testbed_assets.py \
  tools/check_engine_resource_paths.py \
  tools/check_runtime_host_assets.py \
  tools/check_editor_gui_linkage.py \
  tools/check_imguizmo_addpolyline_order.py \
  tools/check_render_setting_keys.py \
  tools/agent_docs_check.py \
  tools/runtime_bridge_angular_acceptance_check.py \
  tools/runtime_bridge_cmd_echo_redirect_check.py \
  tools/runtime_bridge_connection_acceptance_check.py \
  tools/runtime_bridge_controller_acceptance_check.py \
  tools/runtime_bridge_controller_axes_check.py \
  tools/runtime_bridge_controller_probe_acceptance_check.py \
  tools/runtime_bridge_controller_probe_check.py \
  tools/runtime_bridge_editor_host_gate_check.py \
  tools/runtime_bridge_explore_check.py \
  tools/runtime_bridge_keyboard_keys_check.py \
  tools/runtime_bridge_mouse_acceptance_check.py \
  tools/runtime_bridge_scenario_check.py \
  tools/runtime_bridge_world_manip_acceptance_check.py; do
  run "$py" python3 "$py"
done
run "python unittest (agent docs)" \
  python3 -m unittest tools/agent_docs_check_test.py tools/export_agent_docs_check.py

echo
echo "${#passed[@]} passed, ${#failed[@]} failed"
if [ "${#failed[@]}" -ne 0 ]; then
  printf '  failed: %s\n' "${failed[@]}"
  exit 1
fi
