#include "unit_testing.hpp"
#include "frontend/Lexer.hpp"

#include <iostream>

TEST(poopygood)
{
	ASSERT_TRUE(false);
}

TEST(poopybad)
{
}

TEST(poopygood2)
{
	ASSERT_EQ(3, 1 + 2);
}