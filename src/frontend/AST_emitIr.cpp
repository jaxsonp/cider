#include "frontend/AST.hpp"

#include <format>

#include "utils/error.hpp"
#include "utils/logging.hpp"
#include "ir/IR_instructions.hpp"

using namespace ir;

namespace ast
{
	VRegId IntegerLiteralExpression::emitIr(IrWriter &writer) const
	{
		VRegId result_reg = writer.new_vreg();
		writer.emit(new instr::LoadImmInstruction(result_reg, std::bit_cast<uint32_t>(this->raw_value)));
		return result_reg;
	}

	VRegId LogicalOrExpression::emitIr(IrWriter &writer) const
	{
		throw CompilerError::unimplemented("TODO: emit ir");
	}

	VRegId LogicalAndExpression::emitIr(IrWriter &writer) const
	{
		throw CompilerError::unimplemented("TODO: emit ir");
	}

	VRegId EqualityExpression::emitIr(IrWriter &writer) const
	{
		throw CompilerError::unimplemented("TODO: emit ir");
	}

	VRegId ComparisonExpression::emitIr(IrWriter &writer) const
	{
		throw CompilerError::unimplemented("TODO: emit ir");
	}

	VRegId BitwiseOrExpression::emitIr(IrWriter &writer) const
	{
		VRegId l_expr_reg = this->l_expr->emitIr(writer);
		VRegId r_expr_reg = this->r_expr->emitIr(writer);
		VRegId output_reg = writer.new_vreg();
		writer.emit(new instr::BitwiseOrInstruction(output_reg, l_expr_reg, Operand(r_expr_reg)));
		return output_reg;
	}

	VRegId BitwiseXorExpression::emitIr(IrWriter &writer) const
	{
		VRegId l_expr_reg = this->l_expr->emitIr(writer);
		VRegId r_expr_reg = this->r_expr->emitIr(writer);
		VRegId output_reg = writer.new_vreg();
		writer.emit(new instr::BitwiseXorInstruction(output_reg, l_expr_reg, Operand(r_expr_reg)));
		return output_reg;
	}

	VRegId BitwiseAndExpression::emitIr(IrWriter &writer) const
	{
		VRegId l_expr_reg = this->l_expr->emitIr(writer);
		VRegId r_expr_reg = this->r_expr->emitIr(writer);
		VRegId output_reg = writer.new_vreg();
		writer.emit(new instr::BitwiseAndInstruction(output_reg, l_expr_reg, Operand(r_expr_reg)));
		return output_reg;
	}

	VRegId BitshiftExpression::emitIr(IrWriter &writer) const
	{
		throw CompilerError::unimplemented("TODO: emit ir");
	}

	VRegId AdditiveExpression::emitIr(IrWriter &writer) const
	{
		VRegId l_expr_reg = this->l_expr->emitIr(writer);
		VRegId r_expr_reg = this->r_expr->emitIr(writer);
		VRegId output_reg = writer.new_vreg();
		if (this->plus)
			writer.emit(new instr::AddInstruction(output_reg, l_expr_reg, Operand(r_expr_reg)));
		else
			writer.emit(new instr::SubtractInstruction(output_reg, l_expr_reg, Operand(r_expr_reg)));
		return output_reg;
	}

	VRegId MultiplicativeExpression::emitIr(IrWriter &writer) const
	{
		VRegId l_expr_reg = this->l_expr->emitIr(writer);
		VRegId r_expr_reg = this->r_expr->emitIr(writer);
		VRegId output_reg = writer.new_vreg();
		switch (this->operation)
		{
		case MultOperation::Multiplication:
			writer.emit(new instr::MultiplyInstruction(output_reg, l_expr_reg, Operand(r_expr_reg)));
			break;
		case MultOperation::Division:
			writer.emit(new instr::DivideInstruction(output_reg, l_expr_reg, Operand(r_expr_reg)));
			break;
		case MultOperation::Modulus:
			writer.emit(new instr::ModuloInstruction(output_reg, l_expr_reg, Operand(r_expr_reg)));
			break;
		default:
			throw CompilerError::internal("Uncaught multiplication operation variant");
		};
		return output_reg;
	}

	void ReturnStatement::emitIr(IrWriter &writer) const
	{
		if (this->expr != nullptr)
		{
			VRegId result_reg = this->expr->emitIr(writer);
			writer.emit(new instr::ReturnInstruction(result_reg));
		}
		else
		{
			writer.emit(new instr::ReturnInstruction());
		}
	}

	void FunctionDefinition::emitIr(IrWriter &writer) const
	{
		writer.new_function(this->name);

		// args
		unsigned short arg_index = 0;
		for (const ArgDefinition &arg : this->args)
		{
			writer.emit(new instr::LoadArgInstruction(writer.new_vreg(), arg_index));
			++arg_index;
		}

		// body
		for (const std::unique_ptr<StatementNode> &stmt : this->body_statements)
			stmt->emitIr(writer);
		if (this->body_return_expr != nullptr)
		{
			// create implicit return
			VRegId result_reg = this->body_return_expr->emitIr(writer);
			writer.emit(new instr::ReturnInstruction(result_reg));
		}
	}
}
