#include "ir/IR.hpp"

#include <format>

#include "utils/error.hpp"
#include "IR.hpp"

namespace ir
{
	// autoassign id to new bbs
	static unsigned int bb_count = 0;
	BasicBlock::BasicBlock(std::string _note)
		: id(bb_count++), note(_note) {}

	Function::Function(const std::string &_name)
		: name(_name)
	{
		this->entry = new BasicBlock(this->name + " start");
	}
}
