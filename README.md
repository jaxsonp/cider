# Cider Compiler

Cider is a systems programming language focused on marrying the ergonomics of modern languages with the advantages of less-abstracted, low-level languages.

### Supported Targets

| Target | OS | Object format | ISA | ABI |
| --- | --- | --- | --- | --- |
| `linux-riscv32g` | Linux | ELF32 | RV32G | ILP32D |

## Tests

### Running tests

#### Requirements:

- Linux (probably)
- Python >= 3.14
- Qemu (specifically qemu-user-static, for binaries like `qemu-riscv32-static`, etc.)

### To run:

Run `testing/run_tests.py`, providing the path to the compiler binary to test.

```
usage: run_tests.py [-h] [-n N] compiler_path

Test runner

positional arguments:
  compiler_path    Path of compiler binary to test

options:
  -h, --help       show this help message and exit
  -n, --workers N  Number of concurrent workers to use
```

### Defining tests

Tests are defined in the `tests/` subdirectory. Each test consists of a `.cdr` source file with comments *at the very top* defining what the expected output should be. These comments should be of the form: `//! KEY=VALUE` (note the exclamation mark). Keys are case insensitive. Below are the recognized keys:

| Key | Type | Default | Description |
| --- | --- | --- | --- |
| `BUILD_EXIT_CODE` | int | `0` | Expected return value of compilation of program |
| `EXIT_CODE` | int | `0` | Expected return value of the program |
| `STDOUT` | string | `""` | Expected output to stdout |
| `STDERR` | string | `""` | Expected output to stderr |

## TODOs

_For Jaxson's eyes only_

- Soon:
  - investigate if spilling is broken
  - riscv type truncation for small types (before comparison, right shift, division, or explicit casts)
  - Check code for stuff that doesn't need to be in headers
  - Testing improvements:
    - Test timeouts
    - finish stdout/stderr checking
  - turn off colored output for compiler (automatically if not tty perhaps?)
  - Basic type inference for int literals
- Before self-hosting:
  - locals vars
  - if statements
  - loops
  - functions
    - function definitions/declaration
    - arguments
  - floats
  - global vars
    - global init dependency checking
  - structs
  - traits
  - stdlib
  - executable or library
- At some point:
  - Make distinction for non-G riscv ISAs
  - soft floats/multiplication for embedded ISAs
  - Better error messages (include code snippet)
  - 64 bit
  - try doing UTF8
  - labelled code blocks (for early breaks)
- Future optimizations:
  - Better register allocator (use callee saved first on busy functions?)
  - Optimize out LUI?
  
### notes for documentation

- **Test harness can only observe the low 8 bits of a result.** `run_tests.py`
  checks a POSIX process exit code, which the OS always truncates to 8 bits,
  and STDOUT/STDERR comparison isn't implemented yet. Every currently-
  implemented binary op (`+ - * / % & | ^`) is "mod-256-homomorphic" — the
  low byte of the result only depends on the low bytes of the operands, never
  on how the upper bits were padded — so no `.cdr` test can currently prove
  sign-extension vs. zero-extension is correct, or that 16/32-bit truncation
  actually happens, regardless of what values are chosen. This blocks
  meaningful tests for: right shift, comparisons, casts, and negative/
  overflowing int literals, once those land. Fix is the observability item
  above (stdout-based result reporting) — not a language feature, a test
  harness one.
- all top level functions are hoisted (global var initialization is calculated with DAG)
- binary operators are left associative (except for equality/comparison, needs parens)
- type names must start with letters, numbers signifiy number literal

### tests to remember to write

- overflowing int literal
- over/under flowing integers with operations
- bit extensions on things
- arithmetic bitshift right
- left/right associativity
- long chain tests even longer