#include "Target.hpp"

#include "utils/error.hpp"

#include "codegen/riscv/RiscvCodeGenerator.hpp"
#include "codegen/x86/x86CodeGenerator.hpp"

#include "objwriter/elf/ObjectWriter_ELF.hpp"

const std::unordered_map<std::string, Target> Target::supported_targets = {
	{"linux-riscv32g", Target(Arch::RISCV32, RiscvExt::G, ABI::RISCV_ILP32D, OS::Linux, ObjectFormat::ELF32)},
	{"linux-riscv32gc", Target(Arch::RISCV32, RiscvExt::G | RiscvExt::C, ABI::RISCV_ILP32D, OS::Linux, ObjectFormat::ELF32)},
	{"linux-riscv64g", Target(Arch::RISCV64, RiscvExt::G, ABI::RISCV_LP64D, OS::Linux, ObjectFormat::ELF64)},
	{"linux-riscv64gc", Target(Arch::RISCV64, RiscvExt::G | RiscvExt::C, ABI::RISCV_LP64D, OS::Linux, ObjectFormat::ELF64)},
	{"linux-x86", Target(Arch::X86, 0u, ABI::SYSV_I386, OS::Linux, ObjectFormat::ELF32)},
	{"linux-x86_64", Target(Arch::X86_64, 0u, ABI::SYSV_AMD64, OS::Linux, ObjectFormat::ELF64)},
};

std::unique_ptr<CodeGenerator> Target::get_code_generator() const
{
	std::unique_ptr<CodeGenerator> ret;
	switch (this->arch)
	{
	case Arch::RISCV32:
	case Arch::RISCV64:
		ret = std::make_unique<codegen::RiscvCodeGenerator>(*this);
		break;
	case Arch::X86:
	case Arch::X86_64:
		ret = std::make_unique<codegen::X86CodeGenerator>(*this);
	default:
		throw CompilerError::internal("Uncaught architecture variant");
	}
	return ret;
}

std::unique_ptr<ObjectWriter> Target::get_object_writer() const
{
	std::unique_ptr<ObjectWriter> ret;
	switch (this->format)
	{
	case ObjectFormat::ELF32:
		ret = std::make_unique<objwriter::ObjectWriter_ELF32>();
		break;
	case ObjectFormat::ELF64:
		ret = std::make_unique<objwriter::ObjectWriter_ELF64>();
		break;
	default:
		throw CompilerError::internal("Uncaught object format variant");
	}
	return ret;
}

unsigned int Target::register_width() const
{
	switch (this->arch)
	{
	case Arch::RISCV32:
	case Arch::X86:
		return 32;
	case Arch::RISCV64:
	case Arch::X86_64:
		return 64;
	}
	throw CompilerError::internal("Uncaught architecture variant");
}

unsigned short Target::abi_float_precision() const
{
	switch (this->abi)
	{
	case ABI::RISCV_ILP32:
	case ABI::RISCV_LP64:
		return 0;
	case ABI::RISCV_ILP32F:
	case ABI::RISCV_LP64F:
		return 1;
	case ABI::RISCV_ILP32D:
	case ABI::RISCV_LP64D:
	case ABI::SYSV_I386:
	case ABI::SYSV_AMD64:
		return 2;
	}
	throw CompilerError::internal("Uncaught ABI variant");
}
