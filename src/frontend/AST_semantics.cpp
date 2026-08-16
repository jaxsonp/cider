#include "frontend/AST.hpp"

#include <format>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <bit>

#include "utils/error.hpp"
#include "utils/logging.hpp"
#include "AST.hpp"

namespace ast
{
	// === IntegerLiteralExpression ======================

	void IntegerLiteralExpression::resolve_symbols(SymbolScope *scope) {}

	void IntegerLiteralExpression::resolve_type()
	{
		// TODO
	}

	void IntegerLiteralExpression::check_semantics(SemanticAnalysisState &state) const
	{
		// TODO
	}

	// === BooleanLiteralExpression ======================

	void BooleanLiteralExpression::resolve_symbols(SymbolScope *scope) {}

	void BooleanLiteralExpression::resolve_type() {}

	void BooleanLiteralExpression::check_semantics(SemanticAnalysisState &state) const {}

	// === IdentifierExpression ======================

	void IdentifierExpression::resolve_symbols(SymbolScope *scope)
	{
		this->symbol = scope->find(this->name);
		if (this->symbol == nullptr)
			throw CompilerError::name_error(std::format("Can't find symbol \"{}\"", this->name), this->src_loc);
	}

	void IdentifierExpression::resolve_type()
	{
		this->type = this->symbol->type;
	}

	void IdentifierExpression::check_semantics(SemanticAnalysisState &state) const {}

	// === BinaryExpression =========================

	void BinaryExpression::resolve_symbols(SymbolScope *scope)
	{
		this->l_expr->resolve_symbols(scope);
		this->r_expr->resolve_symbols(scope);
	}

	void BinaryExpression::resolve_type()
	{
		if (this->l_expr == nullptr || this->r_expr == nullptr)
			throw CompilerError::internal("Invalid AST node (BinaryExpression)");

		this->l_expr->resolve_type();
		this->r_expr->resolve_type();

		FrontendType l_type = this->l_expr->type;
		FrontendType r_type = this->r_expr->type;

		switch (this->operation)
		{
		case BinaryOperation::LogicalOr:
		case BinaryOperation::LogicalAnd:
			// TODO
			throw CompilerError::unimplemented(std::format("TODO check semantics (operator '{}')", this->operator_string()));

		case BinaryOperation::Equal:
		case BinaryOperation::NotEqual:
		case BinaryOperation::LessThan:
		case BinaryOperation::LessThanOrEqual:
		case BinaryOperation::GreaterThan:
		case BinaryOperation::GreaterThanOrEqual:
			if (l_type != r_type)
				throw CompilerError::type_error(
					std::format("Cannot use operator '{}' with mix-matched types, '{}' and '{}'", this->operator_string(), l_type.to_string(), r_type.to_string()),
					this->src_loc);
			break;

		case BinaryOperation::BitwiseOr:
		case BinaryOperation::BitwiseXor:
		case BinaryOperation::BitwiseAnd:
			if (!l_type.is_integer())
				throw CompilerError::type_error(
					std::format("Cannot perform bitwise operation on non-integer type '{}'", l_type.to_string()),
					this->l_expr->src_loc);
			if (!r_type.is_integer())
				throw CompilerError::type_error(
					std::format("Cannot perform bitwise operation on non-integer type '{}'", r_type.to_string()),
					this->r_expr->src_loc);
			if (l_type != r_type)
				throw CompilerError::type_error(
					std::format("Cannot use operator '{}' with mix-matched types, '{}' and '{}'", this->operator_string(), l_type.to_string(), r_type.to_string()),
					this->src_loc);
			break;

		case BinaryOperation::ShiftLeft:
		case BinaryOperation::ShiftRight:
			// the shifted value can be any integer
			if (!l_type.is_integer())
				throw CompilerError::type_error(
					std::format("Cannot perform bitshift operation on non-integer type, '{}'", l_type.to_string()),
					this->l_expr->src_loc);
			// the shift amount must be unsigned, but its width doesn't have to match the left side
			if (!r_type.is_unsigned_integer())
				throw CompilerError::type_error(
					std::format("Cannot perform bitshift operation by non-unsigned-integer type, '{}'", r_type.to_string()),
					this->r_expr->src_loc);
			break;

		case BinaryOperation::Add:
		case BinaryOperation::Subtract:
		case BinaryOperation::Multiply:
		case BinaryOperation::Divide:
		case BinaryOperation::Modulus:
			if (!l_type.is_numeric())
				throw CompilerError::type_error(
					std::format("Cannot use operator '{}' with non-numeric type, '{}'", this->operator_string(), l_type.to_string()),
					this->l_expr->src_loc);
			if (!r_type.is_numeric())
				throw CompilerError::type_error(
					std::format("Cannot use operator '{}' with non-numeric type, '{}'", this->operator_string(), r_type.to_string()),
					this->r_expr->src_loc);
			if (l_type != r_type)
				throw CompilerError::type_error(
					std::format("Cannot use operator '{}' with mix-matched types, '{}' and '{}'", this->operator_string(), l_type.to_string(), r_type.to_string()),
					this->src_loc);
			break;

		default:
			throw CompilerError::internal("Uncaught BinaryExpression::Operator variant");
		}

		// finally resolve
		switch (this->operation)
		{
		case BinaryOperation::Equal:
		case BinaryOperation::NotEqual:
		case BinaryOperation::LessThan:
		case BinaryOperation::LessThanOrEqual:
		case BinaryOperation::GreaterThan:
		case BinaryOperation::GreaterThanOrEqual:
			this->type = FrontendType::boolean();
			break;
		default:
			this->type = l_type;
		}
	}

	void BinaryExpression::check_semantics(SemanticAnalysisState &state) const
	{
		this->l_expr->check_semantics(state);
		this->r_expr->check_semantics(state);
	}

	// === UnaryExpression =========================

	void UnaryExpression::resolve_symbols(SymbolScope *scope)
	{
		this->expr->resolve_symbols(scope);
	}

	void UnaryExpression::resolve_type()
	{
		this->expr->resolve_type();

		// check subexpression type
		FrontendType type = this->expr->type;

		switch (this->operation)
		{
		case UnaryOperation::LogicalNot:
			if (!this->expr->type.is_bool())
				throw CompilerError::type_error(
					std::format("Cannot use not operator '{}' with non-boolean type, '{}'", this->operator_string(), this->expr->type.to_string()),
					this->expr->src_loc);
			break;
		case UnaryOperation::BitwiseNot:
			if (!this->expr->type.is_integer())
				throw CompilerError::type_error(
					std::format("Cannot use not operator '{}' with non-integer type, '{}'", this->operator_string(), this->expr->type.to_string()),
					this->expr->src_loc);
			break;
		case UnaryOperation::Negation:
			if (!this->expr->type.is_numeric())
				throw CompilerError::type_error(
					std::format("Cannot use not operator '{}' with non-numeric type, '{}'", this->operator_string(), this->expr->type.to_string()),
					this->expr->src_loc);
			if (this->expr->type.is_unsigned_integer())
				throw CompilerError::type_error(
					std::format("Cannot use not operator '{}' with unsigned integer type, '{}'", this->operator_string(), this->expr->type.to_string()),
					this->expr->src_loc);
			break;
		}

		this->type = this->expr->type;
	}

	void UnaryExpression::check_semantics(SemanticAnalysisState &state) const
	{
		this->expr->check_semantics(state);
	}

	// ==== FunctionCall =========================================

	void FunctionCall::resolve_symbols(SymbolScope *scope)
	{
		this->callee->resolve_symbols(scope);
	}

	void FunctionCall::resolve_type()
	{
		this->callee->resolve_type();

		if (!this->callee->type.is_function())
			throw CompilerError::type_error(std::format("Type '{}' is not callable", this->callee->type.to_string()), this->callee->src_loc);

		this->type = *this->callee->type.function_return_type;
	}

	void FunctionCall::check_semantics(SemanticAnalysisState &state) const
	{
		this->callee->check_semantics(state);
	}

	// ==== ReturnSTatement =========================================

	void ReturnStatement::resolve_symbols(SymbolScope *scope)
	{
		if (this->expr.has_value())
			this->expr.value()->resolve_symbols(scope);
	}

	void ReturnStatement::check_semantics(SemanticAnalysisState &state) const
	{
		FrontendType return_type = FrontendType::void_type();

		// check semantics of expression
		if (this->expr.has_value())
		{
			this->expr.value()->resolve_type();
			return_type = this->expr.value()->type;
		}

		// make sure type matches current function

		if (!state.cur_fn_return_type.has_value())
			throw CompilerError::internal("SemanticAnalysisState.cur_fn_return_type is nullopt");
		if (return_type != state.cur_fn_return_type.value())
		{
			throw CompilerError::type_error(
				std::format(
					"Invalid return type, function expects '{}', found '{}'",
					state.cur_fn_return_type.value().to_string(),
					return_type.to_string()),
				this->src_loc);
		}
	}

	// ==== ArgDefinition =========================================

	void ArgDefinition::resolve_symbols(SymbolScope *scope)
	{
		// add this argument to the local scope
		scope->add(this->name, this->type);
	}

	void ArgDefinition::check_semantics(SemanticAnalysisState &state) const
	{
		throw CompilerError::unimplemented("TODO ArgDefinition::check_semantics");
	}

	// ==== FunctionDefinition =========================================

	void FunctionDefinition::resolve_symbols(SymbolScope *parent_scope)
	{
		// create new scope
		this->scope = std::make_unique<SymbolScope>(parent_scope);

		for (auto &arg_def : this->args)
		{
			arg_def.resolve_symbols(this->scope.get());
		}

		for (auto &stmt : this->body_statements)
		{
			stmt->resolve_symbols(this->scope.get());
		}
	}

	void FunctionDefinition::check_semantics(SemanticAnalysisState &state) const
	{
		// TODO check function name

		state.cur_fn_return_type = this->return_type;

		for (const ArgDefinition &arg : this->args)
		{
			arg.check_semantics(state);
		}

		for (const std::unique_ptr<StatementNode> &stmt : this->body_statements)
		{
			stmt->check_semantics(state);
		}

		if (this->body_return_expr.has_value())
		{
			this->body_return_expr.value()->check_semantics(state);
			FrontendType expr_type = this->body_return_expr.value()->type;
			if (state.cur_fn_return_type.has_value() && expr_type != state.cur_fn_return_type.value())
				throw CompilerError::type_error(
					std::format(
						"Invalid implicit return type, function expects '{}', found '{}'",
						state.cur_fn_return_type.value().to_string(),
						expr_type.to_string()),
					this->body_return_expr.value()->src_loc);
		}

		state.cur_fn_return_type = std::nullopt;
	}

}
