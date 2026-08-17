#include "ir/IrWriter.hpp"

IrWriter::IrWriter() = default;

IrFunctionWriter IrWriter::start_function(const std::string &name)
{
	ir::Function *fn = new ir::Function(name);
	this->obj.functions.insert({name, fn});
	return IrFunctionWriter(fn);
}

IrFunctionWriter::IrFunctionWriter(ir::Function *fn)
	: cur_function(fn), cur_bblock(fn->entry)
{
	this->vreg_map_scopes.emplace_back();
}

/*ir::VRegId IrFunctionWriter::new_local(const std::string &name)
{
	ir::VRegId id = this->new_vreg();
	this->vreg_map_scopes.back().insert({name, id});
	return id;
}

ir::VRegId IrFunctionWriter::get_local(const std::string &name) const
{
	// visit scope stack from top to bottom
	for (std::vector<VRegMap>::const_reverse_iterator vreg_map = this->vreg_map_scopes.rbegin(); vreg_map != this->vreg_map_scopes.rend(); ++vreg_map)
	{
		VRegMap::const_iterator found = vreg_map->find(name);
		if (found != vreg_map->end())
		{
			return found->second;
		}
	}
	throw CompilerError::internal(std::format("Failed to find vreg allocation for name \"{}\"", name));
}*/

void IrFunctionWriter::push_scope()
{
	this->vreg_map_scopes.emplace_back();
}

void IrFunctionWriter::pop_scope()
{
	this->vreg_map_scopes.pop_back();
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
		this->add_instr(ir::Op::LoadImm, dest, ir::NO_VREG, ir::NO_VREG, value);

		return dest;
	}
}

void IrFunctionWriter::add_instr(ir::Op opcode, ir::VRegId dst, ir::VRegId op1, ir::VRegId op2, uint64_t data)
{
	ir::Instruction instr{opcode, dst, op1, op2, data};
	this->cur_bblock->instructions.push_back(instr);
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