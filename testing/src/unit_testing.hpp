#pragma once

#include <format>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <string>
#include <vector>

#include "testing.hpp"

namespace testing::unit
{

	class UnitTest : public Test
	{
	public:
		// Test name, with the categories prepended and delimited by '/'
		const std::function<void()> fn;

		UnitTest(std::string_view name, std::function<void()> fn);

		Result run() const override;
	};

	/// Every test known to the runner. Statically defined tests (TEST) add themselves
	/// before main, generated ones (e2e fixtures) are added by the runner at startup
	std::vector<UnitTest> &get_unit_test_registry();

	/// Registers a test, returns a dummy so it can initialize a static
	bool register_unit_test(UnitTest test);

	/// Creates the name of a unit test from the file its defined in and its name.
	/// File_path must be in `src`
	std::string make_test_name(std::string_view file_path, std::string_view test_name);

	/// Thrown by a failing check
	struct TestFailure : std::runtime_error
	{
		explicit TestFailure(const std::string &msg) : std::runtime_error(msg) {}
	};

	/// Thrown to skip a test (e.g. no way to execute a target's binaries on this host)
	struct TestSkipped : std::runtime_error
	{
		explicit TestSkipped(const std::string &reason) : std::runtime_error(reason) {}
	};

	namespace detail
	{

		/// Best-effort printing of a checked value for failure messages
		template <typename T>
		std::string show(const T &val)
		{
			if constexpr (std::is_default_constructible_v<std::formatter<T, char>>)
				return std::format("{}", val);
			else if constexpr (requires(std::ostream &os) { os << val; })
			{
				std::ostringstream ss;
				ss << val;
				return ss.str();
			}
			else if constexpr (requires { to_string(val); })
				return std::string(to_string(val));
			else
				return "<unprintable>";
		}

		/// @brief Print a fail for a unit test
		[[noreturn]] void fail(const char *file, int line, const std::string &msg);

	}
}

/// Macro for defining an in-source unit test follow it's invocation with a function body.
#define TEST(name)                                                                                \
	static void test_fn_##name();                                                                 \
	static const bool test_reg_##name = testing::unit::register_unit_test(                        \
		testing::unit::UnitTest(testing::unit::make_test_name(__FILE__, #name), test_fn_##name)); \
	static void test_fn_##name()

/// Fail the test if `cond` is false
#define ASSERT_TRUE(cond)                                                           \
	do                                                                              \
	{                                                                               \
		using namespace testing::unit::detail;                                      \
		if (!(cond))                                                                \
			fail(__FILE__, __LINE__, std::format("ASSERT_TRUE({}) failed", #cond)); \
	} while (0)

/// Fail the test if `a != b`, printing both values
#define ASSERT_EQ(a, b)                                                            \
	do                                                                             \
	{                                                                              \
		using namespace testing::unit::detail;                                     \
		const auto &check_a_ = (a);                                                \
		const auto &check_b_ = (b);                                                \
		if (!(check_a_ == check_b_))                                               \
			fail(__FILE__, __LINE__,                                               \
				 std::format("ASSERT_EQ({}, {}) failed\n  left:  {}\n  right: {}", \
							 #a, #b, show(check_a_),                               \
							 show(check_b_)));                                     \
	} while (0)

/// Fail the test unless `expr` throws an exception of type `ex`
#define ASSERT_THROWS(expr, ex)                                                                \
	do                                                                                         \
	{                                                                                          \
		using namespace testing::unit::detail;                                                 \
		bool check_threw_ = false;                                                             \
		try                                                                                    \
		{                                                                                      \
			(void)(expr);                                                                      \
		}                                                                                      \
		catch (const ex &)                                                                     \
		{                                                                                      \
			check_threw_ = true;                                                               \
		}                                                                                      \
		if (!check_threw_)                                                                     \
			fail(__FILE__, __LINE__, std::format("ASSERT_THROWS({}, {}) failed", #expr, #ex)); \
	} while (0)
