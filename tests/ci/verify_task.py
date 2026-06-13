#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
verify_task.py <task_id> [--apply] [--build-dir DIR] [--rel-dir DIR]
                         [--impl-dir DIR] [--readme PATH]

Verifies checklist items for a mslang impl task:
  1. Builds Debug and Release.
  2. Runs ctest -L <task_label> in both build dirs.
  3. Maps each checklist line to its <!-- v:... --> tag and join with results.
  4. Prints a PASS/FAIL/UNVERIFIED/MANUAL table.
  5. With --apply: flips passing [ ] -> [x] in the .md file; if all
     non-manual items pass, flips status emoji to ✅ and syncs README.md.

Checklist tag vocabulary:
  <!-- v:build -->             passed by successful Debug + Release build (-Werror)
  <!-- v:ctest:<name> -->      passed when ctest test <name> passes
  <!-- v:golden:<name> -->     alias for v:ctest (golden tests are ctest entries)
  <!-- v:ms:<name> -->         alias for v:ctest (.ms tests are ctest entries)
  <!-- v:manual:<reason> -->   not auto-verified; reported only, never auto-ticked

Lines without a v: tag are reported as UNVERIFIED and never auto-ticked.

Exit: 0 if all non-manual items PASS, non-zero otherwise.
"""
from __future__ import annotations

import sys, subprocess, argparse, re, pathlib, tempfile, os, io
import xml.etree.ElementTree as ET

# Force UTF-8 output so emoji / CJK characters are safe on Windows GBK terminals.
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

IMPL_DIR_DEFAULT = "docs/language/impl"
README_DEFAULT   = "docs/language/impl/README.md"

# Pending-status replacements tried in order when a task fully passes:
# full "emoji + text" forms first, bare-emoji fallback last (README rows,
# blocked lines with free-form reasons, etc.).
PENDING_TO_DONE = (
    ("⬜ 未开始", "✅ 已完成"),
    ("🚧 进行中", "✅ 已完成"),
    ("⬜", "✅"),
    ("🚧", "✅"),
    ("⏸️", "✅"),
)

# golden / ms checklist tags are aliases: both verify via ctest entries.
KIND_ALIASES = {"golden": "ctest", "ms": "ctest"}

# One checklist entry: (line_idx, line, ticked, kind, arg)
Item = tuple[int, str, bool, str, str]


def mark_done(line: str) -> str:
    """Replace the first pending status marker in line with its done form."""
    for old, new in PENDING_TO_DONE:
        if old in line:
            return line.replace(old, new)
    return line


def write_lines(path: str | pathlib.Path, lines: list[str]) -> None:
    """Write lines back with trailing whitespace stripped and a final newline."""
    text = "\n".join(l.rstrip() for l in lines) + "\n"
    pathlib.Path(path).write_text(text, encoding="utf-8")


def normalise_label(raw: str) -> str:
    """'T2' -> 'T002',  'P1-T006' -> 'T006'."""
    m = re.fullmatch(r"(?:P\d+-)?T(\d+)", raw, re.IGNORECASE)
    if not m:
        sys.exit(f"ERROR: unrecognised task format '{raw}' (expected T003 or P0-T003)")
    return f"T{int(m.group(1)):03d}"


def find_md(impl_dir: str, label: str) -> pathlib.Path:
    matches = list(pathlib.Path(impl_dir).glob(f"*-{label}-*.md"))
    if not matches:
        sys.exit(f"ERROR: no .md found for {label} in {impl_dir}")
    if len(matches) > 1:
        names = [p.name for p in matches]
        sys.exit(f"ERROR: multiple files match {label}: {names}")
    return matches[0]


def run_build(build_dir: str, config: str | None = None) -> tuple[bool, str]:
    """Returns (ok: bool, stderr: str).
    Returns (True, '') when build_dir doesn't exist (not yet configured).
    config: 'Debug' or 'Release'; if given, passes --config <config>.
    """
    if not pathlib.Path(build_dir).exists():
        return True, ""
    cmd = ["cmake", "--build", build_dir]
    if config:
        cmd += ["--config", config]
    r = subprocess.run(
        cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    return r.returncode == 0, r.stderr


def build_label(ok: bool, exists: bool) -> str:
    """Human-readable build outcome for the progress line."""
    if not exists:
        return "SKIPPED (not configured)"
    return "OK" if ok else "FAILED"


def is_multiconfig(build_dir: str) -> bool:
    """True when the build dir uses a multi-config generator (VS, Xcode)."""
    cache = pathlib.Path(build_dir) / "CMakeCache.txt"
    try:
        text = cache.read_text(encoding="utf-8", errors="ignore")
        for line in text.splitlines():
            if line.startswith("CMAKE_CONFIGURATION_TYPES"):
                return True
    except OSError:
        pass
    return False


def run_ctest(build_dir: str, label: str, config: str | None = None) -> dict[str, str]:
    """Returns dict {test_name: 'pass'|'fail'}.
    config: 'Debug' or 'Release'; auto-detected when None.
    """
    if not pathlib.Path(build_dir).exists():
        return {}
    with tempfile.NamedTemporaryFile(suffix=".xml", delete=False) as f:
        junit = f.name
    cmd = ["ctest", "--test-dir", build_dir,
           "-L", label, "--output-junit", junit,
           "--output-on-failure"]
    if config and is_multiconfig(build_dir):
        cmd += ["-C", config]
    try:
        subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
        return parse_junit(junit)
    finally:
        os.unlink(junit)


def parse_junit(path: str) -> dict[str, str]:
    """Parse CTest JUnit XML; returns {test_name: 'pass'|'fail'}."""
    results: dict[str, str] = {}
    try:
        for tc in ET.parse(path).iter("testcase"):
            name = tc.get("name", "")
            failed = tc.find("failure") is not None or tc.find("error") is not None
            results[name] = "fail" if failed else "pass"
    except Exception:
        pass
    return results


V_TAG    = re.compile(r"<!--\s*v:(.+?)\s*-->")
CHECKLIST = re.compile(r"^(\s*-\s*\[)([ x])(\].*)")


def parse_checklist(md_path: pathlib.Path) -> tuple[list[Item], list[str]]:
    """Returns (items, lines).
    items: list of (line_idx, line, ticked, kind, arg)
    lines: full file lines as list of str
    """
    lines = pathlib.Path(md_path).read_text(encoding="utf-8").splitlines()
    items: list[Item] = []
    for i, line in enumerate(lines):
        cm = CHECKLIST.match(line)
        if not cm:
            continue
        ticked = cm.group(2) == "x"
        m = V_TAG.search(line)
        if m:
            raw = m.group(1)
            parts = raw.split(":", 1)
            kind, arg = parts[0], (parts[1] if len(parts) > 1 else "")
        else:
            kind, arg = "unverified", ""
        items.append((i, line, ticked, kind, arg))
    return items, lines


def item_result(kind: str, arg: str, build_ok: bool, tests: dict[str, str]) -> str:
    kind = KIND_ALIASES.get(kind, kind)
    if kind == "build":
        return "PASS" if build_ok else "FAIL"
    if kind == "ctest":
        r = tests.get(arg)
        if r is None:
            return "UNVERIFIED"
        return "PASS" if r == "pass" else "FAIL"
    if kind == "manual":
        return "MANUAL"
    return "UNVERIFIED"


def apply_changes(md_path: pathlib.Path, items: list[Item], lines: list[str],
                  results: list[str], full_pass: bool) -> None:
    new = list(lines)
    for (i, line, ticked, kind, arg), result in zip(items, results):
        if result == "PASS" and not ticked:
            new[i] = re.sub(r"\[ \]", "[x]", line, count=1)
    if full_pass:
        for i, line in enumerate(new):
            if "> **状态**：" in line:
                new[i] = mark_done(line)
                break
    write_lines(md_path, new)


def sync_readme(readme_path: str, md_name: str) -> None:
    p = pathlib.Path(readme_path)
    if not p.exists():
        return
    lines = p.read_text(encoding="utf-8").splitlines()
    changed = False
    for i, line in enumerate(lines):
        if md_name in line:
            replaced = mark_done(line)
            if replaced != line:
                lines[i] = replaced
                changed = True
    if changed:
        write_lines(p, lines)
        print(f"  README updated: {md_name} -> ✅")


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Verify and optionally tick checklist items for a mslang task.")
    ap.add_argument("task",
                    help="task id, e.g. T003 or P0-T003")
    ap.add_argument("--apply", action="store_true",
                    help="write [x] / ✅ back to the .md file on pass")
    ap.add_argument("--build-dir", default="build",
                    help="Build directory (default: build)")
    ap.add_argument("--impl-dir", default=IMPL_DIR_DEFAULT)
    ap.add_argument("--readme",   default=README_DEFAULT)
    args = ap.parse_args()

    label   = normalise_label(args.task)
    md_path = find_md(args.impl_dir, label)

    print(f"Task : {label}")
    print(f"Doc  : {md_path.name}")

    # --- build ---
    bld_exists = pathlib.Path(args.build_dir).exists()
    print(f"\n[1/3] cmake --build {args.build_dir} --config Debug  ", end="", flush=True)
    debug_ok, _ = run_build(args.build_dir, config="Debug")
    print(build_label(debug_ok, bld_exists))

    print(f"[2/3] cmake --build {args.build_dir} --config Release  ", end="", flush=True)
    rel_ok, _ = run_build(args.build_dir, config="Release")
    print(build_label(rel_ok, bld_exists))

    build_ok = debug_ok and rel_ok

    # --- ctest ---
    print(f"[3/3] ctest -L {label}", flush=True)
    tests: dict[str, str] = {}
    tests.update(run_ctest(args.build_dir, label, config="Debug"))
    tests.update(run_ctest(args.build_dir, label, config="Release"))

    # --- parse & evaluate ---
    items, lines = parse_checklist(md_path)
    if not items:
        print("\nNo checklist items found in doc.")
        return 0

    results = [item_result(k, a, build_ok, tests) for (_, _, _, k, a) in items]

    # --- table ---
    print()
    W = 55
    print(f"{'#':<4} {'Result':<12} {'Backend':<28} {'Checklist summary'}")
    print("-" * (4 + 12 + 28 + W))
    for (i, line, ticked, kind, arg), result in zip(items, results):
        icon = {"PASS":"✅","FAIL":"🔴","MANUAL":"⚙ ","UNVERIFIED":"❓"}.get(result, "?")
        backend = f"{kind}:{arg}" if arg else kind
        summary = line.strip()[:W]
        print(f"{i+1:<4} {icon} {result:<10} {backend:<28} {summary}")

    # --- summary ---
    non_manual = [r for r in results if r != "MANUAL"]
    full_pass  = bool(non_manual) and all(r == "PASS" for r in non_manual)
    cnt = {r: results.count(r) for r in ("PASS","FAIL","UNVERIFIED","MANUAL")}
    print(f"\nPASS {cnt['PASS']} | FAIL {cnt['FAIL']} | "
          f"UNVERIFIED {cnt['UNVERIFIED']} | MANUAL {cnt['MANUAL']}")

    if args.apply:
        apply_changes(md_path, items, lines, results, full_pass)
        if full_pass:
            sync_readme(args.readme, md_path.name)
            print(f"Applied: ✅ {label} — all non-manual items passed.")
        else:
            print(f"Applied: {cnt['PASS']} items ticked (not all passed).")
    elif full_pass:
        print("All non-manual items PASS — run with --apply to update the doc.")

    return 0 if full_pass else 1


sys.exit(main())
