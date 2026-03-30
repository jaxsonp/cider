#include "CodeGenerator.hpp"

#include <iostream>

// A streambuffer that discards everything
class BlackHoleBuffer : public std::streambuf
{
public:
	int overflow(int c) override { return c; }
};

// An ostream that discards everything
class BlackHoleStream : public std::ostream
{
	BlackHoleBuffer buf;

public:
	BlackHoleStream() : std::ostream(&buf) {}
};

std::ostream &CodeGenerator::asm_out()
{
	static BlackHoleStream null_out = BlackHoleStream();

	if (this->asm_out_ptr != nullptr)
		return *this->asm_out_ptr;
	else
		return null_out;
}

void CodeGenerator::enable_asm_output(std::unique_ptr<std::ostream> out)
{
	this->asm_out_ptr = std::move(out);
}