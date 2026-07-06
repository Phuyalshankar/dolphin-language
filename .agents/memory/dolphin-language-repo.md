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

A `Makefile` and `ROADMAP.md` (gap analysis, Nepali) were added at `dolphin-language/` in the workspace to make it buildable (`make all && ./dolphin run test.dolphin`) and to track what's missing for a production-grade language (docs, tests, stdlib, tooling, packaging).

The original regex/line-based parser (`lexer.hpp/.cpp`, `parser.hpp/.cpp`, `generator.hpp/.cpp`) was replaced with a real pipeline: char-by-char `Lexer` → recursive-descent `Parser` producing a full `ast.hpp` node hierarchy → `Codegen` emitting C++ (same runtime call conventions as before). `transpiler.cpp` and the `Makefile` source list were rewritten to match. Comment preservation was intentionally dropped (cosmetic only).

Gotchas hit while porting codegen to the new AST pipeline (would recur in any similar transpile-to-C++ redesign):
- Every function/lambda `return` must be wrapped as `return var(expr);` — otherwise a body with mixed return types (e.g. a built-in `bool` from `&&`/comparisons vs. the runtime's `var` type) fails to compile with "inconsistent deduced return type".
- Top-level (global-scope) variables must be emitted as real C++ globals (declared in the pre-main section), not as `main()` locals — otherwise separately-generated top-level functions can't see them.
- Keywords that are also usable as constructor-style expressions (this codebase's `pin(13, OUTPUT)`) need a primary-expression parse path, not just a statement-keyword path, or valid source fails to parse.
- A pre-existing runtime limitation surfaced during testing, not fixed: the `Pin` type has no shared/reference semantics like `var` does, so lambda closures capturing a `pin` variable by value don't see mutations made inside the closure.

Added a second codegen target for real-hardware flashing (`dolphin flash <file> <board> <port>`, boards: esp32/esp8266/uno/nano/mega/stm32 via arduino-cli), alongside the original PC-simulation target (`dolphin run`). Key lessons if extending further:
- Any statement-hoisting rule for one top-level statement kind (e.g. `VarAssign` promoting globals when `scopeStack.size() == 1`) must be applied to *every* statement kind that can declare top-level state (`PinDecl` here was missed at first) — otherwise splitting single-function output (like `main()`) into multiple real functions (`setup()`/`loop()`) silently produces variables that are locals in one function and out-of-scope in the other, a compile error only caught by actually generating+compiling output for the new target.
- Never redefine platform-provided macros (`HIGH`/`LOW`/`INPUT`/`OUTPUT` from `Arduino.h`) in a runtime header meant to be compiled by the vendor toolchain — reuse theirs, and only translate identifiers that don't already exist on the target platform (e.g. `INPUT`/`OUTPUT` were remapped in codegen to `PIN_INPUT`/`PIN_OUTPUT` specifically to avoid this collision).
- When you can't run the real target toolchain (no arduino-cli/board in a cloud sandbox), a minimal hand-written stub header replicating just the functions you call (`pinMode`, `digitalWrite`, `Serial.print`, etc.) is enough to syntax- and logic-check generated output with plain `g++` before shipping — catches real bugs (e.g. the scoping issue above) without needing the actual hardware SDK.
- `.gitignore` entries like `.vscode/` silently swallow any new `.vscode/*.json` you add later (e.g. a `tasks.json` meant to ship to users) — check existing ignore rules before assuming a new file in an ignored directory will actually reach git.
