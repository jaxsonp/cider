#pragma once

#include <ostream>
#include <memory>

#include "IR.hpp"

struct Object
{
	std::vector<std::string> functions;

	std::vector<uint8_t> text;
};

class Backend
{
	std::unique_ptr<std::ostream> asm_out_ptr = nullptr;

protected:
	/// @brief Access the assembly output stream. Returns a black hole if asm output is not enabled
	std::ostream &asm_out();

public:
	virtual Object lower_ir(const IrObject &ir) = 0;

	/// @brief Enables dumping assembly to an output stream. ostream object must live as long as this object.
	virtual void enable_asm_output(std::unique_ptr<std::ostream> out);
};