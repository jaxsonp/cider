#pragma once

#include "backend/objwriter/ObjectWriter.hpp"

namespace objwriter
{
	class ObjectWriter_ELF32 : public ObjectWriter
	{
	public:
		virtual void emit(Object &obj, std::ostream &out);
	};

}
