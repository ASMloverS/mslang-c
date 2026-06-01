# mslang

A dynamically typed scripting language implemented in C11. Syntax inspired by Go, semantics inspired by Python.

- Bytecode VM with a generational tracing GC
- `go` + channels and `async`/`await` on a unified coroutine scheduler
- Python-style `class`, exceptions, and built-ins
- C embedding API compatible with a moving GC (handle-based, V8/JNI style)
- CMake build — Windows / Linux / macOS

## Documentation

See [`docs/language/`](docs/language/) for the full language reference:

| Doc | Topic |
|---|---|
| `overview.md` | Design goals and quick examples |
| `syntax.md` | Grammar and operators |
| `type-system.md` | Object model and type descriptors |
| `execution.md` | CLI execution modes and `__mscache__` bytecode caching |
| `vm.md` | Bytecode VM internals |
| `gc.md` | Generational GC strategy |
| `concurrency.md` | Goroutines, channels, async/await |
| `stdlib.md` | Standard library reference |
| `modules.md` | Module resolution |
| `errors.md` | Exception hierarchy |
| `c-api.md` | C embedding and extension API |
| `c-style.md` | C coding style conventions |

## Coding Style

See [`docs/language/c-style.md`](docs/language/c-style.md) for the full C coding style guide.
Naming conventions intentionally deviate from Google C Style (camelCase instead of snake_case).
See [`CLAUDE.md`](CLAUDE.md) for project-level encoding and formatting rules.
