#include "unit_testing.hpp"
#include "ir/IrWriter.hpp"

#include "utils/error.hpp"

using ir::IrType;

namespace
{
	/// A writer for one empty void function
	struct Fixture
	{
		IrWriter writer;
		IrFunctionWriter fn;

		Fixture() : fn(writer.start_function("f", {}, std::nullopt)) {}

		const auto &instructions() const { return this->fn.cur_bblock->instructions; }
	};
}

// functions =====================================================================================================

TEST(start_function_fills_in_the_signature)
{
	IrWriter writer;
	IrFunctionWriter fn = writer.start_function("name", {IrType::u8(), IrType::i32()}, IrType::boolean());
	ASSERT_EQ(fn.cur_function->name, "name");
	ASSERT_EQ(fn.cur_function->argument_types.size(), 2u);
	ASSERT_TRUE(fn.cur_function->argument_types[0].variant == IrType::Variant::U8);
	ASSERT_TRUE(fn.cur_function->argument_types[1].variant == IrType::Variant::I32);
	ASSERT_TRUE(fn.cur_function->return_type.has_value());
	ASSERT_TRUE(fn.cur_function->return_type->variant == IrType::Variant::BOOL);
}

TEST(void_function_has_no_return_type)
{
	Fixture f;
	ASSERT_TRUE(!f.fn.cur_function->return_type.has_value());
}

TEST(function_starts_in_its_entry_block)
{
	Fixture f;
	ASSERT_TRUE(f.fn.cur_bblock == f.fn.cur_function->entry);
	ASSERT_TRUE(f.fn.cur_bblock != nullptr);
	ASSERT_TRUE(f.instructions().empty());
}

TEST(functions_end_up_in_the_object)
{
	IrWriter writer;
	writer.start_function("a", {}, std::nullopt);
	writer.start_function("b", {}, std::nullopt);
	ir::Object obj = writer.get_obj();
	ASSERT_EQ(obj.functions.size(), 2u);
	ASSERT_TRUE(obj.functions.contains("a"));
	ASSERT_TRUE(obj.functions.contains("b"));
}

// virtual registers =============================================================================================

TEST(vregs_are_numbered_from_zero)
{
	Fixture f;
	ASSERT_EQ(f.fn.new_vreg(IrType::i32()), 0u);
	ASSERT_EQ(f.fn.new_vreg(IrType::i8()), 1u);
	ASSERT_EQ(f.fn.new_vreg(IrType::u64()), 2u);
}

TEST(vregs_remember_their_type)
{
	Fixture f;
	ir::VRegId vreg = f.fn.new_vreg(IrType::u16());
	ASSERT_TRUE(f.fn.cur_function->vregs.at(vreg).variant == IrType::Variant::U16);
}

TEST(vregs_are_per_function)
{
	IrWriter writer;
	IrFunctionWriter a = writer.start_function("a", {}, std::nullopt);
	IrFunctionWriter b = writer.start_function("b", {}, std::nullopt);
	a.new_vreg(IrType::i32());
	ASSERT_EQ(b.new_vreg(IrType::i32()), 0u);
}

// locals ========================================================================================================

TEST(locals_are_found_by_name)
{
	Fixture f;
	ir::VRegId a = f.fn.new_local("a", IrType::i32());
	ir::VRegId b = f.fn.new_local("b", IrType::i32());
	ASSERT_TRUE(a != b);
	ASSERT_EQ(f.fn.get_local("a"), a);
	ASSERT_EQ(f.fn.get_local("b"), b);
}

TEST(locals_have_a_vreg_of_their_type)
{
	Fixture f;
	ir::VRegId a = f.fn.new_local("a", IrType::boolean());
	ASSERT_TRUE(f.fn.cur_function->vregs.at(a).variant == IrType::Variant::BOOL);
}

TEST(unknown_local)
{
	Fixture f;
	ASSERT_THROWS(f.fn.get_local("nope"), CompilerError);
}

TEST(outer_scopes_are_visible)
{
	Fixture f;
	ir::VRegId a = f.fn.new_local("a", IrType::i32());
	f.fn.push_scope();
	ASSERT_EQ(f.fn.get_local("a"), a);
}

TEST(inner_scopes_shadow)
{
	Fixture f;
	ir::VRegId outer = f.fn.new_local("a", IrType::i32());
	f.fn.push_scope();
	ir::VRegId inner = f.fn.new_local("a", IrType::i32());
	ASSERT_EQ(f.fn.get_local("a"), inner);
	f.fn.pop_scope();
	ASSERT_EQ(f.fn.get_local("a"), outer);
}

TEST(popped_scopes_are_gone)
{
	Fixture f;
	f.fn.push_scope();
	f.fn.new_local("a", IrType::i32());
	f.fn.pop_scope();
	ASSERT_THROWS(f.fn.get_local("a"), CompilerError);
}

// constants =====================================================================================================

TEST(constant_loads_its_value)
{
	Fixture f;
	ir::VRegId vreg = f.fn.get_const_vreg(IrType::i32(), 42);
	ASSERT_EQ(f.instructions().size(), 1u);
	const auto &imm = std::get<ir::ImmediateInstruction>(f.instructions()[0]);
	ASSERT_EQ(imm.dest, vreg);
	ASSERT_EQ(imm.value, 42u);
}

TEST(same_constant_is_only_loaded_once)
{
	Fixture f;
	ir::VRegId first = f.fn.get_const_vreg(IrType::i32(), 42);
	ir::VRegId second = f.fn.get_const_vreg(IrType::i32(), 42);
	ASSERT_EQ(first, second);
	ASSERT_EQ(f.instructions().size(), 1u);
}

TEST(constants_differ_by_value)
{
	Fixture f;
	ASSERT_TRUE(f.fn.get_const_vreg(IrType::i32(), 1) != f.fn.get_const_vreg(IrType::i32(), 2));
}

TEST(constants_differ_by_type)
{
	Fixture f;
	ASSERT_TRUE(f.fn.get_const_vreg(IrType::i32(), 1) != f.fn.get_const_vreg(IrType::u32(), 1));
}

// instructions ==================================================================================================

TEST(binary_instruction)
{
	Fixture f;
	f.fn.add_binary(ir::BinaryOp::Sub, 1, 2, 3);
	const auto &instr = std::get<ir::BinaryInstruction>(f.instructions().at(0));
	ASSERT_TRUE(instr.op == ir::BinaryOp::Sub);
	ASSERT_EQ(instr.dest, 1u);
	ASSERT_EQ(instr.lhs, 2u);
	ASSERT_EQ(instr.rhs, 3u);
}

TEST(unary_instruction)
{
	Fixture f;
	f.fn.add_unary(ir::UnaryOp::BitNot, 1, 2);
	const auto &instr = std::get<ir::UnaryInstruction>(f.instructions().at(0));
	ASSERT_TRUE(instr.op == ir::UnaryOp::BitNot);
	ASSERT_EQ(instr.dest, 1u);
	ASSERT_EQ(instr.src, 2u);
}

TEST(immediate_instruction)
{
	Fixture f;
	f.fn.add_immediate(1, UINT64_MAX);
	const auto &instr = std::get<ir::ImmediateInstruction>(f.instructions().at(0));
	ASSERT_EQ(instr.dest, 1u);
	ASSERT_EQ(instr.value, UINT64_MAX);
}

TEST(load_arg_instruction)
{
	Fixture f;
	f.fn.add_load_arg(1, 5);
	const auto &instr = std::get<ir::LoadArgInstruction>(f.instructions().at(0));
	ASSERT_EQ(instr.dest, 1u);
	ASSERT_EQ(instr.index, 5u);
}

TEST(instructions_stay_in_order)
{
	Fixture f;
	f.fn.add_immediate(0, 1);
	f.fn.add_unary(ir::UnaryOp::Neg, 1, 0);
	f.fn.add_binary(ir::BinaryOp::Add, 2, 0, 1);
	ASSERT_EQ(f.instructions().size(), 3u);
	ASSERT_TRUE(std::holds_alternative<ir::ImmediateInstruction>(f.instructions()[0]));
	ASSERT_TRUE(std::holds_alternative<ir::UnaryInstruction>(f.instructions()[1]));
	ASSERT_TRUE(std::holds_alternative<ir::BinaryInstruction>(f.instructions()[2]));
}

TEST(call_with_a_return_value)
{
	Fixture f;
	ir::VRegId dest = f.fn.add_call("g", IrType::u8(), {4, 5});
	ASSERT_TRUE(dest != ir::NO_VREG);
	ASSERT_TRUE(f.fn.cur_function->vregs.at(dest).variant == IrType::Variant::U8);

	const auto &instr = std::get<ir::CallInstruction>(f.instructions().at(0));
	ASSERT_EQ(instr.callee, "g");
	ASSERT_TRUE(instr.dest == dest);
	ASSERT_TRUE(instr.args == (std::vector<ir::VRegId>{4, 5}));
}

TEST(call_to_a_void_function)
{
	Fixture f;
	ASSERT_EQ(f.fn.add_call("g", std::nullopt, {}), ir::NO_VREG);
	ASSERT_TRUE(f.fn.cur_function->vregs.empty());
	ASSERT_TRUE(!std::get<ir::CallInstruction>(f.instructions().at(0)).dest.has_value());
}

// returns =======================================================================================================

TEST(return_with_a_value)
{
	Fixture f;
	ir::BasicBlock *block = f.fn.cur_bblock;
	f.fn.add_return(3);
	ASSERT_TRUE(block->terminator.kind == ir::BasicBlockTerminator::RETURN);
	ASSERT_TRUE(block->terminator.ret_reg == ir::VRegId(3));
}

TEST(return_without_a_value)
{
	Fixture f;
	ir::BasicBlock *block = f.fn.cur_bblock;
	f.fn.add_return();
	ASSERT_TRUE(block->terminator.kind == ir::BasicBlockTerminator::RETURN);
	ASSERT_TRUE(!block->terminator.ret_reg.has_value());
}

TEST(return_starts_a_new_block)
{
	Fixture f;
	ir::BasicBlock *block = f.fn.cur_bblock;
	f.fn.add_return();
	ASSERT_TRUE(f.fn.cur_bblock != block);
	ASSERT_TRUE(f.instructions().empty());
	ASSERT_TRUE(f.fn.cur_bblock->id != block->id);
}
