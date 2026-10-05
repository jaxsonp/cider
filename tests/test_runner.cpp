
#include <iostream>

#include "testing.hpp"
#include "utils/CliParser.hpp"

int main(int argc, char **argv)
{
	std::cout << "Hello from test runner!" << std::endl;
	for (auto t : get_test_registry())
	{
		std::cout << "\n --- Running test \"" << t.name << "\" ---" << std::endl;
		t.fn();
		std::cout << "\n --- Test \"" << t.name << "\" complete ---" << std::endl;
	}
	return 0;
}