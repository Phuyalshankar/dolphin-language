---
name: Dolphin language repo (Phuyalshankar/dolphin-language)
description: Notes on cloning/building the "Dolphin" custom-language C++ transpiler project — build fixes and gap-analysis approach.
---

The project transpiles `.dolphin` source into C++ (via `transpiler.cpp` + `generator.cpp`/`parser.cpp`/`lexer.cpp`), then shells out to `g++` to compile/run. Runtime is a single large header (`dolphin_runtime.hpp`) with a dynamic `var` type.

Two real bugs found and fixed when porting to Linux:
- `#define closesocket close` inside a class whose member function is also named `close()` caused the macro to resolve to the member function instead of POSIX `close(fd)`, breaking socket close silently on non-Windows. Fixed by defining the macro as `::close` instead of `close`.
- The CLI (`dolphin.cpp`) hardcoded Windows-only behavior (`.exe` extension, `del` command, `transpiler.exe`) with no `#ifdef _WIN32` guard — needed cross-platform branching to run on Linux/Replit at all.

**Why this matters:** any C++ project with `#ifdef _WIN32` / POSIX macro shims should be treated as untested on Linux until actually compiled there — README/CI claims aside. Always try a real build before trusting portability.

**How to apply:** when auditing or extending a cross-platform C-family codebase, actually attempt a Linux build early; macro-based platform shims are a common source of silent, hard-to-spot bugs (name collisions, missing guards).

A `Makefile` and `ROADMAP.md` (gap analysis, Nepali) were added at `dolphin-language/` in the workspace to make it buildable (`make all && ./dolphin run test.dolphin`) and to track what's missing for a production-grade language (real lexer/AST instead of regex line-parsing, docs, tests, stdlib, tooling, packaging).
