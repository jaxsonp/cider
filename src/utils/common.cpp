#include "common.hpp"

bool is_whitespace(char c)
{
	return (c >= 9 && c <= 13) || c == 32;
}

bool is_numeric(char c)
{
	return c >= '0' && c <= '9';
}

bool is_alpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_delimiter(char c)
{
	return !is_numeric(c) && !is_alpha(c) && c != '_';
}

std::string_view bool_str(bool b)
{
	return (b ? "true" : "false");
}
