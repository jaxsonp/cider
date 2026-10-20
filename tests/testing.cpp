#include "testing.hpp"

#include <filesystem>

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

std::string group_from_file(std::string_view file)
{
	// "Lexer.tests.cpp" -> "Lexer"
	std::string stem = std::filesystem::path(file).stem().string();
	constexpr std::string_view suffix = ".tests";
	if (stem.ends_with(suffix))
		stem.resize(stem.size() - suffix.size());
	return stem;
}

[[noreturn]] void testing_detail::fail(const char *file, int line, const std::string &msg)
{
	throw TestFailure(std::format("{}:{}: {}", std::filesystem::path(file).filename().string(), line, msg));
}
