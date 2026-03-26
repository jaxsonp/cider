#pragma once

#include <vector>
#include <unordered_map>
#include <optional>
#include <stdint.h>

#include "types.hpp"

namespace ir
{
	using BBlockId = unsigned int;
	using VRegId = unsigned short;

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
		struct LoadArgInstruction : public Instruction
		{
			unsigned short arg_index;
			VRegId dest;
			LoadArgInstruction(VRegId dest, unsigned short index);
		};

		struct LoadImmInstruction : public Instruction
		{
			VRegId dest;
			uint32_t value;
			LoadImmInstruction(VRegId dest, uint32_t val);
		};

		struct ReturnInstruction : public TerminalInstruction
		{
			std::optional<VRegId> ret_value = std::nullopt;

			ReturnInstruction();
			ReturnInstruction(VRegId ret_value);

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

/// @brief Wrapper around logic for building an IrObject. User must claim and clean up resultant object
class IrWriter
{
	using VRegMap = std::unordered_map<std::string, ir::VRegId>;

	/// @brief A stack of vreg maps, mapping names to assigned virtual registers
	std::vector<VRegMap> vreg_map_scopes;

	IrObject *obj;

public:
	ir::Function *cur_function = nullptr;
	ir::BasicBlock *cur_bblock = nullptr;
	ir::Instruction *cur_instr = nullptr;

	IrWriter();

	/// @brief Creates a new function, and sets this writer's context there
	void new_function(const std::string &name);

	/// @brief Creates a new local in the current scope, returning its vreg
	ir::VRegId new_local(const std::string &name);

	/// @brief Find the vreg allocation of a name in the current or surrounding scopes (throws if cannot find)
	ir::VRegId get_local(const std::string &name) const;

	void push_scope();
	void pop_scope();

	/// @brief Reserve a new virtual reg
	ir::VRegId new_vreg();

	/// @brief Write an instruction at the current position
	void emit(ir::Instruction *new_instr);

	/// User must delete object
	IrObject *get_obj() { return this->obj; }
};
