#include "Target.hpp"

#include "utils/error.hpp"

#include "codegen/riscv/CodeGenerator_riscv.hpp"

#include "objwriter/elf/ObjectWriter_ELF.hpp"

const std::unordered_map<std::string, Target> Target::supported_targets = {
	{"linux-riscv32g", Target(Arch::RISCV32, RiscvExt::G, ABI::ILP32D, OS::Linux, ObjectFormat::ELF32)},
	{"linux-riscv32gc", Target(Arch::RISCV32, RiscvExt::G | RiscvExt::C, ABI::ILP32D, OS::Linux, ObjectFormat::ELF32)},
	{"linux-riscv64g", Target(Arch::RISCV64, RiscvExt::G, ABI::LP64D, OS::Linux, ObjectFormat::ELF64)},
	{"linux-riscv64gc", Target(Arch::RISCV64, RiscvExt::G | RiscvExt::C, ABI::LP64D, OS::Linux, ObjectFormat::ELF64)},
};

std::unique_ptr<CodeGenerator> Target::get_code_generator() const
{
	std::unique_ptr<CodeGenerator> ret;
	switch (this->arch)
	{
	case Arch::RISCV32:
	case Arch::RISCV64:
		ret = std::make_unique<codegen::CodeGenerator_riscv>(*this);
		break;
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

unsigned int Target::register_bits() const
{
	switch (this->arch)
	{
	case Arch::RISCV32:
		return 32;
	case Arch::RISCV64:
		return 64;
	}
	throw CompilerError::internal("Uncaught architecture variant");
}
