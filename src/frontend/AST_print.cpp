#include "frontend/AST.hpp"

#include <format>

#include "utils/logging.hpp"

namespace ast
{
	void Node::print(std::ostream &out, unsigned int depth) const
	{
		out << std::format("{:{}}{} [{}]\n", "", depth * 2, this->label(), this->src_loc.to_string());
		this->print_children(out, depth + 1);
	}

	// EXPRESSIONS =============================================================

	std::string IntegerLiteralExpression::label() const
	{
		return std::format("Integer literal expression (value: {}, type annotation: {})",
						   this->raw_value, this->type.to_string());
	}

	std::string BooleanLiteralExpression::label() const
	{
		return std::format("Boolean literal expression (value: {})", this->value ? "true" : "false");
	}

	std::string LogicalOrExpression::label() const { return "Logical or expression"; }
	void LogicalOrExpression::print_children(std::ostream &out, unsigned int depth) const
	{
		this->l_expr->print(out, depth);
		this->r_expr->print(out, depth);
	}

	std::string LogicalAndExpression::label() const { return "Logical and expression"; }
	void LogicalAndExpression::print_children(std::ostream &out, unsigned int depth) const
	{
		this->l_expr->print(out, depth);
		this->r_expr->print(out, depth);
	}

	std::string EqualityExpression::label() const
	{
		return std::format("Equality expression (operation: '{}')", this->operator_string());
	}
	void EqualityExpression::print_children(std::ostream &out, unsigned int depth) const
	{
		this->l_expr->print(out, depth);
		this->r_expr->print(out, depth);
	}

	std::string ComparisonExpression::label() const
	{
		return std::format("Comparison expression (operation: '{}')", this->operator_string());
	}
	void ComparisonExpression::print_children(std::ostream &out, unsigned int depth) const
	{
		this->l_expr->print(out, depth);
		this->r_expr->print(out, depth);
	}

	std::string BitwiseOrExpression::label() const { return "Bitwise OR expression"; }
	void BitwiseOrExpression::print_children(std::ostream &out, unsigned int depth) const
	{
		this->l_expr->print(out, depth);
		this->r_expr->print(out, depth);
	}

	std::string BitwiseXorExpression::label() const { return "Bitwise XOR expression"; }
	void BitwiseXorExpression::print_children(std::ostream &out, unsigned int depth) const
	{
		this->l_expr->print(out, depth);
		this->r_expr->print(out, depth);
	}

	std::string BitwiseAndExpression::label() const { return "Bitwise AND expression"; }
	void BitwiseAndExpression::print_children(std::ostream &out, unsigned int depth) const
	{
		this->l_expr->print(out, depth);
		this->r_expr->print(out, depth);
	}

	std::string BitshiftExpression::label() const
	{
		return std::format("Bitshift expression ({})", this->left_shift ? "left" : "right");
	}
	void BitshiftExpression::print_children(std::ostream &out, unsigned int depth) const
	{
		this->l_expr->print(out, depth);
		this->r_expr->print(out, depth);
	}

	std::string AdditiveExpression::label() const
	{
		return std::format("Additive expression (operation: '{}')", this->operator_string());
	}
	void AdditiveExpression::print_children(std::ostream &out, unsigned int depth) const
	{
		this->l_expr->print(out, depth);
		this->r_expr->print(out, depth);
	}

	std::string MultiplicativeExpression::label() const
	{
		return std::format("Multiplicative expression (operation: '{}')", this->operator_string());
	}
	void MultiplicativeExpression::print_children(std::ostream &out, unsigned int depth) const
	{
		this->l_expr->print(out, depth);
		this->r_expr->print(out, depth);
	}

	std::string UnaryExpression::label() const
	{
		return std::format("Unary expression (operation: '{}')", this->operator_string());
	}
	void UnaryExpression::print_children(std::ostream &out, unsigned int depth) const
	{
		this->expr->print(out, depth);
	}

	// STATEMENTS ==============================================================

	std::string ReturnStatement::label() const
	{
		return std::format("Return statement (expression present: {})", bool_str(this->expr != nullptr));
	}
	void ReturnStatement::print_children(std::ostream &out, unsigned int depth) const
	{
		if (this->expr != nullptr)
			this->expr->print(out, depth);
	}

	// FUNCTION STUFF ==========================================================

	std::string ArgDefinition::label() const
	{
		return std::format("Argument definition (type: {}, name: \"{}\")",
						   this->type.to_string(), this->name);
	}

	std::string FunctionDefinition::label() const
	{
		return std::format("Function definition (name: \"{}\", args: {}, return type: {})",
						   this->name, this->args.size(), this->return_type.to_string());
	}
	void FunctionDefinition::print_children(std::ostream &out, unsigned int depth) const
	{
		for (const ArgDefinition &arg : this->args)
			arg.print(out, depth);
		for (const std::unique_ptr<StatementNode> &stmt : this->body_statements)
			stmt->print(out, depth);
		if (this->body_return_expr != nullptr)
			this->body_return_expr->print(out, depth);
	}
}
