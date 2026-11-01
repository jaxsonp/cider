#include "unit_testing.hpp"
#include "utils/common.hpp"

#include <string>

TEST(whitespace_characters)
{
	for (char c : {' ', '\t', '\n', '\v', '\f', '\r'})
		ASSERT_TRUE(is_whitespace(c));
	for (char c : {'a', '0', '_', '\0', char(8), char(14), char(31), char(33)})
		ASSERT_TRUE(!is_whitespace(c));
}

TEST(numeric_characters)
{
	for (char c = '0'; c <= '9'; ++c)
		ASSERT_TRUE(is_numeric(c));
	ASSERT_TRUE(!is_numeric('0' - 1));
	ASSERT_TRUE(!is_numeric('9' + 1));
	ASSERT_TRUE(!is_numeric('a'));
}

TEST(alpha_characters)
{
	for (char c = 'a'; c <= 'z'; ++c)
		ASSERT_TRUE(is_alpha(c));
	for (char c = 'A'; c <= 'Z'; ++c)
		ASSERT_TRUE(is_alpha(c));
	for (char c : {char('a' - 1), char('z' + 1), char('A' - 1), char('Z' + 1), '0', '_'})
		ASSERT_TRUE(!is_alpha(c));
}

TEST(delimiters)
{
	for (char c : {' ', ';', '(', '+', '\n', '\0', char(std::char_traits<char>::eof())})
		ASSERT_TRUE(is_delimiter(c));
	for (char c : {'a', 'Z', '0', '9', '_'})
		ASSERT_TRUE(!is_delimiter(c));
}

TEST(bool_strings)
{
	ASSERT_EQ(bool_str(true), "true");
	ASSERT_EQ(bool_str(false), "false");
}

TEST(lower_bitmasks)
{
	ASSERT_EQ(lower_bitmask<uint32_t>(0), 0u);
	ASSERT_EQ(lower_bitmask<uint32_t>(1), 1u);
	ASSERT_EQ(lower_bitmask<uint32_t>(12), 0xFFFu);
	ASSERT_EQ(lower_bitmask<uint32_t>(31), 0x7FFFFFFFu);
	ASSERT_EQ(lower_bitmask<uint64_t>(33), 0x1FFFFFFFFull);
}

TEST(lower_bitmask_of_the_whole_type)
{
	ASSERT_EQ(lower_bitmask<uint32_t>(32), UINT32_MAX);
	ASSERT_EQ(lower_bitmask<uint64_t>(64), UINT64_MAX);
	ASSERT_EQ(lower_bitmask<uint32_t>(100), UINT32_MAX);
	ASSERT_EQ(int(lower_bitmask<uint8_t>(8)), 0xFF);
}

TEST(lower_bitmask_is_constexpr)
{
	static_assert(lower_bitmask(5) == 0x1Fu);
	static_assert(lower_bitmask<int64_t>(4) == 0xF);
}

TEST(source_location_strings)
{
	ASSERT_EQ((SourceLoc{3, 7}).to_string(), "3:7");
	ASSERT_EQ((SourceLocRange{{1, 2}, {3, 4}}).to_string(), "1:2-3:4");
}

TEST(source_location_defaults)
{
	ASSERT_TRUE(SourceLoc{} == (SourceLoc{0, 0}));
}

TEST(source_location_equality)
{
	ASSERT_TRUE((SourceLoc{1, 2}) == (SourceLoc{1, 2}));
	ASSERT_TRUE(!((SourceLoc{1, 2}) == (SourceLoc{1, 3})));
	ASSERT_TRUE(!((SourceLoc{1, 2}) == (SourceLoc{2, 2})));
	ASSERT_TRUE((SourceLocRange{{1, 2}, {3, 4}}) == (SourceLocRange{{1, 2}, {3, 4}}));
	ASSERT_TRUE(!((SourceLocRange{{1, 2}, {3, 4}}) == (SourceLocRange{{1, 2}, {3, 5}})));
}

TEST(exit_codes_are_stable)
{
	// e2e tests and scripts depend on these numbers
	ASSERT_EQ(exit_code_as_int(ExitCode::Success), 0);
	ASSERT_EQ(exit_code_as_int(ExitCode::SyntaxError), 1);
	ASSERT_EQ(exit_code_as_int(ExitCode::NameError), 2);
	ASSERT_EQ(exit_code_as_int(ExitCode::TypeError), 3);
	ASSERT_EQ(exit_code_as_int(ExitCode::SemanticError), 4);
	ASSERT_EQ(exit_code_as_int(ExitCode::Unsupported), 16);
	ASSERT_EQ(exit_code_as_int(ExitCode::FileIoError), 224);
	ASSERT_EQ(exit_code_as_int(ExitCode::UsageError), 225);
	ASSERT_EQ(exit_code_as_int(ExitCode::Unimplemented), 253);
	ASSERT_EQ(exit_code_as_int(ExitCode::InternalError), 254);
	ASSERT_EQ(exit_code_as_int(ExitCode::UncaughtInternalError), 255);
}
