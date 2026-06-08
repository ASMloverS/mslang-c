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

## Build & Tooling Conventions

- Build directories: **`build/`** (Debug) and **`build_rel/`** (Release) only — matches `tests/ci/build_check.sh`.
  Do **not** create additional generator directories (e.g. `build-ninja/`, `build-make/`).
- clangd include resolution is handled by the root **`compile_flags.txt`** (`-Iinclude -std=c17`).
  Do **not** generate `compile_commands.json` for editor tooling.
