"""Extract public declarations from engine/ and src/app/ headers into markdown."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "_api_extract.md"
SKIP = {"WICTextureLoader11.h"}

decl_re = re.compile(
    r"^\s*((?:inline|static|virtual|explicit|constexpr)\s+)*"
    r"((?:const\s+)?(?:unsigned\s+)?[\w:]+(?:\s*<[^;{]+>)?(?:\s*[*&]+)?)"
    r"\s+([\w:~]+)\s*\(([^;]*)\)\s*(const)?\s*(?:=\s*0)?\s*(?:override)?\s*;",
    re.M,
)
macro_re = re.compile(r"^\s*#define\s+(REGISTER_\w+|SCRIPT_\w+|SAFE_\w+)\b")
enum_re = re.compile(r"^\s*enum\s+(?:class\s+)?(\w+)", re.M)
class_re = re.compile(r"^\s*(?:class|struct)\s+(\w+)", re.M)

out: list[str] = []
for root_name in ("engine", "src/app"):
    root = ROOT / root_name
    for p in sorted(root.rglob("*.h")):
        if p.name in SKIP:
            continue
        text = p.read_text(encoding="utf-8", errors="replace")
        text2 = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        text2 = re.sub(r"//.*?$", "", text2, flags=re.M)
        decls = []
        for m in decl_re.finditer(text2):
            name = m.group(3)
            if name in ("if", "for", "while", "switch", "return", "sizeof"):
                continue
            sig = re.sub(r"\s+", " ", m.group(0).strip())
            decls.append(sig)
        macros = [m.group(1) for m in macro_re.finditer(text)]
        enums = list(dict.fromkeys(enum_re.findall(text2)))
        classes = list(dict.fromkeys(class_re.findall(text2)))
        if not (decls or macros or enums or classes):
            continue
        rel = p.relative_to(ROOT).as_posix()
        out.append(f"### `{rel}`")
        if classes:
            out.append("**Types:** " + ", ".join(classes))
        if enums:
            out.append("**Enums:** " + ", ".join(enums))
        for d in decls:
            out.append(f"- `{d}`")
        for m in macros:
            out.append(f"- macro `{m}`")
        out.append("")

OUT.write_text("\n".join(out), encoding="utf-8")
sections = sum(1 for x in out if x.startswith("###"))
print(f"Wrote {OUT} ({len(out)} lines, {sections} headers)")

# Rebuild the full reference doc when extract is regenerated.
assemble = ROOT / "tools" / "assemble_api_reference.py"
if assemble.is_file():
    import runpy
    runpy.run_path(str(assemble), run_name="__main__")
