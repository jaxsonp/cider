#pragma once

#include <memory>
#include <vector>
#include <array>
#include <unordered_map>
#include <optional>
#include <string>
#include <variant>
#include <stdint.h>

#include "ir/IrType.hpp"

namespace ir
{
	using BBlockId = unsigned int;
	using VRegId = unsigned int;

	/// @brief Placeholder for an instruction operand that isn't used by that opcode
	constexpr VRegId NO_VREG = static_cast<VRegId>(-1);

	enum class BinaryOp
	{
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
		CmpEq,
		CmpNe,
		CmpGt,
		CmpGte,
		CmpLt,
		CmpLte,
	};

	enum class UnaryOp
	{
		Neg,
		BitNot,
	};

	struct VReg
	{
		IrType type;
	};

	class BasicBlock;

	/// @brief Two-operand instruction (arithmetic, bitwise, comparison)
	struct BinaryInstruction
	{
		BinaryOp op;
		VRegId dest;
		VRegId lhs;
		VRegId rhs;
	};

	/// @brief One-operand instruction
	struct UnaryInstruction
	{
		UnaryOp op;
		VRegId dest;
		VRegId src;
	};

	/// @brief Loads a constant immediate into a register
	struct ImmediateInstruction
	{
		VRegId dest;
		uint64_t value;
	};

	/// @brief Loads the Nth argument passed to the current function into a register
	struct LoadArgInstruction
	{
		VRegId dest;
		uint64_t index;
	};

	/// @brief Direct call to a named function
	struct CallInstruction
	{
		std::optional<VRegId> dest;
		std::string callee;
		std::vector<VRegId> args;
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
		std::vector<std::variant<BinaryInstruction, UnaryInstruction, ImmediateInstruction, LoadArgInstruction, CallInstruction>> instructions;

		BasicBlockTerminator terminator;

		BasicBlock(unsigned int id, std::string note = "");
	};

	struct Function
	{
		std::string name = "";
		std::vector<IrType> param_types;
		/// nullopt if the function returns void
		std::optional<IrType> return_type;
		BasicBlock *entry = nullptr;
		std::unordered_map<VRegId, IrType> vregs;
		unsigned int bb_count = 0;
		unsigned long instr_count = 0;

		Function(const std::string &name, std::vector<IrType> param_types, std::optional<IrType> return_type);

		BasicBlock *new_bb();
	};

	struct Object
	{
		std::unordered_map<std::string, ir::Function *> functions;
	};
}
