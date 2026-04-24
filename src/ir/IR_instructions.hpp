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

	struct AddInstruction : public Instruction
	{
		VRegId dest;
		VRegId op1;
		IrValue op2;
		AddInstruction() = delete;
		AddInstruction(VRegId dest, VRegId op1, IrValue op2);
	};

	struct SubtractInstruction : public Instruction
	{
		VRegId dest;
		VRegId op1;
		IrValue op2;
		SubtractInstruction() = delete;
		SubtractInstruction(VRegId dest, VRegId op1, IrValue op2);
	};

	struct MultiplyInstruction : public Instruction
	{
		VRegId dest;
		VRegId op1;
		IrValue op2;
		MultiplyInstruction() = delete;
		MultiplyInstruction(VRegId dest, VRegId op1, IrValue op2);
	};

	struct DivideInstruction : public Instruction
	{
		VRegId dest;
		VRegId op1;
		IrValue op2;
		DivideInstruction() = delete;
		DivideInstruction(VRegId dest, VRegId op1, IrValue op2);
	};

	struct ModuloInstruction : public Instruction
	{
		VRegId dest;
		VRegId op1;
		IrValue op2;
		ModuloInstruction() = delete;
		ModuloInstruction(VRegId dest, VRegId op1, IrValue op2);
	};

	struct BitwiseOrInstruction : public Instruction
	{
		VRegId dest;
		VRegId op1;
		IrValue op2;
		BitwiseOrInstruction() = delete;
		BitwiseOrInstruction(VRegId dest, VRegId op1, IrValue op2);
	};

	struct BitwiseXorInstruction : public Instruction
	{
		VRegId dest;
		VRegId op1;
		IrValue op2;
		BitwiseXorInstruction() = delete;
		BitwiseXorInstruction(VRegId dest, VRegId op1, IrValue op2);
	};
	struct BitwiseAndInstruction : public Instruction
	{
		VRegId dest;
		VRegId op1;
		IrValue op2;
		BitwiseAndInstruction() = delete;
		BitwiseAndInstruction(VRegId dest, VRegId op1, IrValue op2);
	};

	struct ReturnInstruction : public TerminalInstruction
	{
		std::optional<IrValue> ret_value = std::nullopt;

		ReturnInstruction();
		ReturnInstruction(IrValue ret_value);

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