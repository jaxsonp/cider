#include "unit_testing.hpp"
#include "frontend/FrontendType.hpp"

#include <vector>

#include "utils/error.hpp"

using Type = FrontendType;

static const std::vector<Type> signed_types = {Type::i8(), Type::i16(), Type::i32(), Type::i64()};
static const std::vector<Type> unsigned_types = {Type::u8(), Type::u16(), Type::u32(), Type::u64()};

TEST(signed_integers)
{
	for (const Type &type : signed_types)
	{
		ASSERT_TRUE(type.is_integer());
		ASSERT_TRUE(type.is_numeric());
		ASSERT_TRUE(type.is_signed_integer());
		ASSERT_TRUE(!type.is_unsigned_integer());
		ASSERT_TRUE(!type.is_bool());
		ASSERT_TRUE(!type.is_float());
		ASSERT_TRUE(!type.is_function());
	}
}

TEST(unsigned_integers)
{
	for (const Type &type : unsigned_types)
	{
		ASSERT_TRUE(type.is_integer());
		ASSERT_TRUE(type.is_numeric());
		ASSERT_TRUE(type.is_unsigned_integer());
		ASSERT_TRUE(!type.is_signed_integer());
		ASSERT_TRUE(!type.is_bool());
		ASSERT_TRUE(!type.is_float());
		ASSERT_TRUE(!type.is_function());
	}
}

TEST(unresolved_int_could_be_either_sign)
{
	ASSERT_TRUE(Type::unresolved_int().is_integer());
	ASSERT_TRUE(Type::unresolved_int().is_numeric());
	ASSERT_TRUE(Type::unresolved_int().is_signed_integer());
	ASSERT_TRUE(Type::unresolved_int().is_unsigned_integer());
}

TEST(unresolved_float_is_numeric_but_not_an_integer)
{
	Type type(Type::Variant::UNRESOLVED_FLOAT);
	ASSERT_TRUE(type.is_float());
	ASSERT_TRUE(type.is_numeric());
	ASSERT_TRUE(!type.is_integer());
}

TEST(bool_is_not_numeric)
{
	Type type = Type::boolean();
	ASSERT_TRUE(type.is_bool());
	ASSERT_TRUE(!type.is_numeric());
	ASSERT_TRUE(!type.is_integer());
}

TEST(void_is_nothing)
{
	Type type = Type::void_type();
	ASSERT_TRUE(!type.is_numeric());
	ASSERT_TRUE(!type.is_integer());
	ASSERT_TRUE(!type.is_bool());
	ASSERT_TRUE(!type.is_function());
}

TEST(default_type_is_void)
{
	ASSERT_TRUE(Type() == Type::void_type());
}

TEST(function_type_parts)
{
	Type type = Type::function(Type::i32(), {Type::u8(), Type::boolean()});
	ASSERT_TRUE(type.is_function());
	ASSERT_TRUE(type.return_type() == Type::i32());
	ASSERT_EQ(type.param_types().size(), 2u);
	ASSERT_TRUE(type.param_types()[0] == Type::u8());
	ASSERT_TRUE(type.param_types()[1] == Type::boolean());
}

TEST(function_parts_of_non_function_types)
{
	ASSERT_THROWS(Type::i32().return_type(), CompilerError);
	ASSERT_THROWS(Type::i32().param_types(), CompilerError);
}

TEST(names_round_trip)
{
	for (const std::string &name : {"void", "bool", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64"})
		ASSERT_EQ(Type::from_string(name).to_string(), name);
}

TEST(from_string_of_nothing_is_unresolved)
{
	ASSERT_TRUE(Type::from_string("") == Type::unresolved());
}

TEST(from_string_of_an_unknown_name)
{
	for (const std::string &name : {"foo", "I32", "i", "i128", "u", " i32", "i32 ", "addr", "fn"})
		ASSERT_EQ(int(Type::from_string(name).variant), int(Type::Variant::UNKNOWN));
}

TEST(to_string_of_function_types)
{
	ASSERT_EQ(Type::function(Type::void_type(), {}).to_string(), "fn() -> void");
	ASSERT_EQ(Type::function(Type::i32(), {Type::u8()}).to_string(), "fn(u8) -> i32");
	ASSERT_EQ(Type::function(Type::boolean(), {Type::i8(), Type::i16()}).to_string(), "fn(i8, i16) -> bool");
}

TEST(to_string_of_types_with_no_name)
{
	ASSERT_THROWS(Type::unresolved().to_string(), CompilerError);
	ASSERT_THROWS(Type::unresolved_int().to_string(), CompilerError);
	ASSERT_THROWS(Type::unknown().to_string(), CompilerError);
}

TEST(resolving_to_ir_types)
{
	const std::vector<std::pair<Type, ir::IrType::Variant>> cases = {
		{Type::boolean(), ir::IrType::Variant::BOOL},
		{Type::i8(), ir::IrType::Variant::I8},
		{Type::i16(), ir::IrType::Variant::I16},
		{Type::i32(), ir::IrType::Variant::I32},
		{Type::i64(), ir::IrType::Variant::I64},
		{Type::u8(), ir::IrType::Variant::U8},
		{Type::u16(), ir::IrType::Variant::U16},
		{Type::u32(), ir::IrType::Variant::U32},
		{Type::u64(), ir::IrType::Variant::U64},
	};
	for (const auto &[type, variant] : cases)
		ASSERT_EQ(int(type.resolveType().variant), int(variant));
}

TEST(types_with_no_ir_type)
{
	ASSERT_THROWS(Type::void_type().resolveType(), CompilerError);
	ASSERT_THROWS(Type::unresolved().resolveType(), CompilerError);
	ASSERT_THROWS(Type::unresolved_int().resolveType(), CompilerError);
	ASSERT_THROWS(Type::unknown().resolveType(), CompilerError);
	ASSERT_THROWS(Type::function(Type::i32(), {}).resolveType(), CompilerError);
}

TEST(every_type_only_equals_itself)
{
	std::vector<Type> types = {Type::void_type(), Type::boolean()};
	types.insert(types.end(), signed_types.begin(), signed_types.end());
	types.insert(types.end(), unsigned_types.begin(), unsigned_types.end());
	for (size_t i = 0; i < types.size(); ++i)
	{
		for (size_t j = 0; j < types.size(); ++j)
			ASSERT_EQ(types[i] == types[j], i == j);
	}
}

TEST(function_types_compare_by_signature)
{
	Type base = Type::function(Type::i32(), {Type::u8()});
	ASSERT_TRUE(base == Type::function(Type::i32(), {Type::u8()}));
	ASSERT_TRUE(!(base == Type::function(Type::u32(), {Type::u8()})));
	ASSERT_TRUE(!(base == Type::function(Type::i32(), {Type::u16()})));
	ASSERT_TRUE(!(base == Type::function(Type::i32(), {})));
	ASSERT_TRUE(!(base == Type::i32()));
}

TEST(unknown_types_compare_by_name)
{
	Type a = Type::unknown();
	Type b = Type::unknown();
	a.name = "foo";
	b.name = "foo";
	ASSERT_TRUE(a == b);
	b.name = "bar";
	ASSERT_TRUE(!(a == b));
}
