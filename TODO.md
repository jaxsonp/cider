# TODOs

_For Jaxson's eyes only_

## To implement

- Soon:
	- Arbitrary-precision ints
	- Memleaks
	- define/enforce function name rules
	- integer literals wrapping around their range (2000i8 == -48i8), a few tests falsely lean on it, see binary_op/{addition,subtraction,bitwise_*}/i8_*.cdr (maybe not applicable anoymore?)
	- Check code for stuff that doesn't need to be in headers
	- Testing improvements:
		- Overhaul how tests are ran (move away from qemu-user, maybe containers? maybe only native?)
		- Test timeouts
		- finish stdout/stderr checking
	- Use callee saved registers for register allocation
	- Improve riscv codegen
		- Full support for riscv abi (double registers or whatever)
		- riscv type truncation on explicit casts (once it exists)
		- Investigate if large immediates are broken in codegen
	- Fix: hex literals with letter digits don't parse (`0x1Fu8` is a type error, the lexer's digit scan stops at `F`). Probably belongs with "Non-decimal int literals" below
	- Fix: deeply nested expressions (~2000 nested parens) segfault the compiler (parser recursion), should be a proper error
- Before self-hosting:
	- 64 bit types in 32bit archs (needs register pairs, carries, and a software 64 bit divide)
	- Platform detection
	- Non-decimal int literals
	- control flow analysis (for checking if a function returns, among others probably)
	- locals vars
	- if statements
	- loops
	- floats
	- global vars
		- global init dependency checking
	- higher-order functions, indirect calling (AST identifier exprs will no longer need the 'is_callee' member)
	- structs
	- traits
	- stdlib
	- executable or library
- At some point:
	- Basic type inference for int literals
	- Non-G riscv ISAs without M: multiplication/division throw `unsupported` for now, needs runtime routines (soft floats too once there are floats). Target flags exist for M/A/F/D/C, only C changes codegen today
	- Branch encoding (B-type) and jump relaxation for the C extension (jumps and calls are never compressed so offsets are final when emitted), once the IR has branches
	- Better error messages (include code snippet)
	- Improve x86 codegen
		- Red zone?
	- try doing UTF8
	- labelled code blocks (for early breaks)
	- Warnings:
		- Double negation
	- Investigate/reevaluate bitshift behavior
	- x86 subtargets (i686, i386, etc)
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
		- Frame setup: leaf functions save/restore ra for no reason, and leaf functions with no spills don't need a frame at all
		- Spilling: no liveness info, so dead values get spilled (at calls and bb ends) and every spilled vreg keeps its own slot forever. Frames grow with vreg count (8 bytes per vreg on rv64), so big functions get huge frames
		- Spill slots are fp-relative with negative offsets, which can't be compressed. sp-relative slots would shrink riscv *gc output
		- t6 is reserved as scratch for far frame offsets, could be handed back to the allocator on functions with small frames
- Perhaps?
	- No bitwise operators, only methods with explicit behavior such as wrapping, unchecked, etc
	- If I do macros, macros for cur fn and line
	- Trailing commas in function calls, etc
	- register-immediate instructions in IR

## Notes for documentation

- all top level functions are hoisted (global var initialization is calculated with DAG)
- binary operators are left associative (except for equality/comparison, needs parens)
- type names must start with letters, numbers signifiy number literal
- bitshift right is arithmetic shift right for signed types, unsigned types are logical shift right
- the left side of a bitshift is any integer, the shift amount must be an *unsigned* integer.
  the two widths do not have to match (`1i32 << 4u8` is fine, `1i32 << 4i32` is a type error)
- the shift amount is masked to its low 5 bits (low 6 bits when shifting a 64 bit value), then
  the result truncates to the type as usual. this is the same on every target (riscv64 uses the
  32 bit `*w` shifts for types up to 32 bits to keep it that way), kept for now rather than paid
  for at runtime. so `1u32 << 32u32` is 1, `1u8 << 100u8` is 16 (100 & 31 == 4), `1u64 << 64u8`
  is 1 and `1u64 << 32u8` really is 2^32. amounts in [width, 32) are not masked away for small
  types, so those really do shift every bit out (`1u8 << 8u8` is 0) (Dont like this, TODO change)
- operations on types of 32 bits or less behave identically on 32 and 64 bit targets (wrapping
  included). i64/u64 only exist on 64 bit targets for now
- small types (i8/i16/u8/u16) live sign/zero extended in 32 bit registers. add/sub/mul/neg/shl
  and the bitwise ops can leave junk above the type's width, which is fine because their low
  bits are still correct - the backend truncates before anything that reads the upper bits
  (right shift, division, comparison, returning)

## Tests to remember to write
