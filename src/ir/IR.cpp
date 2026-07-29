#include "ir/IR.hpp"

#include <format>

#include "utils/error.hpp"
#include "IR.hpp"

namespace ir
{
	BasicBlock::BasicBlock(unsigned int id, std::string note)
		: id(id), note(note) {}

	Function::Function(const std::string &_name)
		: name(_name)
	{
		this->entry = new BasicBlock((this->bb_count)++, this->name + " start");
	}

	BasicBlock *Function::new_bb()
	{
		return new BasicBlock((this->bb_count)++);
	}
}
