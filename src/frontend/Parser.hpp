#pragma once

#include <memory>
#include <optional>

#include "frontend/AST.hpp"
#include "frontend/Lexer.hpp"

/// @brief Recursive descent parser, converts tokens into an AST
///
/// One function per rule in `grammar.ebnf`, named after the rule. Each returns
/// `std::nullopt` if the rule doesn't match at the current position (leaving the
/// lexer untouched), and throws a `CompilerError` if it matched but was malformed.
namespace parse
{
	// === TOP LEVEL ===========================================================

	std::optional<std::unique_ptr<ast::TopLevelDeclaration>> try_parse_top_level_decl(Lexer &lexer);
	std::optional<ast::FunctionDefinition> try_parse_function_definition(Lexer &lexer);
	std::optional<ast::ArgDefinition> try_parse_arg_definition(Lexer &lexer);

	// === STATEMENTS ==========================================================

	std::optional<std::unique_ptr<ast::StatementNode>> try_parse_stmt(Lexer &lexer);
	std::optional<std::unique_ptr<ast::ReturnStatement>> try_parse_return_stmt(Lexer &lexer);

	// === EXPRESSIONS =========================================================
	// Listed loosest binding first, matching the precedence chain in the grammar
	// in order of increasing precedence

	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_expr(Lexer &lexer);

	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_logical_or(Lexer &lexer);
	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_logical_and(Lexer &lexer);
	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_equality(Lexer &lexer);
	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_comparison(Lexer &lexer);
	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_bitwise_or(Lexer &lexer);
	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_bitwise_xor(Lexer &lexer);
	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_bitwise_and(Lexer &lexer);
	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_bitshift(Lexer &lexer);
	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_additive(Lexer &lexer);
	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_multiplicative(Lexer &lexer);
	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_unary(Lexer &lexer);
	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_postfix(Lexer &lexer);
	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_primary(Lexer &lexer);

	std::optional<std::unique_ptr<ast::IdentifierExpression>> try_parse_identifier(Lexer &lexer);

	std::optional<std::unique_ptr<ast::IntegerLiteralExpression>> try_parse_integer_literal(Lexer &lexer);
	std::optional<std::unique_ptr<ast::BooleanLiteralExpression>> try_parse_boolean_literal(Lexer &lexer);
}
