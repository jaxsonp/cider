# Cider

Cider is a systems programming language focused on marrying the ergonomics of modern languages with the advantages of less-abstracted, low-level languages. This repository currently houses the canonical cider compiler.

## Using the compiler

```
Usage: ciderc [options] <file>

Compiler for the Cider programming language

Positional arguments:
  <file>                 Input file (required)

Flags:
  -v, --verbose          Increase compiler verbosity (repeatable)
  -q, --quiet            Silence compiler output
  -h, --help             Show this help text

Options:
  -o, --out <FILE>       Output file path for the executable, if "--emit exe" [default: a.out]
  -t, --target <TARGET>  Target platform (required)
      --emit <KIND>      Artifacts to produce: ast, ir, asm, exe. Append "=PATH" to write to file, or "=stdout" for standard output (repeatable) [default: exe]
```

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
usage: run_tests.py [-h] [-n N] [--progress {auto,always,never}] [--color {auto,always,never}] [--show-passed {auto,always,never}] [--show-failed {auto,always,never}] compiler_path

Test runner

positional arguments:
  compiler_path         Path of compiler binary to test

options:
  -h, --help            show this help message and exit
  -n N, --workers N     Number of concurrent workers to use
  --progress {auto,always,never}
                        Show progress bar/info (default: auto)
  --color {auto,always,never}
                        Use color in output (default: auto)
  --show-passed {auto,always,never}
                        Print test cases that succeed (default: auto)
  --show-failed {auto,always,never}
                        Print test cases that fail (default: auto)
```

### Defining tests

Tests are defined in the `tests/` subdirectory. Each test consists of a `.cdr` source file with comments *at the very top* defining what the expected output should be. These comments should be of the form: `//! KEY=VALUE` (note the exclamation mark). Keys are case insensitive. Below are the recognized keys:

| Key | Type | Default | Description |
| --- | --- | --- | --- |
| `BUILD_EXIT_CODE` | int | `0` | Expected return value of compilation of program |
| `EXIT_CODE` | int | `0` | Expected return value of the program |
| `STDOUT` | string | `""` | Expected output to stdout |
| `STDERR` | string | `""` | Expected output to stderr |
