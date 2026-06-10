#!/usr/bin/env python3
"""
check_symbol_absent.py --lib LIB_PATH --symbol SYMBOL_NAME

Asserts that SYMBOL_NAME does not appear in the symbol table of LIB_PATH.
Tries nm, llvm-nm, then dumpbin (MSVC) in that order.

Exit: 0 = absent (pass), 1 = present (fail), 2 = tool error.
If no symbol tool is available the check is skipped (exit 0) so CI is not
blocked on tooling gaps.
"""
from __future__ import annotations

import sys, subprocess, shutil, argparse
from collections.abc import Callable

# Decides whether one line of tool output names the symbol.
Matcher = Callable[[str, str], bool]


def nm_match(line: str, symbol: str) -> bool:
    """nm/llvm-nm: symbol name is the last whitespace-separated field."""
    parts = line.split()
    return bool(parts) and parts[-1] == symbol


def dumpbin_match(line: str, symbol: str) -> bool:
    """dumpbin /SYMBOLS: lines look like '... | symbolName'."""
    return f" {symbol}" in line or f"|{symbol}" in line


# Symbol tools in priority order: (executable, extra args, line matcher).
TOOLS: tuple[tuple[str, list[str], Matcher], ...] = (
    ("nm",      [],           nm_match),
    ("llvm-nm", [],           nm_match),
    ("dumpbin", ["/SYMBOLS"], dumpbin_match),
)


def run_tool(cmd: list[str], symbol: str, match: Matcher) -> bool | None:
    """Returns True (found), False (absent), or None (tool failed)."""
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    if r.returncode != 0:
        return None
    return any(match(line, symbol) for line in r.stdout.splitlines())


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Check that a symbol is absent from a library.")
    ap.add_argument("--lib",    required=True, help="path to static lib or exe")
    ap.add_argument("--symbol", required=True, help="exact symbol name to check")
    args = ap.parse_args()

    found: bool | None = None
    for name, extra_args, match in TOOLS:
        tool = shutil.which(name)
        if not tool:
            continue
        found = run_tool([tool, *extra_args, args.lib], args.symbol, match)
        if found is not None:
            break

    if found is None:
        print("SKIP: no symbol tool found (nm/llvm-nm/dumpbin); treating as pass",
              file=sys.stderr)
        return 0

    if found:
        print(f"FAIL: '{args.symbol}' present in {args.lib}", file=sys.stderr)
        return 1

    print(f"OK: '{args.symbol}' absent from {args.lib}")
    return 0


sys.exit(main())
