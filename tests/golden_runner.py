#!/usr/bin/env python3
"""
golden_runner.py  --cmd CMD --input INPUT --expected EXPECTED_FILE

Runs CMD with INPUT as argument, compares stdout to EXPECTED_FILE.
Exit: 0=match, 1=mismatch, 2=run error.
Used by CTest to drive lexer/parser/compiler golden tests.
"""
from __future__ import annotations

import sys, subprocess, argparse, pathlib, difflib, io

# Force UTF-8 output so diff content (may contain CJK) is safe on Windows
# GBK terminals.
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--cmd",      required=True, nargs="+")
    p.add_argument("--input",    required=True)
    p.add_argument("--expected", required=True)
    args = p.parse_args()

    try:
        result = subprocess.run(
            args.cmd + [args.input],
            capture_output=True, text=True, encoding="utf-8", errors="replace",
            timeout=10)
    except (OSError, subprocess.TimeoutExpired) as e:
        print(f"ERROR: {' '.join(args.cmd)}: {e}", file=sys.stderr)
        return 2

    expected = pathlib.Path(args.expected).read_text(encoding="utf-8")
    if result.stdout == expected:
        return 0

    diff = difflib.unified_diff(
        expected.splitlines(keepends=True),
        result.stdout.splitlines(keepends=True),
        fromfile="expected", tofile="actual")
    sys.stderr.writelines(diff)
    return 1


sys.exit(main())
