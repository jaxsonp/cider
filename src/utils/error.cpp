#include "error.hpp"

#include <format>

CompilerError::CompilerError(const char *type_name, ExitCode exit_code, const std::string &error_msg, LocVariant src_loc)
	: code(exit_code)
{
	if (std::holds_alternative<SourceLoc>(src_loc))
	{
		SourceLoc loc = std::get<SourceLoc>(src_loc);
		this->display = std::format("{}: {} (at {}:{})", type_name, error_msg, loc.line, loc.col);
	}
	else if (std::holds_alternative<SourceLocRange>(src_loc))
	{
		SourceLocRange loc = std::get<SourceLocRange>(src_loc);
		this->display = std::format("{}: {} (at {}:{})", type_name, error_msg, loc.start.line, loc.start.col);
	}
	else
	{
		this->display = std::format("{}: {}", type_name, error_msg);
	}
}

CompilerError CompilerError::syntax_error(const std::string &msg, LocVariant loc)
{
	return CompilerError("Syntax error", ExitCode::SyntaxError, msg, loc);
}

CompilerError CompilerError::name_error(const std::string &msg, LocVariant loc)
{
	return CompilerError("Name error", ExitCode::NameError, msg, loc);
}

CompilerError CompilerError::type_error(const std::string &msg, LocVariant loc)
{
	return CompilerError("Type error", ExitCode::TypeError, msg, loc);
}

CompilerError CompilerError::semantic_error(const std::string &msg, LocVariant loc)
{
	return CompilerError("Semantic error", ExitCode::SemanticError, msg, loc);
}

CompilerError CompilerError::unsupported(const std::string &msg, LocVariant loc)
{
	return CompilerError("Unsupported", ExitCode::Unsupported, msg, loc);
}

CompilerError CompilerError::file_io_error(const std::string &msg, LocVariant loc)
{
	return CompilerError("File IO error", ExitCode::FileIoError, msg, loc);
}

CompilerError CompilerError::unimplemented(const std::string &msg, LocVariant loc)
{
	return CompilerError("Unimplemented", ExitCode::Unimplemented, msg, loc);
}

CompilerError CompilerError::internal(const std::string &msg, LocVariant loc)
{
	return CompilerError("Internal error", ExitCode::InternalError, msg, loc);
}
