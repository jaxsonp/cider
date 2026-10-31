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
- `src/backend/` — codegen and object file emission.
  - `Target.*` — the table of supported targets. A target is an architecture
    (`Arch`), a set of feature flags (ISA extensions), an ABI, an OS and an
    object format. Adding a target of an existing architecture is one more
    row in `supported_targets`.
  - `codegen/CodeGenerator.*` — architecture independent driver: walks the
    IR and calls a backend's `lower_*` hooks, then points calls at their
    targets. `codegen/RegAllocator.*` is the (also architecture independent)
    local register allocator backends allocate through.
  - `codegen/riscv/` — RISC-V backend, covering RV32 and RV64 with or
    without the C extension (targets `linux-riscv32g`, `linux-riscv32gc`,
    `linux-riscv64g`, `linux-riscv64gc`). `RiscvInstruction.*` encodes (and
    compresses) single instructions, `RiscvAssembler.*` is the buffer
    instructions are written to, and `RiscvCodeGenerator.*` implements the
    lowering hooks.
  - `codegen/x86/` — x86 backend, covering 32 bit x86 and x86-64 (targets
    `linux-x86`, `linux-x86_64`). `x86Operand.hpp` has the typed operands
    (`Reg<W>`, `Mem<W>`, `Imm<W>`), `x86Assembler.*` the `write_*` functions,
    whose signatures only accept operand kinds/widths that the instruction
    has an encoding for (invalid instructions don't compile), and
    `x86Instruction.*` the untyped encoder, which rejects anything needing a
    REX prefix in 32 bit mode. `x86CodeGenerator.*` implements the lowering
    hooks.
  - `objwriter/elf/` writes ELF output (ELF32 and ELF64 from one template).
- `src/utils/` — `CliParser` (custom argv parser), `error.hpp`
  (`CompilerError` with typed constructors: syntax/name/type/semantic/
  unsupported/file_io/unimplemented/internal, each mapping to an
  `ExitCode`), `logging.hpp` (verbosity-gated `log`/`log_v`/`log_vv`).
- `src/compile.cpp` / `compile.hpp` — top-level pipeline: source → lex →
  parse → semantic check → IR → codegen → object file.
- `src/main.cpp` — CLI entry point (`cdrc`).
- `grammar.ebnf` — the authoritative grammar. Update this whenever syntax
  changes.
- `testing/` — C++ test runner (`src/`) and `.cdr` end to end test fixtures
  (`e2e_tests/`), see below. `.old_run_tests.py` is the previous Python
  runner and is no longer used.
- `TODO.md` — the running TODO list ("For Jaxson's eyes only") tracking
  near-term work and a list of pre-self-hosting requirements, plus notes for
  the eventual language docs. Check it for current priorities before
  assuming a feature doesn't exist yet or planning new work.

## Building

To configure build files:

```
cmake -S . -B build
```

To build

```
cmake --build build
```

Produces `build/ciderc` (also copied to the repository root) and the test
runner, `build/testing/ciderc_test_runner`. The first configure needs network
access, the test runner fetches `cpp-subprocess` through CMake's
`FetchContent`.

## Running the compiler

```
build/ciderc <file.cdr> -t linux-riscv32g -o out
```

Targets are named `<os>-<isa>`, see `Target::supported_targets` (or pass a
bogus `-t` to have them listed). Without `-t` the compiler targets the
platform it was itself built for (`Target::host_target()`, detected from the
host C++ compiler's predefined macros), erroring if that isn't supported.

`--emit=<kind>[,...]` selects which artifacts to produce: `ast`, `ir`,
`asm`, or `exe` (the default). The compiler runs only as far as the furthest
kind requested, so `--emit=ir` dumps IR without running codegen — useful when
the backend is broken. Each kind defaults to a path derived from the input
(`foo.cdr` → `foo.ast`/`foo.ir`/`foo.s`); append `=FILE` to redirect one, or
`=stdout` to write it to standard output. `-o` names the executable only.
`--emit=asm` writes GNU as syntax (Intel syntax on x86) laid out like the
executable's code, `_start` first. On x86 it assembles (`as`, then
`ld -e _start`) to byte-identical code, so it doubles as a check of the
encoder.

Other useful flags: `-v` (repeatable, increases verbosity), `-q` (quiet).
Run `build/ciderc -h` for the full list — flags are defined in `src/main.cpp`
via `CliParser`, keep this doc and `--help` in sync manually if you add any.

## Testing

There are two kinds of tests, both run by the C++ test runner in
`testing/src/`. Build first, then run the whole suite from the repository
root:

```
cmake --build build
build/testing/ciderc_test_runner
```

`--unit` or `--e2e` selects one kind (both run by default), `-l` lists the
selected tests without running them, `-v` also prints passes and skips, and
`-n` sets the number of parallel workers. e2e tests use `./ciderc` if it
exists, else `ciderc` from `PATH`, `-C` overrides this. Run
`build/testing/ciderc_test_runner -h` for the full list. The exit code is `1`
if any test failed.

Each run writes to `.ciderc_testing/<timestamp>-<pid>/` (`-A` changes the
parent directory), which is removed afterwards unless a test failed or
`--noclean` was passed. An e2e test's files are in `e2e/<test name>/`: its
source, compiled `program`, and captured output (`build_stdout.txt`,
`build_stderr.txt`, `stdout.txt`, `stderr.txt`).

### End to end tests

Each `.cdr` file under `testing/e2e_tests/` is a test, named after its
relative path without the extension. Subdirectories group tests by feature
area (e.g. `00_main`, `01_expressions`, `backend`). Files are embedded into
the runner at build time, so rebuild it after adding or changing one.

A test compiles the file for the host target, without passing `-t`, then runs
the program natively. There is no per-target loop or QEMU anymore, so other
targets aren't covered, but expected results must still be target independent
(see the language notes below).

Expected results are declared by `//! KEY=VALUE` lines at the very top of the
file, parsing stops at the first line that isn't one. Keys are case sensitive
and can't have whitespace before the `=`, an unrecognized key throws when the
runner starts. Omitted keys take the defaults below:

| Key | Default | Description |
| --- | --- | --- |
| `BUILD_EXIT_CODE` | `0` | Expected compiler exit code. If nonzero, the test passes without running the program |
| `EXIT_CODE` | `0` | Expected program exit code, or the negated signal number if killed by one |
| `STDOUT` | `""` | Expected program stdout, compared exactly |
| `STDERR` | `""` | Expected program stderr, compared exactly |
| `REQUIRES` | none | TODO: `32bit` or `64bit`. Parsed, but not enforced yet, so the test always runs |

`STDOUT` and `STDERR` values are taken verbatim and can't contain a newline.
No fixture uses them yet, and `TODO.md` lists verifying them as outstanding.
Until `REQUIRES` is implemented, keep marking anything using `i64`/`u64` with
`//! REQUIRES=64bit` anyway.
The same reference lives in the doc comment on `E2eTest`
(`testing/src/e2e_testing.hpp`), keep the two in sync.

### Unit tests

Unit tests are defined in place next to the code they test, in
`src/**/<name>.tests.cpp` files, which are compiled into the test runner
rather than the compiler (see `src/frontend/Lexer.tests.cpp`). Add new files
to the list in `testing/CMakeLists.txt`.

```cpp
#include "unit_testing.hpp"

TEST(some_test)
{
	ASSERT_TRUE(cond);
	ASSERT_EQ(actual, expected);
	ASSERT_THROWS(expr, ExceptionType);
}
```

`TEST` takes an identifier, and the full test name is that prefixed by the
file's path relative to `src/`, e.g. `frontend/Lexer/some_test`. Throw
`testing::unit::TestSkipped` to skip a test, any other uncaught exception
fails it. Tests run in parallel on the runner's worker threads and can use
anything in `ciderc_lib`.

### What to add

When adding a language feature, add `.cdr` test cases alongside it in the
matching (or a new) subdirectory of `testing/e2e_tests/` rather than only
relying on manual testing. Prefer unit tests for behavior of a single
component that doesn't need a full compile.

## Language notes worth knowing before editing the frontend

- Top-level functions are hoisted; global variable initialization order is
  resolved via dependency DAG, not declaration order.
- Binary operators are left-associative, except equality/comparison, which
  require explicit parens when chained.
- Type names start with a letter; a leading digit means it's a numeric
  literal (integer literals carry an explicit type suffix, e.g. `1u32`,
  `100i32` — see `testing/e2e_tests/00_main/*.cdr` for examples).
- `grammar.ebnf` is the source of truth for syntax — if a parser change
  alters accepted syntax, update the grammar file in the same change.
- Language semantics must not depend on the target. Operations on types of
  32 bits or less behave identically on 32 and 64 bit targets (including
  shift amounts: only the lower 5 bits are used, or the lower 6 when
  shifting a 64 bit value). `i64`/`u64` are the exception for now, backends
  reject them on 32 bit targets with an `unsupported` error.

## Development guidelines

### Code style

- Tabs for indentation (match surrounding files).

- New `CompilerError`s should use the existing typed factory functions
  (`syntax_error`, `type_error`, `semantic_error`, etc.) rather than a
  generic message, so exit codes stay meaningful — see
  `testing/e2e_tests/00_main/incorrect_ret_type_1.cdr` for how tests assert on
  `BUILD_EXIT_CODE`.
- This is a solo/early-stage project — prefer small, direct changes over
  speculative abstraction. Don't add functionality beyond what's needed for
  the language features described in `TODO.md`.
