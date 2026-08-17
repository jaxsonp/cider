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

/// @brief Generates a bitmask with the N least-significant bits set
template <typename T>
	requires std::is_integral_v<T> && std::is_unsigned_v<T>
constexpr T lower_bitmask(size_t n)
{
	// static_assert(std::is_integral_v<T>, "Template type must be an integral type.");
	if (n == 0)
		return 0;
	else if (n >= 32)
		return ~T{0}; // all bits set
	else
		return (T{1} << n) - 1;
}
