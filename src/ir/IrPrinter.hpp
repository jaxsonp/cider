#pragma once

#include <ostream>

#include "ir/IR.hpp"

namespace ir
{
	/// @brief Write a textual representation of an IR object to a stream.
	/// Output is deterministic: functions are ordered by name, vregs by id, and
	/// basic blocks in the order they are reached from the function entry.
	void print(const Object &obj, std::ostream &out);

	/// @brief Mnemonic for a binary opcode as it appears in textual IR
	std::string binary_op_mnemonic(BinaryOp op);

	/// @brief Mnemonic for a unary opcode as it appears in textual IR
	std::string unary_op_mnemonic(UnaryOp op);
}
