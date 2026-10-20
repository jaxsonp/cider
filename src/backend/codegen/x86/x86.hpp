#pragma once

#include <stdint.h>
#include <string_view>

#include "backend/codegen/RegAllocator.hpp"

namespace codegen::x86
{
	enum class Register : PhysReg
	{
		// in hardware order: the value of each register is the number it's encoded as in machine code

		// legacy registers
		RAX,
		RCX,
		RDX,
		RBX,
		RSP,
		RBP,
		RSI,
		RDI,
		// x64 specific registers
		R8,
		R9,
		R10,
		R11,
		R12,
		R13,
		R14,
		R15,
	};

	/// @brief Size of an operand (and so of the part of a register accessed, ie RAX vs EAX vs AX vs AL). The value
	/// is the size in bytes, so comparing widths compares sizes
	///
	/// The high bytes (AH, CH, DH, BH) are deliberately not representable: nothing needs them, and they can't be
	/// encoded in any instruction that has a REX prefix. Leaving them out means an 8 bit register is always the
	/// low byte, and the low bytes of SP, BP, SI and DI (SPL, BPL, SIL, DIL) simply need a REX prefix
	enum class Width : uint8_t
	{
		/// @brief Byte
		W8 = 1,
		/// @brief Word
		W16 = 2,
		/// @brief Double word
		W32 = 4,
		/// @brief Quad word (x64 only)
		W64 = 8,
	};

	/// @brief Width of a value of some size in bytes
	Width width_of_size(unsigned int size);

	/// @brief Name of (part of) a register in assembly, ie `eax` for RAX accessed with W32
	std::string_view register_name(Register reg, Width width);

	/// @brief Condition codes, for instructions that test the flags (setcc, jcc, cmovcc). The value is the number it's
	/// encoded as. Below/above are unsigned comparisons, less/greater are signed ones
	enum class Cond : uint8_t
	{
		/// Overflow
		O,
		/// Not overflow
		NO,
		/// Below (unsigned <), aka carry
		B,
		/// Above or equal (unsigned >=), aka not carry
		AE,
		/// Equal, aka zero
		E,
		/// Not equal, aka not zero
		NE,
		/// Below or equal (unsigned <=)
		BE,
		/// Above (unsigned >)
		A,
		/// Sign
		S,
		/// Not sign
		NS,
		/// Parity even
		P,
		/// Parity odd
		NP,
		/// Less (signed <)
		L,
		/// Greater or equal (signed >=)
		GE,
		/// Less or equal (signed <=)
		LE,
		/// Greater (signed >)
		G,
	};

	/// @brief Suffix of a condition in mnemonics, ie `ge` in `setge`
	std::string_view cond_suffix(Cond cond);
}
