#include "x86.hpp"

#include <format>

#include "utils/error.hpp"

namespace codegen::x86
{
	Width width_of_size(unsigned int size)
	{
		switch (size)
		{
		case 1:
			return Width::W8;
		case 2:
			return Width::W16;
		case 4:
			return Width::W32;
		case 8:
			return Width::W64;
		}
		throw CompilerError::internal(std::format("x86: no operand width is {} bytes", size));
	}
}
