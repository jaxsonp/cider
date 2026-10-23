#include "unit_testing.hpp"
#include "frontend/Lexer.hpp"

#include <iostream>

TEST(poopygood)
{
	std::cout << "beep boop doing lexer tests" << std::endl;
}

TEST(poopybad)
{
	ASSERT_THROWS(std::cout << "This doesnt throw teehee" << std::endl, std::exception);
}

TEST(poopygood2)
{
	ASSERT_EQ(3, 1 + 2);
}