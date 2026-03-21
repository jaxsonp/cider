#pragma once

#include <istream>
#include <optional>
#include <string>
#include <format>

#include "utils/common.hpp"

enum class TokenType
{
	END_OF_FILE,
	IDENT,
	INT_LITERAL,
	KEYWORD_FN,
	KEYWORD_RETURN,
	COLON,
	SEMICOLON,
	L_PAREN,
	R_PAREN,
	L_BRACE,
	R_BRACE,
	L_BRACKET,
	R_BRACKET,
	COMMA,
	ARROW,
	ERROR_UNEXPECTED_CHAR,
};

std::string toString(TokenType type);

struct Token
{
	SourceLoc start;
	SourceLoc end;
	std::string token_str;
	TokenType type;
};

std::string toString(Token tok);

class Lexer
{
public:
	SourceLoc pos;

	Lexer(std::istream &in);
	/// Consumes and returns the next token
	Token take();
	/// Returns the next token without consuming it
	Token peek();
	/// Consumes and returns the next token if it matches the expected type, throwing a `CompileError` if not
	Token expect(TokenType expected_type);
	bool is_done() const { return done; }

private:
	std::istream &in;
	std::optional<char> peeked_char;
	bool done;
	std::optional<Token> peeked_token;

	char take_char();
	char peek_char();
};