#include "unit_testing.hpp"
#include "source_testing.hpp"

#include <format>
#include <regex>

using testing::unit::source_to_ir;

/// IR of the first function of a program. They're printed sorted by name, so call it `aaa`
static std::string first_function_ir(const std::string &source)
{
	std::string ir = source_to_ir(source);
	return ir.substr(0, ir.find("}\n") + 2);
}

// expressions ===================================================================================================

TEST(integer_literal_is_a_loadimm_of_its_type)
{
	for (const std::string &type : {"i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64"})
	{
		ASSERT_EQ(source_to_ir(std::format("fn f() -> {0} {{ 7{0} }}", type)),
				  std::format("fn f() -> {0} {{\n  %0: {0}\nbb0: // f start\n  %0 = loadimm 7\n  ret %0\n}}\n", type));
	}
}

TEST(integer_literal_keeps_all_of_its_bits)
{
	std::string ir = source_to_ir("fn f() -> u64 { 18446744073709551615u64 }");
	ASSERT_TRUE(ir.find("loadimm 18446744073709551615") != std::string::npos);
}

TEST(boolean_literals_are_one_and_zero)
{
	ASSERT_TRUE(source_to_ir("fn f() -> bool { true }").find("%0 = loadimm 1") != std::string::npos);
	ASSERT_TRUE(source_to_ir("fn f() -> bool { false }").find("%0 = loadimm 0") != std::string::npos);
}

TEST(binary_operators)
{
	const std::vector<std::pair<std::string, std::string>> arithmetic = {
		{"+", "add"},
		{"-", "sub"},
		{"*", "mul"},
		{"/", "div"},
		{"%", "rem"},
		{"&", "and"},
		{"|", "or"},
		{"^", "xor"},
	};
	for (const auto &[op, mnemonic] : arithmetic)
	{
		ASSERT_EQ(source_to_ir(std::format("fn f() -> i16 {{ 1i16 {} 2i16 }}", op)),
				  std::format("fn f() -> i16 {{\n  %0: i16\n  %1: i16\n  %2: i16\nbb0: // f start\n"
							  "  %0 = loadimm 1\n  %1 = loadimm 2\n  %2 = {} %0 %1\n  ret %2\n}}\n",
							  mnemonic));
	}
}

TEST(comparison_operators_give_a_bool)
{
	const std::vector<std::pair<std::string, std::string>> comparisons = {
		{"==", "cmp_eq"},
		{"!=", "cmp_ne"},
		{"<", "cmp_lt"},
		{"<=", "cmp_lte"},
		{">", "cmp_gt"},
		{">=", "cmp_gte"},
	};
	for (const auto &[op, mnemonic] : comparisons)
	{
		ASSERT_EQ(source_to_ir(std::format("fn f() -> bool {{ 1u8 {} 2u8 }}", op)),
				  std::format("fn f() -> bool {{\n  %0: u8\n  %1: u8\n  %2: bool\nbb0: // f start\n"
							  "  %0 = loadimm 1\n  %1 = loadimm 2\n  %2 = {} %0 %1\n  ret %2\n}}\n",
							  mnemonic));
	}
}

TEST(shift_operators_keep_their_operands_own_types)
{
	ASSERT_EQ(source_to_ir("fn f() -> i32 { 1i32 << 2u8 }"),
			  "fn f() -> i32 {\n  %0: i32\n  %1: u8\n  %2: i32\nbb0: // f start\n"
			  "  %0 = loadimm 1\n  %1 = loadimm 2\n  %2 = shl %0 %1\n  ret %2\n}\n");
	ASSERT_TRUE(source_to_ir("fn f() -> i32 { 1i32 >> 2u8 }").find("%2 = shr %0 %1") != std::string::npos);
}

TEST(operands_are_evaluated_left_to_right)
{
	std::string ir = source_to_ir("fn a() -> i32 { 1i32 }\nfn b() -> i32 { 2i32 }\nfn main() -> i32 { a() + b() }");
	ASSERT_TRUE(ir.find("call a()") < ir.find("call b()"));
}

TEST(negation_and_bitwise_not)
{
	ASSERT_TRUE(source_to_ir("fn f() -> i8 { -1i8 }").find("%1 = neg %0") != std::string::npos);
	ASSERT_TRUE(source_to_ir("fn f() -> u8 { ~1u8 }").find("%1 = not %0") != std::string::npos);
}

TEST(logical_not_is_an_xor_with_one)
{
	ASSERT_EQ(source_to_ir("fn f() -> bool { !true }"),
			  "fn f() -> bool {\n  %0: bool\n  %1: bool\n  %2: bool\nbb0: // f start\n"
			  "  %0 = loadimm 1\n  %2 = loadimm 1\n  %1 = xor %0 %2\n  ret %1\n}\n");
}

TEST(logical_not_shares_its_constant)
{
	std::string ir = source_to_ir("fn f(a: bool, b: bool) { !a; !b; }");
	size_t first = ir.find("loadimm 1");
	ASSERT_TRUE(first != std::string::npos);
	ASSERT_TRUE(ir.find("loadimm 1", first + 1) == std::string::npos);
}

TEST(parameters_are_loaded_in_order)
{
	ASSERT_EQ(source_to_ir("fn f(a: i8, b: u32, c: bool) -> u32 { b }"),
			  "fn f(i8, u32, bool) -> u32 {\n  %0: i8\n  %1: u32\n  %2: bool\nbb0: // f start\n"
			  "  %0 = loadarg 0\n  %1 = loadarg 1\n  %2 = loadarg 2\n  ret %1\n}\n");
}

TEST(a_parameter_is_the_same_vreg_every_time)
{
	std::string ir = source_to_ir("fn f(a: i32) -> i32 { a + a }");
	ASSERT_TRUE(ir.find("%1 = add %0 %0") != std::string::npos);
}

// calls =========================================================================================================

TEST(call_with_arguments_and_a_result)
{
	std::string ir = source_to_ir("fn g(a: i32, b: u8) -> u16 { 1u16 }\nfn main() -> u16 { g(1i32, 2u8) }");
	ASSERT_TRUE(ir.find("  %0 = loadimm 1\n  %1 = loadimm 2\n  %2 = call g(%0, %1)\n  ret %2\n") != std::string::npos);
	ASSERT_TRUE(ir.find("  %2: u16\n") != std::string::npos);
}

TEST(call_to_a_void_function_has_no_destination)
{
	ASSERT_TRUE(source_to_ir("fn g() {}\nfn main() { g(); }").find("  call g()\n") != std::string::npos);
}

TEST(arguments_are_evaluated_left_to_right)
{
	std::string ir = source_to_ir("fn a() -> i32 { 1i32 }\nfn b() -> i32 { 2i32 }\nfn g(x: i32, y: i32) {}\nfn main() { g(a(), b()); }");
	ASSERT_TRUE(ir.find("call a()") < ir.find("call b()"));
	ASSERT_TRUE(ir.find("call b()") < ir.find("call g("));
}

TEST(parenthesized_callee_is_a_direct_call)
{
	ASSERT_TRUE(source_to_ir("fn g() {}\nfn main() { (g)(); }").find("  call g()\n") != std::string::npos);
}

// statements and returns ========================================================================================

TEST(return_statement_with_a_value)
{
	ASSERT_EQ(source_to_ir("fn f() -> i32 { return 5i32; }"), "fn f() -> i32 {\n  %0: i32\nbb0: // f start\n  %0 = loadimm 5\n  ret %0\n}\n");
}

TEST(trailing_expression_is_the_same_as_a_return)
{
	ASSERT_EQ(source_to_ir("fn f() -> i32 { 5i32 }"), source_to_ir("fn f() -> i32 { return 5i32; }"));
}

TEST(void_function_gets_an_implicit_return)
{
	ASSERT_EQ(source_to_ir("fn f() {}"), "fn f() {\nbb0: // f start\n  ret\n}\n");
	ASSERT_EQ(source_to_ir("fn f() {}"), source_to_ir("fn f() { return; }"));
}

TEST(void_call_as_a_trailing_expression_is_still_called)
{
	ASSERT_TRUE(source_to_ir("fn g() {}\nfn main() { g() }").find("  call g()\n  ret\n") != std::string::npos);
}

TEST(returning_a_void_call_is_still_a_call)
{
	ASSERT_TRUE(source_to_ir("fn g() {}\nfn main() { return g(); }").find("  call g()\n  ret\n") != std::string::npos);
}

TEST(expression_statements_are_evaluated_and_dropped)
{
	ASSERT_EQ(source_to_ir("fn f() { 1i32 + 2i32; }"),
			  "fn f() {\n  %0: i32\n  %1: i32\n  %2: i32\nbb0: // f start\n"
			  "  %0 = loadimm 1\n  %1 = loadimm 2\n  %2 = add %0 %1\n  ret\n}\n");
}

TEST(code_after_a_return_is_not_reachable)
{
	ASSERT_EQ(first_function_ir("fn aaa() -> i32 { return 1i32; return 2i32; }\nfn main() {}"),
			  "fn aaa() -> i32 {\n  %0: i32\n  %1: i32\nbb0: // aaa start\n  %0 = loadimm 1\n  ret %0\n}\n");
}

// local variables ===============================================================================================

/// Number of times `needle` appears in `haystack`
static size_t count(const std::string &haystack, const std::string &needle)
{
	size_t n = 0;
	for (size_t pos = haystack.find(needle); pos != std::string::npos; pos = haystack.find(needle, pos + 1))
		++n;
	return n;
}

TEST(local_initializer_is_evaluated_once)
{
	std::string ir = source_to_ir("fn g() -> i32 { 1i32 }\nfn main() -> i32 { let x: i32 = g(); x + x }");
	ASSERT_EQ(count(ir, "call g()"), 1u);
	// both reads of x are the same vreg, like a_parameter_is_the_same_vreg_every_time
	ASSERT_TRUE(std::regex_search(ir, std::regex(R"(= add (%\d+) \1\n)")));
}

TEST(unused_local_initializer_is_still_evaluated)
{
	ASSERT_EQ(count(source_to_ir("fn g() -> i32 { 1i32 }\nfn main() { let x: i32 = g(); }"), "call g()"), 1u);
}

TEST(initializers_are_evaluated_in_declaration_order)
{
	std::string ir = source_to_ir("fn a() -> i32 { 1i32 }\nfn b() -> i32 { 2i32 }\nfn g(x: i32, y: i32) {}\n"
								  "fn main() { let y: i32 = b(); let x: i32 = a(); g(x, y); }");
	ASSERT_TRUE(ir.find("call b()") < ir.find("call a()"));
	ASSERT_TRUE(ir.find("call a()") < ir.find("call g("));
}

TEST(locals_and_statements_interleave)
{
	std::string ir = source_to_ir("fn a() {}\nfn b() -> u8 { 1u8 }\nfn main() -> u8 { a(); let x: u8 = b(); a(); x }");
	size_t first_a = ir.find("call a()");
	ASSERT_TRUE(first_a < ir.find("call b()"));
	ASSERT_TRUE(ir.find("call b()") < ir.find("call a()", first_a + 1));
}

TEST(shadowing_local_is_read_after_its_declaration)
{
	// %0 is the parameter, the trailing x has to be the local that shadows it
	std::string ir = source_to_ir("fn f(x: i32) -> i32 { let x: i32 = 5i32; x }");
	ASSERT_TRUE(ir.find("  ret %0\n") == std::string::npos);
}

TEST(shadowing_initializer_reads_the_shadowed_local)
{
	std::string ir = source_to_ir("fn f(x: i32) -> i32 { let x: i32 = x + x; x }");
	ASSERT_TRUE(ir.find("= add %0 %0\n") != std::string::npos);
	ASSERT_TRUE(ir.find("  ret %0\n") == std::string::npos);
}

// functions =====================================================================================================

TEST(every_function_is_emitted)
{
	std::string ir = source_to_ir("fn a() {}\nfn b() {}\nfn c() {}");
	ASSERT_TRUE(ir.find("fn a()") != std::string::npos);
	ASSERT_TRUE(ir.find("fn b()") != std::string::npos);
	ASSERT_TRUE(ir.find("fn c()") != std::string::npos);
}

TEST(vreg_numbering_starts_over_in_each_function)
{
	std::string ir = source_to_ir("fn a() -> i32 { 1i32 }\nfn b() -> i32 { 1i32 }");
	ASSERT_TRUE(ir.find("%1") == std::string::npos);
}
