#include "frontend/AST.hpp"

#include <format>

#include "ir/IrWriter.hpp"
#include "utils/error.hpp"
#include "utils/logging.hpp"

namespace ast
{
	ir::VRegId IntegerLiteralExpression::emit_ir(IrFunctionWriter &writer) const
	{
		ir::IrType irType = this->type.resolveType();

		ir::VRegId dst_reg = writer.new_vreg(irType);
		writer.add_immediate(dst_reg, raw_value);
		return dst_reg;
	}

	ir::VRegId BooleanLiteralExpression::emit_ir(IrFunctionWriter &writer) const
	{

		ir::VRegId dst_reg = writer.new_vreg(ir::IrType::boolean());
		writer.add_immediate(dst_reg, this->value ? 1u : 0u);
		return dst_reg;
	}

	ir::VRegId IdentifierExpression::emit_ir(IrFunctionWriter &writer) const
	{
		return writer.get_local(this->name);
	}

	ir::VRegId BinaryExpression::emit_ir(IrFunctionWriter &writer) const
	{
		if (this->operation == BinaryOperation::LogicalOr || this->operation == BinaryOperation::LogicalAnd)
			throw CompilerError::unimplemented(std::format("TODO: emit ir (operator '{}')", this->operator_string()));

		ir::BinaryOp op_code;
		switch (this->operation)
		{
		case BinaryOperation::Equal:
			op_code = ir::BinaryOp::CmpEq;
			break;
		case BinaryOperation::NotEqual:
			op_code = ir::BinaryOp::CmpNe;
			break;
		case BinaryOperation::LessThan:
			op_code = ir::BinaryOp::CmpLt;
			break;
		case BinaryOperation::LessThanOrEqual:
			op_code = ir::BinaryOp::CmpLte;
			break;
		case BinaryOperation::GreaterThan:
			op_code = ir::BinaryOp::CmpGt;
			break;
		case BinaryOperation::GreaterThanOrEqual:
			op_code = ir::BinaryOp::CmpGte;
			break;
		case BinaryOperation::BitwiseOr:
			op_code = ir::BinaryOp::BitOr;
			break;
		case BinaryOperation::BitwiseXor:
			op_code = ir::BinaryOp::BitXor;
			break;
		case BinaryOperation::BitwiseAnd:
			op_code = ir::BinaryOp::BitAnd;
			break;
		case BinaryOperation::ShiftLeft:
			op_code = ir::BinaryOp::BitShl;
			break;
		case BinaryOperation::ShiftRight:
			op_code = ir::BinaryOp::BitShr;
			break;
		case BinaryOperation::Add:
			op_code = ir::BinaryOp::Add;
			break;
		case BinaryOperation::Subtract:
			op_code = ir::BinaryOp::Sub;
			break;
		case BinaryOperation::Multiply:
			op_code = ir::BinaryOp::Mul;
			break;
		case BinaryOperation::Divide:
			op_code = ir::BinaryOp::Div;
			break;
		case BinaryOperation::Modulus:
			op_code = ir::BinaryOp::Rem;
			break;
		default:
			throw CompilerError::internal("Uncaught BinaryExpression::Operator variant");
		}

		ir::IrType irType = this->type.resolveType();
		ir::VRegId l_reg = this->l_expr->emit_ir(writer);
		ir::VRegId r_reg = this->r_expr->emit_ir(writer);
		ir::VRegId dst_reg = writer.new_vreg(irType);
		writer.add_binary(op_code, dst_reg, l_reg, r_reg);
		return dst_reg;
	}

	ir::VRegId UnaryExpression::emit_ir(IrFunctionWriter &writer) const
	{
		ir::IrType irType = this->type.resolveType();

		ir::VRegId src_reg = this->expr->emit_ir(writer);
		ir::VRegId dst_reg = writer.new_vreg(irType);
		switch (this->operation)
		{
		case UnaryOperation::Negation:
			writer.add_unary(ir::UnaryOp::Neg, dst_reg, src_reg);
			break;
		case UnaryOperation::LogicalNot:
		{
			ir::VRegId one_reg = writer.get_const_vreg(irType, 1u);
			writer.add_binary(ir::BinaryOp::BitXor, dst_reg, src_reg, one_reg);
			break;
		}
		case UnaryOperation::BitwiseNot:
		{
			writer.add_unary(ir::UnaryOp::BitNot, dst_reg, src_reg);
			break;
		}
		default:
			throw CompilerError::internal("Uncaught UnaryOperation variant");
		};
		return dst_reg;
	}

	ir::VRegId FunctionCall::emit_ir(IrFunctionWriter &writer) const
	{
		// Cider has no function-pointer values yet, so a callable expression can only be a direct
		// reference to a named top-level function (enforced by IdentifierExpression::resolve_type rejecting
		// function names anywhere other than as a callee)
		auto *callee_ident = dynamic_cast<IdentifierExpression *>(this->callee.get());
		if (callee_ident == nullptr)
			throw CompilerError::internal("FunctionCall::emit_ir: callee is not a direct function reference");

		std::vector<ir::VRegId> arg_regs;
		arg_regs.reserve(this->args.size());
		for (const std::unique_ptr<ExpressionNode> &arg : this->args)
			arg_regs.push_back(arg->emit_ir(writer));

		std::optional<ir::IrType> return_type;
		if (this->type.variant != FrontendType::Variant::VOID)
			return_type = this->type.resolveType();

		// if return_type is nullopt, the returned vreg is ir::NO_VREG and must not be used
		return writer.add_call(callee_ident->name, return_type, std::move(arg_regs));
	}

	void ReturnStatement::emit_ir(IrFunctionWriter &writer) const
	{
		if (this->expr.has_value() && this->expr.value()->type.variant != FrontendType::Variant::VOID)
		{
			ir::VRegId return_reg = this->expr.value()->emit_ir(writer);
			writer.add_return(return_reg);
		}
		else
		{
			// a void expression (only a void function call today) is still emitted for its side effects
			if (this->expr.has_value())
				this->expr.value()->emit_ir(writer);
			writer.add_return();
		}
	}

	void ExpressionStatement::emit_ir(IrFunctionWriter &writer) const
	{
		// the resulting vreg (if any) is just never used
		this->expr->emit_ir(writer);
	}

	void FunctionDefinition::emit_ir(IrWriter &writer) const
	{
		std::vector<ir::IrType> param_types;
		param_types.reserve(this->args.size());
		for (const ArgDefinition &arg : this->args)
			param_types.push_back(arg.type.resolveType());

		std::optional<ir::IrType> ir_return_type;
		if (this->return_type.variant != FrontendType::Variant::VOID)
			ir_return_type = this->return_type.resolveType();

		IrFunctionWriter fn_writer = writer.start_function(this->name, std::move(param_types), ir_return_type);

		// args
		uint64_t arg_index = 0;
		for (const ArgDefinition &arg : this->args)
		{
			ir::VRegId arg_reg = fn_writer.new_local(arg.name, arg.type.resolveType());
			fn_writer.add_load_arg(arg_reg, arg_index);
			++arg_index;
		}

		// body
		for (const std::unique_ptr<StatementNode> &stmt : this->body_statements)
			stmt->emit_ir(fn_writer);
		if (this->body_return_expr.has_value())
		{
			// create implicit return
			const std::unique_ptr<ExpressionNode> &return_expr = this->body_return_expr.value();
			if (return_expr->type.variant == FrontendType::Variant::VOID)
			{
				return_expr->emit_ir(fn_writer);
				fn_writer.add_return();
			}
			else
			{
				ir::VRegId return_reg = return_expr->emit_ir(fn_writer);
				fn_writer.add_return(return_reg);
			}
		}
		else if (this->return_type.variant == FrontendType::Variant::VOID)
		{
			// falling off the end of a void function returns. if the body already ended in a return, this lands in
			// the fresh unreachable block that return left behind, which is harmless
			fn_writer.add_return();
		}
	}
}