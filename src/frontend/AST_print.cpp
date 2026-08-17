#include "frontend/AST.hpp"

#include <format>

#include "utils/logging.hpp"

const std::string_view INDENTATION_STR = "  ";

namespace ast
{
	void IntegerLiteralExpression::print(std::ostream &out, unsigned int depth) const
	{
		for (size_t i = 0; i < depth; ++i)
			out << INDENTATION_STR;
		out << "Integer literal (value: " << this->value.to_string() << ", type: " << this->type.to_string() << ")";
		out << " [" << this->src_loc.to_string() << "]" << std::endl;
	}

	void BooleanLiteralExpression::print(std::ostream &out, unsigned int depth) const
	{
		for (size_t i = 0; i < depth; ++i)
			out << INDENTATION_STR;
		out << "Boolean literal (value: " << (this->value ? "true" : "false") << ")";
		out << " [" << this->src_loc.to_string() << "]" << std::endl;
	}

	void IdentifierExpression::print(std::ostream &out, unsigned int depth) const
	{
		for (size_t i = 0; i < depth; ++i)
			out << INDENTATION_STR;
		out << "Identifier (name: " << this->name << ")";
		out << " [" << this->src_loc.to_string() << "]" << std::endl;
	}

	void BinaryExpression::print(std::ostream &out, unsigned int depth) const
	{
		for (size_t i = 0; i < depth; ++i)
			out << INDENTATION_STR;
		out << "Binary expression (operator: '" << this->operator_string() << "')";
		out << " [" << this->src_loc.to_string() << "]" << std::endl;
		this->l_expr->print(out, depth + 1);
		this->r_expr->print(out, depth + 1);
	}

	std::string_view BinaryExpression::operator_string() const
	{
		switch (this->operation)
		{
		case BinaryOperation::LogicalOr:
			return "||";
		case BinaryOperation::LogicalAnd:
			return "&&";
		case BinaryOperation::Equal:
			return "==";
		case BinaryOperation::NotEqual:
			return "!=";
		case BinaryOperation::LessThan:
			return "<";
		case BinaryOperation::LessThanOrEqual:
			return "<=";
		case BinaryOperation::GreaterThan:
			return ">";
		case BinaryOperation::GreaterThanOrEqual:
			return ">=";
		case BinaryOperation::BitwiseOr:
			return "|";
		case BinaryOperation::BitwiseXor:
			return "^";
		case BinaryOperation::BitwiseAnd:
			return "&";
		case BinaryOperation::ShiftLeft:
			return "<<";
		case BinaryOperation::ShiftRight:
			return ">>";
		case BinaryOperation::Add:
			return "+";
		case BinaryOperation::Subtract:
			return "-";
		case BinaryOperation::Multiply:
			return "*";
		case BinaryOperation::Divide:
			return "/";
		case BinaryOperation::Modulus:
			return "%";
		default:
			throw CompilerError::internal("Uncaught BinaryExpression::Operator variant");
		}
	}

	void UnaryExpression::print(std::ostream &out, unsigned int depth) const
	{
		for (size_t i = 0; i < depth; ++i)
			out << INDENTATION_STR;
		out << "Unary expression (operator: '" << this->operator_string() << "')";
		out << " [" << this->src_loc.to_string() << "]" << std::endl;
		this->expr->print(out, depth + 1);
	}

	std::string_view UnaryExpression::operator_string() const
	{
		switch (this->operation)
		{
		case UnaryOperation::LogicalNot:
			return "!";
		case UnaryOperation::BitwiseNot:
			return "~";
		case UnaryOperation::Negation:
			return "-";
		default:
			throw CompilerError::internal("Uncaught UnaryOperation variant");
		}
	}

	void FunctionCall::print(std::ostream &out, unsigned int depth) const
	{
		for (size_t i = 0; i < depth; ++i)
			out << INDENTATION_STR;
		out << "Function call";
		out << " [" << this->src_loc.to_string() << "]" << std::endl;
		// TODO args
		this->callee->print(out, depth + 1);
	}

	// STATEMENTS ==============================================================

	void ReturnStatement::print(std::ostream &out, unsigned int depth) const
	{
		for (size_t i = 0; i < depth; ++i)
			out << INDENTATION_STR;
		out << "Return statement (type: \"" << (this->expr.has_value() ? this->expr.value()->type : FrontendType::void_type()).to_string() << "\")";
		out << " [" << this->src_loc.to_string() << "]" << std::endl;
		if (this->expr.has_value())
			this->expr.value()->print(out, depth + 1);
	}

	// FUNCTION STUFF ==========================================================

	void ArgDefinition::print(std::ostream &out, unsigned int depth) const
	{
		throw CompilerError::unimplemented("TODO printing arg def AST node");
	}

	void FunctionDefinition::print(std::ostream &out, unsigned int depth) const
	{
		for (size_t i = 0; i < depth; ++i)
			out << INDENTATION_STR;
		out << "Function definition (name: \"" << this->name << "\", args: " << this->args.size() << ", return type: " << this->return_type.to_string() << ")";
		out << " [" << this->src_loc.to_string() << "]" << std::endl;
		for (const ArgDefinition &arg : this->args)
			arg.print(out, depth + 1);
		for (const std::unique_ptr<StatementNode> &stmt : this->body_statements)
			stmt->print(out, depth + 1);
		if (this->body_return_expr.has_value())
			this->body_return_expr.value()->print(out, depth + 1);
	}
}
