#pragma once

#include <stdint.h>
#include <vector>
#include <unordered_map>

#include "ir/IR.hpp"

/// @brief Wrapper around logic for building an IrObject. User must claim and clean up resultant object
class IrWriter
{
	using VRegMap = std::unordered_map<std::string, ir::VRegId>;

	/// @brief A stack of vreg maps, mapping names to assigned virtual registers
	std::vector<VRegMap> vreg_map_scopes;

	IrObject obj;

public:
	ir::Function *cur_function = nullptr;
	ir::BasicBlock *cur_bblock = nullptr;
	ir::Instruction *cur_instr = nullptr;

	IrWriter();

	/// @brief Creates a new function, and sets this writer's context there
	void new_function(const std::string &name);

	/// @brief Creates a new local in the current scope, returning its vreg
	// ir::VRegId new_local(const std::string &name);

	/// @brief Find the vreg allocation of a name in the current or surrounding scopes (throws if cannot find)
	// ir::VRegId get_local(const std::string &name) const;

	void push_scope();
	void pop_scope();

	/// @brief Reserve a new virtual reg
	ir::VRegId new_vreg(ir::IrType);

	/// @brief Write an instruction at the current position
	void emit(ir::Instruction *new_instr);

	IrObject get_obj() { return std::move(this->obj); }
};