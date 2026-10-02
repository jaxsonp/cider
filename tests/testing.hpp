#pragma once

#include <string>
#include <stdexcept>
#include <vector>

#define TEST(name)                                        \
	void name();                                          \
	struct test_##name##_registrar                        \
	{                                                     \
		test_##name##_registrar()                         \
		{                                                 \
			get_test_registry().push_back({#name, name}); \
		}                                                 \
	};                                                    \
	static test_##name##_registrar g_test_##name;         \
	void name()

struct TestFailure : std::runtime_error
{
};

using TestFn = void (*)();

struct Test
{
	const char *name;
	TestFn fn;
};

std::vector<Test> &get_test_registry();