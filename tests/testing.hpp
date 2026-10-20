#pragma once

#include <format>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <string>
#include <vector>

enum class TestCategory
{
	Unit,
	End2End,
};

std::string_view to_string(TestCategory category);

struct Test
{
	/// Grouping for reporting, e.g. the source file's stem for unit tests or the
	/// fixture's directory for e2e tests. Full name is "<group>/<name>"
	std::string group;
	std::string name;
	TestCategory category;
	std::function<void()> fn;

	[[nodiscard]] std::string full_name() const { return this->group + "/" + this->name; }
};

/// Every test known to the runner. Statically defined tests (TEST) add themselves
/// before main, generated ones (e2e fixtures) are added by the runner at startup
std::vector<Test> &get_test_registry();

/// Registers a test, returns a dummy so it can initialize a static
bool register_test(Test test);

/// Group name for a statically defined test: the name of the file it's defined in, minus
/// the ".tests.cpp"
std::string group_from_file(std::string_view file);

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

/// Macro for defining an in-source unit test
#define TEST(name)                                                               \
	static void test_fn_##name();                                                \
	static const bool test_reg_##name = register_test(                           \
		{group_from_file(__FILE__), #name, TestCategory::Unit, test_fn_##name}); \
	static void test_fn_##name()

// assertions

namespace testing_detail
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

	[[noreturn]] void fail(const char *file, int line, const std::string &msg);
}

/// Fail the test if `cond` is false
#define ASSERT_TRUE(cond)                                                                           \
	do                                                                                              \
	{                                                                                               \
		if (!(cond))                                                                                \
			testing_detail::fail(__FILE__, __LINE__, std::format("ASSERT_TRUE({}) failed", #cond)); \
	} while (0)

/// Fail the test if `a != b`, printing both values
#define ASSERT_EQ(a, b)                                                                            \
	do                                                                                             \
	{                                                                                              \
		const auto &check_a_ = (a);                                                                \
		const auto &check_b_ = (b);                                                                \
		if (!(check_a_ == check_b_))                                                               \
			testing_detail::fail(__FILE__, __LINE__,                                               \
								 std::format("ASSERT_EQ({}, {}) failed\n  left:  {}\n  right: {}", \
											 #a, #b, testing_detail::show(check_a_),               \
											 testing_detail::show(check_b_)));                     \
	} while (0)

/// Fail the test unless `expr` throws an exception of type `ex`
#define ASSERT_THROWS(expr, ex)                                                                                \
	do                                                                                                         \
	{                                                                                                          \
		bool check_threw_ = false;                                                                             \
		try                                                                                                    \
		{                                                                                                      \
			(void)(expr);                                                                                      \
		}                                                                                                      \
		catch (const ex &)                                                                                     \
		{                                                                                                      \
			check_threw_ = true;                                                                               \
		}                                                                                                      \
		if (!check_threw_)                                                                                     \
			testing_detail::fail(__FILE__, __LINE__, std::format("ASSERT_THROWS({}, {}) failed", #expr, #ex)); \
	} while (0)
