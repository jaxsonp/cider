# Cider

## Overview

Cider is a compiler for a systems programming language: C-like simplicity
(small language, easy to implement, no hidden runtime magic) with syntax and
some features borrowed from Rust. Long-term goals, in order:

1. Bootstrap a usable compiler in C++ (current stage).
2. Self-host: rewrite the compiler in Cider itself.
3. Use Cider to build a full OS.

Because of goals 2 and 3, the language and its future standard library must
stay independent of C and the C standard library — don't reach for libc
semantics as a design crutch, and don't assume a hosted environment exists
underneath the language itself. The *compiler implementation* (this C++
codebase) can and does use the C++ standard library; that constraint only
applies to Cider-the-language and anything written in it.

## Project layout

- `src/frontend/` — lexer (`Lexer.*`), parser/AST (`AST_parse.cpp`,
  `AST.hpp`), semantic analysis (`AST_semantics.cpp`), type system
  (`FrontendType.*`), IR emission (`AST_emitIr.cpp`), debug printing
  (`AST_print.cpp`).
- `src/ir/` — the intermediate representation (`IR.*`, `IR_instructions.*`,
  `IrType.*`), the builder the frontend emits through (`IrWriter.*`), and a
  textual IR printer (`IrPrinter.*`) for debugging.
- `src/backend/` — codegen and object file emission. `codegen/riscv/` is the
  only backend right now (targets `linux-riscv32g`, RV32G/ILP32D, ELF32).
  `objwriter/elf/` writes ELF output.
- `src/utils/` — `CliParser` (custom argv parser), `error.hpp`
  (`CompilerError` with typed constructors: syntax/name/type/semantic/
  unsupported/file_io/unimplemented/internal, each mapping to an
  `ExitCode`), `logging.hpp` (verbosity-gated `log`/`log_v`/`log_vv`).
- `src/compile.cpp` / `compile.hpp` — top-level pipeline: source → lex →
  parse → semantic check → IR → codegen → object file.
- `src/main.cpp` — CLI entry point (`cdrc`).
- `grammar.ebnf` — the authoritative grammar. Update this whenever syntax
  changes.
- `testing/` — Python test runner and `.cdr` test fixtures (see below).
- `README.md` — has a running TODO list (`## TODOs`, "For Jaxson's eyes
  only") tracking near-term work and a list of pre-self-hosting
  requirements. Check it for current priorities before assuming a feature
  doesn't exist yet or planning new work.

## Building

To configure build files:

```
cmake -S . -B build
```

To build

```
cmake --build build
```

Produces `build/ciderc`.

## Running the compiler

```
build/ciderc <file.cdr> -t linux-riscv32g -o out
```

`--emit=<kind>[,...]` selects which artifacts to produce: `ast`, `ir`,
`asm`, or `exe` (the default). The compiler runs only as far as the furthest
kind requested, so `--emit=ir` dumps IR without running codegen — useful when
the backend is broken. Each kind defaults to a path derived from the input
(`foo.cdr` → `foo.ast`/`foo.ir`/`foo.s`); append `=FILE` to redirect one, or
`=stdout` to write it to standard output. `-o` names the executable only.
`--emit=asm` is reserved but not implemented (the RV32 backend can't emit
text yet).

Other useful flags: `-v` (repeatable, increases verbosity), `-q` (quiet).
Run `build/ciderc -h` for the full list — flags are defined in `src/main.cpp`
via `CliParser`, keep this doc and `--help` in sync manually if you add any.

## Testing

Tests are `.cdr` files under `testing/tests/`, organized into numbered
subdirectories by feature area (e.g. `00_main`, `01_expressions`). Expected
results are declared as `//! KEY=VALUE` comments at the very top of the
file (case-insensitive keys): `BUILD_EXIT_CODE`, `EXIT_CODE`, `STDOUT`,
`STDERR`, all optional and defaulting to `0`/`""`.

Run the whole suite with:

```
python3 testing/run_tests.py build/ciderc
```

Requires Python >= 3.14 and `qemu-user-static` (tests run compiled RV32
binaries under `qemu-riscv32-static`) since there's no native backend yet.

When adding a language feature, add `.cdr` test cases alongside it in the
matching (or a new) numbered subdirectory rather than only relying on
manual testing.

## Language notes worth knowing before editing the frontend

- Top-level functions are hoisted; global variable initialization order is
  resolved via dependency DAG, not declaration order.
- Binary operators are left-associative, except equality/comparison, which
  require explicit parens when chained.
- Type names start with a letter; a leading digit means it's a numeric
  literal (integer literals carry an explicit type suffix, e.g. `1u32`,
  `100i32` — see `testing/tests/00_main/*.cdr` for examples).
- `grammar.ebnf` is the source of truth for syntax — if a parser change
  alters accepted syntax, update the grammar file in the same change.

## Development guidelines

### Code style

- Tabs for indentation (match surrounding files).

- New `CompilerError`s should use the existing typed factory functions
  (`syntax_error`, `type_error`, `semantic_error`, etc.) rather than a
  generic message, so exit codes stay meaningful — see
  `testing/tests/00_main/incorrect_ret_type_1.cdr` for how tests assert on
  `BUILD_EXIT_CODE`.
- This is a solo/early-stage project — prefer small, direct changes over
  speculative abstraction. Don't add functionality beyond what's needed for
  the language features described in the README TODO list.
