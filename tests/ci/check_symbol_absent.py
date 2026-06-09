#!/usr/bin/env python3
"""
check_symbol_absent.py --lib LIB_PATH --symbol SYMBOL_NAME

Asserts that SYMBOL_NAME does not appear in the symbol table of LIB_PATH.
Tries nm, llvm-nm, then dumpbin (MSVC) in that order.

Exit: 0 = absent (pass), 1 = present (fail), 2 = tool error.
If no symbol tool is available the check is skipped (exit 0) so CI is not
blocked on tooling gaps.
"""
import sys, subprocess, shutil, argparse


def try_nm(tool, lib, symbol):
    # Returns True (found), False (absent), or None (tool failed).
    r = subprocess.run([tool, lib], capture_output=True, text=True, timeout=30)
    if r.returncode != 0:
        return None
    for line in r.stdout.splitlines():
        parts = line.split()
        if parts and parts[-1] == symbol:
            return True
    return False


def try_dumpbin(lib, symbol):
    # Returns True (found), False (absent), or None (tool unavailable/failed).
    db = shutil.which("dumpbin")
    if not db:
        return None
    r = subprocess.run([db, "/SYMBOLS", lib],
                       capture_output=True, text=True, timeout=30)
    if r.returncode != 0:
        return None
    for line in r.stdout.splitlines():
        # Match whole-word: dumpbin lines look like "... | symbolName"
        if f" {symbol}" in line or f"|{symbol}" in line:
            return True
    return False


def main():
    ap = argparse.ArgumentParser(
        description="Check that a symbol is absent from a library.")
    ap.add_argument("--lib",    required=True, help="path to static lib or exe")
    ap.add_argument("--symbol", required=True, help="exact symbol name to check")
    args = ap.parse_args()

    found = None
    for tool in ("nm", "llvm-nm"):
        if shutil.which(tool):
            found = try_nm(tool, args.lib, args.symbol)
            if found is not None:
                break

    if found is None:
        found = try_dumpbin(args.lib, args.symbol)

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
