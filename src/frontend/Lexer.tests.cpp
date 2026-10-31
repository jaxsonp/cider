#include "unit_testing.hpp"
#include "frontend/Lexer.hpp"

#include <sstream>
#include <vector>

#include "utils/error.hpp"

/// Runs the lexer over a whole input, returning every token type up to (not including) EOF
static std::vector<TokenType> lex_types(const std::string &source)
{
	std::istringstream in(source);
	Lexer lexer(in);

	std::vector<TokenType> types;
	for (Token tok = lexer.take(); tok.type != TokenType::END_OF_FILE; tok = lexer.take())
		types.push_back(tok.type);
	return types;
}

/// Lexes an input that should be a single token, returning it
static Token lex_one(const std::string &source)
{
	std::istringstream in(source);
	Lexer lexer(in);
	Token tok = lexer.take();
	ASSERT_EQ(lexer.take().type, TokenType::END_OF_FILE);
	return tok;
}

using enum TokenType;
using Types = std::vector<TokenType>;

// tokens ========================================================================================================

TEST(single_char_tokens)
{
	const std::vector<std::pair<std::string, TokenType>> cases = {
		{";", SEMICOLON},
		{":", COLON},
		{"(", L_PAREN},
		{")", R_PAREN},
		{"[", L_SQR_BRACKET},
		{"]", R_SQR_BRACKET},
		{"{", L_CURLY_BRACKET},
		{"}", R_CURLY_BRACKET},
		{",", COMMA},
		{"&", AND},
		{"|", OR},
		{"^", CARET},
		{"~", TILDE},
		{"<", LESS},
		{">", GREATER},
		{"=", EQUAL},
		{"!", EXCLAMATION},
		{"+", PLUS},
		{"-", MINUS},
		{"*", ASTERISK},
		{"/", FORWARD_SLASH},
		{"%", PERCENT},
	};
	for (const auto &[source, expected] : cases)
		ASSERT_EQ(lex_one(source).type, expected);
}

TEST(two_char_tokens)
{
	const std::vector<std::pair<std::string, TokenType>> cases = {
		{"&&", AND_AND},
		{"||", OR_OR},
		{"<<", LESS_LESS},
		{">>", GREATER_GREATER},
		{"<=", LESS_EQUAL},
		{">=", GREATER_EQUAL},
		{"==", EQUAL_EQUAL},
		{"!=", EXCLAMATION_EQUAL},
		{"->", THIN_ARROW},
	};
	for (const auto &[source, expected] : cases)
		ASSERT_EQ(lex_one(source).type, expected);
}

TEST(longest_token_is_taken_first)
{
	ASSERT_TRUE(lex_types("&&&") == (Types{AND_AND, AND}));
	ASSERT_TRUE(lex_types("|||") == (Types{OR_OR, OR}));
	ASSERT_TRUE(lex_types("===") == (Types{EQUAL_EQUAL, EQUAL}));
	ASSERT_TRUE(lex_types("!==") == (Types{EXCLAMATION_EQUAL, EQUAL}));
	ASSERT_TRUE(lex_types("<<=") == (Types{LESS_LESS, EQUAL}));
	ASSERT_TRUE(lex_types(">>=") == (Types{GREATER_GREATER, EQUAL}));
	ASSERT_TRUE(lex_types("<<<") == (Types{LESS_LESS, LESS}));
	ASSERT_TRUE(lex_types("->>") == (Types{THIN_ARROW, GREATER}));
}

TEST(whitespace_splits_multi_char_tokens)
{
	ASSERT_TRUE(lex_types("& &") == (Types{AND, AND}));
	ASSERT_TRUE(lex_types("< <") == (Types{LESS, LESS}));
	ASSERT_TRUE(lex_types("= =") == (Types{EQUAL, EQUAL}));
	ASSERT_TRUE(lex_types("- >") == (Types{MINUS, GREATER}));
	ASSERT_TRUE(lex_types("! =") == (Types{EXCLAMATION, EQUAL}));
}

TEST(minus_is_not_part_of_a_literal)
{
	ASSERT_TRUE(lex_types("-1i32") == (Types{MINUS, INT_LITERAL}));
	ASSERT_TRUE(lex_types("1i32-1i32") == (Types{INT_LITERAL, MINUS, INT_LITERAL}));
	ASSERT_TRUE(lex_types("--1i32") == (Types{MINUS, MINUS, INT_LITERAL}));
}

// keywords and identifiers ======================================================================================

TEST(keywords)
{
	ASSERT_EQ(lex_one("fn").type, KEYWORD_FN);
	ASSERT_EQ(lex_one("return").type, KEYWORD_RETURN);
	ASSERT_EQ(lex_one("true").type, KEYWORD_TRUE);
	ASSERT_EQ(lex_one("false").type, KEYWORD_FALSE);
}

TEST(keywords_are_case_sensitive)
{
	for (const std::string &name : {"Fn", "FN", "Return", "True", "FALSE"})
		ASSERT_EQ(lex_one(name).type, IDENT);
}

TEST(identifier_containing_a_keyword_is_an_identifier)
{
	for (const std::string &name : {"fnord", "myfn", "fn_", "_fn", "returned", "truex", "falsey", "fn1"})
	{
		Token tok = lex_one(name);
		ASSERT_EQ(tok.type, IDENT);
		ASSERT_EQ(tok.str, name);
	}
}

TEST(identifier_characters)
{
	for (const std::string &name : {"a", "Z", "_", "__", "_a", "a_b", "a1", "A1_b2", "i32", "void"})
	{
		Token tok = lex_one(name);
		ASSERT_EQ(tok.type, IDENT);
		ASSERT_EQ(tok.str, name);
	}
}

TEST(keywords_have_no_string)
{
	ASSERT_EQ(lex_one("fn").str, "");
	ASSERT_EQ(lex_one("true").str, "");
}

// integer literals ==============================================================================================

TEST(integer_literal_keeps_its_whole_text)
{
	for (const std::string &text : {"0", "7", "100i32", "18446744073709551615u64", "0b101u8", "0o17u8", "0x10u8", "1_000i32"})
	{
		Token tok = lex_one(text);
		ASSERT_EQ(tok.type, INT_LITERAL);
		ASSERT_EQ(tok.str, text);
	}
}

TEST(literal_ends_at_a_delimiter)
{
	ASSERT_TRUE(lex_types("1i32;") == (Types{INT_LITERAL, SEMICOLON}));
	ASSERT_TRUE(lex_types("1i32)") == (Types{INT_LITERAL, R_PAREN}));
	ASSERT_TRUE(lex_types("1i32+2i32") == (Types{INT_LITERAL, PLUS, INT_LITERAL}));
}

TEST(identifier_then_number_needs_a_delimiter)
{
	ASSERT_TRUE(lex_types("a 1") == (Types{IDENT, INT_LITERAL}));
	ASSERT_TRUE(lex_types("1 a") == (Types{INT_LITERAL, IDENT}));
	ASSERT_EQ(lex_one("1a").type, INT_LITERAL);
}

// whitespace and comments =======================================================================================

TEST(empty_input_is_eof)
{
	ASSERT_TRUE(lex_types("").empty());
}

TEST(every_kind_of_whitespace_is_skipped)
{
	for (const std::string &space : {" ", "\t", "\n", "\r", "\v", "\f", "\r\n", " \t\n\r\v\f"})
	{
		ASSERT_TRUE(lex_types(space).empty());
		ASSERT_TRUE(lex_types(space + "a" + space + "b" + space) == (Types{IDENT, IDENT}));
	}
}

TEST(line_comment_is_skipped)
{
	ASSERT_TRUE(lex_types("// nothing\n").empty());
	ASSERT_TRUE(lex_types("a // b c\nd") == (Types{IDENT, IDENT}));
	ASSERT_TRUE(lex_types("// one\n// two\na") == (Types{IDENT}));
	ASSERT_TRUE(lex_types("a//b\n") == (Types{IDENT}));
}

TEST(comment_hides_tokens_until_end_of_line)
{
	ASSERT_TRUE(lex_types("// fn return ; @ # \n;") == (Types{SEMICOLON}));
	ASSERT_TRUE(lex_types("/// also a comment\n;") == (Types{SEMICOLON}));
}

TEST(single_slash_is_not_a_comment)
{
	ASSERT_TRUE(lex_types("a / b") == (Types{IDENT, FORWARD_SLASH, IDENT}));
	ASSERT_TRUE(lex_types("/ /") == (Types{FORWARD_SLASH, FORWARD_SLASH}));
}

// unexpected characters =========================================================================================

TEST(unexpected_character_becomes_an_error_token)
{
	for (const std::string &text : {"@", "#", "$", "`", "'", "\"", "\\", "?", "."})
	{
		Token tok = lex_one(text);
		ASSERT_EQ(tok.type, ERROR_UNEXPECTED_CHAR);
		ASSERT_EQ(tok.str, text);
	}
}

TEST(lexing_continues_after_an_unexpected_character)
{
	ASSERT_TRUE(lex_types("a @ b") == (Types{IDENT, ERROR_UNEXPECTED_CHAR, IDENT}));
}

// locations =====================================================================================================

TEST(location_of_a_token)
{
	std::istringstream in("  abc");
	Lexer lexer(in);
	Token tok = lexer.take();
	ASSERT_EQ(tok.loc.start.line, 0u);
	ASSERT_EQ(tok.loc.start.col, 2u);
	ASSERT_EQ(tok.loc.end.line, 0u);
	ASSERT_EQ(tok.loc.end.col, 5u);
}

TEST(location_of_a_multi_char_token_covers_all_of_it)
{
	std::istringstream in("<<");
	Lexer lexer(in);
	Token tok = lexer.take();
	ASSERT_EQ(tok.loc.start.col, 0u);
	ASSERT_EQ(tok.loc.end.col, 2u);
}

TEST(newlines_advance_the_line_and_reset_the_column)
{
	std::istringstream in("a\n  b\n\nc");
	Lexer lexer(in);

	Token a = lexer.take();
	ASSERT_EQ(a.loc.start.line, 0u);
	ASSERT_EQ(a.loc.start.col, 0u);

	Token b = lexer.take();
	ASSERT_EQ(b.loc.start.line, 1u);
	ASSERT_EQ(b.loc.start.col, 2u);

	Token c = lexer.take();
	ASSERT_EQ(c.loc.start.line, 3u);
	ASSERT_EQ(c.loc.start.col, 0u);
}

TEST(comments_count_towards_lines)
{
	std::istringstream in("// one\n// two\nx");
	Lexer lexer(in);
	Token tok = lexer.take();
	ASSERT_EQ(tok.loc.start.line, 2u);
	ASSERT_EQ(tok.loc.start.col, 0u);
}

TEST(location_of_eof)
{
	std::istringstream in("a\nb ");
	Lexer lexer(in);
	lexer.take();
	lexer.take();
	Token eof = lexer.take();
	ASSERT_EQ(eof.type, END_OF_FILE);
	ASSERT_EQ(eof.loc.start.line, 1u);
	ASSERT_EQ(eof.loc.start.col, 2u);
}

// take, peek and expect =========================================================================================

TEST(peek_does_not_consume)
{
	std::istringstream in("a b");
	Lexer lexer(in);
	ASSERT_EQ(lexer.peek().str, "a");
	ASSERT_EQ(lexer.peek().str, "a");
	ASSERT_EQ(lexer.take().str, "a");
	ASSERT_EQ(lexer.take().str, "b");
}

TEST(peek_keeps_the_token_location)
{
	std::istringstream in(" a");
	Lexer lexer(in);
	Token peeked = lexer.peek();
	Token taken = lexer.take();
	ASSERT_TRUE(peeked.loc == taken.loc);
}

TEST(eof_repeats_forever)
{
	std::istringstream in("a");
	Lexer lexer(in);
	lexer.take();
	for (int i = 0; i < 5; ++i)
		ASSERT_EQ(lexer.take().type, END_OF_FILE);
	ASSERT_EQ(lexer.peek().type, END_OF_FILE);
}

TEST(is_done_after_reading_past_the_end)
{
	std::istringstream in("a");
	Lexer lexer(in);
	ASSERT_TRUE(!lexer.is_done());
	lexer.take();
	lexer.take();
	ASSERT_TRUE(lexer.is_done());
}

TEST(expect_returns_a_matching_token)
{
	std::istringstream in("a;");
	Lexer lexer(in);
	ASSERT_EQ(lexer.expect(IDENT).str, "a");
	ASSERT_EQ(lexer.expect(SEMICOLON).type, SEMICOLON);
}

TEST(expect_throws_a_syntax_error_on_a_mismatch)
{
	std::istringstream in("a");
	Lexer lexer(in);
	try
	{
		lexer.expect(SEMICOLON);
	}
	catch (const CompilerError &err)
	{
		ASSERT_EQ(int(err.exit_code()), int(ExitCode::SyntaxError));
		return;
	}
	ASSERT_TRUE(false);
}

TEST(expect_consumes_the_mismatched_token)
{
	std::istringstream in("a b");
	Lexer lexer(in);
	ASSERT_THROWS(lexer.expect(SEMICOLON), CompilerError);
	ASSERT_EQ(lexer.take().str, "b");
}

// to_string =====================================================================================================

TEST(token_type_names)
{
	ASSERT_EQ(to_string(END_OF_FILE), "EOF");
	ASSERT_EQ(to_string(IDENT), "identifier");
	ASSERT_EQ(to_string(INT_LITERAL), "integer literal");
	ASSERT_EQ(to_string(KEYWORD_FN), "keyword \"fn\"");
	ASSERT_EQ(to_string(KEYWORD_RETURN), "keyword \"return\"");
	ASSERT_EQ(to_string(THIN_ARROW), "\"->\"");
	ASSERT_EQ(to_string(LESS_EQUAL), "\"<=\"");
	ASSERT_EQ(to_string(ERROR_UNEXPECTED_CHAR), "Unexpected char");
}

TEST(token_names_include_their_text)
{
	ASSERT_EQ(to_string(lex_one("abc")), "identifier \"abc\"");
	ASSERT_EQ(to_string(lex_one("12i32")), "integer literal \"12i32\"");
	ASSERT_EQ(to_string(lex_one("@")), "unexpected \"@\"");
	ASSERT_EQ(to_string(lex_one(";")), "\";\"");
	ASSERT_EQ(to_string(lex_one("fn")), "keyword \"fn\"");
}
