#pragma once

#include <stdint.h>
#include <vector>
#include <variant>
#include <optional>

#include "backend/codegen/x86/x86.hpp"

namespace codegen::x86
{

	enum class Mnemonic : uint8_t
	{
		MOV,
		MOVSX, // Move with sign extension
		MOVZX, // Move with zero extension
			   // ... and plenty more, TODO
	};

	struct RegisterOperand
	{
		Register reg;
		RegisterAccess access;

		RegisterOperand(PhysReg reg, RegisterAccess access)
			: reg(static_cast<Register>(reg)), access(access) {}
	};

	struct ImmediateOperand
	{
		int64_t value;

		ImmediateOperand(int64_t value) : value(value) {}
	};

	struct MemoryOperand
	{
		/// Size (in bytes) of the value in memory
		unsigned int size;
		std::optional<Register> base_reg = std::nullopt;
		std::optional<Register> index_reg = std::nullopt;
		uint32_t index_scale = 0; // TODO investigate can this type be smaller?
		int32_t offset = 0;
	};

	using Operand = std::variant<RegisterOperand, ImmediateOperand, MemoryOperand>;

	/// @brief A single machine instruction before it's encoded
	struct Instruction
	{
		Mnemonic mnemonic;
		std::vector<Operand> operands;

		void encode(std::vector<uint8_t> &buf);
	};
}