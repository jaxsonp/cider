#include "e2e_testing.hpp"

#include <stdexcept>

namespace testing::e2e
{
	constexpr std::string_view make_test_name(std::string_view file_path)
	{
		if (!file_path.ends_with(".cdr"))
			throw std::logic_error("e2e test files must end with .cdr");
		return file_path.substr(0, file_path.size() - 4);
	}

	E2eTest::E2eTest(std::string_view file_path, const char *file_contents)
		: Test(make_test_name(file_path), Test::Category::EndToEnd), source(file_contents) {}

	Result E2eTest::run() const
	{
		return Result(Outcome::Fail, "todo");
	}
}