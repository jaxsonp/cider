#include "frontend/AST.hpp"

#include <format>
#include <utility>

#include "utils/error.hpp"

namespace ast
{
	std::optional<std::unique_ptr<TopLevelDeclaration>> TopLevelDeclaration::try_parse(Lexer &lexer)
	{
		if (auto function_def = FunctionDefinition::try_parse(lexer))
		{
			auto ret = std::make_unique<FunctionDefinition>(std::move(function_def.value()));
			ret->src_loc = function_def->src_loc;
			return ret;
		}

		return std::nullopt;
	}

	std::optional<std::unique_ptr<ExpressionNode>> ExpressionNode::try_parse(Lexer &lexer)
	{
		auto maybe_expr = LogicalOrExpression::try_parse(lexer);
		if (maybe_expr.has_value())
			return std::move(maybe_expr.value());
		return std::nullopt;
	}

	std::optional<std::unique_ptr<StatementNode>> StatementNode::try_parse(Lexer &lexer)
	{
		if (auto stmt = ReturnStatement::try_parse(lexer))
		{
			return std::move(stmt.value());
		}
		return std::nullopt;
	}

	std::optional<std::unique_ptr<IntegerLiteralExpression>> IntegerLiteralExpression::try_parse(Lexer &lexer)
	{
		if (lexer.peek().type != TokenType::INT_LITERAL)
			return std::nullopt;

		auto ret = std::make_unique<IntegerLiteralExpression>();
		Token tok = lexer.take();
		ret->src_loc = tok.loc;

		// finding where numbers stop
		auto type_annotation_pos = tok.str.begin();
		while (type_annotation_pos != tok.str.end() && is_numeric(*type_annotation_pos))
			++type_annotation_pos;

		std::string value_str(tok.str.begin(), type_annotation_pos);
		std::string type_str(type_annotation_pos, tok.str.end());

		// parsing type and value
		ret->type = FrontendType(type_str, tok.loc);
		if (ret->type == ConcreteType::I32)
		{
			long long value_ll = std::stoll(value_str);
			if (!std::in_range<int32_t>(value_ll))
				throw TypeError("Integer literal out of bounds for type: i32");
			uint32_t value_int32 = uint32_t(value_ll);
			ret->raw_value = std::bit_cast<uint32_t, int32_t>(value_int32);
		}
		else if (ret->type == ConcreteType::U32)
		{
			long long value_ll = std::stoll(value_str);
			if (!std::in_range<uint32_t>(value_ll))
				throw TypeError("Integer literal out of bounds for type: u32");
			ret->raw_value = uint32_t(value_ll);
		}
		else
			throw TypeError(std::format("Invalid type for integer literal: {}", ret->type.to_string()), ret->src_loc);

		return ret;
	}

	std::optional<std::unique_ptr<ExpressionNode>> LogicalOrExpression::try_parse(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = LogicalAndExpression::try_parse(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		// check for operator
		if (lexer.peek().type != TokenType::OR_OR)
			return std::move(maybe_l_expr.value());
		Token op_tok = lexer.take();

		std::unique_ptr<LogicalOrExpression> expr = std::make_unique<LogicalOrExpression>();
		expr->l_expr = std::move(maybe_l_expr.value());

		// parse right hand expression
		auto maybe_r_expr = LogicalAndExpression::try_parse(lexer);
		if (!maybe_r_expr.has_value())
			throw SyntaxError("Expected expression following " + to_string(op_tok), op_tok.loc.end);
		expr->r_expr = std::move(maybe_r_expr.value());

		return expr;
	}

	std::optional<std::unique_ptr<ExpressionNode>> LogicalAndExpression::try_parse(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = EqualityExpression::try_parse(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		// check for operator
		if (lexer.peek().type != TokenType::AND_AND)
			return std::move(maybe_l_expr.value());
		Token op_tok = lexer.take();

		std::unique_ptr<LogicalAndExpression> expr = std::make_unique<LogicalAndExpression>();
		expr->l_expr = std::move(maybe_l_expr.value());

		// parse right hand expression
		auto maybe_r_expr = EqualityExpression::try_parse(lexer);
		if (!maybe_r_expr.has_value())
			throw SyntaxError("Expected expression following " + to_string(op_tok), op_tok.loc.end);
		expr->r_expr = std::move(maybe_r_expr.value());

		return expr;
	}

	std::optional<std::unique_ptr<ExpressionNode>> EqualityExpression::try_parse(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = ComparisonExpression::try_parse(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		// check for operator
		if (lexer.peek().type != TokenType::EQUAL_EQUAL && lexer.peek().type != TokenType::EXCLAMATION_EQUAL)
			return std::move(maybe_l_expr.value());
		Token op_tok = lexer.take();

		std::unique_ptr<EqualityExpression> expr = std::make_unique<EqualityExpression>();
		expr->l_expr = std::move(maybe_l_expr.value());
		expr->notted = op_tok.type == TokenType::EXCLAMATION_EQUAL;

		// parse right hand expression
		auto maybe_r_expr = ComparisonExpression::try_parse(lexer);
		if (!maybe_r_expr.has_value())
			throw SyntaxError("Expected expression following " + to_string(op_tok), op_tok.loc.end);
		expr->r_expr = std::move(maybe_r_expr.value());

		return expr;
	}

	std::optional<std::unique_ptr<ExpressionNode>> ComparisonExpression::try_parse(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = BitwiseOrExpression::try_parse(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		// check for operator
		ComparisonKind kind;
		switch (lexer.peek().type)
		{
		case TokenType::LESS:
			kind = ComparisonKind::LESS_THAN;
			break;
		case TokenType::LESS_EQUAL:
			kind = ComparisonKind::LESS_THAN_OR_EQUAL;
			break;
		case TokenType::GREATER:
			kind = ComparisonKind::GREATER_THAN;
			break;
		case TokenType::GREATER_EQUAL:
			kind = ComparisonKind::GREATER_THAN_OR_EQUAL;
			break;
		default:
			return std::move(maybe_l_expr.value());
		}
		Token op_tok = lexer.take();

		std::unique_ptr<ComparisonExpression> expr = std::make_unique<ComparisonExpression>();
		expr->l_expr = std::move(maybe_l_expr.value());
		expr->kind = kind;

		// parse right hand expression
		auto maybe_r_expr = BitwiseOrExpression::try_parse(lexer);
		if (!maybe_r_expr.has_value())
			throw SyntaxError("Expected expression following " + to_string(TokenType::AND_AND), op_tok.loc.end);
		expr->r_expr = std::move(maybe_r_expr.value());

		return expr;
	}

	std::optional<std::unique_ptr<ExpressionNode>> BitwiseOrExpression::try_parse(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = BitwiseXorExpression::try_parse(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		// check for operator
		if (lexer.peek().type != TokenType::OR)
			return std::move(maybe_l_expr.value());
		Token op_tok = lexer.take();

		std::unique_ptr<BitwiseOrExpression> expr = std::make_unique<BitwiseOrExpression>();
		expr->l_expr = std::move(maybe_l_expr.value());

		// parse right hand expression
		auto maybe_r_expr = BitwiseXorExpression::try_parse(lexer);
		if (!maybe_r_expr.has_value())
			throw SyntaxError("Expected expression following " + to_string(op_tok), op_tok.loc.end);
		expr->r_expr = std::move(maybe_r_expr.value());

		return expr;
	}

	std::optional<std::unique_ptr<ExpressionNode>> BitwiseXorExpression::try_parse(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = BitwiseAndExpression::try_parse(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		// check for operator
		if (lexer.peek().type != TokenType::CARET)
			return std::move(maybe_l_expr.value());
		Token op_tok = lexer.take();

		std::unique_ptr<BitwiseXorExpression> expr = std::make_unique<BitwiseXorExpression>();
		expr->l_expr = std::move(maybe_l_expr.value());

		// parse right hand expression
		auto maybe_r_expr = BitwiseAndExpression::try_parse(lexer);
		if (!maybe_r_expr.has_value())
			throw SyntaxError("Expected expression following " + to_string(op_tok), op_tok.loc.end);
		expr->r_expr = std::move(maybe_r_expr.value());

		return expr;
	}

	std::optional<std::unique_ptr<ExpressionNode>> BitwiseAndExpression::try_parse(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = BitshiftExpression::try_parse(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		// check for operator
		if (lexer.peek().type != TokenType::AND)
			return std::move(maybe_l_expr.value());
		Token op_tok = lexer.take();

		std::unique_ptr<BitwiseAndExpression> expr = std::make_unique<BitwiseAndExpression>();
		expr->l_expr = std::move(maybe_l_expr.value());

		// parse right hand expression
		auto maybe_r_expr = BitshiftExpression::try_parse(lexer);
		if (!maybe_r_expr.has_value())
			throw SyntaxError("Expected expression following " + to_string(op_tok), op_tok.loc.end);
		expr->r_expr = std::move(maybe_r_expr.value());

		return expr;
	}

	std::optional<std::unique_ptr<ExpressionNode>> BitshiftExpression::try_parse(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = AdditiveExpression::try_parse(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		// check for operator
		if (lexer.peek().type != TokenType::GREATER_GREATER && lexer.peek().type != TokenType::LESS_LESS)
			return std::move(maybe_l_expr.value());
		Token op_tok = lexer.take();

		std::unique_ptr<BitshiftExpression> expr = std::make_unique<BitshiftExpression>();
		expr->l_expr = std::move(maybe_l_expr.value());
		expr->left_shift = op_tok.type == TokenType::LESS_LESS;

		// parse right hand expression
		auto maybe_r_expr = AdditiveExpression::try_parse(lexer);
		if (!maybe_r_expr.has_value())
			throw SyntaxError("Expected expression following " + to_string(op_tok), op_tok.loc.end);
		expr->r_expr = std::move(maybe_r_expr.value());

		return expr;
	}

	std::optional<std::unique_ptr<ExpressionNode>> AdditiveExpression::try_parse(Lexer &lexer)
	{
		// parse left hand expression
		auto maybe_l_expr = MultiplicativeExpression::try_parse(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		// check for operator
		if (lexer.peek().type != TokenType::PLUS && lexer.peek().type != TokenType::MINUS)
			return std::move(maybe_l_expr.value());
		Token op_tok = lexer.take();

		std::unique_ptr<AdditiveExpression> expr = std::make_unique<AdditiveExpression>();
		expr->l_expr = std::move(maybe_l_expr.value());
		expr->plus = op_tok.type == TokenType::PLUS;

		// parse right hand expression
		auto maybe_r_expr = MultiplicativeExpression::try_parse(lexer);
		if (!maybe_r_expr.has_value())
			throw SyntaxError("Expected expression following " + to_string(op_tok), op_tok.loc.end);
		expr->r_expr = std::move(maybe_r_expr.value());

		return expr;
	}

	std::optional<std::unique_ptr<ExpressionNode>> MultiplicativeExpression::try_parse(Lexer &lexer)
	{
		// TODO change these integer literals to primary

		// parse left hand expression
		auto maybe_l_expr = IntegerLiteralExpression::try_parse(lexer);
		if (!maybe_l_expr.has_value())
			return std::nullopt;

		// check for operator
		MultOperation op;
		switch (lexer.peek().type)
		{
		case TokenType::ASTERISK:
			op = MultOperation::Multiplication;
			break;
		case TokenType::FORWARD_SLASH:
			op = MultOperation::Division;
			break;
		case TokenType::PERCENT:
			op = MultOperation::Modulus;
			break;
		default:
			return std::move(maybe_l_expr.value());
		}
		Token op_tok = lexer.take();

		std::unique_ptr<MultiplicativeExpression> expr = std::make_unique<MultiplicativeExpression>();
		expr->l_expr = std::move(maybe_l_expr.value());
		expr->operation = op;

		// parse right hand expression
		auto maybe_r_expr = IntegerLiteralExpression::try_parse(lexer);
		if (!maybe_r_expr.has_value())
			throw SyntaxError("Expected expression following " + to_string(op_tok), op_tok.loc.end);
		expr->r_expr = std::move(maybe_r_expr.value());

		return expr;
	}

	std::optional<std::unique_ptr<ReturnStatement>> ReturnStatement::try_parse(Lexer &lexer)
	{
		if (lexer.peek().type != TokenType::KEYWORD_RETURN)
			return std::nullopt;

		auto ret = std::make_unique<ReturnStatement>();
		ret->src_loc.start = lexer.take().loc.start;

		if (lexer.peek().type == TokenType::SEMICOLON)
		{
			lexer.take();
			return ret;
		}

		SourceLoc expr_start = lexer.peek().loc.start;
		if (auto parsed_expr = ExpressionNode::try_parse(lexer))
			ret->expr = std::move(parsed_expr.value());
		else
			throw SyntaxError("Expected expression", expr_start);

		ret->src_loc.end = lexer.expect(TokenType::SEMICOLON).loc.end;

		return ret;
	}

	std::optional<ArgDefinition> ArgDefinition::try_parse(Lexer &lexer)
	{
		if (lexer.peek().type != TokenType::IDENT)
			return std::nullopt;

		ArgDefinition ret;

		Token name_tok = lexer.take();
		ret.name = name_tok.str;
		ret.src_loc.start = name_tok.loc.start;

		lexer.expect(TokenType::COLON);

		Token type_tok = lexer.expect(TokenType::IDENT);
		ret.type = FrontendType(type_tok);
		ret.src_loc.end = type_tok.loc.end;

		return ret;
	}

	std::optional<FunctionDefinition> FunctionDefinition::try_parse(Lexer &lexer)
	{
		// fn keyword
		if (lexer.peek().type != TokenType::KEYWORD_FN)
			return std::nullopt;

		FunctionDefinition ret;
		ret.src_loc.start = lexer.take().loc.start;

		// name
		Token name_tok = lexer.expect(TokenType::IDENT);
		ret.name = name_tok.str;

		// args
		lexer.expect(TokenType::L_PAREN);
		while (true)
		{
			if (auto arg = ArgDefinition::try_parse(lexer))
			{
				ret.args.push_back(arg.value());
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
		ret.return_type = FrontendType(ConcreteType::VOID);
		if (lexer.peek().type == TokenType::THIN_ARROW)
		{
			lexer.take();
			ret.return_type = FrontendType(lexer.expect(TokenType::IDENT));
		}

		// body
		lexer.expect(TokenType::L_CURLY_BRACKET);
		while (true)
		{
			if (auto stmt = StatementNode::try_parse(lexer))
			{
				ret.body_statements.push_back(std::move(stmt.value()));
				continue;
			}
			break;
		}

		// optional return expression
		if (auto expr = ExpressionNode::try_parse(lexer))
		{
			ret.body_return_expr = std::move(expr.value());
		}

		ret.src_loc.end = lexer.expect(TokenType::R_CURLY_BRACKET).loc.end;
		return ret;
	}

}
