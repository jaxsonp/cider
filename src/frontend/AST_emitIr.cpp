#include "frontend/AST.hpp"

#include <format>

#include "utils/error.hpp"
#include "utils/logging.hpp"

namespace ast
{
	ir::VRegId IntegerLiteralExpression::emitIr(IrWriter &writer) const
	{
		ir::VRegId result_reg(writer.new_vreg());
		writer.emit(new ir::instr::LoadImmInstruction(result_reg, std::bit_cast<uint32_t>(this->raw_value)));
		return result_reg;
	}

	ir::VRegId LogicalOrExpression::emitIr(IrWriter &writer) const
	{
		throw UnimplementedError("TODO: emit ir");
	}

	ir::VRegId LogicalAndExpression::emitIr(IrWriter &writer) const
	{
		throw UnimplementedError("TODO: emit ir");
	}

	ir::VRegId EqualityExpression::emitIr(IrWriter &writer) const
	{
		throw UnimplementedError("TODO: emit ir");
	}

	ir::VRegId ComparisonExpression::emitIr(IrWriter &writer) const
	{
		throw UnimplementedError("TODO: emit ir");
	}

	ir::VRegId BitwiseOrExpression::emitIr(IrWriter &writer) const
	{
		throw UnimplementedError("TODO: emit ir");
	}

	ir::VRegId BitwiseXorExpression::emitIr(IrWriter &writer) const
	{
		throw UnimplementedError("TODO: emit ir");
	}

	ir::VRegId BitwiseAndExpression::emitIr(IrWriter &writer) const
	{
		throw UnimplementedError("TODO: emit ir");
	}

	ir::VRegId BitshiftExpression::emitIr(IrWriter &writer) const
	{
		throw UnimplementedError("TODO: emit ir");
	}

	ir::VRegId AdditiveExpression::emitIr(IrWriter &writer) const
	{
		throw UnimplementedError("TODO: emit ir");
	}

	ir::VRegId MultiplicativeExpression::emitIr(IrWriter &writer) const
	{
		throw UnimplementedError("TODO: emit ir");
	}

	void ReturnStatement::emitIr(IrWriter &writer) const
	{
		if (this->expr != nullptr)
		{
			ir::VRegId result_reg = this->expr->emitIr(writer);
			writer.emit(new ir::instr::ReturnInstruction(result_reg));
		}
		else
		{
			writer.emit(new ir::instr::ReturnInstruction());
		}
	}

	void FunctionDefinition::emitIr(IrWriter &writer) const
	{
		writer.new_function(this->name);

		// args
		unsigned short arg_index = 0;
		for (const ArgDefinition &arg : this->args)
		{
			writer.emit(new ir::instr::LoadArgInstruction(writer.new_vreg(), arg_index));
			++arg_index;
		}

		// body
		for (const std::unique_ptr<StatementNode> &stmt : this->body_statements)
			stmt->emitIr(writer);
		if (this->body_return_expr != nullptr)
		{
			// create implicit return
			ir::VRegId result_reg = this->body_return_expr->emitIr(writer);
			writer.emit(new ir::instr::ReturnInstruction(result_reg));
		}
	}
}
