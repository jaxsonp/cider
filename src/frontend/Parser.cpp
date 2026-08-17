#include "frontend/Parser.hpp"

#include <format>
#include <utility>

#include "utils/error.hpp"
#include "utils/logging.hpp"
#include "Parser.hpp"

namespace parse
{
	// TOP LEVEL ===============================================================

	std::optional<std::unique_ptr<ast::TopLevelDeclaration>> try_parse_top_level_decl(Lexer &lexer)
	{
		if (auto function_def = try_parse_function_definition(lexer))
		{
			return std::make_unique<ast::FunctionDefinition>(std::move(function_def.value()));
		}

		return std::nullopt;
	}

	std::optional<ast::FunctionDefinition> try_parse_function_definition(Lexer &lexer)
	{
		// fn keyword
		if (lexer.peek().type != TokenType::KEYWORD_FN)
			return std::nullopt;

		SourceLoc start = lexer.take().loc.start;

		// name
		Token name_tok = lexer.expect(TokenType::IDENT);

		// args
		lexer.expect(TokenType::L_PAREN);
		std::vector<ast::ArgDefinition> args;
		while (true)
		{
			if (auto arg = try_parse_arg_definition(lexer))
			{
				args.push_back(arg.value());
				if (lexer.peek().type == TokenType::COMMA)
				{
					lexer.take();
					continue;
				}
				else if (lexer.peek().type == TokenType::R_PAREN)
					lexer.take();
			}
			else if (lexer.peek().type == TokenType::R_PAREN)
				lexer.take();

			break;
		}

		// return type
		FrontendType return_type = FrontendType::void_type();
		if (lexer.peek().type == TokenType::THIN_ARROW)
		{
			lexer.take();
			Token type_tok = lexer.expect(TokenType::IDENT);
			return_type = FrontendType::from_string(type_tok.str);
		}

		// body
		lexer.expect(TokenType::L_CURLY_BRACKET);
		std::vector<std::unique_ptr<ast::StatementNode>> body_statements;
		while (true)
		{
			if (auto statement = try_parse_stmt(lexer))
			{
				body_statements.emplace_back(std::move(statement.value()));
				continue;
			}
			break;
		}

		// optional return expression
		std::optional<std::unique_ptr<ast::ExpressionNode>> return_expr = std::nullopt;
		if (auto parsed_return_expr = try_parse_expr(lexer))
		{
			return_expr = std::move(parsed_return_expr.value());
		}

		SourceLoc end = lexer.expect(TokenType::R_CURLY_BRACKET).loc.end;

		return ast::FunctionDefinition(SourceLocRange{start, end}, std::move(name_tok.str), std::move(args), return_type, std::move(body_statements), std::move(return_expr));
	}

	std::optional<ast::ArgDefinition> try_parse_arg_definition(Lexer &lexer)
	{
		if (lexer.peek().type != TokenType::IDENT)
			return std::nullopt;

		Token name_tok = lexer.take();
		lexer.expect(TokenType::COLON);
		Token type_tok = lexer.expect(TokenType::IDENT);
		return ast::ArgDefinition(
			SourceLocRange{name_tok.loc.start, type_tok.loc.end},
			std::move(name_tok.str),
			FrontendType::from_string(type_tok.str));
	}

	// STATEMENTS ==============================================================

	std::optional<std::unique_ptr<ast::StatementNode>> try_parse_stmt(Lexer &lexer)
	{
		if (auto statement = try_parse_return_stmt(lexer))
		{
			return std::move(statement.value());
		}
		return std::nullopt;
	}

	std::optional<std::unique_ptr<ast::ReturnStatement>> try_parse_return_stmt(Lexer &lexer)
	{
		if (lexer.peek().type != TokenType::KEYWORD_RETURN)
			return std::nullopt;

		SourceLoc start = lexer.take().loc.start;

		if (lexer.peek().type == TokenType::SEMICOLON)
		{
			SourceLoc end = lexer.take().loc.end;
			return std::make_unique<ast::ReturnStatement>(SourceLocRange{start, end});
		}

		SourceLoc expr_start = lexer.peek().loc.start;
		std::unique_ptr<ast::ExpressionNode> expr;
		if (auto parsed_expr = try_parse_expr(lexer))
			expr = std::move(parsed_expr.value());
		else
			throw CompilerError::syntax_error("Expected expression", expr_start);

		SourceLoc end = lexer.expect(TokenType::SEMICOLON).loc.end;

		return std::make_unique<ast::ReturnStatement>(SourceLocRange{start, end}, std::move(expr));
	}

	// EXPRESSIONS =============================================================

	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_expr(Lexer &lexer)
	{
		auto maybe_expr = try_parse_logical_or(lexer);
		if (maybe_expr.has_value())
			return std::move(maybe_expr.value());
		return std::nullopt;
	}

	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_logical_or(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = try_parse_logical_and(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		// check for operator
		if (lexer.peek().type != TokenType::OR_OR)
			return std::move(maybe_l_expr.value());
		Token op_tok = lexer.take();

		std::unique_ptr<ast::ExpressionNode> l_expr = std::move(maybe_l_expr.value());

		// parse right hand expression
		auto maybe_r_expr = try_parse_logical_and(lexer);
		if (!maybe_r_expr.has_value())
			throw CompilerError::syntax_error("Expected expression following " + to_string(op_tok), op_tok.loc.end);
		std::unique_ptr<ast::ExpressionNode> r_expr = std::move(maybe_r_expr.value());

		SourceLocRange src_loc{l_expr->src_loc.start, r_expr->src_loc.end};
		return std::make_unique<ast::BinaryExpression>(src_loc, std::move(l_expr), std::move(r_expr), ast::BinaryExpression::BinaryOperation::LogicalOr);
	}

	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_logical_and(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = try_parse_equality(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		// check for operator
		if (lexer.peek().type != TokenType::AND_AND)
			return std::move(maybe_l_expr.value());
		Token op_tok = lexer.take();

		std::unique_ptr<ast::ExpressionNode> l_expr = std::move(maybe_l_expr.value());

		// parse right hand expression
		auto maybe_r_expr = try_parse_equality(lexer);
		if (!maybe_r_expr.has_value())
			throw CompilerError::syntax_error("Expected expression following " + to_string(op_tok), op_tok.loc.end);
		std::unique_ptr<ast::ExpressionNode> r_expr = std::move(maybe_r_expr.value());

		SourceLocRange src_loc{l_expr->src_loc.start, r_expr->src_loc.end};
		return std::make_unique<ast::BinaryExpression>(src_loc, std::move(l_expr), std::move(r_expr), ast::BinaryExpression::BinaryOperation::LogicalAnd);
	}

	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_equality(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = try_parse_comparison(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		// check for operator
		if (lexer.peek().type != TokenType::EQUAL_EQUAL && lexer.peek().type != TokenType::EXCLAMATION_EQUAL)
			return std::move(maybe_l_expr.value());
		Token op_tok = lexer.take();

		auto operation = op_tok.type == TokenType::EXCLAMATION_EQUAL
							 ? ast::BinaryExpression::BinaryOperation::NotEqual
							 : ast::BinaryExpression::BinaryOperation::Equal;
		std::unique_ptr<ast::ExpressionNode> l_expr = std::move(maybe_l_expr.value());

		// parse right hand expression
		auto maybe_r_expr = try_parse_comparison(lexer);
		if (!maybe_r_expr.has_value())
			throw CompilerError::syntax_error("Expected expression following " + to_string(op_tok), op_tok.loc.end);
		std::unique_ptr<ast::ExpressionNode> r_expr = std::move(maybe_r_expr.value());

		SourceLocRange src_loc{l_expr->src_loc.start, r_expr->src_loc.end};
		return std::make_unique<ast::BinaryExpression>(src_loc, std::move(l_expr), std::move(r_expr), operation);
	}

	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_comparison(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = try_parse_bitwise_or(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;
		auto l_expr = std::move(maybe_l_expr.value());

		// check for operator
		ast::BinaryExpression::BinaryOperation operation;
		switch (lexer.peek().type)
		{
		case TokenType::LESS:
			operation = ast::BinaryExpression::BinaryOperation::LessThan;
			break;
		case TokenType::LESS_EQUAL:
			operation = ast::BinaryExpression::BinaryOperation::LessThanOrEqual;
			break;
		case TokenType::GREATER:
			operation = ast::BinaryExpression::BinaryOperation::GreaterThan;
			break;
		case TokenType::GREATER_EQUAL:
			operation = ast::BinaryExpression::BinaryOperation::GreaterThanOrEqual;
			break;
		default:
			return std::move(l_expr);
		}
		Token op_tok = lexer.take();

		// parse right hand expression
		auto maybe_r_expr = try_parse_bitwise_or(lexer);
		if (!maybe_r_expr.has_value())
			throw CompilerError::syntax_error("Expected expression following " + to_string(TokenType::AND_AND), op_tok.loc.end);
		auto r_expr = std::move(maybe_r_expr.value());

		return std::make_unique<ast::BinaryExpression>(
			SourceLocRange{l_expr->src_loc.start, r_expr->src_loc.end},
			std::move(l_expr),
			std::move(r_expr),
			operation);
	}

	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_bitwise_or(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = try_parse_bitwise_xor(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		// check for operator
		if (lexer.peek().type != TokenType::OR)
			return std::move(maybe_l_expr.value());
		Token op_tok = lexer.take();

		std::unique_ptr<ast::ExpressionNode> l_expr = std::move(maybe_l_expr.value());

		// parse right hand expression
		auto maybe_r_expr = try_parse_bitwise_xor(lexer);
		if (!maybe_r_expr.has_value())
			throw CompilerError::syntax_error("Expected expression following " + to_string(op_tok), op_tok.loc.end);
		std::unique_ptr<ast::ExpressionNode> r_expr = std::move(maybe_r_expr.value());

		SourceLocRange src_loc{l_expr->src_loc.start, r_expr->src_loc.end};
		return std::make_unique<ast::BinaryExpression>(src_loc, std::move(l_expr), std::move(r_expr), ast::BinaryExpression::BinaryOperation::BitwiseOr);
	}

	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_bitwise_xor(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = try_parse_bitwise_and(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		// check for operator
		if (lexer.peek().type != TokenType::CARET)
			return std::move(maybe_l_expr.value());
		Token op_tok = lexer.take();

		std::unique_ptr<ast::ExpressionNode> l_expr = std::move(maybe_l_expr.value());

		// parse right hand expression
		auto maybe_r_expr = try_parse_bitwise_and(lexer);
		if (!maybe_r_expr.has_value())
			throw CompilerError::syntax_error("Expected expression following " + to_string(op_tok), op_tok.loc.end);
		std::unique_ptr<ast::ExpressionNode> r_expr = std::move(maybe_r_expr.value());

		SourceLocRange src_loc{l_expr->src_loc.start, r_expr->src_loc.end};
		return std::make_unique<ast::BinaryExpression>(src_loc, std::move(l_expr), std::move(r_expr), ast::BinaryExpression::BinaryOperation::BitwiseXor);
	}

	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_bitwise_and(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = try_parse_bitshift(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		// check for operator
		if (lexer.peek().type != TokenType::AND)
			return std::move(maybe_l_expr.value());
		Token op_tok = lexer.take();

		std::unique_ptr<ast::ExpressionNode> l_expr = std::move(maybe_l_expr.value());

		// parse right hand expression
		auto maybe_r_expr = try_parse_bitshift(lexer);
		if (!maybe_r_expr.has_value())
			throw CompilerError::syntax_error("Expected expression following " + to_string(op_tok), op_tok.loc.end);
		std::unique_ptr<ast::ExpressionNode> r_expr = std::move(maybe_r_expr.value());

		SourceLocRange src_loc{l_expr->src_loc.start, r_expr->src_loc.end};
		return std::make_unique<ast::BinaryExpression>(src_loc, std::move(l_expr), std::move(r_expr), ast::BinaryExpression::BinaryOperation::BitwiseAnd);
	}

	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_bitshift(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = try_parse_additive(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		// check for operator
		if (lexer.peek().type != TokenType::GREATER_GREATER && lexer.peek().type != TokenType::LESS_LESS)
			return std::move(maybe_l_expr.value());
		Token op_tok = lexer.take();

		auto operation = op_tok.type == TokenType::LESS_LESS
							 ? ast::BinaryExpression::BinaryOperation::ShiftLeft
							 : ast::BinaryExpression::BinaryOperation::ShiftRight;
		std::unique_ptr<ast::ExpressionNode> l_expr = std::move(maybe_l_expr.value());

		// parse right hand expression
		auto maybe_r_expr = try_parse_additive(lexer);
		if (!maybe_r_expr.has_value())
			throw CompilerError::syntax_error("Expected expression following " + to_string(op_tok), op_tok.loc.end);
		std::unique_ptr<ast::ExpressionNode> r_expr = std::move(maybe_r_expr.value());

		SourceLocRange src_loc{l_expr->src_loc.start, r_expr->src_loc.end};
		return std::make_unique<ast::BinaryExpression>(src_loc, std::move(l_expr), std::move(r_expr), operation);
	}

	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_additive(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = try_parse_multiplicative(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		std::unique_ptr<ast::ExpressionNode> ret = std::move(maybe_l_expr.value());
		while (true)
		{
			// check for operator
			if (lexer.peek().type != TokenType::PLUS && lexer.peek().type != TokenType::MINUS)
				return ret;
			Token op_tok = lexer.take();

			auto operation = op_tok.type == TokenType::PLUS
								 ? ast::BinaryExpression::BinaryOperation::Add
								 : ast::BinaryExpression::BinaryOperation::Subtract;

			// parse right hand expression
			auto maybe_r_expr = try_parse_multiplicative(lexer);
			if (!maybe_r_expr.has_value())
				throw CompilerError::syntax_error("Expected expression following " + to_string(op_tok), op_tok.loc.end);
			std::unique_ptr<ast::ExpressionNode> r_expr = std::move(maybe_r_expr.value());

			SourceLocRange src_loc{ret->src_loc.start, r_expr->src_loc.end};
			ret = std::make_unique<ast::BinaryExpression>(src_loc, std::move(ret), std::move(r_expr), operation);
		}
	}

	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_multiplicative(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = try_parse_unary(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		std::unique_ptr<ast::ExpressionNode> ret = std::move(maybe_l_expr.value());

		while (true)
		{
			// check for operator
			ast::BinaryExpression::BinaryOperation operation;
			switch (lexer.peek().type)
			{
			case TokenType::ASTERISK:
				operation = ast::BinaryExpression::BinaryOperation::Multiply;
				break;
			case TokenType::FORWARD_SLASH:
				operation = ast::BinaryExpression::BinaryOperation::Divide;
				break;
			case TokenType::PERCENT:
				operation = ast::BinaryExpression::BinaryOperation::Modulus;
				break;
			default:
				// no more mult operators, return what we got
				return ret;
			}
			Token op_tok = lexer.take();

			// parse right hand expression
			auto maybe_r_expr = try_parse_unary(lexer);
			if (!maybe_r_expr.has_value())
				throw CompilerError::syntax_error("Expected expression following " + to_string(op_tok), op_tok.loc.end);

			std::unique_ptr<ast::ExpressionNode> r_expr = std::move(maybe_r_expr.value());
			SourceLocRange src_loc{ret->src_loc.start, r_expr->src_loc.end};
			ret = std::make_unique<ast::BinaryExpression>(src_loc, std::move(ret), std::move(r_expr), operation);
		}
	}

	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_unary(Lexer &lexer)
	{
		using UnaryOperation = ast::UnaryExpression::UnaryOperation;

		UnaryOperation operation;
		if (lexer.peek().type == TokenType::EXCLAMATION)
			operation = UnaryOperation::LogicalNot;
		else if (lexer.peek().type == TokenType::TILDE)
			operation = UnaryOperation::BitwiseNot;
		else if (lexer.peek().type == TokenType::MINUS)
			operation = UnaryOperation::Negation;
		else
			return try_parse_postfix(lexer);

		Token op_tok = lexer.take();

		auto maybe_expr = try_parse_unary(lexer);
		if (!maybe_expr.has_value())
			throw CompilerError::syntax_error("Expected expression following " + to_string(op_tok), op_tok.loc.end);
		std::unique_ptr<ast::ExpressionNode> expr = std::move(maybe_expr.value());

		SourceLocRange src_loc{op_tok.loc.start, expr->src_loc.end};
		return std::make_unique<ast::UnaryExpression>(src_loc, std::move(expr), operation);
	}

	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_postfix(Lexer &lexer)
	{
		// TODO array indexing here

		auto subject = try_parse_primary(lexer);
		if (!subject.has_value())
			return std::nullopt;

		if (lexer.peek().type == TokenType::L_PAREN)
		{
			// function call
			lexer.take();

			auto callee_expr = std::move(subject.value());

			// TODO args

			SourceLoc end = lexer.expect(TokenType::R_PAREN).loc.end;

			return std::make_unique<ast::FunctionCall>(SourceLocRange{callee_expr->src_loc.start, end}, std::move(callee_expr));
		}

		// normal expression, no postfix operator
		return subject;
	}

	/// Primary expression (aka "atom"), transparent so doesn't produce a node of its own
	std::optional<std::unique_ptr<ast::ExpressionNode>> try_parse_primary(Lexer &lexer)
	{
		// check if this is an identifier
		if (auto a = try_parse_identifier(lexer))
			return std::move(a.value());

		// check if this is bool
		if (auto a = try_parse_boolean_literal(lexer))
			return std::move(a.value());

		// check if this is int
		if (auto a = try_parse_integer_literal(lexer))
			return std::move(a.value());

		// TODO check for ifs, blocks, etc

		// check for parenthesized expressions
		if (lexer.peek().type == TokenType::L_PAREN)
		{
			Token l_paren_tok = lexer.take();
			auto inner_expr = try_parse_expr(lexer);
			if (!inner_expr.has_value())
				throw CompilerError::syntax_error("Expected expression following " + to_string(TokenType::L_PAREN), l_paren_tok.loc.end);
			Token r_paren_tok = lexer.expect(TokenType::R_PAREN);

			return inner_expr;
		}

		return std::nullopt;
	}

	std::optional<std::unique_ptr<ast::IdentifierExpression>> try_parse_identifier(Lexer &lexer)
	{
		if (lexer.peek().type != TokenType::IDENT)
			return std::nullopt;

		Token tok = lexer.take();
		return std::make_unique<ast::IdentifierExpression>(tok.loc, tok.str);
	}

	std::optional<std::unique_ptr<ast::IntegerLiteralExpression>> try_parse_integer_literal(Lexer &lexer)
	{
		if (lexer.peek().type != TokenType::INT_LITERAL)
			return std::nullopt;

		Token tok = lexer.take();

		int base = 10;
		if (tok.str.size() == 0)
			throw CompilerError::internal("Empty string for int literal token");
		else if (tok.str[0] == '0' && tok.str.size() >= 2)
		{
			if (tok.str[1] == 'b')
			{
				// binary
				base = 2;
				tok.str = tok.str.substr(2, tok.str.size() - 2);
			}
			else if (tok.str[1] == 'o')
			{
				// octal
				base = 8;
				tok.str = tok.str.substr(2, tok.str.size() - 2);
			}
			else if (tok.str[1] == 'x')
			{
				// hex
				base = 16;
				tok.str = tok.str.substr(2, tok.str.size() - 2);
			}
		}

		// finding where numbers stop
		auto type_annotation_pos = tok.str.begin();
		while (type_annotation_pos != tok.str.end() && is_numeric(*type_annotation_pos))
			++type_annotation_pos;

		std::string_view value_str(tok.str.begin(), type_annotation_pos);
		std::string_view type_str(type_annotation_pos, tok.str.end());

		// parsing type and value
		FrontendType type_annotation = FrontendType::from_string(type_str);
		if (!type_annotation.is_integer())
			throw CompilerError::type_error("Invalid type annotation on integer literal, must be an integer type", tok.loc);

		ArbInteger value;
		switch (base)
		{
		case 2:
		case 8:
		case 16:
			throw CompilerError::unimplemented("TODO non-decimal int literals");
			break;
		case 10:
			value = ArbInteger::from_str_base10(value_str);
			break;
		default:
			throw CompilerError::internal("Invalid base in \"try_parse_integer_literal\"");
			break;
		}

		return std::make_unique<ast::IntegerLiteralExpression>(tok.loc, value, type_annotation);
	}

	std::optional<std::unique_ptr<ast::BooleanLiteralExpression>> try_parse_boolean_literal(Lexer &lexer)
	{
		if (lexer.peek().type != TokenType::KEYWORD_FALSE && lexer.peek().type != TokenType::KEYWORD_TRUE)
			return std::nullopt;

		Token tok = lexer.take();
		return std::make_unique<ast::BooleanLiteralExpression>(tok.loc, tok.type == TokenType::KEYWORD_TRUE);
	}
}
