#pragma once

#include <string>
#include <stdint.h>

enum class ExitCode : uint8_t
{
	Success = 0,

	// compilation errors

	SyntaxError = 0x1,
	NameError = 0x2,
	TypeError = 0x3,
	SemanticError = 0x4,

	Unsupported = 0x10,

	// non-compilation errors

	FileIoError = 0xE0,
	UsageError = 0xE1,

	// internal errors

	// glorified TODO
	Unimplemented = 0xFD,
	// internal error thrown manually during compilation
	InternalError = 0xFE,
	// exception not caught by compilation code
	UncaughtInternalError = 0xFF,
};

inline int exit_code_as_int(ExitCode code) noexcept { return static_cast<int>(code); }

struct SourceLoc
{
	unsigned int line = 0;
	unsigned int col = 0;

	std::string to_string() const;

	bool operator==(const SourceLoc &other) const = default;
};

struct SourceLocRange
{
	SourceLoc start;
	SourceLoc end;

	std::string to_string() const;

	bool operator==(const SourceLocRange &other) const = default;
};

bool is_whitespace(char c);

bool is_numeric(char c);

bool is_alpha(char c);

bool is_delimiter(char c);

std::string_view bool_str(bool b);
