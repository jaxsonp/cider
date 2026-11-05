#include "unit_testing.hpp"
#include "ir/IrPrinter.hpp"

#include <sstream>

#include "ir/IrWriter.hpp"

using ir::IrType;

static std::string print_object(const ir::Object &obj)
{
	std::ostringstream out;
	ir::print(obj, out);
	return out.str();
}

TEST(binary_mnemonics)
{
	using Op = ir::BinaryOp;
	const std::vector<std::pair<Op, std::string>> cases = {
		{Op::Add, "add"},
		{Op::Sub, "sub"},
		{Op::Mul, "mul"},
		{Op::Div, "div"},
		{Op::Rem, "rem"},
		{Op::BitAnd, "and"},
		{Op::BitOr, "or"},
		{Op::BitXor, "xor"},
		{Op::BitShl, "shl"},
		{Op::BitShr, "shr"},
		{Op::CmpEq, "cmp_eq"},
		{Op::CmpNe, "cmp_ne"},
		{Op::CmpGt, "cmp_gt"},
		{Op::CmpGte, "cmp_gte"},
		{Op::CmpLt, "cmp_lt"},
		{Op::CmpLte, "cmp_lte"},
	};
	for (const auto &[op, mnemonic] : cases)
		ASSERT_EQ(ir::binary_op_mnemonic(op), mnemonic);
}

TEST(unary_mnemonics)
{
	ASSERT_EQ(ir::unary_op_mnemonic(ir::UnaryOp::Mov), "mov");
	ASSERT_EQ(ir::unary_op_mnemonic(ir::UnaryOp::Neg), "neg");
	ASSERT_EQ(ir::unary_op_mnemonic(ir::UnaryOp::BitNot), "not");
}

TEST(empty_object_prints_nothing)
{
	ir::Object obj;
	ASSERT_EQ(print_object(obj), "");
}

TEST(every_instruction)
{
	IrWriter writer;
	IrFunctionWriter fn = writer.start_function("f", {IrType::i32(), IrType::u8()}, IrType::i32());
	ir::VRegId a = fn.new_vreg(IrType::i32());
	ir::VRegId b = fn.new_vreg(IrType::i32());
	ir::VRegId c = fn.new_vreg(IrType::i32());
	ir::VRegId d = fn.new_vreg(IrType::i32());
	fn.add_load_arg(a, 0);
	fn.add_immediate(b, 7);
	fn.add_binary(ir::BinaryOp::Add, c, a, b);
	fn.add_unary(ir::UnaryOp::Neg, d, c);
	ir::VRegId result = fn.add_call("g", IrType::i32(), {a, d});
	fn.add_call("h", std::nullopt, {});
	fn.add_return(result);

	ASSERT_EQ(print_object(writer.get_obj()),
			  "fn f(i32, u8) -> i32 {\n"
			  "  %0: i32\n"
			  "  %1: i32\n"
			  "  %2: i32\n"
			  "  %3: i32\n"
			  "  %4: i32\n"
			  "bb0: // f start\n"
			  "  %0 = loadarg 0\n"
			  "  %1 = loadimm 7\n"
			  "  %2 = add %0 %1\n"
			  "  %3 = neg %2\n"
			  "  %4 = call g(%0, %3)\n"
			  "  call h()\n"
			  "  ret %4\n"
			  "}\n");
}

TEST(void_function_has_no_arrow)
{
	IrWriter writer;
	IrFunctionWriter fn = writer.start_function("f", {}, std::nullopt);
	fn.add_return();
	ASSERT_EQ(print_object(writer.get_obj()), "fn f() {\nbb0: // f start\n  ret\n}\n");
}

TEST(functions_print_in_name_order_with_a_blank_line_between)
{
	IrWriter writer;
	for (const char *name : {"c", "a", "b"})
	{
		IrFunctionWriter fn = writer.start_function(name, {}, std::nullopt);
		fn.add_return();
	}
	ASSERT_EQ(print_object(writer.get_obj()),
			  "fn a() {\nbb0: // a start\n  ret\n}\n\n"
			  "fn b() {\nbb0: // b start\n  ret\n}\n\n"
			  "fn c() {\nbb0: // c start\n  ret\n}\n");
}

TEST(vregs_print_in_order_of_id)
{
	IrWriter writer;
	IrFunctionWriter fn = writer.start_function("f", {}, std::nullopt);
	for (int i = 0; i < 12; ++i)
		fn.new_vreg(IrType::u8());
	fn.add_return();

	std::string text = print_object(writer.get_obj());
	size_t last = 0;
	for (int i = 0; i < 12; ++i)
	{
		size_t found = text.find(std::format("  %{}: u8\n", i));
		ASSERT_TRUE(found != std::string::npos);
		ASSERT_TRUE(found >= last);
		last = found;
	}
}

TEST(unreachable_blocks_are_not_printed)
{
	IrWriter writer;
	IrFunctionWriter fn = writer.start_function("f", {}, std::nullopt);
	fn.add_return();
	fn.add_immediate(0, 1); // lands in the block left behind by the return
	ASSERT_TRUE(print_object(writer.get_obj()).find("loadimm") == std::string::npos);
}

TEST(jump_terminator)
{
	ir::Function fn("f", {}, std::nullopt);
	ir::BasicBlock *target = fn.new_bb();
	target->terminator.kind = ir::BasicBlockTerminator::RETURN;
	fn.entry->terminator.kind = ir::BasicBlockTerminator::JUMP;
	fn.entry->terminator.successors[0] = {target, {1, 2}};

	ir::Object obj;
	obj.functions.insert({"f", &fn});
	std::string text = print_object(obj);
	ASSERT_TRUE(text.find("  jump bb1(%1, %2)\n") != std::string::npos);
	ASSERT_TRUE(text.find("bb1:") != std::string::npos);
}

TEST(jump_without_block_arguments)
{
	ir::Function fn("f", {}, std::nullopt);
	ir::BasicBlock *target = fn.new_bb();
	target->terminator.kind = ir::BasicBlockTerminator::RETURN;
	fn.entry->terminator.kind = ir::BasicBlockTerminator::JUMP;
	fn.entry->terminator.successors[0] = {target, {}};

	ir::Object obj;
	obj.functions.insert({"f", &fn});
	ASSERT_TRUE(print_object(obj).find("  jump bb1\n") != std::string::npos);
}

TEST(branch_terminator)
{
	ir::Function fn("f", {}, std::nullopt);
	ir::BasicBlock *yes = fn.new_bb();
	ir::BasicBlock *no = fn.new_bb();
	yes->terminator.kind = ir::BasicBlockTerminator::RETURN;
	no->terminator.kind = ir::BasicBlockTerminator::RETURN;
	fn.entry->terminator.kind = ir::BasicBlockTerminator::BRANCH;
	fn.entry->terminator.successors[0] = {yes, {}};
	fn.entry->terminator.successors[1] = {no, {3}};

	ir::Object obj;
	obj.functions.insert({"f", &fn});
	std::string text = print_object(obj);
	ASSERT_TRUE(text.find("  branch bb1, bb2(%3)\n") != std::string::npos);
	ASSERT_TRUE(text.find("bb1:") < text.find("bb2:"));
}

TEST(blocks_shared_by_two_paths_print_once)
{
	ir::Function fn("f", {}, std::nullopt);
	ir::BasicBlock *shared = fn.new_bb();
	shared->terminator.kind = ir::BasicBlockTerminator::RETURN;
	fn.entry->terminator.kind = ir::BasicBlockTerminator::BRANCH;
	fn.entry->terminator.successors[0] = {shared, {}};
	fn.entry->terminator.successors[1] = {shared, {}};

	ir::Object obj;
	obj.functions.insert({"f", &fn});
	std::string text = print_object(obj);
	size_t first = text.find("bb1:");
	ASSERT_TRUE(first != std::string::npos);
	ASSERT_TRUE(text.find("bb1:", first + 1) == std::string::npos);
}
