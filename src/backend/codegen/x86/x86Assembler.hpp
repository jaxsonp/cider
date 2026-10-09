#pragma once

#include <stdint.h>

#include "backend/codegen/x86/x86Instruction.hpp"

namespace codegen::x86
{
	/// @brief Buffer for emission functions to write to
	class Assembler
	{
		std::vector<uint8_t> buf;

		bool is_x64;

	public:
		Assembler(bool is_x64);

		/// @brief Reset buffer contents
		void clear();

		size_t write(Instruction inst);

		void write_mov(Operand dest, Operand src, bool signed_type);
	};
}