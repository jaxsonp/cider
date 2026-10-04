#pragma once

#include "backend/objwriter/ObjectWriter.hpp"

namespace objwriter
{
	/// Tag selecting the 32 bit ELF structures
	struct Elf32;
	/// Tag selecting the 64 bit ELF structures
	struct Elf64;

	/// @brief Writes ELF executables. The 32 and 64 bit formats only differ in the layout and field widths
	/// of their headers, so one implementation is instantiated for both
	template <typename ElfClass>
	class ObjectWriter_ELF : public ObjectWriter
	{
	public:
		virtual void emit(const Object &obj, const Target &target, std::ostream &out);
	};

	using ObjectWriter_ELF32 = ObjectWriter_ELF<Elf32>;
	using ObjectWriter_ELF64 = ObjectWriter_ELF<Elf64>;
}
