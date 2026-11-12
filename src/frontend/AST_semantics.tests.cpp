#include "unit_testing.hpp"
#include "source_testing.hpp"

#include <format>
#include <vector>

#include "frontend/FrontendType.hpp"

static const std::vector<std::string> int_types = {"i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64"};
static const std::vector<std::string> signed_types = {"i8", "i16", "i32", "i64"};
static const std::vector<std::string> unsigned_types = {"u8", "u16", "u32", "u64"};
/// Every type a value can have
static const std::vector<std::string> value_types = {"bool", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64"};

/// A literal of some type
static std::string value_of(const std::string &type)
{
	return type == "bool" ? "true" : "1" + type;
}

/// Source with an expression in a function, where it's checked but gives nothing that needs a type to match
static std::string as_statement(const std::string &expr)
{
	return std::format("fn f() {{ {}; }}", expr);
}

static bool is_integer(const std::string &type)
{
	return type != "bool";
}

static bool is_unsigned(const std::string &type)
{
	return type[0] == 'u';
}

static constexpr ExitCode OK = ExitCode::Success;
static constexpr ExitCode NAME_ERROR = ExitCode::NameError;
static constexpr ExitCode TYPE_ERROR = ExitCode::TypeError;
static constexpr ExitCode SEMANTIC_ERROR = ExitCode::SemanticError;
static constexpr ExitCode SYNTAX_ERROR = ExitCode::SyntaxError;

// binary operators, every operator with every pair of types =====================================================

TEST(arithmetic_needs_two_integers_of_one_type)
{
	for (const std::string &op : {"+", "-", "*", "/", "%"})
	{
		for (const std::string &l : value_types)
		{
			for (const std::string &r : value_types)
				ASSERT_SOURCE(as_statement(std::format("{} {} {}", value_of(l), op, value_of(r))), l == r && is_integer(l) ? OK : TYPE_ERROR);
		}
	}
}

TEST(bitwise_needs_two_integers_of_one_type)
{
	for (const std::string &op : {"&", "|", "^"})
	{
		for (const std::string &l : value_types)
		{
			for (const std::string &r : value_types)
				ASSERT_SOURCE(as_statement(std::format("{} {} {}", value_of(l), op, value_of(r))), l == r && is_integer(l) ? OK : TYPE_ERROR);
		}
	}
}

TEST(shift_needs_an_integer_and_an_unsigned_integer)
{
	for (const std::string &op : {"<<", ">>"})
	{
		for (const std::string &l : value_types)
		{
			for (const std::string &r : value_types)
				ASSERT_SOURCE(as_statement(std::format("{} {} {}", value_of(l), op, value_of(r))), is_integer(l) && is_unsigned(r) ? OK : TYPE_ERROR);
		}
	}
}

TEST(comparison_needs_two_values_of_one_type)
{
	for (const std::string &op : {"==", "!=", "<", "<=", ">", ">="})
	{
		for (const std::string &l : value_types)
		{
			for (const std::string &r : value_types)
				ASSERT_SOURCE(as_statement(std::format("{} {} {}", value_of(l), op, value_of(r))), l == r ? OK : TYPE_ERROR);
		}
	}
}

TEST(binary_operator_results)
{
	for (const std::string &type : int_types)
	{
		for (const std::string &op : {"+", "-", "*", "/", "%", "&", "|", "^"})
		{
			std::string expr = std::format("1{0} {1} 1{0}", type, op);
			ASSERT_SOURCE(std::format("fn f() -> {} {{ {} }}", type, expr), OK);
			ASSERT_SOURCE(std::format("fn f() -> bool {{ {} }}", expr), TYPE_ERROR);
		}
		for (const std::string &op : {"==", "!=", "<", "<=", ">", ">="})
		{
			std::string expr = std::format("1{0} {1} 1{0}", type, op);
			ASSERT_SOURCE(std::format("fn f() -> bool {{ {} }}", expr), OK);
			ASSERT_SOURCE(std::format("fn f() -> {} {{ {} }}", type, expr), TYPE_ERROR);
		}
	}
}

TEST(shift_result_has_the_type_of_its_left_side)
{
	for (const std::string &l : int_types)
	{
		for (const std::string &r : unsigned_types)
		{
			ASSERT_SOURCE(std::format("fn f() -> {} {{ 1{} << 1{} }}", l, l, r), OK);
			ASSERT_SOURCE(std::format("fn f() -> {} {{ 1{} >> 1{} }}", l, l, r), OK);
		}
	}
	ASSERT_SOURCE("fn f() -> u8 { 1i32 << 1u8 }", TYPE_ERROR);
}

TEST(nested_expressions_are_type_checked)
{
	ASSERT_SOURCE(as_statement("1i32 + (1i32 + 1u32)"), TYPE_ERROR);
	ASSERT_SOURCE(as_statement("(1i32 < 2i32) + 1i32"), TYPE_ERROR);
	ASSERT_SOURCE(as_statement("-(1u8 + 1u8)"), TYPE_ERROR);
	ASSERT_SOURCE(as_statement("(1i32 < 2i32) == (3u8 > 4u8)"), OK);
}

TEST(logical_operators_are_unimplemented)
{
	ASSERT_SOURCE(as_statement("true || false"), ExitCode::Unimplemented);
	ASSERT_SOURCE(as_statement("true && false"), ExitCode::Unimplemented);
}

// unary operators ===============================================================================================

TEST(negation_needs_a_signed_integer)
{
	for (const std::string &type : signed_types)
		ASSERT_SOURCE(as_statement("-" + value_of(type)), OK);
	for (const std::string &type : unsigned_types)
		ASSERT_SOURCE(as_statement("-" + value_of(type)), TYPE_ERROR);
	ASSERT_SOURCE(as_statement("-true"), TYPE_ERROR);
}

TEST(bitwise_not_needs_an_integer)
{
	for (const std::string &type : int_types)
		ASSERT_SOURCE(as_statement("~" + value_of(type)), OK);
	ASSERT_SOURCE(as_statement("~true"), TYPE_ERROR);
}

TEST(logical_not_needs_a_bool)
{
	for (const std::string &type : int_types)
		ASSERT_SOURCE(as_statement("!" + value_of(type)), TYPE_ERROR);
	ASSERT_SOURCE(as_statement("!true"), OK);
	ASSERT_SOURCE(as_statement("!(1i32 < 2i32)"), OK);
}

TEST(unary_result_has_the_type_of_its_operand)
{
	for (const std::string &type : signed_types)
		ASSERT_SOURCE(std::format("fn f() -> {0} {{ -1{0} }}", type), OK);
	for (const std::string &type : int_types)
		ASSERT_SOURCE(std::format("fn f() -> {0} {{ ~1{0} }}", type), OK);
	ASSERT_SOURCE("fn f() -> bool { !true }", OK);
}

// names =========================================================================================================

TEST(undefined_names)
{
	ASSERT_SOURCE("fn main() -> i32 { return missing(); }", NAME_ERROR);
	ASSERT_SOURCE("fn main() -> i32 { missing(); return 0i32; }", NAME_ERROR);
	ASSERT_SOURCE("fn main() -> i32 { x }", NAME_ERROR);
	ASSERT_SOURCE("fn main() { x; }", NAME_ERROR);
	ASSERT_SOURCE("fn main() -> i32 { return x; }", NAME_ERROR);
	ASSERT_SOURCE("fn main() { 1i32 + x; }", NAME_ERROR);
	ASSERT_SOURCE("fn main() { x + 1i32; }", NAME_ERROR);
	ASSERT_SOURCE("fn main() { -x; }", NAME_ERROR);
}

TEST(params_are_not_visible_in_other_functions)
{
	ASSERT_SOURCE("fn f(x: i32) -> i32 { x }\nfn main() -> i32 { x }", NAME_ERROR);
}

TEST(undefined_name_in_an_argument)
{
	ASSERT_SOURCE("fn add(a: i32, b: i32) -> i32 { a + b }\nfn main() -> i32 { add(y, 1i32) }", NAME_ERROR);
}

TEST(duplicate_names)
{
	ASSERT_SOURCE("fn five() -> i32 { 5i32 }\nfn five() -> i32 { 6i32 }\nfn main() -> i32 { five() }", NAME_ERROR);
	ASSERT_SOURCE("fn f(a: i32, a: i32) -> i32 { a }\nfn main() -> i32 { f(1i32, 2i32) }", NAME_ERROR);
	ASSERT_SOURCE("fn f(a: i32, a: u8) {}", NAME_ERROR);
	ASSERT_SOURCE("fn main() {}\nfn main() {}", NAME_ERROR);
}

TEST(functions_are_hoisted)
{
	ASSERT_SOURCE("fn main() -> i32 { return later(); }\nfn later() -> i32 { return 9i32; }", OK);
	ASSERT_SOURCE("fn a() { b(); }\nfn b() { a(); }", OK);
	ASSERT_SOURCE("fn a() { a(); }", OK);
}

TEST(params_cannot_shadow_functions)
{
	// functions are top level, and only locals can shadow
	ASSERT_SOURCE("fn five() -> i32 { 5i32 }\nfn g(five: i32) -> i32 { five }\nfn main() -> i32 { g(1i32) }", NAME_ERROR);
	ASSERT_SOURCE("fn a(a: i32) -> i32 { a }\nfn main() -> i32 { a(1i32) }", NAME_ERROR);
	// not even a function defined later, since functions are hoisted
	ASSERT_SOURCE("fn g(five: i32) -> i32 { five }\nfn five() -> i32 { 5i32 }", NAME_ERROR);
}

// functions as values ===========================================================================================

TEST(functions_are_not_values)
{
	const std::string five = "fn five() -> i32 { 5i32 }\n";
	ASSERT_SOURCE(five + "fn main() -> bool { five == five() }", TYPE_ERROR);
	ASSERT_SOURCE(five + "fn main() -> bool { five == five }", TYPE_ERROR);
	ASSERT_SOURCE(five + "fn main() -> bool { five != five }", TYPE_ERROR);
	ASSERT_SOURCE(five + "fn main() -> bool { five < five }", TYPE_ERROR);
	ASSERT_SOURCE(five + "fn main() -> i32 { five + 1i32 }", TYPE_ERROR);
	ASSERT_SOURCE(five + "fn main() -> i32 { return five; }", TYPE_ERROR);
	ASSERT_SOURCE(five + "fn main() -> i32 { five; 0i32 }", TYPE_ERROR);
	ASSERT_SOURCE(five + "fn main() -> i32 { five }", TYPE_ERROR);
	ASSERT_SOURCE(five + "fn main() { -five; }", TYPE_ERROR);
	ASSERT_SOURCE(five + "fn id(x: i32) -> i32 { x }\nfn main() -> i32 { id(five) }", TYPE_ERROR);
}

TEST(parenthesized_callee_is_still_a_direct_call)
{
	ASSERT_SOURCE("fn five() -> i32 { 5i32 }\nfn main() -> i32 { (five)() }", OK);
}

// calls =========================================================================================================

TEST(only_functions_are_callable)
{
	const std::string five = "fn five() -> i32 { 5i32 }\n";
	ASSERT_SOURCE("fn main(x: i32) -> i32 { return x(); }", TYPE_ERROR);
	ASSERT_SOURCE(five + "fn main() -> i32 { five()() }", TYPE_ERROR);
	ASSERT_SOURCE(five + "fn main() -> i32 { five()(1i32)(2i32) }", TYPE_ERROR);
	// the inner call is checked first, so its argument count is the reported error
	ASSERT_SOURCE(five + "fn main() -> i32 { five(1i32)() }", TYPE_ERROR);
	ASSERT_SOURCE("fn main() { 1i32(); }", TYPE_ERROR);
	ASSERT_SOURCE("fn main() { true(); }", TYPE_ERROR);
	ASSERT_SOURCE("fn main() { (1i32 + 2i32)(); }", TYPE_ERROR);
}

TEST(argument_count_has_to_match)
{
	const std::string add = "fn add(a: i32, b: i32) -> i32 { return a + b; }\n";
	ASSERT_SOURCE(add + "fn main() -> i32 { return add(1i32); }", TYPE_ERROR);
	ASSERT_SOURCE(add + "fn main() -> i32 { return add(1i32, 2i32, 3i32); }", TYPE_ERROR);
	ASSERT_SOURCE(add + "fn main() -> i32 { return add(); }", TYPE_ERROR);
	ASSERT_SOURCE(add + "fn main() -> i32 { return add(1i32, 2i32); }", OK);
	ASSERT_SOURCE("fn nothing() {}\nfn main() -> i32 { nothing(1i32); return 0i32; }", TYPE_ERROR);
}

TEST(argument_type_has_to_match_exactly)
{
	for (const std::string &param : value_types)
	{
		for (const std::string &arg : value_types)
		{
			std::string source = std::format("fn take(x: {}) {{}}\nfn main() {{ take({}); }}", param, value_of(arg));
			ASSERT_SOURCE(source, param == arg ? OK : TYPE_ERROR);
		}
	}
}

TEST(every_argument_is_checked)
{
	const std::string add = "fn add(a: i32, b: i32) -> i32 { a + b }\n";
	ASSERT_SOURCE(add + "fn main() -> i32 { return add(1u32, 2i32); }", TYPE_ERROR);
	ASSERT_SOURCE(add + "fn main() -> i32 { return add(1i32, 2u32); }", TYPE_ERROR);
	ASSERT_SOURCE(add + "fn main() -> i32 { add(true, 1i32) }", TYPE_ERROR);
	ASSERT_SOURCE(add + "fn main() -> i32 { add(add(1i32, 2u32), 3i32) }", TYPE_ERROR);
}

TEST(call_has_the_type_of_the_function_return)
{
	for (const std::string &ret : value_types)
	{
		for (const std::string &used_as : value_types)
		{
			std::string source = std::format("fn get() -> {0} {{ {1} }}\nfn main() -> {2} {{ get() }}", ret, value_of(ret), used_as);
			ASSERT_SOURCE(source, ret == used_as ? OK : TYPE_ERROR);
		}
	}
}

TEST(void_results_are_not_values)
{
	const std::string nothing = "fn nothing() {}\n";
	ASSERT_SOURCE(nothing + "fn main() -> i32 { nothing() }", TYPE_ERROR);
	ASSERT_SOURCE(nothing + "fn main() -> i32 { nothing() + 1i32 }", TYPE_ERROR);
	ASSERT_SOURCE(nothing + "fn add(a: i32, b: i32) -> i32 { a + b }\nfn main() -> i32 { add(nothing(), 1i32) }", TYPE_ERROR);
	ASSERT_SOURCE(nothing + "fn main() { -nothing(); }", TYPE_ERROR);
	ASSERT_SOURCE(nothing + "fn main() { ~nothing(); }", TYPE_ERROR);
	ASSERT_SOURCE(nothing + "fn main() { !nothing(); }", TYPE_ERROR);
}

TEST(statements_are_checked)
{
	ASSERT_SOURCE("fn nothing() {}\nfn main() -> i32 { nothing(1i32); return 0i32; }", TYPE_ERROR);
	ASSERT_SOURCE("fn main() { 1i32 + 1u32; }", TYPE_ERROR);
}

// parameters ====================================================================================================

TEST(params_have_their_declared_type)
{
	for (const std::string &param : value_types)
	{
		for (const std::string &ret : value_types)
			ASSERT_SOURCE(std::format("fn f(x: {}) -> {} {{ x }}", param, ret), param == ret ? OK : TYPE_ERROR);
	}
}

TEST(params_cannot_be_void)
{
	ASSERT_SOURCE("fn f(x: void) {}\nfn main() -> i32 { 0i32 }", TYPE_ERROR);
	ASSERT_SOURCE("fn f(a: i32, x: void) {}", TYPE_ERROR);
}

// returning =====================================================================================================

TEST(return_statement_has_to_match_exactly)
{
	for (const std::string &ret : value_types)
	{
		for (const std::string &value : value_types)
			ASSERT_SOURCE(std::format("fn f() -> {} {{ return {}; }}", ret, value_of(value)), ret == value ? OK : TYPE_ERROR);
	}
}

TEST(trailing_expression_has_to_match_exactly)
{
	for (const std::string &ret : value_types)
	{
		for (const std::string &value : value_types)
			ASSERT_SOURCE(std::format("fn f() -> {} {{ {} }}", ret, value_of(value)), ret == value ? OK : TYPE_ERROR);
	}
}

TEST(bare_return_only_in_void_functions)
{
	ASSERT_SOURCE("fn main() -> u32 { return; }", TYPE_ERROR);
	ASSERT_SOURCE("fn main() { return; }", OK);
	ASSERT_SOURCE("fn main() -> void { return; }", OK);
}

TEST(void_function_cannot_return_a_value)
{
	ASSERT_SOURCE("fn main() { return 1i32; }", TYPE_ERROR);
	ASSERT_SOURCE("fn main() { 1i32 }", TYPE_ERROR);
	ASSERT_SOURCE("fn five() -> i32 { 5i32 }\nfn main() { return five(); }", TYPE_ERROR);
}

TEST(void_function_can_return_a_void_call)
{
	ASSERT_SOURCE("fn nothing() { return; }\nfn main() { return nothing(); }", OK);
	ASSERT_SOURCE("fn nothing() {}\nfn main() { nothing() }", OK);
}

TEST(non_void_function_has_to_end_by_returning)
{
	const std::string five = "fn five() -> i32 { 5i32 }\n";
	ASSERT_SOURCE("fn main() -> u32 {}", TYPE_ERROR);
	ASSERT_SOURCE("fn main() -> i32 { 5i32; }", TYPE_ERROR);
	ASSERT_SOURCE(five + "fn main() -> i32 { five(); }", TYPE_ERROR);
	ASSERT_SOURCE(five + "fn main() -> i32 { return 1i32; five(); }", TYPE_ERROR);
}

TEST(void_function_can_fall_off_the_end)
{
	ASSERT_SOURCE("fn main() {}", OK);
	ASSERT_SOURCE("fn main() { 1i32; }", OK);
}

TEST(statements_before_the_end_of_a_function)
{
	const std::string five = "fn five() -> i32 { 5i32 }\n";
	ASSERT_SOURCE(five + "fn main() -> i32 { five(); 4i32 }", OK);
	ASSERT_SOURCE(five + "fn main() -> i32 { five(); return 4i32; }", OK);
	ASSERT_SOURCE("fn main() -> i32 { return 1i32; return 2i32; }", OK);
}

TEST(each_function_is_checked_against_its_own_return_type)
{
	ASSERT_SOURCE("fn a() -> i32 { 1i32 }\nfn b() -> u8 { 1u8 }\nfn c() { }", OK);
	ASSERT_SOURCE("fn a() -> i32 { 1i32 }\nfn b() -> u8 { 1i32 }", TYPE_ERROR);
	ASSERT_SOURCE("fn a() -> u32 { return 1u32; }\nfn b() -> i32 { return 1u32; }", TYPE_ERROR);
}

// local variables ===============================================================================================

TEST(local_has_its_declared_type)
{
	for (const std::string &decl : value_types)
	{
		for (const std::string &ret : value_types)
		{
			std::string source = std::format("fn f() -> {1} {{ let x: {0} = {2}; x }}", decl, ret, value_of(decl));
			ASSERT_SOURCE(source, decl == ret ? OK : TYPE_ERROR);
		}
	}
}

TEST(initializer_has_to_match_the_declared_type_exactly)
{
	for (const std::string &decl : value_types)
	{
		for (const std::string &value : value_types)
			ASSERT_SOURCE(as_statement(std::format("let x: {} = {}", decl, value_of(value))), decl == value ? OK : TYPE_ERROR);
	}
}

TEST(initializer_type_is_checked_against_a_call_result)
{
	const std::string five = "fn five() -> i32 { 5i32 }\n";
	ASSERT_SOURCE(five + "fn main() { let x: i32 = five(); }", OK);
	ASSERT_SOURCE(five + "fn main() { let x: u32 = five(); }", TYPE_ERROR);
	ASSERT_SOURCE(five + "fn main() { let x: bool = five() == 5i32; }", OK);
	ASSERT_SOURCE(five + "fn main() { let x: i32 = five() == 5i32; }", TYPE_ERROR);
}

TEST(locals_cannot_be_void)
{
	const std::string nothing = "fn nothing() {}\n";
	ASSERT_SOURCE(nothing + "fn main() { let x: void = nothing(); }", TYPE_ERROR);
	ASSERT_SOURCE(nothing + "fn main() { let x: i32 = nothing(); }", TYPE_ERROR);
	ASSERT_SOURCE("fn main() { let x: void = 1i32; }", TYPE_ERROR);
}

TEST(initializer_is_type_checked)
{
	const std::string five = "fn five() -> i32 { 5i32 }\n";
	ASSERT_SOURCE(as_statement("let x: i32 = 1i32 + 1u32"), TYPE_ERROR);
	ASSERT_SOURCE(as_statement("let x: i32 = -1u32"), TYPE_ERROR);
	ASSERT_SOURCE(five + "fn main() { let x: i32 = five(1i32); }", TYPE_ERROR);
	ASSERT_SOURCE(five + "fn main() { let x: i32 = five; }", TYPE_ERROR);
	ASSERT_SOURCE(as_statement("let x: i32 = 1i32()"), TYPE_ERROR);
}

TEST(initializer_names_are_resolved)
{
	ASSERT_SOURCE(as_statement("let x: i32 = y"), NAME_ERROR);
	ASSERT_SOURCE(as_statement("let x: i32 = 1i32 + y"), NAME_ERROR);
	ASSERT_SOURCE(as_statement("let x: i32 = missing()"), NAME_ERROR);
}

TEST(local_is_not_visible_in_its_own_initializer)
{
	ASSERT_SOURCE(as_statement("let x: i32 = x"), NAME_ERROR);
	ASSERT_SOURCE(as_statement("let x: i32 = x + 1i32"), NAME_ERROR);
}

TEST(shadowing_initializer_sees_the_shadowed_symbol)
{
	ASSERT_SOURCE("fn f(x: u8) -> u8 { let x: u8 = x + 1u8; x }", OK);
	ASSERT_SOURCE("fn f(x: u8) -> i32 { let x: i32 = x; x }", TYPE_ERROR);
	ASSERT_SOURCE("fn f() -> bool { let x: i32 = 1i32; let x: bool = x == 1i32; x }", OK);
}

TEST(local_is_not_visible_before_its_declaration)
{
	ASSERT_SOURCE("fn main() { x; let x: i32 = 1i32; }", NAME_ERROR);
	ASSERT_SOURCE("fn main() -> i32 { return x; let x: i32 = 1i32; }", NAME_ERROR);
	ASSERT_SOURCE("fn main() { let y: i32 = x; let x: i32 = 1i32; }", NAME_ERROR);
}

TEST(locals_are_not_visible_in_other_functions)
{
	ASSERT_SOURCE("fn f() { let x: i32 = 1i32; }\nfn main() -> i32 { x }", NAME_ERROR);
	// not even in a function defined after it
	ASSERT_SOURCE("fn main() -> i32 { x }\nfn f() { let x: i32 = 1i32; }", NAME_ERROR);
}

TEST(locals_can_use_params_and_earlier_locals)
{
	ASSERT_SOURCE("fn f(a: i32) -> i32 { let b: i32 = a + 1i32; let c: i32 = b * a; c }", OK);
	ASSERT_SOURCE("fn f(a: u8) -> bool { let b: u8 = a; let c: bool = a == b; c }", OK);
	ASSERT_SOURCE("fn f(a: i32) -> i32 { let b: i32 = a; return b; }", OK);
}

TEST(locals_can_be_used_as_arguments)
{
	const std::string add = "fn add(a: i32, b: i32) -> i32 { a + b }\n";
	ASSERT_SOURCE(add + "fn main() -> i32 { let x: i32 = 1i32; let y: i32 = add(x, x); add(x, y) }", OK);
	ASSERT_SOURCE(add + "fn main() -> i32 { let x: u32 = 1u32; add(x, 1i32) }", TYPE_ERROR);
}

TEST(locals_can_shadow_locals)
{
	ASSERT_SOURCE(as_statement("let x: i32 = 1i32; let x: i32 = 2i32"), OK);
	ASSERT_SOURCE(as_statement("let x: i32 = 1i32; let x: i32 = 2i32; let x: i32 = 3i32"), OK);
}

TEST(locals_can_shadow_params)
{
	ASSERT_SOURCE("fn f(a: i32) { let a: i32 = 1i32; }", OK);
	ASSERT_SOURCE("fn f(a: i32) { let a: bool = true; }", OK);
}

TEST(shadowing_local_has_its_own_type)
{
	ASSERT_SOURCE("fn f() -> u8 { let x: i32 = 1i32; let x: u8 = 2u8; x }", OK);
	ASSERT_SOURCE("fn f() -> i32 { let x: i32 = 1i32; let x: u8 = 2u8; x }", TYPE_ERROR);
	ASSERT_SOURCE("fn f(a: i32) -> bool { let a: bool = true; a }", OK);
	ASSERT_SOURCE("fn f(a: i32) -> i32 { let a: bool = true; a }", TYPE_ERROR);
}

TEST(reads_before_a_shadow_keep_the_shadowed_type)
{
	// y was resolved against the first x, shadowing it afterwards doesn't change what y read
	ASSERT_SOURCE("fn f() -> i32 { let x: i32 = 1i32; let y: i32 = x; let x: bool = true; y }", OK);
	ASSERT_SOURCE("fn f(a: u8) -> u8 { let b: u8 = a; let a: bool = true; b }", OK);
}

TEST(the_same_local_name_in_different_functions)
{
	ASSERT_SOURCE("fn f() -> i32 { let x: i32 = 1i32; x }\nfn g() -> u8 { let x: u8 = 1u8; x }", OK);
	ASSERT_SOURCE("fn f(x: i32) -> i32 { x }\nfn g() -> u8 { let x: u8 = 1u8; x }", OK);
}

TEST(locals_do_not_shadow_functions)
{
	// functions are top level, and only locals can shadow, and only params and other locals
	const std::string five = "fn five() -> i32 { 5i32 }\n";
	ASSERT_SOURCE(five + "fn main() { let five: i32 = 1i32; }", NAME_ERROR);
	ASSERT_SOURCE(five + "fn main() -> i32 { let five: i32 = five(); five }", NAME_ERROR);
	ASSERT_SOURCE("fn main() { let main: i32 = 1i32; }", NAME_ERROR);
}

TEST(locals_are_not_callable)
{
	ASSERT_SOURCE(as_statement("let x: i32 = 1i32; x()"), TYPE_ERROR);
	ASSERT_SOURCE(as_statement("let x: bool = true; x()"), TYPE_ERROR);
}

TEST(locals_work_with_every_operator)
{
	ASSERT_SOURCE("fn f() -> i32 { let a: i32 = 6i32; let b: i32 = 3i32; (a + b) * (a - b) / (a % b) }", OK);
	ASSERT_SOURCE("fn f() -> i32 { let a: i32 = 6i32; let s: u8 = 1u8; (a << s) >> s & ~a | -a ^ a }", OK);
	ASSERT_SOURCE("fn f() -> bool { let a: i32 = 6i32; let b: bool = a < 1i32; !b == (a >= a) }", OK);
	ASSERT_SOURCE("fn f() { let a: i32 = 6i32; let s: i32 = 1i32; a << s; }", TYPE_ERROR);
	ASSERT_SOURCE("fn f() { let a: u32 = 6u32; -a; }", TYPE_ERROR);
}

TEST(declaration_does_not_return)
{
	ASSERT_SOURCE("fn main() -> i32 { let x: i32 = 1i32; }", TYPE_ERROR);
	ASSERT_SOURCE("fn main() { let x: i32 = 1i32; }", OK);
}

TEST(unused_locals_are_fine)
{
	ASSERT_SOURCE("fn main() -> i32 { let x: i32 = 1i32; let y: bool = false; 0i32 }", OK);
}

// assignment ====================================================================================================

TEST(mut_locals_can_be_assigned)
{
	for (const std::string &type : value_types)
		ASSERT_SOURCE(as_statement(std::format("let mut x: {0} = {1}; x = {1}", type, value_of(type))), OK);
}

TEST(mut_params_can_be_assigned)
{
	ASSERT_SOURCE("fn f(mut a: i32) { a = 1i32; }", OK);
	ASSERT_SOURCE("fn f(a: u8, mut b: bool) { b = true; }", OK);
}

TEST(immutable_locals_cannot_be_assigned)
{
	ASSERT_SOURCE(as_statement("let x: i32 = 1i32; x = 2i32"), SEMANTIC_ERROR);
	ASSERT_SOURCE(as_statement("let x: bool = true; x = x"), SEMANTIC_ERROR);
}

TEST(immutable_params_cannot_be_assigned)
{
	ASSERT_SOURCE("fn f(a: i32) { a = 1i32; }", SEMANTIC_ERROR);
	ASSERT_SOURCE("fn f(mut a: i32, b: i32) { b = a; }", SEMANTIC_ERROR);
}

TEST(mutability_follows_shadowing)
{
	// the assignment is to whichever symbol the name refers to at that point
	ASSERT_SOURCE(as_statement("let mut x: i32 = 1i32; let x: i32 = 2i32; x = 3i32"), SEMANTIC_ERROR);
	ASSERT_SOURCE(as_statement("let x: i32 = 1i32; let mut x: i32 = 2i32; x = 3i32"), OK);
	ASSERT_SOURCE("fn f(a: i32) { let mut a: i32 = a; a = 1i32; }", OK);
	ASSERT_SOURCE("fn f(mut a: i32) { let a: i32 = a; a = 1i32; }", SEMANTIC_ERROR);
}

TEST(functions_cannot_be_assigned)
{
	const std::string five = "fn five() -> i32 { 5i32 }\n";
	ASSERT_SOURCE(five + "fn main() { five = 1i32; }", SEMANTIC_ERROR);
	ASSERT_SOURCE("fn main() { main = main; }", SEMANTIC_ERROR);
}

TEST(only_variables_can_be_assigned)
{
	const std::string five = "fn five() -> i32 { 5i32 }\n";
	ASSERT_SOURCE(as_statement("1i32 = 1i32"), SEMANTIC_ERROR);
	ASSERT_SOURCE(as_statement("true = false"), SEMANTIC_ERROR);
	ASSERT_SOURCE(as_statement("let mut x: i32 = 1i32; x + 1i32 = 2i32"), SEMANTIC_ERROR);
	ASSERT_SOURCE(as_statement("let mut x: i32 = 1i32; -x = 2i32"), SEMANTIC_ERROR);
	ASSERT_SOURCE(five + "fn main() { five() = 1i32; }", SEMANTIC_ERROR);
	ASSERT_SOURCE(as_statement("let mut x: i32 = 1i32; let mut y: i32 = 1i32; (x = y) = 2i32"), SEMANTIC_ERROR);
	// parens don't make an expression any less of a place
	ASSERT_SOURCE(as_statement("let mut x: i32 = 1i32; (x) = 2i32"), OK);
}

TEST(assigned_value_has_to_match_exactly)
{
	for (const std::string &var : value_types)
	{
		for (const std::string &value : value_types)
		{
			std::string source = as_statement(std::format("let mut x: {} = {}; x = {}", var, value_of(var), value_of(value)));
			ASSERT_SOURCE(source, var == value ? OK : TYPE_ERROR);
		}
	}
}

TEST(assigned_value_is_type_checked)
{
	ASSERT_SOURCE(as_statement("let mut x: i32 = 1i32; x = 1i32 + 1u32"), TYPE_ERROR);
	ASSERT_SOURCE(as_statement("let mut x: u32 = 1u32; x = -x"), TYPE_ERROR);
	ASSERT_SOURCE("fn nothing() {}\nfn main() { let mut x: i32 = 1i32; x = nothing(); }", TYPE_ERROR);
}

TEST(assignment_names_are_resolved)
{
	ASSERT_SOURCE(as_statement("x = 1i32"), NAME_ERROR);
	ASSERT_SOURCE(as_statement("let mut x: i32 = 1i32; x = y"), NAME_ERROR);
	// assignment doesn't declare anything
	ASSERT_SOURCE(as_statement("let mut x: i32 = 1i32; x = 2i32; y"), NAME_ERROR);
}

TEST(assignment_can_read_what_it_assigns)
{
	ASSERT_SOURCE(as_statement("let mut x: i32 = 1i32; x = x + x"), OK);
	ASSERT_SOURCE("fn f(mut n: u8) -> u8 { n = n * 2u8; n = n + 1u8; n }", OK);
}

TEST(assignment_is_void)
{
	const std::string decl = "let mut x: i32 = 1i32; ";
	ASSERT_SOURCE("fn f() { let mut x: i32 = 1i32; x = 2i32 }", OK);
	ASSERT_SOURCE("fn f() -> i32 { let mut x: i32 = 1i32; x = 2i32 }", TYPE_ERROR);
	ASSERT_SOURCE("fn f() -> i32 { let mut x: i32 = 1i32; return x = 2i32; }", TYPE_ERROR);
	ASSERT_SOURCE("fn f() { let mut x: i32 = 1i32; return x = 2i32; }", OK);
	ASSERT_SOURCE(as_statement(decl + "let y: i32 = x = 2i32"), TYPE_ERROR);
	ASSERT_SOURCE(as_statement(decl + "let y: void = x = 2i32"), TYPE_ERROR);
	ASSERT_SOURCE(as_statement(decl + "let mut y: i32 = 1i32; x = (y = 2i32)"), TYPE_ERROR);
	ASSERT_SOURCE(as_statement(decl + "(x = 2i32) + 1i32"), TYPE_ERROR);
	ASSERT_SOURCE(as_statement(decl + "!(x = 2i32)"), TYPE_ERROR);
	ASSERT_SOURCE("fn g(a: i32) {}\nfn main() { let mut x: i32 = 1i32; g(x = 2i32); }", TYPE_ERROR);
}

TEST(chained_assignment_is_a_syntax_error)
{
	ASSERT_SOURCE(as_statement("let mut x: i32 = 1i32; let mut y: i32 = 1i32; x = y = 2i32"), SYNTAX_ERROR);
}

// main isn't special to the frontend, a missing one is only found when linking the runtime ======================

TEST(empty_program_passes_the_frontend)
{
	ASSERT_SOURCE("", OK);
	ASSERT_SOURCE("fn not_main() {}", OK);
}
