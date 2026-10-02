
#include <iostream>

#include "testing.hpp"

int main(int argc, char **argv)
{
	std::cout << "Hello from test runner" << std::endl;
	for (auto t : get_test_registry())
	{
		std::cout << "Test: " << t.name << std::endl;
	}
	return 0;
}