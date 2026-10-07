#include "x86.hpp"

#include "utils/error.hpp"

namespace codegen::x86
{

	constexpr std::array X86_REGISTERS{
		Register::RAX,
		Register::RBX,
		Register::RCX,
		Register::RDX,
		Register::RSI,
		Register::RDI,
		Register::RSP,
		Register::RBP,
	};

	constexpr std::array X64_REGISTERS{
		Register::RAX,
		Register::RBX,
		Register::RCX,
		Register::RDX,
		Register::RSI,
		Register::RDI,
		Register::RSP,
		Register::RBP,
		Register::R8,
		Register::R9,
		Register::R10,
		Register::R11,
		Register::R12,
		Register::R13,
		Register::R14,
		Register::R15,
	};

	std::span<const Register> get_x86_registers(Target target)
	{
		switch (target.arch)
		{
		case Target::Arch::X86:
			return X86_REGISTERS;
		case Target::Arch::X86_64:
			return X64_REGISTERS;
		default:
			throw CompilerError::internal("Cannot get x86 registers from non-x86 arch");
		}
	}
}
