#pragma once

#include <stdint.h>
#include <ostream>
#include <memory>
#include <vector>

#include "backend/Target.hpp"

struct Object
{
	struct Function
	{
		std::string name;
		size_t code_offset;
	};

	std::vector<Function> functions;

	/// Where machine code to be executed goes
	std::vector<uint8_t> code;
};

/// @brief Interface for consuming a blob of machine code and writing it to file
class ObjectWriter
{
public:
	virtual void emit(const Object &obj, const Target &target, std::ostream &out) = 0;
};