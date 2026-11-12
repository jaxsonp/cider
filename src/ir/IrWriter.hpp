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
	IrFunctionWriter start_function(const std::string &name, std::vector<ir::IrType> param_types, std::optional<ir::IrType> return_type);

	ir::Object get_obj() { return std::move(this->obj); }
};

/// @brief Container for logic/state for building a single ir::Function. Created with IrWriter::start_function
class IrFunctionWriter
{
	friend class IrWriter;

public:
	/// @brief Opaque identity of a source level variable, the frontend uses its `ast::Symbol *`. Keyed by identity
	/// rather than by name, so shadowing and scoping are left to the frontend's name resolution
	using Variable = const void *;

private:
	/// @brief The vreg holding each variable's current value
	std::unordered_map<Variable, ir::VRegId> var_values;

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

	/// @brief Defines (declares or assigns) a variable, its value is `value` from here on. Vregs are never written
	/// twice, an assignment just makes the variable refer to the vreg holding the new value
	/// TODO once there's control flow, this has to track definitions per basic block and add block arguments where
	/// they merge (Braun et al., "Simple and Efficient Construction of SSA Form")
	void def_var(Variable var, ir::VRegId value);

	/// @brief The vreg holding a variable's current value (throws if it was never defined)
	ir::VRegId use_var(Variable var) const;

	/// @brief Reserve a new virtual reg
	ir::VRegId new_vreg(ir::IrType);

	/// @brief Get a vreg with a constant value (loading it if doesn't exist)
	ir::VRegId get_const_vreg(ir::IrType type, uint64_t value);

	/// @brief Creates and appends a binary instruction into the current basic block
	void add_binary(ir::BinaryOp op, ir::VRegId dest, ir::VRegId lhs, ir::VRegId rhs);

	/// @brief Creates and appends a unary instruction into the current basic block
	void add_unary(ir::UnaryOp op, ir::VRegId dest, ir::VRegId src);

	/// @brief Creates and appends an instruction loading a constant immediate into dest
	void add_immediate(ir::VRegId dest, uint64_t value);

	/// @brief Creates and appends an instruction loading the Nth argument into dest
	void add_load_arg(ir::VRegId dest, uint64_t index);

	/// @brief Creates and appends a call instruction into the current basic block. Returns the dest vreg
	/// holding the return value, or ir::NO_VREG if return_type is nullopt (the callee returns void)
	ir::VRegId add_call(const std::string &callee, std::optional<ir::IrType> return_type, std::vector<ir::VRegId> args);

	/// @brief Sets the current basic block's terminator to a return instruction and sets up a new basic block
	void add_return(ir::VRegId ret_value);

	/// @brief Sets the current basic block's terminator to a return instruction and sets up a new basic block
	void add_return();
};