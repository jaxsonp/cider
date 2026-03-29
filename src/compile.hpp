#pragma once

#include <string>

struct CompileSettings
{
	std::string input_filename;
	std::string output_filename;
	bool debug_print_asm = false;
};

void compile(const std::string &, const CompileSettings &);
