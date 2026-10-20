#include "testing.hpp"

#include <filesystem>
#include <iostream>

const static std::filesystem::path SRC_DIR =
#ifdef CIDER_SRC_DIR
	// this value is set in cmakelists
	std::filesystem::path(CIDER_SRC_DIR);
#else
	// fallback
	std::filesystem::path(__FILE__).parent_path().parent_path().append("src");
#endif

Test::Test(std::string_view name, TestCategory category, std::function<void()> fn)
	: name(name), category(category), fn(fn) {}

std::string_view to_string(TestCategory category)
{
	switch (category)
	{
	case TestCategory::Unit:
		return "unit";
	case TestCategory::End2End:
		return "e2e";
	}
	return "?";
}

std::vector<Test> &get_test_registry()
{
	static std::vector<Test> tests;
	return tests;
}

bool register_test(Test test)
{
	get_test_registry().push_back(std::move(test));
	return true;
}

std::string make_test_name(std::string_view test_file, std::string_view test_name)
{
	constexpr std::string_view expected_ext(".tests.cpp");
	if (test_file.ends_with(expected_ext))
		test_file = test_file.substr(0, test_file.size() - expected_ext.size());
	else
		throw std::logic_error("Unit tests must be defined in a file ending with \".test.cpp\"");

	// get source file path relative to `src` directory
	std::filesystem::path relative_path = std::filesystem::relative(std::filesystem::path(test_file), SRC_DIR);
	return std::format("{}/{}", relative_path.generic_string(), test_name);
}

[[noreturn]] void testing_detail::fail(const char *file, int line, const std::string &msg)
{
	throw TestFailure(std::format("{}:{}: {}", std::filesystem::path(file).string(), line, msg));
}
