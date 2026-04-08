#pragma once

#include <exception>
#include <string>
#include <variant>

#include "utils/common.hpp"

class CompilerError : public std::exception
{
public:
	using LocVariant = std::variant<std::monostate, SourceLoc, SourceLocRange>;

private:
	const ExitCode code = ExitCode::UncaughtInternalError;

protected:
	// ctor for implementors
	CompilerError(
		const char *type_name,
		ExitCode code,
		const std::string &msg,
		LocVariant loc);

public:
	std::string display;
	const LocVariant loc;

	inline ExitCode exit_code() const noexcept { return this->code; }

	inline const char *what() const noexcept override { return display.c_str(); };

	/// @brief Intantiate a compiler error related to invalid syntax
	static CompilerError syntax_error(const std::string &msg, LocVariant loc = std::monostate{});
	/// @brief Intantiate a compiler error related to an erroneous name
	static CompilerError name_error(const std::string &msg, LocVariant loc = std::monostate{});
	/// @brief Intantiate a compiler error related to types
	static CompilerError type_error(const std::string &msg, LocVariant loc = std::monostate{});
	/// @brief Intantiate a compiler error related to language semantics
	static CompilerError semantic_error(const std::string &msg, LocVariant loc = std::monostate{});
	/// @brief Intantiate a compiler error indicating a feature/function is unsupported
	static CompilerError unsupported(const std::string &msg, LocVariant loc = std::monostate{});
	/// @brief Intantiate a compiler error related to opening/reading/writing with OS files
	static CompilerError file_io_error(const std::string &msg, LocVariant loc = std::monostate{});
	/// @brief Intantiate a compiler error for an unimplemented feature/function (generally for TODOs)
	static CompilerError unimplemented(const std::string &msg, LocVariant loc = std::monostate{});
	/// @brief Intantiate a compiler error for a state that _should_ not be thrown, indicating invalid state internally. Jaxson is a bad programmer if this one is thrown
	static CompilerError internal(const std::string &msg, LocVariant loc = std::monostate{});
};