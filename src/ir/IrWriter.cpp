#include "ir/IrWriter.hpp"

#include <format>

#include "utils/error.hpp"

IrWriter::IrWriter() = default;

IrFunctionWriter IrWriter::start_function(const std::string &name, std::vector<ir::IrType> param_types, std::optional<ir::IrType> return_type)
{
	ir::Function *fn = new ir::Function(name, std::move(param_types), std::move(return_type));
	this->obj.functions.insert({name, fn});
	return IrFunctionWriter(fn);
}

IrFunctionWriter::IrFunctionWriter(ir::Function *fn)
	: cur_function(fn), cur_bblock(fn->entry)
{
}

void IrFunctionWriter::def_var(Variable var, ir::VRegId value)
{
	this->var_values.insert_or_assign(var, value);
}

ir::VRegId IrFunctionWriter::use_var(Variable var) const
{
	auto found = this->var_values.find(var);
	if (found == this->var_values.end())
		throw CompilerError::internal("Variable used before any definition of it was emitted");
	return found->second;
}

ir::VRegId IrFunctionWriter::new_vreg(ir::IrType ty)
{
	ir::VRegId id(this->cur_function->vregs.size());
	this->cur_function->vregs.insert({id, ty});
	return id;
}

ir::VRegId IrFunctionWriter::get_const_vreg(ir::IrType type, uint64_t value)
{
	IrFunctionWriter::ConstCacheKey key{
		.type_variant = type.variant,
		.value = value,
	};
	if (auto search = this->const_cache.find(key); search != this->const_cache.end())
		// const already exists
		return search->second;
	else
	{
		// create new vreg for this const and cache it
		ir::VRegId dest = this->new_vreg(type);
		this->const_cache.insert({key, dest});

		// load the value
		this->add_immediate(dest, value);

		return dest;
	}
}

void IrFunctionWriter::add_binary(ir::BinaryOp op, ir::VRegId dest, ir::VRegId lhs, ir::VRegId rhs)
{
	this->cur_bblock->instructions.push_back(ir::BinaryInstruction{op, dest, lhs, rhs});
}

void IrFunctionWriter::add_unary(ir::UnaryOp op, ir::VRegId dest, ir::VRegId src)
{
	this->cur_bblock->instructions.push_back(ir::UnaryInstruction{op, dest, src});
}

void IrFunctionWriter::add_immediate(ir::VRegId dest, uint64_t value)
{
	this->cur_bblock->instructions.push_back(ir::ImmediateInstruction{dest, value});
}

void IrFunctionWriter::add_load_arg(ir::VRegId dest, uint64_t index)
{
	this->cur_bblock->instructions.push_back(ir::LoadArgInstruction{dest, index});
}

ir::VRegId IrFunctionWriter::add_call(const std::string &callee, std::optional<ir::IrType> return_type, std::vector<ir::VRegId> args)
{
	std::optional<ir::VRegId> dest;
	if (return_type.has_value())
		dest = this->new_vreg(*return_type);

	this->cur_bblock->instructions.push_back(ir::CallInstruction{dest, callee, std::move(args)});

	return dest.value_or(ir::NO_VREG);
}

void IrFunctionWriter::add_return(ir::VRegId ret_value)
{
	this->cur_bblock->terminator = ir::BasicBlockTerminator{
		.kind = ir::BasicBlockTerminator::RETURN,
		.ret_reg = ret_value};

	this->cur_bblock = this->cur_function->new_bb();
}

void IrFunctionWriter::add_return()
{
	this->cur_bblock->terminator = ir::BasicBlockTerminator{
		.kind = ir::BasicBlockTerminator::RETURN};

	this->cur_bblock = this->cur_function->new_bb();
}