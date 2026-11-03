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
  -V, --version          Print the program's version and exit

Options:
  -o, --out <FILE>       Output file path for the executable, if "--emit exe" [default: a.out]
  -t, --target <TARGET>  Target platform (defaults to the host's target, linux-x86_64)
      --emit <KIND>      Artifacts to produce: ast, ir, asm, exe. Append "=PATH" to write to file, or "=stdout" for standard output (repeatable) [default: exe]
```

### Supported Targets

| Target | OS | Object format | ISA | ABI |
| --- | --- | --- | --- | --- |
| `linux-riscv32g` | Linux | ELF32 | RV32G | ILP32D |
| `linux-riscv32gc` | Linux | ELF32 | RV32GC | ILP32D |
| `linux-riscv64g` | Linux | ELF64 | RV64G | LP64D |
| `linux-riscv64gc` | Linux | ELF64 | RV64GC | LP64D |
| `linux-x86` | Linux | ELF32 | IA-32 | SysV i386 |
| `linux-x86_64` | Linux | ELF64 | x86_64 | SysV AMD64 |

64 bit integers (`i64`, `u64`) are only supported on the 64 bit targets for now.

## Tests

### Running tests

#### Requirements:

- A C++20 compiler and CMake
- Internet access the first time CMake configures the project (the test runner fetches [cpp-subprocess](https://github.com/arun11299/cpp-subprocess))
- A host platform that is a [supported target](#supported-targets). End to end tests are compiled for the host and run natively by default, testing another target needs a way to run its programs, ie QEMU user mode emulation (`-t linux-riscv64gc --e2e-prefix qemu-riscv64-static`)

### To run:

The test runner is a C++ program, built alongside the compiler. Run it from the repository root, where the build places a copy of `ciderc`:

```
cmake --build build
build/testing/ciderc_test_runner
```

```
Usage: ciderc_test_runner [options]

Runs Cider's unit and e2e tests

Flags:
      --unit              Run unit tests (if no category is given, all are run)
      --e2e               Run end to end tests
  -l, --list              List the selected tests instead of running them
  -v, --verbose           Print every test, not just failures
      --noclean           Don't remove testing artifacts, even if successful
  -h, --help              Show this help text

Options:
  -n, --workers <N>       Number of parallel test runners [default: 4]
  -C, --ciderc <path>     Ciderc executable used for e2e tests. Defaults to './ciderc' if it exists, else 'ciderc'
  -t, --target <name>     Target to compile e2e tests for. Defaults to the host
      --e2e-prefix <cmd>  Command to run e2e test programs through, ie an emulator for other targets
  -A, --artifacts <path>  Output directory for testing artifacts [default: ./.ciderc_testing/]
```

The test files are embedded into the runner when it's built, so rebuild after changing them. The runner exits with `1` if any test failed, and leaves that run's artifacts in the artifacts directory. How tests are defined is documented in [AGENTS.md](AGENTS.md#testing).

## Reference material

Relevant specs, manuals and documents that I'm bookmarking here:

- ELF spec: [https://refspecs.linuxfoundation.org/elf/elf.pdf](https://refspecs.linuxfoundation.org/elf/elf.pdf)
- Risc-V calling conventions/ABIs: [https://riscv.org/wp-content/uploads/2024/12/riscv-calling.pdf](https://riscv.org/wp-content/uploads/2024/12/riscv-calling.pdf)
- Risc-V instruction set manual (unpriv): [https://docs.riscv.org/reference/isa/v20260120/unpriv/unpriv-index.html](https://docs.riscv.org/reference/isa/v20260120/unpriv/unpriv-index.html)
- System V AMD64 ABI spec: [https://refspecs.linuxbase.org/elf/x86_64-abi-0.99.pdf](https://refspecs.linuxbase.org/elf/x86_64-abi-0.99.pdf)
- System V i386 ABI spec: [https://refspecs.linuxbase.org/elf/abi386-4.pdf](https://refspecs.linuxbase.org/elf/abi386-4.pdf)