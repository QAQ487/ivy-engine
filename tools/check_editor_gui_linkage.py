#!/usr/bin/env python3
"""Mechanical linkage/ODR checks for the phase-1 engine_gui.cpp editor split.

Linux cannot meaningfully g++ -fsyntax-only the D3D11 editor TUs, so this script
catches the MSVC-class failures that are visible from source alone:

1. Header `extern` variables still defined `static` in a TU (C2370).
2. Header function declarations whose only definition is `static` (or missing).
3. `static` functions defined in one phase-1 TU but called from another.
4. Duplicate non-static function definitions across phase-1 TUs (LNK2005).
5. Known MSVC-break symbols (IM_TRUNC, ICON_FA_*, panorama::, …) used without
   including the header that provides them (via direct or common includes).

Exit 0 on clean; exit 1 and print findings otherwise.

Usage:
  python3 tools/check_editor_gui_linkage.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

PHASE1_TUS = [
    ROOT / "engine/core/engine_gui.cpp",
    ROOT / "engine/editor/editor_layout.cpp",
    ROOT / "engine/editor/editor_docking.cpp",
    ROOT / "engine/editor/editor_toolbar.cpp",
    ROOT / "engine/editor/editor_hierarchy_panel.cpp",
    ROOT / "engine/editor/editor_inspector_panel.cpp",
    ROOT / "engine/editor/editor_asset_panel.cpp",
    ROOT / "engine/editor/editor_menu_bar.cpp",
    ROOT / "engine/editor/editor_popups.cpp",
    ROOT / "engine/editor/editor_start_screen.cpp",
    ROOT / "engine/editor/editor_tool_windows.cpp",
]

HEADERS = [
    ROOT / "engine/editor/editor_gui_internal.h",
    ROOT / "engine/editor/editor_layout.h",
    ROOT / "engine/editor/editor_docking.h",
]

# Symbol pattern -> providing header basename (caught in Terry's MSVC Debug log).
SYMBOL_HEADER_REQUIREMENTS = [
    (r"\bIM_TRUNC\b", "imgui_internal.h"),
    (r"\bImMax\b", "imgui_internal.h"),
    (r"\bGetTopMostPopupModal\b", "imgui_internal.h"),
    (r"\bDockBuilder", "imgui_internal.h"),
    (r"\bICON_FA_", "icons_font_awesome.h"),
    (r"\bpanorama::", "panorama_sky.h"),
    (r"\bPlayablePackage::", "playable_package_builder.h"),
    (r"\bModelAssetCache\b", "model_asset_cache.h"),
    (r"\bRenderSettings_Get", "render_settings.h"),
    (r"\bNormalizeSceneFileName\b", "editor_gui_internal.h"),
    (r"\bTrimString\b", "editor_gui_internal.h"),
    (r"\bMoveNodeOrder\b", "editor_gui_internal.h"),
    (r"\bMoveNodeToBack\b", "editor_gui_internal.h"),
    (r"\bMoveNodeToFront\b", "editor_gui_internal.h"),
    (r"\bUndoableReorderNode\b", "editor_gui_internal.h"),
    (r"\bResolveSceneReloadConflict\b", "editor_gui_internal.h"),
    (r"\bEditorDocking_", "editor_docking.h"),
    (r"\bkEditorHierarchyWindowTitle\b", "editor_docking.h"),
]


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def parse_extern_vars(header_text: str) -> list[str]:
    names: list[str] = []
    for m in re.finditer(r"^extern\s+(.+);", header_text, re.M):
        names.extend(re.findall(r"\b(g_\w+)\b", m.group(1)))
    return sorted(set(names))


def parse_func_decls(header_text: str) -> list[str]:
    names: list[str] = []
    for m in re.finditer(
        r"^([\w:<>*&,\s]+)\s+([A-Za-z_]\w*)\s*\(([^;]*)\)\s*;", header_text, re.M
    ):
        name = m.group(2)
        if name in {"if", "for", "while", "switch"}:
            continue
        names.append(name)
    return sorted(set(names))


# Match top-level function definitions only (column 0). Nested parentheses in
# parameter lists (e.g. function-pointer params) are balanced manually.
_DEF_HEAD_RE = re.compile(
    r"^(?P<static>static\s+)?(?P<ret>(?:std::)?[\w:<>*&]+(?:\s+(?:std::)?[\w:<>*&]+)*)\s+"
    r"(?P<name>[A-Za-z_]\w*)\s*\(",
    re.M,
)


def _find_balanced_paren_end(text: str, open_idx: int) -> int | None:
    """open_idx points at '('; return index of matching ')' or None."""
    depth = 0
    i = open_idx
    while i < len(text):
        c = text[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i
        elif c in "'\"":
            quote = c
            i += 1
            while i < len(text) and text[i] != quote:
                if text[i] == "\\" and i + 1 < len(text):
                    i += 2
                    continue
                i += 1
        i += 1
    return None


def collect_defs(texts: dict[Path, str]) -> dict[str, list[dict]]:
    defs: dict[str, list[dict]] = {}
    for path, text in texts.items():
        for m in _DEF_HEAD_RE.finditer(text):
            name = m.group("name")
            if name in {"if", "for", "while", "switch", "catch"}:
                continue
            open_paren = m.end() - 1
            close_paren = _find_balanced_paren_end(text, open_paren)
            if close_paren is None:
                continue
            after = text[close_paren + 1 : close_paren + 32].lstrip()
            if not after.startswith("{"):
                # Allow `) \n {` already covered by lstrip; reject decls ending `;`
                continue
            line = text[: m.start()].count("\n") + 1
            defs.setdefault(name, []).append(
                {
                    "path": path,
                    "static": bool(m.group("static")),
                    "line": line,
                    "snippet": text.splitlines()[line - 1].strip()[:140],
                }
            )
    return defs


def has_header_body(name: str, headers_text: str) -> bool:
    return bool(re.search(rf"\b{re.escape(name)}\s*\([^;]*\)\s*\{{", headers_text))


def main() -> int:
    headers_text = "\n".join(read(p) for p in HEADERS)
    texts = {p: read(p) for p in PHASE1_TUS if p.exists()}
    extern_vars = parse_extern_vars(headers_text)
    func_decls = parse_func_decls(headers_text)
    defs = collect_defs(texts)

    findings: list[str] = []

    # 1) extern + static definition
    for var in extern_vars:
        for path, text in texts.items():
            for m in re.finditer(
                rf"^static\s+[^=\n]*\b{re.escape(var)}\b", text, re.M
            ):
                line = text[: m.start()].count("\n") + 1
                findings.append(
                    f"EXTERN_STATIC_VAR {var} declared extern in editor headers but "
                    f"defined static at {path.relative_to(ROOT)}:{line}: "
                    f"{text.splitlines()[line - 1].strip()}"
                )

    # 2) header decl with only-static or missing non-static def
    for name in func_decls:
        if has_header_body(name, headers_text):
            continue
        locs = defs.get(name, [])
        nonstatic = [d for d in locs if not d["static"]]
        statics = [d for d in locs if d["static"]]
        if statics and not nonstatic:
            for d in statics:
                findings.append(
                    f"HEADER_STATIC_FN {name} declared in editor headers but defined "
                    f"static at {d['path'].relative_to(ROOT)}:{d['line']}: {d['snippet']}"
                )
        elif not locs:
            findings.append(
                f"HEADER_MISSING_FN {name} declared in editor headers but no "
                f"non-static definition found in phase-1 TUs"
            )

    # 3) static function called from another phase-1 TU
    for name, locs in sorted(defs.items()):
        static_files = {d["path"] for d in locs if d["static"]}
        if not static_files:
            continue
        for path, text in texts.items():
            if path in static_files:
                continue
            if any(d["path"] == path and not d["static"] for d in locs):
                continue
            matches = list(re.finditer(rf"\b{re.escape(name)}\s*\(", text))
            if not matches:
                continue
            lines = [text[: m.start()].count("\n") + 1 for m in matches]
            defined = ", ".join(
                f"{p.relative_to(ROOT)}" for p in sorted(static_files)
            )
            findings.append(
                f"CROSS_TU_STATIC {name} defined static in [{defined}] but called from "
                f"{path.relative_to(ROOT)} lines {lines}"
            )

    # 4) duplicate non-static defs across TUs
    for name, locs in sorted(defs.items()):
        nonstatic = [d for d in locs if not d["static"]]
        files = sorted({d["path"] for d in nonstatic})
        if len(files) > 1:
            detail = "; ".join(
                f"{d['path'].relative_to(ROOT)}:{d['line']}" for d in nonstatic
            )
            findings.append(f"DUP_NONSTATIC_FN {name}: {detail}")

    # 5) known symbols require their providing headers (direct or via common includes)
    def included_basenames(path: Path, depth: int = 0, seen: set[Path] | None = None) -> set[str]:
        if seen is None:
            seen = set()
        text = read(path)
        bases: set[str] = set()
        for m in re.finditer(r'#include\s+"([^"]+)"', text):
            inc = m.group(1)
            bases.add(Path(inc).name)
            candidates = [
                path.parent / inc,
                ROOT / inc,
                ROOT / "engine/editor" / Path(inc).name,
                ROOT / "engine/editor" / inc,
            ]
            for candidate in candidates:
                resolved = candidate.resolve() if candidate.exists() else None
                if resolved and resolved not in seen and depth < 4:
                    seen.add(resolved)
                    bases |= included_basenames(candidate, depth + 1, seen)
        return bases

    for path in PHASE1_TUS:
        if path.name == "engine_gui.cpp":
            continue
        if not path.exists():
            continue
        text = texts[path]
        bases = included_basenames(path)
        for pattern, header_name in SYMBOL_HEADER_REQUIREMENTS:
            if re.search(pattern, text) and header_name not in bases:
                findings.append(
                    f"MISSING_HEADER {path.relative_to(ROOT)} uses /{pattern}/ "
                    f"but does not include {header_name}"
                )

    if findings:
        print(f"FAIL: {len(findings)} editor GUI linkage finding(s)")
        for item in findings:
            print(f"  - {item}")
        return 1

    print(
        "PASS: editor GUI linkage check "
        f"({len(extern_vars)} extern vars, {len(func_decls)} decls, "
        f"{len(texts)} TUs, symbol-header rules={len(SYMBOL_HEADER_REQUIREMENTS)})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
