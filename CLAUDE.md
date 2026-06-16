# CLAUDE.md

## Encoding & Formatting

- All source files must be **UTF-8** encoded with **LF** line endings (no CRLF).
- Trailing whitespace must be stripped from every line automatically.

## C Coding Style

- Follow `docs/language/c-style.md` for all C source and header files.
- Comments must use `//` only; block comments (`/* ... */`) are not permitted.

## .ms Script Style

- Follow `docs/language/ms-style.md` for all `.ms` script files.
- Comments must use `//` only; block comments (`/* ... */`) are not permitted.

## Python Script Style

- All functions must have full type hints; use PEP 604 `X | None` / `X | Y` — never `Optional` / `Union`.
- Use builtin generics (`list[str]`, `dict[str, str]`, `tuple[...]`) — never `typing.List` / `typing.Dict`.
- Put `from __future__ import annotations` as the first import so `|` annotations work on older interpreters.
- Annotate empty-container locals explicitly (e.g. `results: dict[str, str] = {}`).
- Scripts that print emoji / CJK must wrap `sys.stdout` / `sys.stderr` in a UTF-8 `io.TextIOWrapper`
  (Windows GBK terminal safety) — see `tests/ci/verify_task.py`.
- Naming: `snake_case` for functions/variables, `UPPER_SNAKE` for module-level constants.

## C Formatting

- After any C implementation step (creating or modifying `.c`/`.h` files), run
  `clang-format -i <changed-files>` before committing.
- The project root `.clang-format` enforces `IndentWidth: 2` per c-style.md §5.1.
- Verify with `clang-format --dry-run --Werror <file>` — a non-zero exit means the file is still dirty.

## Build & Tooling Conventions

- Build directory: **`build/`** only, using a multi-config generator.
  Debug: `cmake --build build --config Debug` · Release: `cmake --build build --config Release`
  On Linux/macOS configure with `-G "Ninja Multi-Config"`; Windows uses the VS default.
  Do **not** create any `build_*` or `build-*` directories.
- clangd include resolution is handled by the root **`compile_flags.txt`** (`-Iinclude -std=c17`).
  Do **not** generate `compile_commands.json` for editor tooling.
