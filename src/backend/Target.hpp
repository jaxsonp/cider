#pragma once

#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <unordered_map>
#include <memory>

class CodeGenerator;
class ObjectWriter;

class Target
{
public:
	/// Architecture family and register width. Finer grained differences between targets of the same
	/// architecture (ISA extensions) are described by `features`
	enum class Arch
	{
		/// RISC-V with 32 bit registers (RV32I base)
		RISCV32,
		/// RISC-V with 64 bit registers (RV64I base)
		RISCV64,
	};

	/// @brief RISC-V ISA extensions, combined as bit flags in `Target::features`
	struct RiscvExt
	{
		/// Integer multiplication and division
		static constexpr uint32_t M = 1u << 0;
		/// Atomics
		static constexpr uint32_t A = 1u << 1;
		/// Single precision floats
		static constexpr uint32_t F = 1u << 2;
		/// Double precision floats
		static constexpr uint32_t D = 1u << 3;
		/// Compressed (16 bit) instructions
		static constexpr uint32_t C = 1u << 4;

		/// "General purpose" shorthand, IMAFD (also implies Zicsr and Zifencei, which codegen never uses)
		static constexpr uint32_t G = M | A | F | D;
	};

	enum class ABI
	{
		/// RISC-V 32 bit non-float ABI
		ILP32,
		/// RISC-V 32 bit single precision float ABI
		ILP32F,
		/// RISC-V 32 bit double precision float ABI
		ILP32D,
		/// RISC-V 64 bit non-float ABI
		LP64,
		/// RISC-V 64 bit single precision float ABI
		LP64F,
		/// RISC-V 64 bit double precision float ABI
		LP64D,
	};

	enum class OS
	{
		Linux,
	};

	enum class ObjectFormat
	{
		ELF32,
		ELF64,
	};

	Arch arch;
	/// Architecture specific feature flags (for RISC-V, a combination of `RiscvExt` flags)
	uint32_t features;
	ABI abi;
	OS os;
	ObjectFormat format;

	/// Map of all supported targets
	static const std::unordered_map<std::string, Target> supported_targets;

	std::unique_ptr<CodeGenerator> get_code_generator() const;
	std::unique_ptr<ObjectWriter> get_object_writer() const;

	/// @brief Width (in bits) of a general purpose register, which is also the width of an address
	unsigned int register_bits() const;

	/// @brief Whether every one of the given feature flags is enabled for this target
	bool has(uint32_t feature_flags) const { return (this->features & feature_flags) == feature_flags; }

	Target() = delete;

private:
	// constructors are private, only allowed targets are accessible through map

	Target(Arch arch, uint32_t features, ABI abi, OS os, ObjectFormat format)
		: arch(arch), features(features), abi(abi), os(os), format(format) {}
};
