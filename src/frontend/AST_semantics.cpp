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

	void BooleanLiteralExpression::check_semantics(SemanticAnalysisState state) const
	{
		// nothing to do
	}

	void PrimaryExpression::check_semantics(SemanticAnalysisState state) const
	{
		this->expr->check_semantics(state);
	}

	void LogicalOrExpression::check_semantics(SemanticAnalysisState state) const
	{
		throw CompilerError::unimplemented("TODO check semantics (LogicalOrExpression)");
	}

	void LogicalAndExpression::check_semantics(SemanticAnalysisState state) const
	{
		throw CompilerError::unimplemented("TODO check semantics (LogicalAndExpression)");
	}

	void EqualityExpression::check_semantics(SemanticAnalysisState state) const
	{
		if (this->l_expr == nullptr || this->r_expr == nullptr)
			throw CompilerError::internal("Invalid AST node (EqualityExpression)");

		this->l_expr->check_semantics(state);
		this->r_expr->check_semantics(state);

		FrontendType l_type = this->l_expr->get_type();
		FrontendType r_type = this->r_expr->get_type();
		if (l_type != r_type)
			throw CompilerError::type_error(
				std::format("Cannot use operator '{}' with mix-matched types, '{}' and '{}'", this->operator_string(), l_type.to_string(), r_type.to_string()),
				this->src_loc);
	}

	void ComparisonExpression::check_semantics(SemanticAnalysisState state) const
	{
		if (this->l_expr == nullptr || this->r_expr == nullptr)
			throw CompilerError::internal("Invalid AST node (ComparisonExpression)");

		this->l_expr->check_semantics(state);
		this->r_expr->check_semantics(state);

		FrontendType l_type = this->l_expr->get_type();
		FrontendType r_type = this->r_expr->get_type();
		if (l_type != r_type)
			throw CompilerError::type_error(
				std::format("Cannot use operator '{}' with mix-matched types, '{}' and '{}'", this->operator_string(), l_type.to_string(), r_type.to_string()),
				this->src_loc);
	}

	void BitwiseOrExpression::check_semantics(SemanticAnalysisState state) const
	{
		if (this->l_expr == nullptr || this->r_expr == nullptr)
			throw CompilerError::internal("Invalid AST node (BitwiseOrExpression)");

		this->l_expr->check_semantics(state);
		this->r_expr->check_semantics(state);

		// make sure subexpression types match
		FrontendType l_type = this->l_expr->get_type();
		if (!l_type.is_integer())
			throw CompilerError::type_error(
				std::format("Cannot perform bitwise operation on non-integer type '{}'", l_type.to_string()),
				this->l_expr->src_loc);
		FrontendType r_type = this->r_expr->get_type();
		if (!r_type.is_integer())
			throw CompilerError::type_error(
				std::format("Cannot perform bitwise operation on non-integer type '{}'", r_type.to_string()),
				this->r_expr->src_loc);
		if (l_type != r_type)
			throw CompilerError::type_error(
				std::format("Cannot use operator '||' with mix-matched types, '{}' and '{}'", l_type.to_string(), r_type.to_string()),
				this->src_loc);
	}

	void BitwiseXorExpression::check_semantics(SemanticAnalysisState state) const
	{
		if (this->l_expr == nullptr || this->r_expr == nullptr)
			throw CompilerError::internal("Invalid AST node (BitwiseXorExpression)");

		this->l_expr->check_semantics(state);
		this->r_expr->check_semantics(state);

		// make sure subexpression types match
		FrontendType l_type = this->l_expr->get_type();
		if (!l_type.is_integer())
			throw CompilerError::type_error(
				std::format("Cannot perform bitwise operation on non-integer type '{}'", l_type.to_string()),
				this->l_expr->src_loc);
		FrontendType r_type = this->r_expr->get_type();
		if (!r_type.is_integer())
			throw CompilerError::type_error(
				std::format("Cannot perform bitwise operation on non-integer type '{}'", r_type.to_string()),
				this->r_expr->src_loc);
		if (l_type != r_type)
			throw CompilerError::type_error(
				std::format("Cannot use operator '^' with mix-matched types, '{}' and '{}'", l_type.to_string(), r_type.to_string()),
				this->src_loc);
	}

	void BitwiseAndExpression::check_semantics(SemanticAnalysisState state) const
	{
		if (this->l_expr == nullptr || this->r_expr == nullptr)
			throw CompilerError::internal("Invalid AST node (BitwiseAndExpression)");

		this->l_expr->check_semantics(state);
		this->r_expr->check_semantics(state);

		// make sure subexpression types match
		FrontendType l_type = this->l_expr->get_type();
		if (!l_type.is_integer())
			throw CompilerError::type_error(
				std::format("Cannot perform bitwise operation on non-integer type, '{}'", l_type.to_string()),
				this->l_expr->src_loc);
		FrontendType r_type = this->r_expr->get_type();
		if (!r_type.is_integer())
			throw CompilerError::type_error(
				std::format("Cannot perform bitwise operation on non-integer type, '{}'", r_type.to_string()),
				this->r_expr->src_loc);
		if (l_type != r_type)
			throw CompilerError::type_error(
				std::format("Cannot use operator '&&' with mix-matched types, '{}' and '{}'", l_type.to_string(), r_type.to_string()),
				this->src_loc);
	}

	void BitshiftExpression::check_semantics(SemanticAnalysisState state) const
	{
		if (this->l_expr == nullptr || this->r_expr == nullptr)
			throw CompilerError::internal("Invalid AST node (BitshiftExpression)");

		this->l_expr->check_semantics(state);
		this->r_expr->check_semantics(state);

		// the shifted value can be any integer
		FrontendType l_type = this->l_expr->get_type();
		if (!l_type.is_integer())
			throw CompilerError::type_error(
				std::format("Cannot perform bitshift operation on non-integer type, '{}'", l_type.to_string()),
				this->l_expr->src_loc);
		// the shift amount must be unsigned, but its width doesn't have to match the left side
		FrontendType r_type = this->r_expr->get_type();
		if (!r_type.is_unsigned_integer())
			throw CompilerError::type_error(
				std::format("Cannot perform bitshift operation by non-unsigned-integer type, '{}'", r_type.to_string()),
				this->r_expr->src_loc);
	}

	void AdditiveExpression::check_semantics(SemanticAnalysisState state) const
	{
		if (this->l_expr == nullptr || this->r_expr == nullptr)
			throw CompilerError::internal("Invalid AST node (AdditiveExpression)");

		this->l_expr->check_semantics(state);
		this->r_expr->check_semantics(state);

		// check subexpression types
		FrontendType l_type = this->l_expr->get_type();
		if (!l_type.is_numeric())
			throw CompilerError::type_error(
				std::format("Cannot use operator '{}' with non-numeric type, '{}'", this->operator_string(), l_type.to_string()),
				this->l_expr->src_loc);
		FrontendType r_type = this->r_expr->get_type();
		if (!r_type.is_numeric())
			throw CompilerError::type_error(
				std::format("Cannot use operator '{}' with non-numeric type, '{}'", this->operator_string(), r_type.to_string()),
				this->r_expr->src_loc);
		if (l_type != r_type)
			throw CompilerError::type_error(
				std::format("Cannot use operator '{}' with mix-matched types, '{}' and '{}'", this->operator_string(), l_type.to_string(), r_type.to_string()),
				this->src_loc);
	}

	void MultiplicativeExpression::check_semantics(SemanticAnalysisState state) const
	{
		if (this->l_expr == nullptr || this->r_expr == nullptr)
			throw CompilerError::internal("Invalid AST node (Multiplicative expresssion)");

		this->l_expr->check_semantics(state);
		this->r_expr->check_semantics(state);

		// check subexpression types
		FrontendType l_type = this->l_expr->get_type();
		if (!l_type.is_numeric())
			throw CompilerError::type_error(
				std::format("Cannot use operator '{}' with non-numeric type, '{}'", this->operator_string(), l_type.to_string()),
				this->l_expr->src_loc);
		FrontendType r_type = this->r_expr->get_type();
		if (!r_type.is_numeric())
			throw CompilerError::type_error(
				std::format("Cannot use operator '{}' with non-numeric type, '{}'", this->operator_string(), r_type.to_string()),
				this->r_expr->src_loc);
		if (l_type != r_type)
			throw CompilerError::type_error(
				std::format(
					"Cannot use operator '{}' with mix-matched types, '{}' and '{}'",
					this->operator_string(),
					l_type.to_string(),
					r_type.to_string()),
				this->src_loc);
	}

	void UnaryExpression::check_semantics(SemanticAnalysisState state) const
	{
		if (this->expr == nullptr)
			throw CompilerError::internal("Invalid AST node (Unary expresssion)");

		this->expr->check_semantics(state);

		// check subexpression type
		FrontendType type = this->expr->get_type();

		if (this->operation == UnaryOperation::LogicalNot && !type.is_bool())
			throw CompilerError::type_error(
				std::format("Cannot use not operator '{}' with non-boolean type, '{}'", this->operator_string(), type.to_string()),
				this->expr->src_loc);

		if (this->operation == UnaryOperation::Negation)
		{
			if (!type.is_numeric())
				throw CompilerError::type_error(
					std::format("Cannot use not operator '{}' with non-numeric type, '{}'", this->operator_string(), type.to_string()),
					this->expr->src_loc);
			if (type.is_unsigned_integer())
				throw CompilerError::type_error(
					std::format("Cannot use not operator '{}' with unsigned integer type, '{}'", this->operator_string(), type.to_string()),
					this->expr->src_loc);
		}
	}

	void ReturnStatement::check_semantics(SemanticAnalysisState state) const
	{
		// check semantics of expression
		if (this->expr != nullptr)
			this->expr->check_semantics(state);

		// make sure type matches current function
		if (state.fn_return_type.has_value() && this->return_type() != state.fn_return_type.value())
		{
			throw CompilerError::type_error(
				std::format(
					"Invalid return type, function expects '{}', found '{}'",
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
						"Invalid implicit return type, function expects '{}', found '{}'",
						state.fn_return_type.value().to_string(),
						expr_type.to_string()),
					this->body_return_expr->src_loc);
		}
	}

}
