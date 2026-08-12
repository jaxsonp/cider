# TODOs

_For Jaxson's eyes only_

## To implement

- Soon:
	- fix bitshifting right, rv32g masks lower 5 bits for bitshift instructions, so currently x << 10000 is the same as x << 0, need to decide how to handle (only shift amounts >= 32 are actually wrong, [width, 32) already works out)
	- investigate if spilling is broken (ref count slots? abstract out register loading?)
	- investigate function frame setup, unnecessary extra registers being saved?
	- riscv type truncation on explicit casts (once it exists)
	- integer literals wrapping around their range (2000i8 == -48i8), a few tests falsely lean on it, see binary_op/{addition,subtraction,bitwise_*}/i8_*.cdr
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
- Tests to write:
	- overflowing int literal
	- left/right associativity, and comparisions needing parens
	- long chain tests even longer
- Future optimizations:
	- Codegen:
		- Only truncate register's if necessary, ie check if the upper bits can even have junk
		- Optimize load immediate then operations into immediate operations
		- Better register allocator (use callee saved first on busy functions?)
		- Optimize out LUI (how?)
- Perhaps?
	- No bitwise operators, turn them into functions
	- Wrapping bitshifts

## Notes for documentation

- all top level functions are hoisted (global var initialization is calculated with DAG)
- binary operators are left associative (except for equality/comparison, needs parens)
- type names must start with letters, numbers signifiy number literal
- bitshift right is arithmetic shift right for signed types, unsigned types are logical shift right
- the left side of a bitshift is any integer, the shift amount must be an *unsigned* integer.
  the two widths do not have to match (`1i32 << 4u8` is fine, `1i32 << 4i32` is a type error)
- a bitshift by n is defined as shifting one bit at a time, n times. so shifting by n >= the
  type's width shifts every bit out: `<<` and unsigned `>>` give 0, signed `>>` gives 0 for a
  positive value and -1 for a negative one. NOT yet implemented for n >= 32, rv32 masks the
  shift amount to its low 5 bits (`1u32 << 32u32` is currently 1)
- small types (i8/i16/u8/u16) live sign/zero extended in 32 bit registers. add/sub/mul/neg/shl
  and the bitwise ops can leave junk above the type's width, which is fine because their low
  bits are still correct - the backend truncates before anything that reads the upper bits
  (right shift, division, comparison, returning)

## Tests to remember to write
