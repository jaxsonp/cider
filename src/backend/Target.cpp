#include "Target.hpp"

#include <format>

#include "utils/error.hpp"

#include "codegen/riscv/RiscvCodeGenerator.hpp"
#include "codegen/x86/x86CodeGenerator.hpp"

#include "objwriter/elf/ObjectWriter_ELF.hpp"

const std::unordered_map<std::string_view, Target> Target::supported_targets = {
	{"linux-riscv32g", Target(Arch::RISCV32, RiscvExt::G, ABI::RISCV_ILP32D, OS::Linux, ObjectFormat::ELF32)},
	{"linux-riscv32gc", Target(Arch::RISCV32, RiscvExt::G | RiscvExt::C, ABI::RISCV_ILP32D, OS::Linux, ObjectFormat::ELF32)},
	{"linux-riscv64g", Target(Arch::RISCV64, RiscvExt::G, ABI::RISCV_LP64D, OS::Linux, ObjectFormat::ELF64)},
	{"linux-riscv64gc", Target(Arch::RISCV64, RiscvExt::G | RiscvExt::C, ABI::RISCV_LP64D, OS::Linux, ObjectFormat::ELF64)},
	{"linux-x86", Target(Arch::X86, 0u, ABI::SYSV_I386, OS::Linux, ObjectFormat::ELF32)},
	{"linux-x86_64", Target(Arch::X86_64, 0u, ABI::SYSV_AMD64, OS::Linux, ObjectFormat::ELF64)},
};

std::string Target::detect_host_target()
{
	static std::string cached_value;

	if (cached_value.empty())
	{

#if defined(__linux__)
		std::string os = "linux";
#elif defined(__APPLE__) && defined(__MACH__)
		std::string os = "macos";
#elif defined(_WIN32)
		std::string os = "windows";
#elif defined(__FreeBSD__)
		std::string os = "freebsd";
#elif defined(__OpenBSD__)
		std::string os = "openbsd";
#elif defined(__NetBSD__)
		std::string os = "netbsd";
#else
		std::string os = "unknown";
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
		std::string isa = "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
		std::string isa = "x86";
#elif defined(__riscv)
		std::string isa = std::format("riscv{}", __riscv_xlen);
		// G (IMAFD) when the host has all of it, otherwise list the extensions individually so the name won't
		// match a supported target (TODO maybe fix this? idk)
#if defined(__riscv_mul) && defined(__riscv_atomic) && defined(__riscv_flen) && __riscv_flen >= 64
		isa += "g";
#else
		isa += "i";
#if defined(__riscv_mul)
		isa += "m";
#endif
#if defined(__riscv_atomic)
		isa += "a";
#endif
#if defined(__riscv_flen) && __riscv_flen >= 32
		isa += "f";
#endif
#if defined(__riscv_flen) && __riscv_flen >= 64
		isa += "d";
#endif
#endif
#if defined(__riscv_compressed)
		isa += "c";
#endif
#elif defined(__aarch64__) || defined(_M_ARM64)
		std::string isa = "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
		std::string isa = "arm";
#else
		std::string isa = "unknown";
#endif

		cached_value = std::format("{}-{}", os, isa);
	}
	return cached_value;
}

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
		break;
	default:
		throw CompilerError::internal("Uncaught architecture variant (Target::get_code_generator())");
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
	throw CompilerError::internal("Uncaught architecture variant (Target::register_width())");
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

Target::Target()
	: Target(Target::detect_host_target()) {}

Target::Target(std::string_view target_name)
{
	auto found_target = Target::supported_targets.find(target_name);
	if (found_target == Target::supported_targets.end())
	{
		std::string supported;
		for (const auto &[name, target] : Target::supported_targets)
			supported += std::format("{}{}", supported.empty() ? "" : ", ", name);
		throw CompilerError::unsupported(
			std::format("Unknown target \"{}\" (supported targets: {})", target_name, supported));
	}

	*this = found_target->second;
}
