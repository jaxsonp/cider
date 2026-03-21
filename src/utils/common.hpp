#pragma once

#include <string>

struct SourceLoc
{
	unsigned int line = 0;
	unsigned int col = 0;
};

bool is_whitespace(char c);

bool is_numeric(char c);

bool is_alpha(char c);

bool is_delimiter(char c);

std::string_view bool_str(bool b);
