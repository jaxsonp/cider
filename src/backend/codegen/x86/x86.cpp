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

	std::string_view register_name(Register reg, Width width)
	{
		static constexpr std::string_view NAMES_64[] = {"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
														"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"};
		static constexpr std::string_view NAMES_32[] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
														"r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"};
		static constexpr std::string_view NAMES_16[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
														"r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"};
		static constexpr std::string_view NAMES_8[] = {"al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
													   "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"};

		size_t index = size_t(reg);
		if (index >= 16)
			throw CompilerError::internal("x86: register number out of range");
		switch (width)
		{
		case Width::W8:
			return NAMES_8[index];
		case Width::W16:
			return NAMES_16[index];
		case Width::W32:
			return NAMES_32[index];
		case Width::W64:
			return NAMES_64[index];
		}
		throw CompilerError::internal("Uncaught Width variant");
	}

	std::string_view cond_suffix(Cond cond)
	{
		static constexpr std::string_view SUFFIXES[] = {"o", "no", "b", "ae", "e", "ne", "be", "a",
														"s", "ns", "p", "np", "l", "ge", "le", "g"};
		return SUFFIXES[uint8_t(cond) & 0xF];
	}
}
