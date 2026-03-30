#include "Target.hpp"

#include "utils/error.hpp"

#include "codegen/riscv/CodeGenerator_riscv32.hpp"

#include "objwriter/elf/ObjectWriter_ELF32.hpp"

const std::unordered_map<std::string, Target> Target::supported_targets = {
	{"linux-riscv32g", Target(ISA::RV32G, ABI::ILP32D, OS::Linux, ObjectFormat::ELF32)},
};

std::unique_ptr<CodeGenerator> Target::get_code_generator() const
{
	std::unique_ptr<CodeGenerator> ret;
	switch (this->isa)
	{
	case ISA::RV32G:
		ret = std::make_unique<codegen::CodeGenerator_riscv32>();
		break;
	default:
		throw InternalError("Uncaught ISA variant");
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
	default:
		throw InternalError("Uncaught object format variant");
	}
	return ret;
}