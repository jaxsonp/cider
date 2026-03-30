#pragma once

#include <string>
#include "backend/Target.hpp"

struct CompileSettings
{
	std::string output_filename;
	Target target = Target::supported_targets.find("linux-riscv32g")->second;
	bool debug_print_asm = false;
};

void compile(const std::string &, const CompileSettings &);
