#pragma once

#include <stdint.h>

#include "ir/IR.hpp"

namespace ir::instr
{

	struct LoadArgInstruction : public Instruction
	{
		unsigned short arg_index;
		VRegId dest;
		LoadArgInstruction() = delete;
		LoadArgInstruction(VRegId dest, unsigned short index);
	};

	struct LoadImmInstruction : public Instruction
	{
		VRegId dest;
		uint64_t value;
		LoadImmInstruction() = delete;
		LoadImmInstruction(VRegId dest, uint64_t val);
	};

	/// @brief Operands must not be immediate-immediate
	struct AddInstruction : public Instruction
	{
		VRegId dest;
		VRegId op1;
		Operand op2;
		AddInstruction() = delete;
		AddInstruction(VRegId dest, VRegId op1, Operand op2);
	};

	/// @brief Operands must not be immediate-immediate
	struct SubtractInstruction : public Instruction
	{
		VRegId dest;
		VRegId op1;
		Operand op2;
		SubtractInstruction() = delete;
		SubtractInstruction(VRegId dest, VRegId op1, Operand op2);
	};

	struct ReturnInstruction : public TerminalInstruction
	{
		std::optional<Operand> ret_value = std::nullopt;

		ReturnInstruction();
		ReturnInstruction(Operand ret_value);

		std::vector<BasicBlock *> successors() const override { return std::vector<BasicBlock *>(); }
	};

	/// @brief Unconditional jump
	struct JumpInstruction : public TerminalInstruction
	{
		BasicBlock *target;

		JumpInstruction(BasicBlock *t)
			: TerminalInstruction(Op::Jump), target(t) {}

		std::vector<BasicBlock *> successors() const override { return std::vector<BasicBlock *>{this->target}; }
	};
}