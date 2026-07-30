#pragma once

#include <memory>
#include <vector>
#include <array>
#include <unordered_map>
#include <optional>
#include <stdint.h>

#include "ir/IrType.hpp"

namespace ir
{
	using BBlockId = unsigned int;
	using VRegId = unsigned int;

	enum class Op
	{
		// arithmetic ---
		Add,
		Sub,
		Mul,
		Div,
		Rem,
		BitAnd,
		BitOr,
		BitXor,
		BitShl,
		BitShr,
		Neg,
		CmpEq,
		CmpNe,
		CmpGt,
		CmpGte,
		CmpLt,
		CmpLte,
		// memory ---
		Load,
		Store,
		// Specials ---
		Move,	 // copy reg to reg
		LoadImm, // load constant into reg
		LoadArg, // load argument into reg
	};

	struct VReg
	{
		IrType type;
	};

	class BasicBlock;

	/// @brief Instruction
	struct Instruction
	{
		Op opcode;

		VRegId dest;
		VRegId op1;
		VRegId op2;

		uint64_t data = 0;
	};

	/// @brief Base class for instructions that belongs at the end of a basic block
	struct BasicBlockTerminator
	{
		enum
		{
			RETURN,
			JUMP,
			BRANCH
		} kind;

		struct Successor
		{
			BasicBlock *bb;
			std::vector<ir::VRegId> args;
		};

		// return uses 0, jump 1, and branch 2
		std::array<Successor, 2> successors;

		// register to return if return type
		std::optional<VRegId> ret_reg;
	};

	class BasicBlock
	{
	public:
		const unsigned int id;
		std::string note = "";
		std::vector<Instruction> instructions;

		BasicBlockTerminator terminator;

		BasicBlock(unsigned int id, std::string note = "");
	};

	struct Function
	{
		std::string name = "";
		BasicBlock *entry = nullptr;
		std::unordered_map<VRegId, IrType> vregs;
		unsigned int bb_count = 0;
		unsigned long instr_count = 0;

		Function(const std::string &name);

		BasicBlock *new_bb();
	};

	struct Object
	{
		std::unordered_map<std::string, ir::Function *> functions;
	};
}
