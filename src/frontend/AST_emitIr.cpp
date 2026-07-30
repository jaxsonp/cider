#include "frontend/AST.hpp"

#include "utils/error.hpp"
#include "utils/logging.hpp"

namespace ast
{
	ir::VRegId IntegerLiteralExpression::emitIr(IrWriter &writer) const
	{
		ir::IrType irType = this->type.resolveType();

		ir::VRegId dst_reg = writer.new_vreg(irType);
		// load imm instruction ignores op1 and op2 regs
		writer.add_instr(ir::Op::LoadImm, dst_reg, -1, -1, std::bit_cast<uint32_t>(this->raw_value));
		return dst_reg;
	}

	ir::VRegId BooleanLiteralExpression::emitIr(IrWriter &writer) const
	{

		ir::VRegId dst_reg = writer.new_vreg(ir::IrType::boolean());
		// load imm instruction ignores op1 and op2 regs
		writer.add_instr(ir::Op::LoadImm, dst_reg, -1, -1, this->value ? 1u : 0u);
		return dst_reg;
	}

	ir::VRegId PrimaryExpression::emitIr(IrWriter &writer) const
	{
		return this->expr->emitIr(writer);
	}

	ir::VRegId LogicalOrExpression::emitIr(IrWriter &writer) const
	{
		throw CompilerError::unimplemented("TODO: emit ir (LogicalOrExpression)");
	}

	ir::VRegId LogicalAndExpression::emitIr(IrWriter &writer) const
	{
		throw CompilerError::unimplemented("TODO: emit ir (LogicalAndExpression)");
	}

	ir::VRegId EqualityExpression::emitIr(IrWriter &writer) const
	{
		ir::VRegId l_reg = this->l_expr->emitIr(writer);
		ir::VRegId r_reg = this->r_expr->emitIr(writer);
		ir::VRegId dst_reg = writer.new_vreg(ir::IrType::boolean());
		writer.add_instr(this->notted ? ir::Op::CmpNe : ir::Op::CmpEq, dst_reg, l_reg, r_reg);
		return dst_reg;
	}

	ir::VRegId ComparisonExpression::emitIr(IrWriter &writer) const
	{
		ir::VRegId l_reg = this->l_expr->emitIr(writer);
		ir::VRegId r_reg = this->r_expr->emitIr(writer);
		ir::VRegId dst_reg = writer.new_vreg(ir::IrType::boolean());
		ir::Op op_code;
		switch (this->operation)
		{
		case ComparisonOperation::GREATER_THAN:
			op_code = ir::Op::CmpGt;
			break;
		case ComparisonOperation::GREATER_THAN_OR_EQUAL:
			op_code = ir::Op::CmpGte;
			break;
		case ComparisonOperation::LESS_THAN:
			op_code = ir::Op::CmpLt;
			break;
		case ComparisonOperation::LESS_THAN_OR_EQUAL:
			op_code = ir::Op::CmpLte;
			break;
		}
		writer.add_instr(op_code, dst_reg, l_reg, r_reg);
		return dst_reg;
	}

	ir::VRegId BitwiseOrExpression::emitIr(IrWriter &writer) const
	{
		ir::IrType irType = this->get_type().resolveType();

		ir::VRegId l_reg = this->l_expr->emitIr(writer);
		ir::VRegId r_reg = this->r_expr->emitIr(writer);
		ir::VRegId dst_reg = writer.new_vreg(irType);
		writer.add_instr(ir::Op::BitOr, dst_reg, l_reg, r_reg);
		return dst_reg;
	}

	ir::VRegId BitwiseXorExpression::emitIr(IrWriter &writer) const
	{
		ir::IrType irType = this->get_type().resolveType();

		ir::VRegId l_reg = this->l_expr->emitIr(writer);
		ir::VRegId r_reg = this->r_expr->emitIr(writer);
		ir::VRegId dst_reg = writer.new_vreg(irType);
		writer.add_instr(ir::Op::BitXor, dst_reg, l_reg, r_reg);
		return dst_reg;
	}

	ir::VRegId BitwiseAndExpression::emitIr(IrWriter &writer) const
	{
		ir::IrType irType = this->get_type().resolveType();

		ir::VRegId l_reg = this->l_expr->emitIr(writer);
		ir::VRegId r_reg = this->r_expr->emitIr(writer);
		ir::VRegId dst_reg = writer.new_vreg(irType);
		writer.add_instr(ir::Op::BitAnd, dst_reg, l_reg, r_reg);
		return dst_reg;
	}

	ir::VRegId BitshiftExpression::emitIr(IrWriter &writer) const
	{
		throw CompilerError::unimplemented("TODO: emit ir (BitshiftExpression)");
	}

	ir::VRegId AdditiveExpression::emitIr(IrWriter &writer) const
	{
		ir::IrType irType = this->get_type().resolveType();

		ir::VRegId l_reg = this->l_expr->emitIr(writer);
		ir::VRegId r_reg = this->r_expr->emitIr(writer);
		ir::VRegId dst_reg = writer.new_vreg(irType);
		if (this->plus)
			writer.add_instr(ir::Op::Add, dst_reg, l_reg, r_reg);
		else
			writer.add_instr(ir::Op::Sub, dst_reg, l_reg, r_reg);
		return dst_reg;
	}

	ir::VRegId MultiplicativeExpression::emitIr(IrWriter &writer) const
	{
		ir::IrType irType = this->get_type().resolveType();

		ir::VRegId l_reg = this->l_expr->emitIr(writer);
		ir::VRegId r_reg = this->r_expr->emitIr(writer);
		ir::VRegId dst_reg = writer.new_vreg(irType);
		switch (this->operation)
		{
		case MultOperation::Multiplication:
			writer.add_instr(ir::Op::Mul, dst_reg, l_reg, r_reg);
			break;
		case MultOperation::Division:
			writer.add_instr(ir::Op::Div, dst_reg, l_reg, r_reg);
			break;
		case MultOperation::Modulus:
			writer.add_instr(ir::Op::Rem, dst_reg, l_reg, r_reg);
			break;
		default:
			throw CompilerError::internal("Uncaught multiplication operation variant");
		};
		return dst_reg;
	}

	ir::VRegId UnaryExpression::emitIr(IrWriter &writer) const
	{
		ir::IrType irType = this->get_type().resolveType();

		ir::VRegId src_reg = this->expr->emitIr(writer);
		ir::VRegId dst_reg = writer.new_vreg(irType);
		switch (this->operation)
		{
		case UnaryOperation::Negation:
			writer.add_instr(ir::Op::Neg, dst_reg, src_reg, -1);
			break;
		case UnaryOperation::LogicalNot:
		{
			ir::VRegId one_reg = writer.get_const_vreg(irType, 1u);
			writer.add_instr(ir::Op::BitXor, dst_reg, src_reg, one_reg);
			break;
		}
		default:
			throw CompilerError::internal("Uncaught UnaryOperation variant");
		};
		return dst_reg;
	}

	void ReturnStatement::emitIr(IrWriter &writer) const
	{
		if (this->expr != nullptr)
		{
			ir::VRegId return_reg = this->expr->emitIr(writer);
			writer.add_return(return_reg);
		}
		else
		{
			writer.add_return();
		}
	}

	void FunctionDefinition::emitIr(IrWriter &writer) const
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
			stmt->emitIr(writer);
		if (this->body_return_expr != nullptr)
		{
			// create implicit return
			ir::VRegId return_reg = this->body_return_expr->emitIr(writer);
			writer.add_return(return_reg);
		}
	}
}