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
		LEA,
		ADD,
		SUB,
		MUL,
		IMUL,
		DIV,
		IDIV,
		PUSH,
		POP,
		CALL,
		RET,
		JMP,
		JE,
		JNE,
		// ... and plenty more, TODO
	};

	struct RegisterOperand
	{
		Register reg;
		RegisterAccess access;
	};

	struct ImmediateOperand
	{
		int64_t value;
	};

	struct MemoryOperand
	{
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
	};
}