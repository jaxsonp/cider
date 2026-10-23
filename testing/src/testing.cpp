#include "testing.hpp"

namespace testing
{
	Test::Test(std::string_view name, Category category)
		: name(name), category(category) {}
}