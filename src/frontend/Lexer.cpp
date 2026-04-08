#include "Lexer.hpp"

#include <stdexcept>

#include "utils/error.hpp"

char Lexer::take_char()
{
	if (done)
		return std::char_traits<char>::eof();

	int next = in.get();
	char out = static_cast<char>(next);
	if (next == std::char_traits<char>::eof())
	{
		done = true;
		return std::char_traits<char>::eof();
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
	return static_cast<char>(in.peek());
}

Lexer::Lexer(std::istream &in_stream) : in(in_stream), done(false)
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
	tok.loc.start = pos;

	char c = take_char();

	switch (c)
	{
	case std::char_traits<char>::eof():
		tok.type = TokenType::END_OF_FILE;
		break;
	case ';':
		tok.type = TokenType::SEMICOLON;
		break;
	case '(':
		tok.type = TokenType::L_PAREN;
		break;
	case ')':
		tok.type = TokenType::R_PAREN;
		break;
	case '[':
		tok.type = TokenType::L_SQR_BRACKET;
		break;
	case ']':
		tok.type = TokenType::R_SQR_BRACKET;
		break;
	case '{':
		tok.type = TokenType::L_CURLY_BRACKET;
		break;
	case '}':
		tok.type = TokenType::R_CURLY_BRACKET;
		break;
	case ',':
		tok.type = TokenType::COMMA;
		break;
	case '&':
		if (peek_char() == '&')
		{
			take_char();
			tok.type = TokenType::AND_AND;
		}
		else
		{
			tok.type = TokenType::AND;
		}
		break;
	case '|':
		if (peek_char() == '|')
		{
			take_char();
			tok.type = TokenType::OR_OR;
		}
		else
		{
			tok.type = TokenType::OR;
		}
		break;
	case '^':
		tok.type = TokenType::CARET;
		break;
	case '<':
		if (peek_char() == '<')
		{
			take_char();
			tok.type = TokenType::LESS_LESS;
		}
		else if (peek_char() == '=')
		{
			take_char();
			tok.type = TokenType::LESS_EQUAL;
		}
		else
		{
			tok.type = TokenType::LESS;
		}
		break;
	case '>':
		if (peek_char() == '>')
		{
			take_char();
			tok.type = TokenType::GREATER_GREATER;
		}
		else if (peek_char() == '=')
		{
			take_char();
			tok.type = TokenType::GREATER_EQUAL;
		}
		else
		{
			tok.type = TokenType::GREATER;
		}
		break;
	case '=':
		if (peek_char() == '=')
		{
			take_char();
			tok.type = TokenType::EQUAL_EQUAL;
		}
		else
		{
			tok.type = TokenType::EQUAL;
		}
		break;
	case '!':
		if (peek_char() == '=')
		{
			take_char();
			tok.type = TokenType::EXCLAMATION_EQUAL;
		}
		else
		{
			tok.type = TokenType::EXCLAMATION;
		}
		break;
	case '+':
		tok.type = TokenType::PLUS;
		break;
	case '-':
		if (peek_char() == '>')
		{
			take_char(); // consume arrow head
			tok.type = TokenType::THIN_ARROW;
		}
		else
		{
			tok.type = TokenType::MINUS;
		}
		break;
	case '*':
		tok.type = TokenType::ASTERISK;
		break;
	case '/':
		if (peek_char() == '/')
		{
			// line comment, consume till newline
			while (take_char() != '\n')
				;
			// next token
			return this->take(); // TODO is this bad? possible stack overflow?
		}
		else
		{
			tok.type = TokenType::FORWARD_SLASH;
		}
		break;
	case '%':
		tok.type = TokenType::PERCENT;
		break;
	default:
		if (is_whitespace(c))
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
				tok.str = token_str;
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
				tok.str = token_str;
			}
		}
		else
		{
			tok.type = TokenType::ERROR_UNEXPECTED_CHAR;
			tok.str = std::string(1, c);
		}
		break;
	}

	tok.loc.end = pos;
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
		throw CompilerError::syntax_error(std::format("Unexpected token: {}, expected {}", to_string(tok), to_string(expected_type)), tok.loc);
	return tok;
}

std::string to_string(TokenType type)
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
	case TokenType::L_SQR_BRACKET:
		return "\"[\"";
	case TokenType::R_SQR_BRACKET:
		return "\"]\"";
	case TokenType::L_CURLY_BRACKET:
		return "\"{\"";
	case TokenType::R_CURLY_BRACKET:
		return "\"}\"";
	case TokenType::COMMA:
		return "\",\"";
	case TokenType::THIN_ARROW:
		return "\"->\"";
	case TokenType::AND:
		return "\"&\"";
	case TokenType::AND_AND:
		return "\"&&\"";
	case TokenType::OR:
		return "\"|\"";
	case TokenType::OR_OR:
		return "\"||\"";
	case TokenType::CARET:
		return "\"^\"";
	case TokenType::LESS_LESS:
		return "\"<<\"";
	case TokenType::GREATER_GREATER:
		return "\">>\"";
	case TokenType::EQUAL_EQUAL:
		return "\"==\"";
	case TokenType::EXCLAMATION_EQUAL:
		return "\"!=\"";
	case TokenType::LESS:
		return "\"<\"";
	case TokenType::LESS_EQUAL:
		return "\"<=\"";
	case TokenType::GREATER:
		return "\">\"";
	case TokenType::GREATER_EQUAL:
		return "\">=\"";
	case TokenType::PLUS:
		return "\"+\"";
	case TokenType::MINUS:
		return "\"-\"";
	case TokenType::ASTERISK:
		return "\"*\"";
	case TokenType::FORWARD_SLASH:
		return "\"/\"";
	case TokenType::PERCENT:
		return "\"%\"";
	case TokenType::EXCLAMATION:
		return "\"!\"";
	case TokenType::EQUAL:
		return "\"=\"";
	case TokenType::ERROR_UNEXPECTED_CHAR:
		return "Unexpected char";
	}

	throw CompilerError::unimplemented("Unhandled token type (in TokenType::to_string)");
}

std::string to_string(Token tok)
{
	switch (tok.type)
	{
	case TokenType::IDENT:
		return std::format("identifier \"{}\"", tok.str);
	case TokenType::INT_LITERAL:
		return std::format("integer literal \"{}\"", tok.str);
	case TokenType::KEYWORD_FN:
		return "keyword \"fn\"";
	case TokenType::KEYWORD_RETURN:
		return "keyword \"return\"";
	case TokenType::ERROR_UNEXPECTED_CHAR:
		return std::format("unexpected \"{}\"", tok.str);
	}
	return to_string(tok.type);
}
