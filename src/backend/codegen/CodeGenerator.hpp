#pragma once

#include <stdint.h>
#include <ostream>
#include <memory>
#include <vector>

#include "IR.hpp"
#include "backend/objwriter/ObjectWriter.hpp"
#include "backend/Target.hpp"

class Target;

class CodeGenerator
{
	std::unique_ptr<std::ostream> asm_out_ptr = nullptr;

protected:
	/// @brief Access the assembly output stream. Returns a black hole if asm output is not enabled
	std::ostream &asm_out();

public:
	virtual Object lower_ir(const IrObject &ir) = 0;

	/// @brief Creates the small chunk of entry code for an executable
	/// @param main_offset Offset of the main function BEFORE prepending this runtime code
	/// @param t Target
	/// @return Runtime code in a byte vector
	virtual std::vector<uint8_t> build_runtime_code(uint64_t main_offset, Target t) = 0;

	/// @brief Enables dumping assembly to an output stream. ostream object must live as long as this object.
	virtual void enable_asm_output(std::unique_ptr<std::ostream> out);
};