#pragma once

#include <exception>
#include <string>
#include <optional>

#include "utils/common.hpp"

class CompileError : public std::exception
{
	std::string msg;
	std::optional<SourceLoc> src_start;
	std::optional<SourceLoc> src_end;

public:
	CompileError(const std::string &_msg);
	CompileError(const std::string &_msg, SourceLoc start);
	CompileError(const std::string &_msg, SourceLoc start, SourceLoc end);

	const char *what();

	// prepends a prefix to this error
	void add_prefix(const std::string &prefix);
};