#include "ir/IrWriter.hpp"

#include <format>

#include "utils/error.hpp"
#include "IrWriter.hpp"

IrWriter::IrWriter() = default;

void IrWriter::new_function(const std::string &name)
{
	this->cur_function = new ir::Function(name);
	this->obj.functions.insert({name, this->cur_function});

	this->cur_bblock = this->cur_function->entry;

	// reset vreg maps
	this->vreg_map_scopes.clear();
	this->vreg_map_scopes.emplace_back();
}

/*ir::VRegId IrWriter::new_local(const std::string &name)
{
	ir::VRegId id = this->new_vreg();
	this->vreg_map_scopes.back().insert({name, id});
	return id;
}

ir::VRegId IrWriter::get_local(const std::string &name) const
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

void IrWriter::push_scope()
{
	this->vreg_map_scopes.emplace_back();
}

void IrWriter::pop_scope()
{
	this->vreg_map_scopes.pop_back();
}

ir::VRegId IrWriter::new_vreg(ir::IrType ty)
{
	if (this->cur_function == nullptr)
		throw CompilerError::internal("Attempted to reserve vreg before a function was created");
	ir::VRegId id(this->cur_function->vregs.size());
	this->cur_function->vregs.insert({id, ty});
	return id;
}

void IrWriter::add_instr(ir::Op opcode, ir::VRegId dst, ir::VRegId op1, ir::VRegId op2, uint64_t data)
{
	if (this->cur_bblock == nullptr)
		throw CompilerError::internal("Attempted to add instruction before a function was created");

	ir::Instruction instr{opcode, dst, op1, op2, data};
	this->cur_bblock->instructions.push_back(instr);
}

void IrWriter::add_return(ir::VRegId ret_value)
{
	if (this->cur_bblock == nullptr)
		throw CompilerError::internal("Attempted to add return before a function was created");

	this->cur_bblock->terminator = ir::BasicBlockTerminator{
		.kind = ir::BasicBlockTerminator::RETURN,
		.ret_reg = ret_value};

	this->cur_bblock = this->cur_function->new_bb();
}

void IrWriter::add_return()
{
	if (this->cur_bblock == nullptr)
		throw CompilerError::internal("Attempted to add return before a function was created");

	this->cur_bblock->terminator = ir::BasicBlockTerminator{
		.kind = ir::BasicBlockTerminator::RETURN};

	this->cur_bblock = this->cur_function->new_bb();
}