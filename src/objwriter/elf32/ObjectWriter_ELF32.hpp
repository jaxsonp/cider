#pragma once

#include "objwriter/ObjectWriter.hpp"

namespace objwriter::elf32
{
	class ObjectWriter_ELF32 : public ObjectWriter
	{
	public:
		ObjectWriter_ELF32(Object obj);
		void emit(std::ostream &out);
	};

}
