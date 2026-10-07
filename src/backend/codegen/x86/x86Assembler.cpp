#include "x86Assembler.hpp"

namespace codegen::x86
{
	void Assembler::clear()
	{
		this->buf.clear();
	}

	size_t Assembler::write(Instruction inst)
	{
		size_t pos = this->buf.size();
		inst.encode(this->buf);
		return pos;
	}
}