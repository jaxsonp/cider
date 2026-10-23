#include "unit_testing.hpp"

#include <filesystem>
#include <iostream>

const static std::filesystem::path cider_src_dir =
#ifdef CIDER_SRC_DIR
	// this value is set in cmakelists
	std::filesystem::path(CIDER_SRC_DIR);
#else
	// fallback
	std::filesystem::path(__FILE__).parent_path().parent_path().append("src");
#endif

namespace testing::unit
{

	UnitTest::UnitTest(std::string_view name, std::function<void()> fn)
		: Test(name, Test::Category::Unit), fn(fn) {}

	Result UnitTest::run() const
	{
		try
		{
			this->fn();
			return {Outcome::Pass, ""};
		}
		catch (const TestFailure &e)
		{
			return {Outcome::Fail, e.what()};
		}
		catch (const TestSkipped &e)
		{
			return {Outcome::Skip, e.what()};
		}
		catch (const std::exception &e)
		{
			return {Outcome::Fail, std::format("uncaught exception: {}", e.what())};
		}
		catch (...)
		{
			return {Outcome::Fail, "uncaught non-std exception"};
		}
	}

	std::vector<UnitTest> &get_unit_test_registry()
	{
		static std::vector<UnitTest> tests;
		return tests;
	}

	bool register_unit_test(UnitTest test)
	{
		get_unit_test_registry().push_back(std::move(test));
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
		std::filesystem::path relative_path = std::filesystem::relative(std::filesystem::path(test_file), cider_src_dir);
		return std::format("{}/{}", relative_path.generic_string(), test_name);
	}

	namespace detail
	{
		[[noreturn]] void fail(const char *file, int line, const std::string &msg)
		{
			throw TestFailure(std::format("{}:{}: {}", std::filesystem::path(file).string(), line, msg));
		}
	}
}