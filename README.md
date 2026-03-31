# SAS-C Compiler

### Supported Targets

| Target | OS | Object format | ISA | Default ABI |
| --- | --- | --- | --- | --- |
| `linux-riscv32g` | Linux | ELF32 | RV32G | ILP32D |

## TODOs

Next:

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

## tests to remember to write

- overflowing int literal