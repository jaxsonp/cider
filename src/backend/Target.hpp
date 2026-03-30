#pragma once

#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <unordered_map>
#include <memory>

#include "codegen/CodeGenerator.hpp"
#include "objwriter/ObjectWriter.hpp"

class CodeGenerator;
class ObjectWriter;

class Target
{
public:
	enum class ISA
	{
		RV32G,
	};

	enum class ABI
	{
		/// RISC-V 32 bit non-float ABI
		ILP32,
		/// RISC-V 32 bit single precision float ABI
		ILP32F,
		/// RISC-V 32 bit double precision float ABI
		ILP32D,
	};

	enum class OS
	{
		Linux,
	};

	enum class ObjectFormat
	{
		ELF32,
	};

	ISA isa;
	ABI abi;
	OS os;
	ObjectFormat format;

	/// Map of all supported targets
	static const std::unordered_map<std::string, Target> supported_targets;

	std::unique_ptr<CodeGenerator> get_code_generator() const;
	std::unique_ptr<ObjectWriter> get_object_writer() const;

	Target() = delete;

private:
	// constructors are private, only allowed targets are accessible through map

	Target(ISA isa, ABI abi, OS os, ObjectFormat format)
		: isa(isa), abi(abi), os(os), format(format) {}
};
