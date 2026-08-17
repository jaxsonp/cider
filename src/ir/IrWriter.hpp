#pragma once

#include <string>

#include "ir/IR.hpp"

class IrFunctionWriter;

/// @brief Container for logic/state for building an ir::Object. User must claim and clean up resultant object
class IrWriter
{
	ir::Object obj;

public:
	IrWriter();

	/// @brief Creates a new function in this object, returning a writer scoped to it
	IrFunctionWriter start_function(const std::string &name);

	ir::Object get_obj() { return std::move(this->obj); }
};

/// @brief Container for logic/state for building a single ir::Function. Created with IrWriter::start_function
class IrFunctionWriter
{
	friend class IrWriter;

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
	std::unordered_map<ConstCacheKey, ir::VRegId, ConstCacheKeyHash> const_cache;

	IrFunctionWriter(ir::Function *fn);

public:
	ir::Function *cur_function;
	ir::BasicBlock *cur_bblock;

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

	/// @brief Creates and appends an instruction into the current basic block
	void add_instr(ir::Op opcode, ir::VRegId dst, ir::VRegId op1, ir::VRegId op2, uint64_t data = 0u);

	/// @brief Sets the current basic block's terminator to a return instruction and sets up a new basic block
	void add_return(ir::VRegId ret_value);

	/// @brief Sets the current basic block's terminator to a return instruction and sets up a new basic block
	void add_return();
};