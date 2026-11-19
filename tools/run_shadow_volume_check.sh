#!/usr/bin/env bash
# Linux checks for stencil shadow volumes: topology builder, pass policy, mode ids (unit tests) and
# the testbed caster meshes (every shadowVolume Visual must be a closed ASCII FBX).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}"
g++ -std=c++20 -Wall -Wextra -I"$ROOT" "$ROOT/tests/shadow_volume_mesh_test.cpp" -o "$OUT/shadow_volume_mesh_test"
g++ -std=c++17 -Wall -Wextra -I"$ROOT" "$ROOT/tests/shadow_volume_policy_test.cpp" -o "$OUT/shadow_volume_policy_test"
"$OUT/shadow_volume_policy_test"
"$OUT/shadow_volume_mesh_test"
g++ -std=c++17 -Wall -Wextra -I"$ROOT" "$ROOT/tests/shadow_volume_policy_test.cpp" -o "$OUT/shadow_volume_policy_test"
"$OUT/shadow_volume_policy_test"
g++ -std=c++17 -Wall -Wextra -I"$ROOT" "$ROOT/tests/shadow_mode_test.cpp" -o "$OUT/shadow_mode_test"
"$OUT/shadow_mode_test"
g++ -std=c++20 -O1 -Wall -Wextra -I"$ROOT" "$ROOT/tools/shadow_volume_asset_check.cpp" -o "$OUT/shadow_volume_asset_check"
"$OUT/shadow_volume_asset_check" "$ROOT"
