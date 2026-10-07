#pragma once

#include <stdint.h>
#include <array>
#include <span>

#include "backend/Target.hpp"
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

	/// @brief How a register can be accessed, for example: RAX vs EAX vs AX vs AH vs AL.
	enum class RegisterAccess : uint8_t
	{
		/// @brief Low 8 bits
		LByte,
		/// @brief High 8 bits (of lower word)
		HByte,
		/// @brief 16-bit
		Word,
		/// @brief 32-bit
		DWord,
		/// @brief 64-bit
		QWord,
	};
}