#include "frontend/AST.hpp"

#include <format>
#include <iostream>

#include "utils/error.hpp"
#include "utils/logging.hpp"

namespace ast
{
	void IntegerLiteralExpression::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Integer literal expression (value: " << this->raw_value << ", type annotation: " << this->type.to_string() << ")";
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
	}

	void LogicalOrExpression::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Logical or expression";
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
		l_expr->debug_print(depth + 1);
		r_expr->debug_print(depth + 1);
	}

	void LogicalAndExpression::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Logical and expression";
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
		l_expr->debug_print(depth + 1);
		r_expr->debug_print(depth + 1);
	}

	void EqualityExpression::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Equality expression (";
		if (this->notted)
			std::cout << "not ";
		std::cout << "equals)";
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
		l_expr->debug_print(depth + 1);
		r_expr->debug_print(depth + 1);
	}

	void ComparisonExpression::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Comparison expression (";
		switch (this->kind)
		{
		case ComparisonKind::GREATER_THAN:
			std::cout << ">)";
			break;
		case ComparisonKind::GREATER_THAN_OR_EQUAL:
			std::cout << ">=)";
			break;
		case ComparisonKind::LESS_THAN:
			std::cout << "<)";
			break;
		case ComparisonKind::LESS_THAN_OR_EQUAL:
			std::cout << "<=)";
			break;
		}
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
		l_expr->debug_print(depth + 1);
		r_expr->debug_print(depth + 1);
	}

	void BitwiseOrExpression::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Bitwise OR expression";
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
		l_expr->debug_print(depth + 1);
		r_expr->debug_print(depth + 1);
	}

	void BitwiseXorExpression::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Bitwise XOR expression";
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
		l_expr->debug_print(depth + 1);
		r_expr->debug_print(depth + 1);
	}

	void BitwiseAndExpression::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Bitwise AND expression";
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
		l_expr->debug_print(depth + 1);
		r_expr->debug_print(depth + 1);
	}

	void BitshiftExpression::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Bitshift expression (";
		if (this->left_shift)
			std::cout << "left)";
		else
			std::cout << "right)";
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
		l_expr->debug_print(depth + 1);
		r_expr->debug_print(depth + 1);
	}

	void AdditiveExpression::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Additive expression (";
		if (this->plus)
			std::cout << "add)";
		else
			std::cout << "subtract)";
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
		l_expr->debug_print(depth + 1);
		r_expr->debug_print(depth + 1);
	}

	void MultiplicativeExpression::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Multiplicative expression (";
		switch (this->operation)
		{
		case MultOperation::Multiplication:
			std::cout << "multiply)";
			break;
		case MultOperation::Division:
			std::cout << "divide)";
			break;
		case MultOperation::Modulus:
			std::cout << "modulo)";
			break;
		}
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
		l_expr->debug_print(depth + 1);
		r_expr->debug_print(depth + 1);
	}

	void ReturnStatement::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Return statement (expression present: " << bool_str(this->expr != nullptr) << ")";
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
		if (this->expr != nullptr)
			this->expr->debug_print(depth + 1);
	}

	void ArgDefinition::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Argument definition (type: " << this->type.to_string() << ", name: \"" << this->name << "\")";
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
	}

	void FunctionDefinition::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Function definition (name: \"" << this->name << "\", args: " << this->args.size() << ", return type: " << this->return_type.to_string() << ")";
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
		for (const ArgDefinition &arg : this->args)
			arg.debug_print(depth + 1);
		for (const std::unique_ptr<StatementNode> &stmt : this->body_statements)
			stmt->debug_print(depth + 1);
		if (this->body_return_expr != nullptr)
			this->body_return_expr->debug_print(depth + 1);
	}

}
