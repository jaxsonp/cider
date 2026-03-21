#include "Lexer.hpp"

#include "utils/error.hpp"

#include <utility>

char Lexer::take_char()
{
	if (done)
		return std::char_traits<char>::eof();

	char out;
	if (peeked_char.has_value())
	{
		out = peeked_char.value();
		peeked_char = std::nullopt;
	}
	else
	{
		int next = in.get();
		if (next == std::char_traits<char>::eof())
		{
			done = true;
			return next;
		}
		out = static_cast<char>(next);
	}

	if (out == '\n')
	{
		pos.col = 0;
		pos.line++;
	}
	else
	{
		pos.col++;
	}

	return out;
}

char Lexer::peek_char()
{
	if (done)
		return std::char_traits<char>::eof();

	if (!peeked_char.has_value())
	{
		int next = in.get();
		peeked_char = static_cast<char>(next);
	}
	return peeked_char.value();
}

Lexer::Lexer(std::istream &in_stream) : in(in_stream), peeked_char(std::nullopt), done(false)
{
	pos.line = 0;
	pos.col = 0;
}

Token Lexer::take()
{
	if (peeked_token.has_value())
	{
		Token tok = peeked_token.value();
		peeked_token = std::nullopt;
		return tok;
	}

	Token tok;
	tok.start = pos;

	char c = take_char();

	if (c == std::char_traits<char>::eof())
	{
		tok.type = TokenType::END_OF_FILE;
	}
	else if (c == ';')
	{
		tok.type = TokenType::SEMICOLON;
	}
	else if (c == '(')
	{
		tok.type = TokenType::L_PAREN;
	}
	else if (c == ')')
	{
		tok.type = TokenType::R_PAREN;
	}
	else if (c == '[')
	{
		tok.type = TokenType::L_BRACE;
	}
	else if (c == ']')
	{
		tok.type = TokenType::R_BRACE;
	}
	else if (c == '{')
	{
		tok.type = TokenType::L_BRACKET;
	}
	else if (c == '}')
	{
		tok.type = TokenType::R_BRACKET;
	}
	else if (c == ',')
	{
		tok.type = TokenType::COMMA;
	}
	else if (c == '-')
	{
		if (peek_char() == '>')
		{
			take_char(); // consume arrow head
			tok.type = TokenType::ARROW;
		}
		else
		{
			// TODO
		}
	}
	else if (is_whitespace(c))
	{
		while (is_whitespace(peek_char()))
		{
			take_char();
		}
		return take();
	}
	else if (is_numeric(c) || is_alpha(c) || c == '_')
	{
		std::string token_str;
		token_str += c;

		while (!is_delimiter(peek_char()))
		{
			token_str += take_char();
		}

		if (is_numeric(token_str[0]))
		{
			tok.type = TokenType::INT_LITERAL;
			tok.token_str = token_str;
		}
		else if (token_str == "fn")
		{
			tok.type = TokenType::KEYWORD_FN;
		}
		else if (token_str == "return")
		{
			tok.type = TokenType::KEYWORD_RETURN;
		}
		else
		{
			tok.type = TokenType::IDENT;
			tok.token_str = token_str;
		}
	}
	else
	{
		tok.type = TokenType::ERROR_UNEXPECTED_CHAR;
		tok.token_str = std::string(1, c);
	}

	tok.end = pos;
	return tok;
}

Token Lexer::peek()
{
	if (!peeked_token.has_value())
	{
		peeked_token = take();
	}
	return peeked_token.value();
}

Token Lexer::expect(TokenType expected_type)
{
	Token tok = this->take();
	if (tok.type != expected_type)
		throw CompileError(std::format("Unexpected token: {}, expected {}", toString(tok), toString(expected_type)));
	return tok;
}

std::string toString(TokenType type)
{
	switch (type)
	{
	case TokenType::END_OF_FILE:
		return "EOF";
	case TokenType::IDENT:
		return "identifier";
	case TokenType::INT_LITERAL:
		return "integer literal";
	case TokenType::KEYWORD_FN:
		return "keyword \"fn\"";
	case TokenType::KEYWORD_RETURN:
		return "keyword \"return\"";
	case TokenType::COLON:
		return "\":\"";
	case TokenType::SEMICOLON:
		return "\";\"";
	case TokenType::L_PAREN:
		return "\"(\")";
	case TokenType::R_PAREN:
		return "\")\"";
	case TokenType::L_BRACE:
		return "\"[\"";
	case TokenType::R_BRACE:
		return "\"]\"";
	case TokenType::L_BRACKET:
		return "\"{\"";
	case TokenType::R_BRACKET:
		return "\"}\"";
	case TokenType::COMMA:
		return "\",\"";
	case TokenType::ARROW:
		return "\"->\"";
	case TokenType::ERROR_UNEXPECTED_CHAR:
		return "Unexpected char";
	}

	throw std::logic_error("Unhandled token type");
}

std::string toString(Token tok)
{
	switch (tok.type)
	{
	case TokenType::IDENT:
		return std::format("identifier \"{}\"", tok.token_str);
	case TokenType::INT_LITERAL:
		return std::format("integer literal \"{}\"", tok.token_str);
	case TokenType::KEYWORD_FN:
		return "keyword \"fn\"";
	case TokenType::KEYWORD_RETURN:
		return "keyword \"return\"";
	case TokenType::ERROR_UNEXPECTED_CHAR:
		return std::format("unexpected \"{}\"", tok.token_str);
	}
	return toString(tok.type);
}
