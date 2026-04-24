#include "frontend/AST.hpp"

#include <format>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <bit>

#include "utils/error.hpp"
#include "utils/logging.hpp"

namespace ast
{
	void IntegerLiteralExpression::check_semantics(SemanticAnalysisState state) const
	{
		// TODO
	}

	void LogicalOrExpression::check_semantics(SemanticAnalysisState state) const
	{
		throw CompilerError::unimplemented("TODO check semantics");
	}

	void LogicalAndExpression::check_semantics(SemanticAnalysisState state) const
	{
		throw CompilerError::unimplemented("TODO check semantics");
	}

	void EqualityExpression::check_semantics(SemanticAnalysisState state) const
	{
		throw CompilerError::unimplemented("TODO check semantics");
	}

	void ComparisonExpression::check_semantics(SemanticAnalysisState state) const
	{
		throw CompilerError::unimplemented("TODO check semantics");
	}

	void BitwiseOrExpression::check_semantics(SemanticAnalysisState state) const
	{
		if (this->l_expr == nullptr || this->r_expr == nullptr)
			throw CompilerError::internal("Invalid AST node (BitwiseOrExpression)");

		// make sure subexpression types match
		FrontendType l_type = this->l_expr->get_type();
		FrontendType r_type = this->r_expr->get_type();
		if (l_type != r_type)
			throw CompilerError::type_error(
				std::format(
					"Binary operation contains mix-matched types, {} and {}",
					l_type.to_string(),
					r_type.to_string()),
				this->src_loc);

		// make sure type is integral TODO
	}

	void BitwiseXorExpression::check_semantics(SemanticAnalysisState state) const
	{
		if (this->l_expr == nullptr || this->r_expr == nullptr)
			throw CompilerError::internal("Invalid AST node (BitwiseXorExpression)");

		// make sure subexpression types match
		FrontendType l_type = this->l_expr->get_type();
		FrontendType r_type = this->r_expr->get_type();
		if (l_type != r_type)
			throw CompilerError::type_error(
				std::format(
					"Binary operation contains mix-matched types, {} and {}",
					l_type.to_string(),
					r_type.to_string()),
				this->src_loc);

		// make sure type is integral TODO
	}

	void BitwiseAndExpression::check_semantics(SemanticAnalysisState state) const
	{
		if (this->l_expr == nullptr || this->r_expr == nullptr)
			throw CompilerError::internal("Invalid AST node (BitwiseAndExpression)");

		// make sure subexpression types match
		FrontendType l_type = this->l_expr->get_type();
		FrontendType r_type = this->r_expr->get_type();
		if (l_type != r_type)
			throw CompilerError::type_error(
				std::format(
					"Binary operation contains mix-matched types, {} and {}",
					l_type.to_string(),
					r_type.to_string()),
				this->src_loc);

		// make sure type is integral TODO
	}

	void BitshiftExpression::check_semantics(SemanticAnalysisState state) const
	{
		throw CompilerError::unimplemented("TODO check semantics");
	}

	void AdditiveExpression::check_semantics(SemanticAnalysisState state) const
	{
		if (this->l_expr == nullptr || this->r_expr == nullptr)
			throw CompilerError::internal("Invalid AST node (AdditiveExpression)");

		// make sure subexpression types match
		FrontendType l_type = this->l_expr->get_type();
		FrontendType r_type = this->r_expr->get_type();
		if (l_type != r_type)
			throw CompilerError::type_error(
				std::format(
					"Binary operation contains mix-matched types, {} and {}",
					l_type.to_string(),
					r_type.to_string()),
				this->src_loc);
	}

	void MultiplicativeExpression::check_semantics(SemanticAnalysisState state) const
	{
		if (this->l_expr == nullptr || this->r_expr == nullptr || (this->operation != MultOperation::Multiplication && this->operation != MultOperation::Division && this->operation != MultOperation::Modulus))
			throw CompilerError::internal("Invalid AST node (Multiplicative expresssion)");

		// make sure subexpression types match
		FrontendType l_type = this->l_expr->get_type();
		FrontendType r_type = this->r_expr->get_type();
		if (l_type != r_type)
			throw CompilerError::type_error(
				std::format(
					"Binary operation contains mix-matched types, {} and {}",
					l_type.to_string(),
					r_type.to_string()),
				this->src_loc);
	}

	void ReturnStatement::check_semantics(SemanticAnalysisState state) const
	{
		// check semantics of expression
		if (this->expr != nullptr)
		{
			this->expr->check_semantics(state);
		}

		// make sure type matches current function
		if (state.fn_return_type.has_value() && this->return_type() != state.fn_return_type.value())
		{
			throw CompilerError::type_error(
				std::format(
					"Invalid return type, function requires {}, found: {}",
					state.fn_return_type.value().to_string(),
					this->return_type().to_string()),
				this->src_loc);
		}
	}

	void ArgDefinition::check_semantics(SemanticAnalysisState state) const
	{
		state.cur_scope->add({this->name, this->type});
	}

	void FunctionDefinition::check_semantics(SemanticAnalysisState state) const
	{
		// TODO check function name
		// TODO add function to symbol table

		// new scope whose parent scope is global
		this->scope->parent = state.cur_scope;
		state.cur_scope = this->scope;

		state.fn_return_type = this->return_type;

		// check args
		for (const ArgDefinition &arg : this->args)
		{
			arg.check_semantics(state);
		}
		// check return type
		// TODO

		// check body
		for (const std::unique_ptr<StatementNode> &stmt : this->body_statements)
		{
			stmt->check_semantics(state);
		}

		if (this->body_return_expr != nullptr)
		{
			this->body_return_expr->check_semantics(state);
			FrontendType expr_type = this->body_return_expr->get_type();
			if (state.fn_return_type.has_value() && expr_type != state.fn_return_type.value())
				throw CompilerError::type_error(
					std::format(
						"Invalid return type, function requires {}, found: {}",
						state.fn_return_type.value().to_string(),
						expr_type.to_string()),
					this->src_loc);
		}
	}

}
