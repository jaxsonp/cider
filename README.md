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

Next:

- Finish machine code generation of smaller int types.

Soon:

- Check code for stuff that doesn't need to be in headers
- Testing improvements:
  - Timeout
  - finish stdout/stderr checking
- turn off colored output (automatically if not tty perhaps?)
- Improve int literals
  - negative ints (broken rn perhaps?)

Fixes:
- Convert int literal AST to by 64 bits

Before self-hosting:

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

maybe eventually:

- 64 bit
- try doing UTF8
- labelled code blocks (for early breaks)
- soft floats
- soft multiplication

future optimizations:
- Better register allocator (use callee saved first on busy functions?)
- Optimize out LUI?
  
### notes for documentation

- all top level functions are hoisted (global var initialization is calculated with DAG)
- binary operators are left associative (except for equality/comparison, needs parens)
- type names must start with letters, numbers signifiy number literal

### tests to remember to write

- overflowing int literal
- overflowing integers with operations
- arithmetic bitshift right
- left/right associativity