# SAS-C Compiler

### Supported Targets

| Target | OS | Object format | ISA | Default ABI |
| --- | --- | --- | --- | --- |
| `linux-riscv32g` | Linux | ELF32 | RV32G | ILP32D |

## Testing

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

Tests are defined in the `tests/` subdirectory. Each test consists of a `.sasc` source file with comments *at the very top* defining what the expected output should be. These comments should be of the form: `//! KEY=VALUE` (note the exclamation mark). Keys are case insensitive. Below are the recognized keys:

| Key | Type | Default | Description |
| --- | --- | --- | --- |
| `BUILD_EXIT_CODE` | int | `0` | Expected return value of compilation of program |
| `EXIT_CODE` | int | `0` | Expected return value of the program |
| `STDOUT` | string | `""` | Expected output to stdout |
| `STDERR` | string | `""` | Expected output to stderr |

### Test groups

- `00_main`: Testing returning values from main
- `01_expressions`: Testing expressions/operators


## TODOs

Next:

- Expression testing
- Check code for stuff that doesn't need to be in headers
- Refactor errors to be one class
- Testing improvements:
  - Timeout
  - finish stdout/stderr checking
- turn off colored output (automatically if not tty perhaps?)

Soon:

- Negative literals (broken right now I think)

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

maybe eventually:

- executable or library
- try doing UTF8
- labelled code blocks (for early breaks)

Maybe one day:

- soft floats

futuer optimizations:
- Better register allocator (use callee saved first on busy functions?)
- Optimize out LUI
  
## Notes for self

- RISC-V addi only supports a 12-bit signed immediate. The code generator will eventually have to "expand" a large LoadImm into a sequence like lui and addi.

## Notes for documentation

- only supporting 32-bit
- ok main return values: void, i32, u32
- all top level functions are hoisted (global var initialization is calculated with DAG)
- exit codes found in utils/common.hpp
- binary operators are left associative (except for equality/comparison, needs parens)

## tests to remember to write

- overflowing int literal
- overflowing integers with operations
- arithmetic bitshift right
- left/right associativity