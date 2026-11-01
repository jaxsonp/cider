#pragma once

#include <string>
#include <string_view>

#include "utils/common.hpp"
#include "unit_testing.hpp"

/// Helpers for unit tests that feed Cider source to the compiler in memory. Nothing here spawns a process or
/// touches a file
namespace testing::unit
{
	/// @brief Runs source through the parser, symbol resolution and type checking
	/// @return `ExitCode::Success` if it's accepted, otherwise the exit code of the error it's rejected with.
	/// An exception that isn't a `CompilerError` gives `ExitCode::UncaughtInternalError`, like in `main`
	ExitCode check_source(const std::string &source);

	/// @brief Textual IR of source, which must be valid
	std::string source_to_ir(const std::string &source);

	/// @brief Name of an exit code, for failure messages
	std::string_view exit_code_name(ExitCode code);
}

/// Fail the test unless `source` makes it through the frontend with the exit code `expected`
#define ASSERT_SOURCE(source, expected)                                                          \
	do                                                                                           \
	{                                                                                            \
		using namespace testing::unit::detail;                                                   \
		const std::string check_source_ = (source);                                              \
		const ExitCode check_got_ = testing::unit::check_source(check_source_);                  \
		const ExitCode check_expected_ = (expected);                                             \
		if (check_got_ != check_expected_)                                                       \
			fail(__FILE__, __LINE__,                                                             \
				 std::format("ASSERT_SOURCE failed\n  source:   {}\n  got:      {}\n  expected: {}", \
							 check_source_, testing::unit::exit_code_name(check_got_),            \
							 testing::unit::exit_code_name(check_expected_)));                    \
	} while (0)
