#!/usr/bin/env python3
"""Static check: the audio path must not contain dynamic-allocation constructs.

The brief requires "no heap allocations in the audio callback".  Two independent
checks enforce that here:

  * tools/check_no_alloc.py (this script) — greps the DSP sources for allocation
    constructs (new/delete, malloc, std::vector, push_back, reserve, ...); and
  * tests/test_engine.cpp — a runtime allocator sentinel that counts bytes
    allocated while rendering, plus static_asserts that every DSP class is
    trivially copyable (so no member can own heap memory).

Exit code 0 means clean.  Run from the repository root or with --root.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# Constructs that would allocate (or could hide allocation) in the audio path.
FORBIDDEN = [
    (r"\bnew\b(?!\[?\s*\])", "operator new (heap allocation)"),
    (r"\bdelete\b", "operator delete"),
    (r"\bmalloc\s*\(", "malloc"),
    (r"\bcalloc\s*\(", "calloc"),
    (r"\brealloc\s*\(", "realloc"),
    (r"\bstd::vector\b", "std::vector (dynamic container)"),
    (r"\bstd::string\b", "std::string (dynamic container)"),
    (r"\bstd::function\b", "std::function (may allocate)"),
    (r"\bstd::map\b", "std::map (dynamic container)"),
    (r"\bstd::unordered_map\b", "std::unordered_map (dynamic container)"),
    (r"\bstd::shared_ptr\b", "std::shared_ptr"),
    (r"\bstd::unique_ptr\b", "std::unique_ptr"),
    (r"\.push_back\s*\(", "push_back"),
    (r"\.resize\s*\(", "resize"),
    (r"\.reserve\s*\(", "reserve"),
]

# Files allowed to include the headers that pull in containers, and lines that
# legitimately mention a forbidden token (comments documenting the rule).
ALLOWED_FILE_PATTERNS = [
    r"^src/sh101/Params\.h$",      # parameter tables only, no audio-path state
    # Preset files are read and written on the message thread (save/load, the
    # host's state blob); nothing in this header runs during rendering.  The
    # callback itself is covered by the runtime allocator sentinel in
    # tests/test_engine.cpp.
    r"^src/sh101/PresetIO\.h$",
]

# Comments are stripped before scanning, so the many "brief: no heap allocations"
# notes in the headers do not trip the check.
COMMENT_BLOCK = re.compile(r"/\*.*?\*/", re.DOTALL)
COMMENT_LINE = re.compile(r"//[^\n]*")


def strip_comments(text: str) -> str:
    return COMMENT_LINE.sub("", COMMENT_BLOCK.sub("", text))


def scan(root: Path, verbose: bool = False) -> int:
    src_dirs = [root / "src" / "sh101", root / "src" / "plugin"]
    problems: list[str] = []
    files = 0
    for d in src_dirs:
        if not d.is_dir():
            continue
        for path in sorted(d.glob("*.h")) + sorted(d.glob("*.cpp")):
            rel = path.relative_to(root).as_posix()
            if any(re.match(p, rel) for p in ALLOWED_FILE_PATTERNS):
                if verbose:
                    print(f"  skip (allowed) {rel}")
                continue
            files += 1
            text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
            for pattern, why in FORBIDDEN:
                for m in re.finditer(pattern, text):
                    line = text[: m.start()].count("\n") + 1
                    problems.append(f"{rel}:{line}: {why}")

    print(f"check_no_alloc: scanned {files} audio-path file(s)")
    for p in problems:
        print(f"  VIOLATION {p}")
    if problems:
        print(f"check_no_alloc: FAILED ({len(problems)} violation(s))")
        return 1
    print("check_no_alloc: OK - no dynamic-allocation constructs in the DSP sources")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".", help="repository root")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()
    return scan(Path(args.root).resolve(), args.verbose)


if __name__ == "__main__":
    sys.exit(main())
