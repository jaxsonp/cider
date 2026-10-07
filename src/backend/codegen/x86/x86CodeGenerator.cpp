#include "x86CodeGenerator.hpp"

#include "utils/error.hpp"

namespace codegen
{

	X86CodeGenerator::X86CodeGenerator(const Target &target)
		: target(target)
	{
		if (target.arch != Target::Arch::X86 && target.arch != Target::Arch::X86_64)
			throw CompilerError::internal("Tried to initalize x86 code generator with non-x86 arch");
	}

	void X86CodeGenerator::store_spilled_vreg(PhysReg src, ir::VRegId vreg, size_t spill_index)
	{
		throw CompilerError::unimplemented("TODO x86 register spilling");
	}

	void X86CodeGenerator::load_spilled_vreg(PhysReg dest, ir::VRegId vreg, size_t spill_index)
	{
		throw CompilerError::unimplemented("TODO x86 register spilling");
	}
}
