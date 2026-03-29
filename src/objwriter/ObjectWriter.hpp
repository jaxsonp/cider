#pragma once

#include <ostream>
#include <memory>

#include "backend/Backend.hpp"

/// @brief Interface for consuming a blob of machine code and writing it to file
class ObjectWriter
{
	Object obj;

public:
	ObjectWriter() = delete;
	/// @brief
	ObjectWriter(Object object) : obj(std::move(object)) {}
	virtual void emit(std::ostream &out) = 0;
};