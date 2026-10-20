#include "testing.hpp"
#include "frontend/Lexer.hpp"

#include <iostream>

TEST(poopy)
{
	std::cout << "beep boop doing lexer tests" << std::endl;
}

TEST(poopy2)
{
	ASSERT_EQ(3, 1 + 2);
}

TEST(poopy3)
{
	ASSERT_THROWS(std::cout << "This doesnt throw teehee" << std::endl, std::exception);
}