#include "ObjectWriter_ELF32.hpp"

namespace objwriter::elf32
{
	struct Elf32_;

	ObjectWriter_ELF32::ObjectWriter_ELF32(Object obj)
		: ObjectWriter(std::move(obj))
	{
	}

	void ObjectWriter_ELF32::emit(std::ostream &out)
	{
	}

}