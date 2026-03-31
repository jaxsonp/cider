#pragma once

#include "backend/objwriter/ObjectWriter.hpp"

namespace objwriter
{
	class ObjectWriter_ELF32 : public ObjectWriter
	{
	public:
		virtual void emit(const Object &obj, const Target &target, std::ostream &out);
	};

}
