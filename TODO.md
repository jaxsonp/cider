# TODOs

_For Jaxson's eyes only_

## To implement

- Soon:
	- Bitwise not unary operator
	- investigate if spilling is broken
	- investigate function frame setup, unnecessary extra registers being saved?
	- riscv type truncation for small types (before comparison, right shift, division, or explicit casts)
	- Check code for stuff that doesn't need to be in headers
	- Testing improvements:
		- Overhaul how tests are ran (move away from qemu-user, maybe containers?)
		- Test timeouts
		- finish stdout/stderr checking
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
	- Basic type inference for int literals
	- Make distinction for non-G riscv ISAs
	- soft floats/multiplication for embedded ISAs
	- Better error messages (include code snippet)
	- 64 bit
	- try doing UTF8
	- labelled code blocks (for early breaks)
- Future optimizations:
	- Better register allocator (use callee saved first on busy functions?)
	- Optimize out LUI (how?)
  
## Notes for documentation

- all top level functions are hoisted (global var initialization is calculated with DAG)
- binary operators are left associative (except for equality/comparison, needs parens)
- type names must start with letters, numbers signifiy number literal

## Tests to remember to write

- overflowing int literal
- over/under flowing integers with operations
- bit extensions on things
- arithmetic bitshift right
- left/right associativity
- long chain tests even longer