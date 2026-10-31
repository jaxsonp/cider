#include "unit_testing.hpp"
#include "ir/IrType.hpp"

#include <vector>

using ir::IrType;

TEST(type_properties)
{
	struct Case
	{
		IrType type;
		unsigned int size;
		bool is_signed;
		std::string name;
	};
	const std::vector<Case> cases = {
		{IrType::boolean(), 1, false, "bool"},
		{IrType::i8(), 1, true, "i8"},
		{IrType::i16(), 2, true, "i16"},
		{IrType::i32(), 4, true, "i32"},
		{IrType::i64(), 8, true, "i64"},
		{IrType::u8(), 1, false, "u8"},
		{IrType::u16(), 2, false, "u16"},
		{IrType::u32(), 4, false, "u32"},
		{IrType::u64(), 8, false, "u64"},
	};
	for (const Case &c : cases)
	{
		ASSERT_EQ(c.type.get_size(), c.size);
		ASSERT_EQ(c.type.get_alignment(), c.size);
		ASSERT_EQ(c.type.is_signed(), c.is_signed);
		ASSERT_EQ(c.type.to_string(), c.name);
	}
}
