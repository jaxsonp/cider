#include "unit_testing.hpp"
#include "backend/codegen/CodeGenerator.hpp"

#include <format>
#include <sstream>
#include <vector>

#include "frontend/AST.hpp"
#include "ir/IrWriter.hpp"
#include "utils/error.hpp"

static const std::vector<std::string> targets_32 = {"linux-riscv32g", "linux-riscv32gc", "linux-x86"};
static const std::vector<std::string> targets_64 = {"linux-riscv64g", "linux-riscv64gc", "linux-x86_64"};
static const std::vector<std::string> all_targets = {"linux-riscv32g", "linux-riscv32gc", "linux-x86", "linux-riscv64g", "linux-riscv64gc", "linux-x86_64"};

static ir::Object to_ir(const std::string &source)
{
	std::istringstream in(source);
	AST ast(in);
	return ast.emit_ir();
}

/// Lowers source to machine code, without running any of it
static Object lower(const std::string &source, const std::string &target)
{
	return Target(target).get_code_generator()->lower_ir(to_ir(source));
}

/// Exit code of lowering source, Success if it worked
static ExitCode lower_result(const std::string &source, const std::string &target)
{
	try
	{
		lower(source, target);
		return ExitCode::Success;
	}
	catch (const CompilerError &err)
	{
		return err.exit_code();
	}
}

/// Textual assembly of source, which has to have a main
static std::string assembly(const std::string &source, const std::string &target)
{
	Target t(target);
	auto generator = t.get_code_generator();
	generator->enable_asm_output();
	Object obj = generator->lower_ir(to_ir(source));
	for (const Object::Function &fn : obj.functions)
	{
		if (fn.name == "main")
			generator->build_runtime_code(fn.code_offset, t);
	}

	std::ostringstream out;
	generator->write_asm(out);
	return out.str();
}

/// Fail the test unless the assembly of a function returning `expr` contains a line starting with `mnemonic`
static void assert_lowers_to(const std::string &target, const std::string &type, const std::string &expr, const std::string &mnemonic)
{
	std::string source = std::format("fn f(a: {0}, b: {0}) -> {0} {{ {1} }}\nfn main() {{}}", type, expr);
	std::string text = assembly(source, target);
	if (text.find("\t" + mnemonic + " ") == std::string::npos)
		testing::unit::detail::fail(__FILE__, __LINE__, std::format("{} of {} {} has no {}:\n{}", target, type, expr, mnemonic, text));
}

/// Fail the test if the assembly of a function returning `expr` contains a line starting with `mnemonic`
static void assert_does_not_lower_to(const std::string &target, const std::string &type, const std::string &expr, const std::string &mnemonic)
{
	std::string source = std::format("fn f(a: {0}, b: {0}) -> {0} {{ {1} }}\nfn main() {{}}", type, expr);
	std::string text = assembly(source, target);
	if (text.find("\t" + mnemonic + " ") != std::string::npos)
		testing::unit::detail::fail(__FILE__, __LINE__, std::format("{} of {} {} has a {}:\n{}", target, type, expr, mnemonic, text));
}

/// Fail the test unless source lowers on every one of these targets
#define ASSERT_LOWERS(source, targets)                                                 \
	do                                                                                 \
	{                                                                                  \
		for (const std::string &target_ : targets)                                     \
			ASSERT_EQ(int(lower_result(source, target_)), int(ExitCode::Success));     \
	} while (0)

// every target ==================================================================================================

TEST(every_operator_of_every_small_type_lowers)
{
	for (const std::string &type : {"i8", "i16", "i32", "u8", "u16", "u32"})
	{
		std::string source;
		int fn = 0;
		for (const std::string &op : {"+", "-", "*", "/", "%", "&", "|", "^"})
			source += std::format("fn f{0}(a: {1}, b: {1}) -> {1} {{ a {2} b }}\n", fn++, type, op);
		for (const std::string &op : {"<<", ">>"})
			source += std::format("fn f{0}(a: {1}, b: u8) -> {1} {{ a {2} b }}\n", fn++, type, op);
		for (const std::string &op : {"==", "!=", "<", "<=", ">", ">="})
			source += std::format("fn f{0}(a: {1}, b: {1}) -> bool {{ a {2} b }}\n", fn++, type, op);
		source += std::format("fn f{0}(a: {1}) -> {1} {{ ~a }}\n", fn++, type);
		ASSERT_LOWERS(source, all_targets);
	}
}

TEST(every_operator_of_the_64_bit_types_lowers_on_64_bit_targets)
{
	for (const std::string &type : {"i64", "u64"})
	{
		std::string source;
		int fn = 0;
		for (const std::string &op : {"+", "-", "*", "/", "%", "&", "|", "^"})
			source += std::format("fn f{0}(a: {1}, b: {1}) -> {1} {{ a {2} b }}\n", fn++, type, op);
		for (const std::string &op : {"<<", ">>"})
			source += std::format("fn f{0}(a: {1}, b: u8) -> {1} {{ a {2} b }}\n", fn++, type, op);
		for (const std::string &op : {"==", "!=", "<", "<=", ">", ">="})
			source += std::format("fn f{0}(a: {1}, b: {1}) -> bool {{ a {2} b }}\n", fn++, type, op);
		ASSERT_LOWERS(source, targets_64);
	}
}

TEST(wider_than_a_register_is_unsupported)
{
	for (const std::string &target : targets_32)
	{
		for (const std::string &source : {
				 "fn f() -> i64 { 1i64 }",
				 "fn f() -> u64 { 1u64 }",
				 "fn f(x: i64) {}",
				 "fn f(x: u64) {}",
				 "fn f() { 1u64; }",
				 "fn f() -> bool { 1i64 == 1i64 }",
				 "fn f() -> i32 { 1i32 << 1u64 }",
			 })
			ASSERT_EQ(int(lower_result(source, target)), int(ExitCode::Unsupported));
	}
}

TEST(sixty_four_bit_values_fit_on_64_bit_targets)
{
	for (const std::string &target : targets_64)
	{
		ASSERT_EQ(int(lower_result("fn f() -> i64 { 1i64 }", target)), int(ExitCode::Success));
		ASSERT_EQ(int(lower_result("fn f() -> i32 { 1i32 << 1u64 }", target)), int(ExitCode::Success));
	}
}

TEST(register_pressure_lowers)
{
	std::string expr = "1u32";
	for (int i = 0; i < 64; ++i)
		expr = std::format("(1u32 + {})", expr);
	ASSERT_LOWERS(std::format("fn f() -> u32 {{ {} }}", expr), all_targets);
}

TEST(frames_too_big_for_a_load_offset_lower)
{
	std::string args;
	for (int i = 0; i < 600; ++i)
		args += std::format("{}x{}: u32", i == 0 ? "" : ", ", i);
	std::string call_args;
	for (int i = 0; i < 600; ++i)
		call_args += std::format("{}{}u32", i == 0 ? "" : ", ", i);
	ASSERT_LOWERS(std::format("fn callee({}) -> u32 {{ x599 }}\nfn f() -> u32 {{ callee({}) }}", args, call_args), all_targets);
}

TEST(calls_with_more_arguments_than_registers_lower)
{
	const std::string source =
		"fn many(a: i8, b: u16, c: i32, d: u8, e: bool, f: i16, g: u32, h: i8, i: u8, j: bool, k: i16) -> bool { j }\n"
		"fn f() -> bool { many(1i8, 2u16, 3i32, 4u8, true, 6i16, 7u32, 8i8, 9u8, false, 11i16) }";
	ASSERT_LOWERS(source, all_targets);
}

// the driver ====================================================================================================

TEST(every_function_ends_up_in_the_object)
{
	for (const std::string &target : all_targets)
	{
		Object obj = lower("fn a() {}\nfn b() {}\nfn c() {}", target);
		ASSERT_EQ(obj.functions.size(), 3u);
		ASSERT_TRUE(!obj.code.empty());
	}
}

TEST(function_offsets_are_distinct_and_in_the_code)
{
	for (const std::string &target : all_targets)
	{
		Object obj = lower("fn a() {}\nfn b() {}\nfn c() {}", target);
		for (size_t i = 0; i < obj.functions.size(); ++i)
		{
			ASSERT_TRUE(obj.functions[i].code_offset < obj.code.size());
			for (size_t j = i + 1; j < obj.functions.size(); ++j)
				ASSERT_TRUE(obj.functions[i].code_offset != obj.functions[j].code_offset);
		}
	}
}

TEST(call_to_an_unknown_function)
{
	IrWriter writer;
	IrFunctionWriter fn = writer.start_function("f", {}, std::nullopt);
	fn.add_call("nowhere", std::nullopt, {});
	fn.add_return();
	ir::Object obj = writer.get_obj();

	for (const std::string &target : all_targets)
		ASSERT_THROWS(Target(target).get_code_generator()->lower_ir(obj), CompilerError);
}

TEST(x86_calls_are_pointed_at_their_targets)
{
	for (const std::string &target : {"linux-x86", "linux-x86_64"})
	{
		Object obj = lower("fn callee() {}\nfn f() { callee(); }", target);

		size_t callee_offset = 0;
		for (const Object::Function &fn : obj.functions)
		{
			if (fn.name == "callee")
				callee_offset = fn.code_offset;
		}

		bool found = false;
		for (size_t at = 0; at + 5 <= obj.code.size(); ++at)
		{
			if (obj.code[at] != 0xE8)
				continue;
			int32_t rel = int32_t(uint32_t(obj.code[at + 1]) | uint32_t(obj.code[at + 2]) << 8 | uint32_t(obj.code[at + 3]) << 16 | uint32_t(obj.code[at + 4]) << 24);
			found |= int64_t(at) + 5 + rel == int64_t(callee_offset);
		}
		ASSERT_TRUE(found);
	}
}

TEST(a_generator_can_be_reused)
{
	for (const std::string &target : all_targets)
	{
		auto generator = Target(target).get_code_generator();
		ir::Object first = to_ir("fn a() {}");
		ir::Object second = to_ir("fn b() { c(); }\nfn c() {}");
		ASSERT_EQ(generator->lower_ir(first).functions.size(), 1u);
		ASSERT_EQ(generator->lower_ir(second).functions.size(), 2u);
	}
}

TEST(runtime_code_exists_for_every_target)
{
	for (const std::string &target : all_targets)
	{
		Target t(target);
		ASSERT_TRUE(!t.get_code_generator()->build_runtime_code(0, t).empty());
	}
}

TEST(compressed_targets_give_smaller_code)
{
	const std::string source = "fn f(a: i32, b: i32) -> i32 { a + b }";
	ASSERT_TRUE(lower(source, "linux-riscv32gc").code.size() < lower(source, "linux-riscv32g").code.size());
	ASSERT_TRUE(lower(source, "linux-riscv64gc").code.size() < lower(source, "linux-riscv64g").code.size());
}

TEST(uncompressed_riscv_code_is_whole_words)
{
	ASSERT_EQ(lower("fn f(a: i32) -> i32 { a }", "linux-riscv32g").code.size() % 4, 0u);
	ASSERT_EQ(lower("fn f(a: i32) -> i32 { a }", "linux-riscv64g").code.size() % 4, 0u);
}

// textual assembly ==============================================================================================

TEST(asm_needs_to_be_enabled_first)
{
	for (const std::string &target : all_targets)
	{
		auto generator = Target(target).get_code_generator();
		std::ostringstream out;
		ASSERT_THROWS(generator->write_asm(out), CompilerError);
	}
}

TEST(asm_layout)
{
	for (const std::string &target : all_targets)
	{
		std::string text = assembly("fn main() {}\nfn other() {}", target);
		size_t globl = text.find("\t.globl _start\n");
		size_t start = text.find("_start:\n");
		size_t main_label = text.find("\"main\":\n");
		size_t other_label = text.find("\"other\":\n");
		ASSERT_TRUE(globl != std::string::npos);
		ASSERT_TRUE(start != std::string::npos);
		ASSERT_TRUE(main_label != std::string::npos);
		ASSERT_TRUE(other_label != std::string::npos);
		ASSERT_TRUE(globl < start);
		ASSERT_TRUE(start < main_label);
		ASSERT_TRUE(start < other_label);
	}
}

TEST(asm_quotes_symbols_that_look_like_registers)
{
	ASSERT_TRUE(assembly("fn rax() {}\nfn main() { rax(); }", "linux-x86_64").find("call \"rax\"") != std::string::npos);
	ASSERT_TRUE(assembly("fn a0() {}\nfn main() { a0(); }", "linux-riscv64g").find("call \"a0\"") != std::string::npos);
}

TEST(asm_directives)
{
	ASSERT_TRUE(assembly("fn main() {}", "linux-x86_64").starts_with("\t.intel_syntax noprefix\n\t.code64\n"));
	ASSERT_TRUE(assembly("fn main() {}", "linux-x86").starts_with("\t.intel_syntax noprefix\n\t.code32\n"));
	ASSERT_TRUE(assembly("fn main() {}", "linux-riscv64gc").find(".option rvc") != std::string::npos);
	ASSERT_TRUE(assembly("fn main() {}", "linux-riscv64g").find(".option rvc") == std::string::npos);
}

// which instruction a type lowers to ============================================================================
// what these instructions do is covered by running them (e2e runtime/), this is that every target picks the one
// the type asks for

TEST(riscv64_uses_word_operations_up_to_32_bits)
{
	const std::vector<std::tuple<std::string, std::string>> cases = {
		{"a + b", "addw"},
		{"a - b", "subw"},
		{"a * b", "mulw"},
		{"a / b", "divw"},
		{"a % b", "remw"},
	};
	for (const auto &[expr, mnemonic] : cases)
		assert_lowers_to("linux-riscv64g", "i32", expr, mnemonic);
	assert_lowers_to("linux-riscv64g", "u32", "a / b", "divuw");
	assert_lowers_to("linux-riscv64g", "u32", "a % b", "remuw");
}

TEST(riscv64_uses_whole_register_operations_for_64_bits)
{
	assert_lowers_to("linux-riscv64g", "i64", "a + b", "add");
	assert_lowers_to("linux-riscv64g", "i64", "a * b", "mul");
	assert_lowers_to("linux-riscv64g", "i64", "a / b", "div");
	assert_lowers_to("linux-riscv64g", "u64", "a / b", "divu");
	assert_lowers_to("linux-riscv64g", "u64", "a % b", "remu");
	assert_does_not_lower_to("linux-riscv64g", "i64", "a + b", "addw");
}

TEST(riscv_shifts_follow_type_and_signedness)
{
	assert_lowers_to("linux-riscv64g", "i32", "a << 3u8", "sllw");
	assert_lowers_to("linux-riscv64g", "i32", "a >> 3u8", "sraw");
	assert_lowers_to("linux-riscv64g", "u32", "a >> 3u8", "srlw");
	assert_lowers_to("linux-riscv64g", "i64", "a << 3u8", "sll");
	assert_lowers_to("linux-riscv64g", "i64", "a >> 3u8", "sra");
	assert_lowers_to("linux-riscv64g", "u64", "a >> 3u8", "srl");
	assert_lowers_to("linux-riscv32g", "i32", "a >> 3u8", "sra");
	assert_lowers_to("linux-riscv32g", "u32", "a >> 3u8", "srl");
}

TEST(riscv_division_follows_signedness)
{
	assert_lowers_to("linux-riscv32g", "i32", "a / b", "div");
	assert_lowers_to("linux-riscv32g", "u32", "a / b", "divu");
	assert_lowers_to("linux-riscv32g", "i32", "a % b", "rem");
	assert_lowers_to("linux-riscv32g", "u32", "a % b", "remu");
}

TEST(x86_division_follows_signedness)
{
	for (const std::string &target : {"linux-x86", "linux-x86_64"})
	{
		assert_lowers_to(target, "i32", "a / b", "idiv");
		assert_lowers_to(target, "u32", "a / b", "div");
		assert_does_not_lower_to(target, "i32", "a / b", "div");
		assert_does_not_lower_to(target, "u32", "a / b", "idiv");
	}
	assert_lowers_to("linux-x86_64", "i64", "a / b", "idiv");
	assert_lowers_to("linux-x86_64", "u64", "a / b", "div");
}

TEST(x86_right_shift_follows_signedness)
{
	for (const std::string &target : {"linux-x86", "linux-x86_64"})
	{
		assert_lowers_to(target, "i32", "a >> 3u8", "sar");
		assert_lowers_to(target, "u32", "a >> 3u8", "shr");
		assert_does_not_lower_to(target, "i32", "a >> 3u8", "shr");
		assert_does_not_lower_to(target, "u32", "a >> 3u8", "sar");
	}
}

/// Fail the test unless comparing two values of `type` on x86 sets from condition `suffix`
static void assert_compares_with(const std::string &type, const std::string &op, const std::string &suffix)
{
	std::string source = std::format("fn f(a: {0}, b: {0}) -> bool {{ a {1} b }}\nfn main() {{}}", type, op);
	std::string text = assembly(source, "linux-x86_64");
	if (text.find(std::format("\tset{} ", suffix)) == std::string::npos)
		testing::unit::detail::fail(__FILE__, __LINE__, std::format("{} {} has no set{}:\n{}", type, op, suffix, text));
}

TEST(x86_comparison_conditions)
{
	assert_compares_with("i32", "==", "e");
	assert_compares_with("i32", "!=", "ne");
	assert_compares_with("i32", "<", "l");
	assert_compares_with("i32", "<=", "le");
	assert_compares_with("i32", ">", "g");
	assert_compares_with("i32", ">=", "ge");
	assert_compares_with("u32", "==", "e");
	assert_compares_with("u32", "!=", "ne");
	assert_compares_with("u32", "<", "b");
	assert_compares_with("u32", "<=", "be");
	assert_compares_with("u32", ">", "a");
	assert_compares_with("u32", ">=", "ae");
	assert_compares_with("bool", "==", "e");
}
