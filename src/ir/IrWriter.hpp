#pragma once

#include <stdint.h>
#include <vector>
#include <unordered_map>

#include "ir/IR.hpp"

/// @brief Wrapper around logic for building an ir::Object. User must claim and clean up resultant object
class IrWriter
{
	using VRegMap = std::unordered_map<std::string, ir::VRegId>;

	/// @brief A stack of vreg maps, mapping names to assigned virtual registers
	std::vector<VRegMap> vreg_map_scopes;

	struct ConstCacheKey
	{
		ir::IrType::Variant type_variant;
		uint64_t value;
		bool operator==(const ConstCacheKey &) const = default;
	};
	struct ConstCacheKeyHash
	{
		size_t operator()(const ConstCacheKey &k) const
		{
			return std::hash<uint64_t>()(k.value) ^ (std::hash<int>()((int)k.type_variant) << 1);
		}
	};
	/// @brief Map of VRegs loaded with constant immediates, keyed by their type and value
	/// Note to self, clear for every function
	std::unordered_map<ConstCacheKey, ir::VRegId, ConstCacheKeyHash> const_cache;

	ir::Object obj;

public:
	ir::Function *cur_function = nullptr;
	ir::BasicBlock *cur_bblock = nullptr;

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

	/// @brief Get a vreg with a constant value (loading it if doesn't exist)
	ir::VRegId get_const_vreg(ir::IrType type, uint64_t value);

	ir::Object get_obj() { return std::move(this->obj); }

	/// @brief Creates and appends an instruction into the current basic block
	void add_instr(ir::Op opcode, ir::VRegId dst, ir::VRegId op1, ir::VRegId op2, uint64_t data = 0u);

	/// @brief Sets the current basic block's terminator to a return instruction and sets up a new basic block
	void add_return(ir::VRegId ret_value);

	/// @brief Sets the current basic block's terminator to a return instruction and sets up a new basic block
	void add_return();
};