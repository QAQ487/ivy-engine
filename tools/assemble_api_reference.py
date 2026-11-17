"""Assemble docs/ENGINE_API_REFERENCE.md from its hand-written intro + header extract.

The intro (everything above the per-header catalog heading) is maintained by hand
inside ENGINE_API_REFERENCE.md itself. This script keeps that intro, refreshes the
header count line, and replaces the catalog with docs/_api_extract.md (a generated,
gitignored intermediate written by tools/extract_api_headers.py).
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXTRACT = ROOT / "docs" / "_api_extract.md"
OUT = ROOT / "docs" / "ENGINE_API_REFERENCE.md"

CATALOG_HEADING = "## 6. Complete per-header function catalog"
CATALOG_PREAMBLE = (
    "Declarations below were machine-extracted from public headers under `engine/` and `src/app/`.\n"
    "Constructors, destructors, and some `private:` helpers declared in headers may appear.\n"
    "Vendor header `WICTextureLoader11.h` is omitted.\n\n"
)
COUNT_RE = re.compile(r"\(~?\d+ headers[^)]*\)")


def header_count(extract_text: str) -> int:
    return sum(1 for line in extract_text.splitlines() if line.startswith("### `"))


def main() -> int:
    if not EXTRACT.is_file():
        print(f"error: missing {EXTRACT}; run tools/extract_api_headers.py first", file=sys.stderr)
        return 1
    if not OUT.is_file():
        print(f"error: missing {OUT}; the hand-written intro lives there", file=sys.stderr)
        return 1

    current = OUT.read_text(encoding="utf-8")
    idx = current.find(CATALOG_HEADING)
    if idx < 0:
        print(f"error: {OUT} has no '{CATALOG_HEADING}' heading", file=sys.stderr)
        return 1

    extract = EXTRACT.read_text(encoding="utf-8")
    count = header_count(extract)
    intro = COUNT_RE.sub(f"({count} headers; regenerate to refresh)", current[:idx], count=1)
    OUT.write_text(intro + CATALOG_HEADING + "\n\n" + CATALOG_PREAMBLE + extract, encoding="utf-8")
    print(f"Wrote {OUT} ({OUT.stat().st_size} bytes, {count} headers)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
