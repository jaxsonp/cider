#include "error.hpp"

#include <format>

CompileError::CompileError(const std::string &_msg)
	: std::exception(), msg(_msg), src_start(), src_end() {}

CompileError::CompileError(const std::string &_msg, SourceLoc start)
	: std::exception(), src_start(start), src_end()
{
	this->msg = std::format("{} (at {}:{})", _msg, start.line, start.col);
}

CompileError::CompileError(const std::string &_msg, SourceLoc start, SourceLoc end)
	: std::exception(), src_start(start), src_end(end)
{
	this->msg = std::format("{} (at {}:{})", _msg, start.line, start.col);
}

const char *CompileError::what()
{
	return msg.c_str();
}

void CompileError::add_prefix(const std::string &prefix)
{
	msg = prefix + msg;
}
