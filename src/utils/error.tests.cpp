#include "unit_testing.hpp"
#include "utils/error.hpp"

#include <vector>

TEST(each_factory_gives_its_exit_code_and_label)
{
	struct Case
	{
		CompilerError error;
		ExitCode code;
		std::string display;
	};
	const std::vector<Case> cases = {
		{CompilerError::syntax_error("msg"), ExitCode::SyntaxError, "Syntax error: msg"},
		{CompilerError::name_error("msg"), ExitCode::NameError, "Name error: msg"},
		{CompilerError::type_error("msg"), ExitCode::TypeError, "Type error: msg"},
		{CompilerError::semantic_error("msg"), ExitCode::SemanticError, "Semantic error: msg"},
		{CompilerError::unsupported("msg"), ExitCode::Unsupported, "Unsupported: msg"},
		{CompilerError::file_io_error("msg"), ExitCode::FileIoError, "File IO error: msg"},
		{CompilerError::unimplemented("msg"), ExitCode::Unimplemented, "Unimplemented: msg"},
		{CompilerError::internal("msg"), ExitCode::InternalError, "Internal error: msg"},
	};
	for (const Case &c : cases)
	{
		ASSERT_EQ(int(c.error.exit_code()), int(c.code));
		ASSERT_EQ(std::string(c.error.what()), c.display);
		ASSERT_EQ(c.error.display, c.display);
	}
}

TEST(location_is_appended_to_the_message)
{
	CompilerError error = CompilerError::syntax_error("msg", SourceLoc{3, 7});
	ASSERT_EQ(std::string(error.what()), "Syntax error: msg (at 3:7)");
}

TEST(range_shows_where_it_starts)
{
	CompilerError error = CompilerError::type_error("msg", SourceLocRange{{1, 2}, {3, 4}});
	ASSERT_EQ(std::string(error.what()), "Type error: msg (at 1:2)");
}

TEST(no_location_by_default)
{
	CompilerError error = CompilerError::internal("msg");
	ASSERT_TRUE(std::holds_alternative<std::monostate>(error.loc));
}

TEST(is_a_std_exception)
{
	ASSERT_THROWS(throw CompilerError::internal("msg"), std::exception);
}
