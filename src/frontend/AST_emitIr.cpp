#include "frontend/AST.hpp"

#include <format>

#include "utils/error.hpp"
#include "utils/logging.hpp"

namespace ast
{
	ir::VRegId IntegerLiteralExpression::emit_ir(IrWriter &writer) const
	{
		ir::IrType irType = this->type.resolveType();

		ir::VRegId dst_reg = writer.new_vreg(irType);
		// load imm instruction ignores op1 and op2 regs
		throw CompilerError::unimplemented("TODO");
		// writer.add_instr(ir::Op::LoadImm, dst_reg, ir::NO_VREG, ir::NO_VREG, raw_value);
		return dst_reg;
	}

	ir::VRegId BooleanLiteralExpression::emit_ir(IrWriter &writer) const
	{

		ir::VRegId dst_reg = writer.new_vreg(ir::IrType::boolean());
		// load imm instruction ignores op1 and op2 regs
		writer.add_instr(ir::Op::LoadImm, dst_reg, ir::NO_VREG, ir::NO_VREG, this->value ? 1u : 0u);
		return dst_reg;
	}

	ir::VRegId IdentifierExpression::emit_ir(IrWriter &writer) const
	{
		throw CompilerError::unimplemented("TODO IdentifierExpression::emit_ir");
	}

	ir::VRegId BinaryExpression::emit_ir(IrWriter &writer) const
	{
		if (this->operation == BinaryOperation::LogicalOr || this->operation == BinaryOperation::LogicalAnd)
			throw CompilerError::unimplemented(std::format("TODO: emit ir (operator '{}')", this->operator_string()));

		ir::Op op_code;
		switch (this->operation)
		{
		case BinaryOperation::Equal:
			op_code = ir::Op::CmpEq;
			break;
		case BinaryOperation::NotEqual:
			op_code = ir::Op::CmpNe;
			break;
		case BinaryOperation::LessThan:
			op_code = ir::Op::CmpLt;
			break;
		case BinaryOperation::LessThanOrEqual:
			op_code = ir::Op::CmpLte;
			break;
		case BinaryOperation::GreaterThan:
			op_code = ir::Op::CmpGt;
			break;
		case BinaryOperation::GreaterThanOrEqual:
			op_code = ir::Op::CmpGte;
			break;
		case BinaryOperation::BitwiseOr:
			op_code = ir::Op::BitOr;
			break;
		case BinaryOperation::BitwiseXor:
			op_code = ir::Op::BitXor;
			break;
		case BinaryOperation::BitwiseAnd:
			op_code = ir::Op::BitAnd;
			break;
		case BinaryOperation::ShiftLeft:
			op_code = ir::Op::BitShl;
			break;
		case BinaryOperation::ShiftRight:
			op_code = ir::Op::BitShr;
			break;
		case BinaryOperation::Add:
			op_code = ir::Op::Add;
			break;
		case BinaryOperation::Subtract:
			op_code = ir::Op::Sub;
			break;
		case BinaryOperation::Multiply:
			op_code = ir::Op::Mul;
			break;
		case BinaryOperation::Divide:
			op_code = ir::Op::Div;
			break;
		case BinaryOperation::Modulus:
			op_code = ir::Op::Rem;
			break;
		default:
			throw CompilerError::internal("Uncaught BinaryExpression::Operator variant");
		}

		ir::IrType irType = this->type.resolveType();
		ir::VRegId l_reg = this->l_expr->emit_ir(writer);
		ir::VRegId r_reg = this->r_expr->emit_ir(writer);
		ir::VRegId dst_reg = writer.new_vreg(irType);
		writer.add_instr(op_code, dst_reg, l_reg, r_reg);
		return dst_reg;
	}

	ir::VRegId UnaryExpression::emit_ir(IrWriter &writer) const
	{
		ir::IrType irType = this->type.resolveType();

		ir::VRegId src_reg = this->expr->emit_ir(writer);
		ir::VRegId dst_reg = writer.new_vreg(irType);
		switch (this->operation)
		{
		case UnaryOperation::Negation:
			writer.add_instr(ir::Op::Neg, dst_reg, src_reg, ir::NO_VREG);
			break;
		case UnaryOperation::LogicalNot:
		{
			ir::VRegId one_reg = writer.get_const_vreg(irType, 1u);
			writer.add_instr(ir::Op::BitXor, dst_reg, src_reg, one_reg);
			break;
		}
		case UnaryOperation::BitwiseNot:
		{
			writer.add_instr(ir::Op::BitNot, dst_reg, src_reg, ir::NO_VREG);
			break;
		}
		default:
			throw CompilerError::internal("Uncaught UnaryOperation variant");
		};
		return dst_reg;
	}

	ir::VRegId FunctionCall::emit_ir(IrWriter &writer) const
	{
		throw CompilerError::unimplemented("TODO FunctionCall::emit_ir");
	}

	void ReturnStatement::emit_ir(IrWriter &writer) const
	{
		if (this->expr.has_value())
		{
			ir::VRegId return_reg = this->expr.value()->emit_ir(writer);
			writer.add_return(return_reg);
		}
		else
		{
			writer.add_return();
		}
	}

	void FunctionDefinition::emit_ir(IrWriter &writer) const
	{
		writer.new_function(this->name);

		// args
		unsigned short arg_index = 0;
		for (const ArgDefinition &arg : this->args)
		{
			// TODO
			// writer.add_instr(new instr::LoadArgInstruction(writer.new_vreg(), arg_index));
			++arg_index;
		}

		// body
		for (const std::unique_ptr<StatementNode> &stmt : this->body_statements)
			stmt->emit_ir(writer);
		if (this->body_return_expr.has_value())
		{
			// create implicit return
			ir::VRegId return_reg = this->body_return_expr.value()->emit_ir(writer);
			writer.add_return(return_reg);
		}
	}
}