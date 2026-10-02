#include "testing.hpp"

std::vector<Test> &get_test_registry()
{
	static std::vector<Test> tests;
	return tests;
}