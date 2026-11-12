#include "unit_testing.hpp"
#include "source_testing.hpp"
#include "frontend/Parser.hpp"

#include <format>
#include <sstream>

#include "utils/error.hpp"

/// Writes an expression tree as an s-expression, so a test can spell out the exact shape it expects
static std::string shape(const ast::ExpressionNode &expr)
{
	if (auto *binary = dynamic_cast<const ast::BinaryExpression *>(&expr))
		return std::format("({} {} {})", binary->operator_string(), shape(*binary->l_expr), shape(*binary->r_expr));
	if (auto *unary = dynamic_cast<const ast::UnaryExpression *>(&expr))
		return std::format("({} {})", unary->operator_string(), shape(*unary->expr));
	if (auto *call = dynamic_cast<const ast::FunctionCall *>(&expr))
	{
		std::string out = "(call " + shape(*call->callee);
		for (const auto &arg : call->args)
			out += " " + shape(*arg);
		return out + ")";
	}
	if (auto *ident = dynamic_cast<const ast::IdentifierExpression *>(&expr))
		return ident->name;
	if (auto *integer = dynamic_cast<const ast::IntegerLiteralExpression *>(&expr))
		return std::format("{}{}", integer->raw_value, integer->type.to_string());
	if (auto *boolean = dynamic_cast<const ast::BooleanLiteralExpression *>(&expr))
		return boolean->value ? "true" : "false";
	return "<unknown node>";
}

/// Parses a whole input as one expression, and returns its shape
static std::string parse_shape(const std::string &source)
{
	std::istringstream in(source);
	Lexer lexer(in);
	auto expr = parse::try_parse_expr(lexer);
	if (!expr.has_value())
		return "<nothing parsed>";
	lexer.expect(TokenType::END_OF_FILE);
	return shape(**expr);
}

/// Parses a single function definition
static ast::FunctionDefinition parse_function(const std::string &source)
{
	std::istringstream in(source);
	Lexer lexer(in);
	auto fn = parse::try_parse_function_definition(lexer);
	ASSERT_TRUE(fn.has_value());
	lexer.expect(TokenType::END_OF_FILE);
	return std::move(*fn);
}

/// Parses a single integer literal
static std::unique_ptr<ast::IntegerLiteralExpression> parse_literal(const std::string &source)
{
	std::istringstream in(source);
	Lexer lexer(in);
	auto literal = parse::try_parse_integer_literal(lexer);
	ASSERT_TRUE(literal.has_value());
	return std::move(*literal);
}

/// Exit code of parsing one integer literal
static ExitCode literal_error(const std::string &source)
{
	std::istringstream in(source);
	Lexer lexer(in);
	try
	{
		parse::try_parse_integer_literal(lexer);
		return ExitCode::Success;
	}
	catch (const CompilerError &err)
	{
		return err.exit_code();
	}
}

/// Wraps statements in a function, for error tests
static std::string in_function(const std::string &body)
{
	return std::format("fn main() {{ {} }}", body);
}

// literals ======================================================================================================

TEST(literal_suffix_sets_its_type)
{
	const std::vector<std::pair<std::string, FrontendType>> cases = {
		{"1i8", FrontendType::i8()},
		{"1i16", FrontendType::i16()},
		{"1i32", FrontendType::i32()},
		{"1i64", FrontendType::i64()},
		{"1u8", FrontendType::u8()},
		{"1u16", FrontendType::u16()},
		{"1u32", FrontendType::u32()},
		{"1u64", FrontendType::u64()},
	};
	for (const auto &[source, type] : cases)
		ASSERT_TRUE(parse_literal(source)->type == type);
}

TEST(literal_decimal_values)
{
	ASSERT_EQ(parse_literal("0i32")->raw_value, 0u);
	ASSERT_EQ(parse_literal("7i32")->raw_value, 7u);
	ASSERT_EQ(parse_literal("007i32")->raw_value, 7u);
	ASSERT_EQ(parse_literal("4294967296u64")->raw_value, 4294967296u);
	ASSERT_EQ(parse_literal("18446744073709551615u64")->raw_value, UINT64_MAX);
}

TEST(literal_radix_prefixes)
{
	ASSERT_EQ(parse_literal("0b0u8")->raw_value, 0u);
	ASSERT_EQ(parse_literal("0b101u8")->raw_value, 5u);
	ASSERT_EQ(parse_literal("0o17u8")->raw_value, 15u);
	ASSERT_EQ(parse_literal("0o777u16")->raw_value, 511u);
	ASSERT_EQ(parse_literal("0x10u8")->raw_value, 16u);
	ASSERT_EQ(parse_literal("0x99u32")->raw_value, 153u);
}

TEST(literal_radix_keeps_its_type)
{
	ASSERT_TRUE(parse_literal("0b1i16")->type == FrontendType::i16());
	ASSERT_TRUE(parse_literal("0o1u64")->type == FrontendType::u64());
	ASSERT_TRUE(parse_literal("0x1i8")->type == FrontendType::i8());
}

TEST(literal_needs_an_integer_type_suffix)
{
	for (const std::string &source : {"1", "0", "1foo", "1bool", "1void", "1addr", "1f32", "1I32", "1u", "1i", "1i32x", "1_i32"})
		ASSERT_EQ(int(literal_error(source)), int(ExitCode::TypeError));
}

TEST(literal_too_big_for_any_type)
{
	ASSERT_EQ(int(literal_error("18446744073709551616u64")), int(ExitCode::SemanticError));
	ASSERT_EQ(int(literal_error("99999999999999999999999999i32")), int(ExitCode::SemanticError));
}

TEST(not_a_literal_leaves_the_lexer_alone)
{
	std::istringstream in("abc");
	Lexer lexer(in);
	ASSERT_TRUE(!parse::try_parse_integer_literal(lexer).has_value());
	ASSERT_TRUE(!parse::try_parse_boolean_literal(lexer).has_value());
	ASSERT_EQ(lexer.take().str, "abc");
}

TEST(boolean_literals)
{
	ASSERT_EQ(parse_shape("true"), "true");
	ASSERT_EQ(parse_shape("false"), "false");
}

// precedence ====================================================================================================

TEST(each_level_binds_tighter_than_the_one_before)
{
	// loosest to tightest, an operator of every level of the grammar
	const std::vector<std::string> levels = {"||", "&&", "==", "<", "|", "^", "&", "<<", "+", "*"};
	for (size_t i = 0; i < levels.size(); ++i)
	{
		for (size_t j = i + 1; j < levels.size(); ++j)
		{
			const std::string &loose = levels[i];
			const std::string &tight = levels[j];
			ASSERT_EQ(parse_shape(std::format("a {} b {} c", loose, tight)), std::format("({} a ({} b c))", loose, tight));
			ASSERT_EQ(parse_shape(std::format("a {} b {} c", tight, loose)), std::format("({} ({} a b) c)", loose, tight));
		}
	}
}

TEST(operators_within_a_level)
{
	ASSERT_EQ(parse_shape("a != b"), "(!= a b)");
	ASSERT_EQ(parse_shape("a == b"), "(== a b)");
	ASSERT_EQ(parse_shape("a < b"), "(< a b)");
	ASSERT_EQ(parse_shape("a <= b"), "(<= a b)");
	ASSERT_EQ(parse_shape("a > b"), "(> a b)");
	ASSERT_EQ(parse_shape("a >= b"), "(>= a b)");
	ASSERT_EQ(parse_shape("a >> b"), "(>> a b)");
	ASSERT_EQ(parse_shape("a << b"), "(<< a b)");
	ASSERT_EQ(parse_shape("a - b"), "(- a b)");
	ASSERT_EQ(parse_shape("a / b"), "(/ a b)");
	ASSERT_EQ(parse_shape("a % b"), "(% a b)");
}

TEST(additive_and_multiplicative_operators_share_a_level)
{
	ASSERT_EQ(parse_shape("a + b - c"), "(- (+ a b) c)");
	ASSERT_EQ(parse_shape("a - b + c"), "(+ (- a b) c)");
	ASSERT_EQ(parse_shape("a * b / c % d"), "(% (/ (* a b) c) d)");
	ASSERT_EQ(parse_shape("a % b * c"), "(* (% a b) c)");
}

TEST(binary_operators_are_left_associative)
{
	for (const std::string &op : {"+", "-", "*", "/", "%", "&", "|", "^"})
		ASSERT_EQ(parse_shape(std::format("a {0} b {0} c {0} d", op)), std::format("({0} ({0} ({0} a b) c) d)", op));
}

TEST(parens_override_precedence)
{
	ASSERT_EQ(parse_shape("(a + b) * c"), "(* (+ a b) c)");
	ASSERT_EQ(parse_shape("a * (b + c)"), "(* a (+ b c))");
	ASSERT_EQ(parse_shape("a - (b - c)"), "(- a (- b c))");
	ASSERT_EQ(parse_shape("((a))"), "a");
}

TEST(mixed_bitwise_precedence)
{
	ASSERT_EQ(parse_shape("a | b & c ^ d"), "(| a (^ (& b c) d))");
	ASSERT_EQ(parse_shape("(a | b) & c ^ d | e"), "(| (^ (& (| a b) c) d) e)");
}

TEST(comparisons_do_not_chain)
{
	for (const std::string &op : {"==", "!=", "<", "<=", ">", ">="})
		ASSERT_SOURCE(in_function(std::format("1i32 {0} 2i32 {0} 3i32;", op)), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("1i32 < 2i32 == 3i32 > 4i32 == true;"), ExitCode::SyntaxError);
}

TEST(comparisons_chain_with_parens)
{
	ASSERT_EQ(parse_shape("(a < b) == (c > d)"), "(== (< a b) (> c d))");
	ASSERT_EQ(parse_shape("(a == b) == c"), "(== (== a b) c)");
}

// unary =========================================================================================================

TEST(unary_operators)
{
	ASSERT_EQ(parse_shape("-a"), "(- a)");
	ASSERT_EQ(parse_shape("!a"), "(! a)");
	ASSERT_EQ(parse_shape("~a"), "(~ a)");
}

TEST(unary_operators_nest)
{
	ASSERT_EQ(parse_shape("--a"), "(- (- a))");
	ASSERT_EQ(parse_shape("!!a"), "(! (! a))");
	ASSERT_EQ(parse_shape("-~!a"), "(- (~ (! a)))");
}

TEST(unary_binds_tighter_than_binary)
{
	ASSERT_EQ(parse_shape("-a * b"), "(* (- a) b)");
	ASSERT_EQ(parse_shape("a * -b"), "(* a (- b))");
	ASSERT_EQ(parse_shape("~a & b"), "(& (~ a) b)");
	ASSERT_EQ(parse_shape("!a == b"), "(== (! a) b)");
	ASSERT_EQ(parse_shape("a - -b"), "(- a (- b))");
}

TEST(call_binds_tighter_than_unary)
{
	ASSERT_EQ(parse_shape("-f()"), "(- (call f))");
}

TEST(unary_operand_can_be_parenthesized)
{
	ASSERT_EQ(parse_shape("-(a + b)"), "(- (+ a b))");
}

// calls =========================================================================================================

TEST(call_argument_lists)
{
	ASSERT_EQ(parse_shape("f()"), "(call f)");
	ASSERT_EQ(parse_shape("f(a)"), "(call f a)");
	ASSERT_EQ(parse_shape("f(a, b, c)"), "(call f a b c)");
	ASSERT_EQ(parse_shape("f(a + b, -c)"), "(call f (+ a b) (- c))");
	ASSERT_EQ(parse_shape("f(g(a), h())"), "(call f (call g a) (call h))");
}

TEST(calls_chain_left_to_right)
{
	ASSERT_EQ(parse_shape("f()()"), "(call (call f))");
	ASSERT_EQ(parse_shape("f(a)(b)(c)"), "(call (call (call f a) b) c)");
}

TEST(parenthesized_callee)
{
	ASSERT_EQ(parse_shape("(f)()"), "(call f)");
}

TEST(call_syntax_errors)
{
	const std::string add = "fn add(a: i32, b: i32) -> i32 { a + b }\n";
	ASSERT_SOURCE(add + "fn main() -> i32 { add(1i32, , 2i32) }", ExitCode::SyntaxError);
	ASSERT_SOURCE(add + "fn main() -> i32 { add(, 1i32) }", ExitCode::SyntaxError);
	ASSERT_SOURCE(add + "fn main() -> i32 { add(1i32, 2i32 }", ExitCode::SyntaxError);
	ASSERT_SOURCE(add + "fn main() -> i32 { add(1i32 2i32) }", ExitCode::SyntaxError);
	ASSERT_SOURCE(add + "fn main() -> i32 { add(1i32, 2i32,) }", ExitCode::SyntaxError);
	ASSERT_SOURCE(add + "fn main() -> i32 { add(1i32", ExitCode::SyntaxError);
}

// expression errors =============================================================================================

TEST(not_an_expression_leaves_the_lexer_alone)
{
	std::istringstream in(";");
	Lexer lexer(in);
	ASSERT_TRUE(!parse::try_parse_expr(lexer).has_value());
	ASSERT_EQ(lexer.take().type, TokenType::SEMICOLON);
}

TEST(binary_operator_needs_a_right_side)
{
	for (const std::string &op : {"||", "&&", "==", "!=", "<", "<=", ">", ">=", "|", "^", "&", "<<", ">>", "+", "-", "*", "/", "%"})
		ASSERT_SOURCE(in_function(std::format("1i32 {};", op)), ExitCode::SyntaxError);
}

TEST(unary_operator_needs_an_operand)
{
	for (const std::string &op : {"-", "!", "~"})
		ASSERT_SOURCE(in_function(std::format("{};", op)), ExitCode::SyntaxError);
}

TEST(paren_errors)
{
	ASSERT_SOURCE(in_function("();"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("(1i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("1i32);"), ExitCode::SyntaxError);
}

TEST(two_expressions_in_a_row)
{
	ASSERT_SOURCE(in_function("1i32 2i32;"), ExitCode::SyntaxError);
}

TEST(unexpected_characters_are_syntax_errors)
{
	ASSERT_SOURCE(in_function("@;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("1i32 @ 2i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE("@", ExitCode::SyntaxError);
}

TEST(locations_of_expressions)
{
	std::istringstream in("  a + b");
	Lexer lexer(in);
	auto expr = parse::try_parse_expr(lexer);
	ASSERT_TRUE(expr.has_value());
	ASSERT_EQ((*expr)->src_loc.start.col, 2u);
	ASSERT_EQ((*expr)->src_loc.end.col, 7u);
}

TEST(location_of_a_unary_expression_includes_the_operator)
{
	std::istringstream in("-a");
	Lexer lexer(in);
	auto expr = parse::try_parse_expr(lexer);
	ASSERT_TRUE(expr.has_value());
	ASSERT_EQ((*expr)->src_loc.start.col, 0u);
	ASSERT_EQ((*expr)->src_loc.end.col, 2u);
}

TEST(location_of_a_call_includes_the_closing_paren)
{
	std::istringstream in("f(a)");
	Lexer lexer(in);
	auto expr = parse::try_parse_expr(lexer);
	ASSERT_TRUE(expr.has_value());
	ASSERT_EQ((*expr)->src_loc.end.col, 4u);
}

// statements ====================================================================================================

TEST(return_statement_forms)
{
	ast::FunctionDefinition with_value = parse_function("fn f() -> i32 { return 1i32; }");
	auto *ret = dynamic_cast<ast::ReturnStatement *>(with_value.body_statements.at(0).get());
	ASSERT_TRUE(ret != nullptr);
	ASSERT_TRUE(ret->expr.has_value());

	ast::FunctionDefinition without_value = parse_function("fn f() { return; }");
	ret = dynamic_cast<ast::ReturnStatement *>(without_value.body_statements.at(0).get());
	ASSERT_TRUE(ret != nullptr);
	ASSERT_TRUE(!ret->expr.has_value());
}

TEST(return_syntax_errors)
{
	ASSERT_SOURCE("fn main() { return fn; }", ExitCode::SyntaxError);
	ASSERT_SOURCE("fn main() { return = 0; }", ExitCode::SyntaxError);
	ASSERT_SOURCE("fn main() { return 1i32 }", ExitCode::SyntaxError);
	ASSERT_SOURCE("fn main() { return", ExitCode::SyntaxError);
}

TEST(expression_with_a_semicolon_is_a_statement)
{
	ast::FunctionDefinition fn = parse_function("fn f() { a; b; }");
	ASSERT_EQ(fn.body_statements.size(), 2u);
	ASSERT_TRUE(dynamic_cast<ast::ExpressionStatement *>(fn.body_statements[0].get()) != nullptr);
	ASSERT_TRUE(!fn.body_return_expr.has_value());
}

TEST(expression_without_a_semicolon_is_the_trailing_expression)
{
	ast::FunctionDefinition fn = parse_function("fn f() { a; b }");
	ASSERT_EQ(fn.body_statements.size(), 1u);
	ASSERT_TRUE(fn.body_return_expr.has_value());
	ASSERT_EQ(shape(**fn.body_return_expr), "b");
}

TEST(trailing_expression_has_to_be_last)
{
	const std::string five = "fn five() -> i32 { 5i32 }\n";
	ASSERT_SOURCE(five + "fn main() -> i32 { 5i32 five(); }", ExitCode::SyntaxError);
	ASSERT_SOURCE(five + "fn main() -> i32 { five() return 0i32; }", ExitCode::SyntaxError);
	ASSERT_SOURCE("fn main() -> i32 { 1i32 2i32 }", ExitCode::SyntaxError);
}

TEST(stray_semicolon_is_not_a_statement)
{
	ASSERT_SOURCE(in_function(";"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("return;;"), ExitCode::SyntaxError);
}

// declarations ===============================================================================================

/// A pattern, which has to be a binding
static const ast::BindingPattern *as_binding(const std::unique_ptr<ast::PatternNode> &pattern)
{
	auto *binding = dynamic_cast<const ast::BindingPattern *>(pattern.get());
	ASSERT_TRUE(binding != nullptr);
	return binding;
}

/// The declaration a function body starts with
static ast::DeclarationStatement *first_declaration(ast::FunctionDefinition &fn)
{
	ASSERT_TRUE(!fn.body_statements.empty());
	auto *decl = dynamic_cast<ast::DeclarationStatement *>(fn.body_statements[0].get());
	ASSERT_TRUE(decl != nullptr);
	return decl;
}

TEST(declaration_parts)
{
	ast::FunctionDefinition fn = parse_function("fn f() { let x: i32 = a + b; }");
	ASSERT_EQ(fn.body_statements.size(), 1u);
	ast::DeclarationStatement *decl = first_declaration(fn);
	ASSERT_EQ(as_binding(decl->pattern)->name, "x");
	ASSERT_TRUE(!as_binding(decl->pattern)->mut);
	ASSERT_TRUE(decl->declared_type == FrontendType::i32());
	ASSERT_EQ(shape(*decl->init_expr), "(+ a b)");
}

TEST(mut_declaration)
{
	ast::FunctionDefinition fn = parse_function("fn f() { let mut x: i32 = a; }");
	const ast::BindingPattern *binding = as_binding(first_declaration(fn)->pattern);
	ASSERT_EQ(binding->name, "x");
	ASSERT_TRUE(binding->mut);
}

TEST(mut_needs_a_name)
{
	ASSERT_SOURCE(in_function("let mut: i32 = 1i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let mut mut x: i32 = 1i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let x mut: i32 = 1i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let mut 1i32: i32 = 1i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE("fn f(mut: i32) {}", ExitCode::SyntaxError);
}

TEST(location_of_a_binding_pattern)
{
	ast::FunctionDefinition fn = parse_function("fn f() { let mut abc: i32 = a; }");
	const ast::BindingPattern *binding = as_binding(first_declaration(fn)->pattern);
	// from the start of 'mut' to the end of the name
	ASSERT_EQ(binding->src_loc.start.col, 13u);
	ASSERT_EQ(binding->src_loc.end.col, 20u);
}

TEST(every_type_name_as_declaration_type)
{
	const std::vector<std::pair<std::string, FrontendType>> cases = {
		{"bool", FrontendType::boolean()},
		{"i8", FrontendType::i8()},
		{"i16", FrontendType::i16()},
		{"i32", FrontendType::i32()},
		{"i64", FrontendType::i64()},
		{"u8", FrontendType::u8()},
		{"u16", FrontendType::u16()},
		{"u32", FrontendType::u32()},
		{"u64", FrontendType::u64()},
		{"void", FrontendType::void_type()},
	};
	for (const auto &[name, type] : cases)
	{
		ast::FunctionDefinition fn = parse_function(std::format("fn f() {{ let x: {} = a; }}", name));
		ASSERT_TRUE(first_declaration(fn)->declared_type == type);
	}
}

TEST(declarations_mix_with_other_statements)
{
	ast::FunctionDefinition fn = parse_function("fn f() -> i32 { let a: i32 = 1i32; g(a); let b: i32 = a; return b; b }");
	ASSERT_EQ(fn.body_statements.size(), 4u);
	ASSERT_TRUE(dynamic_cast<ast::DeclarationStatement *>(fn.body_statements[0].get()) != nullptr);
	ASSERT_TRUE(dynamic_cast<ast::ExpressionStatement *>(fn.body_statements[1].get()) != nullptr);
	ASSERT_TRUE(dynamic_cast<ast::DeclarationStatement *>(fn.body_statements[2].get()) != nullptr);
	ASSERT_TRUE(dynamic_cast<ast::ReturnStatement *>(fn.body_statements[3].get()) != nullptr);
	ASSERT_TRUE(fn.body_return_expr.has_value());
}

TEST(declaration_initializer_is_a_full_expression)
{
	ast::FunctionDefinition fn = parse_function("fn f() { let x: bool = (a + b) * c == -f(d); }");
	ASSERT_EQ(shape(*first_declaration(fn)->init_expr), "(== (* (+ a b) c) (- (call f d)))");
}

TEST(declaration_syntax_errors)
{
	// a type annotation is required until there's type inference
	ASSERT_SOURCE(in_function("let x = 1i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let x;"), ExitCode::SyntaxError);
	// so is an initializer, there are no uninitialized variables
	ASSERT_SOURCE(in_function("let x: i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let x: i32 = ;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let x: i32 = 1i32"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let x: i32 = 1i32 2i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let x i32 = 1i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let x: = 1i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let x: 1i32 = 1i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let x: i32 == 1i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let : i32 = 1i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let"), ExitCode::SyntaxError);
	ASSERT_SOURCE("fn main() { let x: i32 = 1i32;", ExitCode::SyntaxError);
}

TEST(declaration_name_has_to_be_an_identifier)
{
	ASSERT_SOURCE(in_function("let 1i32: i32 = 1i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let let: i32 = 1i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let fn: i32 = 1i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let return: i32 = 1i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let true: bool = true;"), ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let (x): i32 = 1i32;"), ExitCode::SyntaxError);
}

TEST(declaration_is_not_an_expression)
{
	ASSERT_SOURCE("fn main() -> i32 { let x: i32 = 1i32 }", ExitCode::SyntaxError);
	ASSERT_SOURCE("fn main() -> i32 { return let x: i32 = 1i32; }", ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("let x: i32 = let y: i32 = 1i32;"), ExitCode::SyntaxError);
	ASSERT_SOURCE("fn g(a: i32) {}\nfn main() { g(let x: i32 = 1i32); }", ExitCode::SyntaxError);
	ASSERT_SOURCE(in_function("(let x: i32 = 1i32);"), ExitCode::SyntaxError);
}

TEST(declaration_type_errors_from_the_parser)
{
	ASSERT_SOURCE(in_function("let x: foo = 1i32;"), ExitCode::NameError);
	ASSERT_SOURCE(in_function("let x: I32 = 1i32;"), ExitCode::NameError);
	// addr is reserved by the grammar but not implemented yet. update this test once it is
	ASSERT_SOURCE(in_function("let x: addr = 1i32;"), ExitCode::Unimplemented);
}

TEST(location_of_a_declaration)
{
	ast::FunctionDefinition fn = parse_function("fn f() {\n\tlet x: i32 =\n\t\t1i32;\n}");
	ast::DeclarationStatement *decl = first_declaration(fn);
	// from the start of 'let' to the end of the semicolon
	ASSERT_EQ(decl->src_loc.start.line, 1u);
	ASSERT_EQ(decl->src_loc.start.col, 1u);
	ASSERT_EQ(decl->src_loc.end.line, 2u);
	ASSERT_EQ(decl->src_loc.end.col, 7u);
}

// function definitions ==========================================================================================

TEST(function_name_and_no_params)
{
	ast::FunctionDefinition fn = parse_function("fn name() {}");
	ASSERT_EQ(fn.name, "name");
	ASSERT_TRUE(fn.args.empty());
	ASSERT_TRUE(fn.body_statements.empty());
	ASSERT_TRUE(!fn.body_return_expr.has_value());
}

TEST(function_params_in_order)
{
	ast::FunctionDefinition fn = parse_function("fn f(a: i32, b: bool, c: u8) {}");
	ASSERT_EQ(fn.args.size(), 3u);
	ASSERT_EQ(as_binding(fn.args[0].pattern)->name, "a");
	ASSERT_TRUE(fn.args[0].type == FrontendType::i32());
	ASSERT_EQ(as_binding(fn.args[1].pattern)->name, "b");
	ASSERT_TRUE(fn.args[1].type == FrontendType::boolean());
	ASSERT_EQ(as_binding(fn.args[2].pattern)->name, "c");
	ASSERT_TRUE(fn.args[2].type == FrontendType::u8());
}

TEST(mut_function_param)
{
	ast::FunctionDefinition fn = parse_function("fn f(a: i32, mut b: bool) {}");
	ASSERT_TRUE(!as_binding(fn.args[0].pattern)->mut);
	ASSERT_TRUE(as_binding(fn.args[1].pattern)->mut);
	ASSERT_EQ(as_binding(fn.args[1].pattern)->name, "b");
}

TEST(function_param_trailing_comma)
{
	ASSERT_EQ(parse_function("fn f(a: i32,) {}").args.size(), 1u);
	ASSERT_EQ(parse_function("fn f(a: i32, b: i32,) {}").args.size(), 2u);
}

TEST(return_type_defaults_to_void)
{
	ASSERT_TRUE(parse_function("fn f() {}").return_type == FrontendType::void_type());
	ASSERT_TRUE(parse_function("fn f() -> void {}").return_type == FrontendType::void_type());
}

TEST(every_type_name_as_return_type)
{
	const std::vector<std::pair<std::string, FrontendType>> cases = {
		{"bool", FrontendType::boolean()},
		{"i8", FrontendType::i8()},
		{"i16", FrontendType::i16()},
		{"i32", FrontendType::i32()},
		{"i64", FrontendType::i64()},
		{"u8", FrontendType::u8()},
		{"u16", FrontendType::u16()},
		{"u32", FrontendType::u32()},
		{"u64", FrontendType::u64()},
	};
	for (const auto &[name, type] : cases)
		ASSERT_TRUE(parse_function(std::format("fn f() -> {} {{}}", name)).return_type == type);
}

TEST(function_definition_syntax_errors)
{
	const std::string main_fn = "\nfn main() -> i32 { 0i32 }";
	ASSERT_SOURCE("fn f -> i32 { 0i32 }" + main_fn, ExitCode::SyntaxError);
	ASSERT_SOURCE("fn f(a: i32,,) {}" + main_fn, ExitCode::SyntaxError);
	ASSERT_SOURCE("fn f(,) {}" + main_fn, ExitCode::SyntaxError);
	ASSERT_SOURCE("fn f(a i32) {}" + main_fn, ExitCode::SyntaxError);
	ASSERT_SOURCE("fn f(a: i32 b: i32) {}" + main_fn, ExitCode::SyntaxError);
	ASSERT_SOURCE("fn f(a) {}" + main_fn, ExitCode::SyntaxError);
	ASSERT_SOURCE("fn f(a:) {}" + main_fn, ExitCode::SyntaxError);
	ASSERT_SOURCE("fn f(a: 1i32) {}" + main_fn, ExitCode::SyntaxError);
	ASSERT_SOURCE("fn f() -> {}" + main_fn, ExitCode::SyntaxError);
	ASSERT_SOURCE("fn f() -> 1i32 {}" + main_fn, ExitCode::SyntaxError);
	ASSERT_SOURCE("fn f()" + main_fn, ExitCode::SyntaxError);
	ASSERT_SOURCE("fn main()", ExitCode::SyntaxError);
	ASSERT_SOURCE("fn main() {", ExitCode::SyntaxError);
	ASSERT_SOURCE("fn", ExitCode::SyntaxError);
	ASSERT_SOURCE("fn 1i32() {}", ExitCode::SyntaxError);
	ASSERT_SOURCE("fn fn() {}", ExitCode::SyntaxError);
}

TEST(unknown_type_names)
{
	const std::string main_fn = "\nfn main() -> i32 { 0i32 }";
	ASSERT_SOURCE("fn main() -> foo { 0i32 }", ExitCode::NameError);
	ASSERT_SOURCE("fn main() -> I32 { 0i32 }", ExitCode::NameError);
	ASSERT_SOURCE("fn f(x: foo) {}" + main_fn, ExitCode::NameError);
	ASSERT_SOURCE("fn f(a: i32, b: foo) {}" + main_fn, ExitCode::NameError);
}

TEST(reserved_type_names_are_unimplemented)
{
	// addr is reserved by the grammar but not implemented yet. update this test once it is
	ASSERT_SOURCE("fn f(x: addr) {}\nfn main() -> i32 { 0i32 }", ExitCode::Unimplemented);
	ASSERT_SOURCE("fn main() -> addr { 0i32 }", ExitCode::Unimplemented);
}

TEST(location_of_a_function)
{
	ast::FunctionDefinition fn = parse_function("fn f() {\n}");
	ASSERT_EQ(fn.src_loc.start.line, 0u);
	ASSERT_EQ(fn.src_loc.start.col, 0u);
	ASSERT_EQ(fn.src_loc.end.line, 1u);
	ASSERT_EQ(fn.src_loc.end.col, 1u);
}

// top level =====================================================================================================

TEST(not_a_function_leaves_the_lexer_alone)
{
	std::istringstream in("abc");
	Lexer lexer(in);
	ASSERT_TRUE(!parse::try_parse_top_level_decl(lexer).has_value());
	ASSERT_EQ(lexer.take().str, "abc");
}

TEST(only_functions_at_the_top_level)
{
	ASSERT_SOURCE("x", ExitCode::SyntaxError);
	ASSERT_SOURCE("1i32;", ExitCode::SyntaxError);
	ASSERT_SOURCE("fn main() {} }", ExitCode::SyntaxError);
	ASSERT_SOURCE("fn main() {};", ExitCode::SyntaxError);
}

TEST(no_declarations_at_the_top_level_yet)
{
	// the grammar has top level declarations, but global variables don't exist yet. update this test once they do
	ASSERT_SOURCE("let x: i32 = 1i32;\nfn main() {}", ExitCode::SyntaxError);
}
