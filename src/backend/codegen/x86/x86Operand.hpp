#pragma once

#include <stdint.h>

#include "backend/codegen/x86/x86.hpp"
#include "backend/codegen/RegAllocator.hpp"

namespace codegen::x86
{
	namespace operands
	{
		class r8
		{
			Register reg;

		public:
			r8(Register reg) : reg(reg) {}
		};

		class rm8
		{
			Register reg;
			bool is_mem_access;
			int32_t offset;

		public:
			rm8(Register reg, bool is_mem_access, int32_t offset = 0) : reg(reg), is_mem_access(is_mem_access), offset(offset) {}
		};
	}
}