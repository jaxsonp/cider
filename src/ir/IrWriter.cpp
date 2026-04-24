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

void IrWriter::emit(ir::Instruction *new_instr)
{
	if (this->cur_bblock == nullptr)
		throw CompilerError::internal("Attempted to emit IR instruction before a basic block was \"chosen\"");

	this->cur_function->instr_count += 1;
	if (this->cur_instr == nullptr)
	{
		// bblock is empty
		this->cur_bblock->start = new_instr;
		this->cur_bblock->end = new_instr;
		this->cur_instr = new_instr;
	}
	else
	{
		// insert in bblock
		this->cur_instr->next = new_instr;
		new_instr->prev = this->cur_instr;

		if (this->cur_bblock->end == this->cur_instr)
			this->cur_bblock->end = new_instr;
		this->cur_instr = new_instr;
	}
}