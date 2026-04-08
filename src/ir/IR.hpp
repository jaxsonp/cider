#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <optional>
#include <stdint.h>

#include "types.hpp"

namespace ir
{
	using BBlockId = unsigned int;
	using VRegId = unsigned int;

	// TODO generalize for structs etc
	enum class Type
	{
		U32,
		I32,
	};

	/*enum class TypeVariant
	{
		U32,
		I32,
	};

	struct Type
	{
		TypeVariant variant;

		inline static Type u32() { return Type{TypeVariant::U32}; }
		inline static Type i32() { return Type{TypeVariant::I32}; }
	};

	enum class ValueVariant
	{
		VirtualReg,
		Immediate,
	};

	struct Value
	{
		ValueVariant variant;
		Type type;
		union
		{
			/// Used if variant is virtual register
			int vreg_id;
			/// Used if variant is immediate
			int32_t imm_val;
		} data;

		static Value new_vreg(VRegId id, Type t);
		static Value new_immediate(int32_t val);
	};*/

	enum class Op
	{
		// arithmetic ---
		Add,
		Sub,
		Mul,
		Div,
		Rem,
		And,
		Or,
		Xor,
		Shl,
		Shr,
		// memory ---
		Load,
		Store,
		// control flow ---
		Jump,
		BranchEq,
		BranchNe,
		BranchLt,
		BranchLe,
		BranchGt,
		BranchGe,
		Call,
		Return,
		// Specials ---
		Move,	 // copy reg to reg
		LoadImm, // load constant into reg
		LoadArg, // load argument into reg
	};

	class BasicBlock;

	/// @brief For operands that can be either a register or a constant value
	struct Operand
	{
		enum
		{
			VREG,
			IMMEDIATE
		} type;
		union
		{
			VRegId vreg_id;
			struct
			{
				uint64_t imm_value;
				Type imm_type;
			};
		};

		Operand(VRegId vreg_id)
			: type(VREG), vreg_id(vreg_id) {}

		Operand(uint64_t immediate_value, Type type)
			: type(VREG), imm_value(immediate_value), imm_type(type) {}

		inline bool is_vreg() { return this->type == VREG; }
		inline bool is_immediate() { return this->type == IMMEDIATE; }
	};

	/// @brief Instruction base class
	struct Instruction
	{
		Op opcode;

		Instruction *next = nullptr;
		Instruction *prev = nullptr;

		/// @brief Whether this instruct is a terminal instruction. Defaults to false
		virtual bool is_terminal() const { return false; }

		Instruction(Op op) : opcode(op) {}
	};

	/// @brief Base class for instructions that belongs at the end of a basic block
	struct TerminalInstruction : public Instruction
	{
		explicit TerminalInstruction(Op op) : Instruction(op) {}

		virtual bool is_terminal() const { return true; }

		// TerminalInstruction(Op op) : Instruction(op) {}
		virtual std::vector<BasicBlock *> successors() const = 0;
	};

	namespace instr
	{
	}

	/// TODO conditional jump

	class BasicBlock
	{
	public:
		const unsigned int id;
		std::string note = "";
		Instruction *start = nullptr;
		Instruction *end = nullptr;

		BasicBlock(std::string note = "");
	};

	/*enum class OperandType {
		Register,  // virtual register (v0, v1, ...)
		Immediate, // 32-bit integer constant
		Label	   // For branch/jump targets
	};

	struct Operand
	{
		OperandType type;
		union
		{
			int reg_id;		   // Index for virtual registers
			int32_t val;	   // Constant value
			const char *label; // if
		};
	};*/

	struct Function
	{
		std::string name = "";
		BasicBlock *entry = nullptr;
		unsigned short vreg_count = 0;
		unsigned long long instr_count = 0;

		Function(const std::string &name);
	};
}

struct IrObject
{
	std::unordered_map<std::string, ir::Function *> functions;
};
