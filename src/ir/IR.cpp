#include "ir/IR.hpp"

#include <format>

#include "utils/error.hpp"
#include "IR.hpp"

namespace ir
{
	BasicBlock::BasicBlock(unsigned int id, std::string note)
		: id(id), note(note) {}

	Function::Function(const std::string &_name, std::vector<IrType> _param_types, std::optional<IrType> _return_type)
		: name(_name), param_types(std::move(_param_types)), return_type(std::move(_return_type))
	{
		this->entry = new BasicBlock((this->bb_count)++, this->name + " start");
	}

	BasicBlock *Function::new_bb()
	{
		return new BasicBlock((this->bb_count)++);
	}
}
