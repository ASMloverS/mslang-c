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
import sys, subprocess, argparse, re, pathlib, tempfile, os, io
import xml.etree.ElementTree as ET

# Force UTF-8 output so emoji / CJK characters are safe on Windows GBK terminals.
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

IMPL_DIR_DEFAULT = "docs/language/impl"
README_DEFAULT   = "docs/language/impl/README.md"


def normalise_label(raw):
    """'T2' -> 'T002',  'P1-T006' -> 'T006'."""
    m = re.fullmatch(r"(?:P\d+-)?T(\d+)", raw, re.IGNORECASE)
    if not m:
        sys.exit(f"ERROR: unrecognised task format '{raw}' (expected T003 or P0-T003)")
    return f"T{int(m.group(1)):03d}"


def find_md(impl_dir, label):
    matches = list(pathlib.Path(impl_dir).glob(f"*-{label}-*.md"))
    if not matches:
        sys.exit(f"ERROR: no .md found for {label} in {impl_dir}")
    if len(matches) > 1:
        names = [p.name for p in matches]
        sys.exit(f"ERROR: multiple files match {label}: {names}")
    return matches[0]


def run_build(build_dir):
    """Returns (ok: bool, stderr: str).
    Returns (True, '') when build_dir doesn't exist (not yet configured).
    """
    if not pathlib.Path(build_dir).exists():
        return True, ""
    r = subprocess.run(
        ["cmake", "--build", build_dir],
        capture_output=True, text=True, encoding="utf-8", errors="replace")
    return r.returncode == 0, r.stderr


def is_multiconfig(build_dir):
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


def run_ctest(build_dir, label, config=None):
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
    subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    return parse_junit(junit)


def parse_junit(path):
    """Parse CTest JUnit XML; returns {test_name: 'pass'|'fail'}."""
    results = {}
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


def parse_checklist(md_path):
    """Returns (items, lines).
    items: list of (line_idx, line, ticked, kind, arg)
    lines: full file lines as list of str
    """
    lines = pathlib.Path(md_path).read_text(encoding="utf-8").splitlines()
    items = []
    for i, line in enumerate(lines):
        if not CHECKLIST.match(line):
            continue
        ticked = CHECKLIST.match(line).group(2) == "x"
        m = V_TAG.search(line)
        if m:
            raw = m.group(1)
            parts = raw.split(":", 1)
            kind, arg = parts[0], (parts[1] if len(parts) > 1 else "")
        else:
            kind, arg = "unverified", ""
        items.append((i, line, ticked, kind, arg))
    return items, lines


def item_result(kind, arg, build_ok, tests):
    if kind == "build":
        return "PASS" if build_ok else "FAIL"
    if kind in ("ctest", "golden", "ms"):
        r = tests.get(arg)
        if r is None:
            return "UNVERIFIED"
        return "PASS" if r == "pass" else "FAIL"
    if kind == "manual":
        return "MANUAL"
    return "UNVERIFIED"


def apply_changes(md_path, items, lines, results, full_pass):
    new = list(lines)
    for (i, line, ticked, kind, arg), result in zip(items, results):
        if result == "PASS" and not ticked:
            new[i] = re.sub(r"\[ \]", "[x]", line, count=1)
    if full_pass:
        for i, line in enumerate(new):
            if "> **状态**：" in line:
                for old in ("⬜", "🚧", "⏸️"):
                    if old in line:
                        new[i] = line.replace(old, "✅")
                        break
                break
    text = "\n".join(l.rstrip() for l in new) + "\n"
    pathlib.Path(md_path).write_text(text, encoding="utf-8")


def sync_readme(readme_path, md_name):
    p = pathlib.Path(readme_path)
    if not p.exists():
        return
    lines = p.read_text(encoding="utf-8").splitlines()
    changed = False
    for i, line in enumerate(lines):
        if md_name in line:
            for old in ("⬜", "🚧", "⏸️"):
                if old in line:
                    lines[i] = line.replace(old, "✅")
                    changed = True
                    break
    if changed:
        p.write_text("\n".join(l.rstrip() for l in lines) + "\n", encoding="utf-8")
        print(f"  README updated: {md_name} -> ✅")


def main():
    ap = argparse.ArgumentParser(
        description="Verify and optionally tick checklist items for a mslang task.")
    ap.add_argument("task",
                    help="task id, e.g. T003 or P0-T003")
    ap.add_argument("--apply", action="store_true",
                    help="write [x] / ✅ back to the .md file on pass")
    ap.add_argument("--build-dir", default="build",
                    help="Debug build directory (default: build)")
    ap.add_argument("--rel-dir", default="build_rel",
                    help="Release build directory (default: build_rel)")
    ap.add_argument("--impl-dir", default=IMPL_DIR_DEFAULT)
    ap.add_argument("--readme",   default=README_DEFAULT)
    args = ap.parse_args()

    label   = normalise_label(args.task)
    md_path = find_md(args.impl_dir, label)

    print(f"Task : {label}")
    print(f"Doc  : {md_path.name}")

    # --- build ---
    print(f"\n[1/3] cmake --build {args.build_dir} (Debug)  ", end="", flush=True)
    debug_ok, _ = run_build(args.build_dir)
    dbg_label = "OK" if debug_ok else ("SKIPPED (not configured)" if not pathlib.Path(args.build_dir).exists() else "FAILED")
    print(dbg_label)

    print(f"[2/3] cmake --build {args.rel_dir} (Release)", end="", flush=True)
    rel_exists = pathlib.Path(args.rel_dir).exists()
    rel_ok, _  = run_build(args.rel_dir)
    rel_label  = "  OK" if (rel_ok and rel_exists) else ("  SKIPPED (not configured)" if not rel_exists else "  FAILED")
    print(rel_label)

    build_ok = debug_ok and rel_ok

    # --- ctest ---
    print(f"[3/3] ctest -L {label}", flush=True)
    tests = {}
    tests.update(run_ctest(args.build_dir, label, config="Debug"))
    tests.update(run_ctest(args.rel_dir,   label, config="Release"))

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
