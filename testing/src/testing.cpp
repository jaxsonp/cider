#include "testing.hpp"

#include <format>

namespace testing
{
	Test::Test(std::string_view name, Category category)
		: name(name), category(category) {}

	std::string Test::label() const
	{
		return std::format("[{}] {}", to_string(this->category), this->name);
	}

	std::string_view to_string(Test::Category category)
	{
		switch (category)
		{
		case Test::Category::Unit:
			return "unit";
		case Test::Category::EndToEnd:
			return "e2e";
		}
		return "???";
	}
}