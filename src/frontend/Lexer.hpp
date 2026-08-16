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
	KEYWORD_TRUE,
	KEYWORD_FALSE,
	KEYWORD_FN,
	KEYWORD_RETURN,
	COLON,
	SEMICOLON,
	L_PAREN,
	R_PAREN,
	L_SQR_BRACKET,
	R_SQR_BRACKET,
	L_CURLY_BRACKET,
	R_CURLY_BRACKET,
	COMMA,
	THIN_ARROW,
	AND,
	AND_AND,
	OR,
	OR_OR,
	CARET,
	LESS_LESS,
	GREATER_GREATER,
	EQUAL_EQUAL,
	EXCLAMATION_EQUAL,
	LESS,
	LESS_EQUAL,
	GREATER,
	GREATER_EQUAL,
	PLUS,
	MINUS,
	ASTERISK,
	FORWARD_SLASH,
	PERCENT,
	EXCLAMATION,
	EQUAL,
	TILDE,
	ERROR_UNEXPECTED_CHAR,
};

std::string to_string(TokenType type);

struct Token
{
	SourceLocRange loc;
	std::string str;
	TokenType type;
};

std::string to_string(Token tok);

/// @brief Lexer, converts characters to tokens on demand. Infinitely returns EOF token when completed.
class Lexer
{
	std::istream &in;
	bool done;
	std::optional<Token> peeked_token;

	/// @brief Consume the next character from the stream and return it
	char take_char();
	/// @brief Return the next character in the stream WITHOUT consuming it
	char peek_char();

public:
	SourceLoc pos;

	Lexer(std::istream &in);

	/// @brief Consumes and returns the next token
	Token take();

	/// @brief Returns the next token without consuming it
	Token peek();

	/// @brief Consumes and returns the next token if it matches the expected type, throwing a `CompileError` if not
	/// @param expected_type Token type to expect
	/// @return Token consumed
	Token expect(TokenType expected_type);

	inline bool is_done() const noexcept { return done; }
};