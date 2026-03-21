#include <fstream>
#include <iostream>
#include <cstring>

#include "utils/common.hpp"
#include "utils/logging.hpp"
#include "utils/error.hpp"

#include "Lexer.hpp"
#include "AST.hpp"

void compile(const std::string &);

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		std::cerr << "Usage: sasc-compiler <file>" << std::endl;
		return 1;
	}

	for (int i = 2; i < argc; ++i)
	{
		// TODO improve this
		if (!strcmp(argv[i], "-v"))
			settings::log_verbosity += 1;
		else if (!strcmp(argv[i], "--print-ast"))
			settings::print_ast = true;
	}
	log_vv("verbosity: {}", settings::log_verbosity);
	log_vv("print ast: {}", bool_str(settings::print_ast));

	std::string filename = argv[1];
	log_v("input file: {:s}", filename.c_str());

	try
	{
		compile(filename);
	}
	catch (CompileError e)
	{
		std::cerr << "\n\nCompilation failed: " << e.what() << "\n";
		return 1;
	}
	catch (std::exception e)
	{
		std::cerr << "\n\nException occured: " << e.what() << "\n";
		return 2;
	}

	return 0;
}

void compile(const std::string &filename)
{
	log_v("starting compilation");
	std::ifstream file(filename);
	if (!file.is_open())
		throw std::runtime_error(std::format("Failed to open file \"{}\"", filename));

	log_vv("File opened");

	log_vv("Initializing lexer");
	Lexer lexer(file);

	log_vv("Parsing AST");
	AST ast(lexer);

	if (settings::print_ast)
	{
		log_vv("Printing AST");
		ast.debug_print();
	}

	log_v("compilation complete");
}