#pragma once

namespace ir
{
	struct IrType
	{
		enum class Variant
		{
			BOOL,
			U8,
			U16,
			U32,
			I8,
			I16,
			I32,
		};

		Variant variant;

		IrType(Variant variant) : variant(variant) {}

		// convenience factories
		static IrType boolean() { return IrType(Variant::BOOL); }
		static IrType i8() { return IrType(Variant::I8); }
		static IrType i16() { return IrType(Variant::I16); }
		static IrType i32() { return IrType(Variant::I32); }
		static IrType u8() { return IrType(Variant::U8); }
		static IrType u16() { return IrType(Variant::U16); }
		static IrType u32() { return IrType(Variant::U32); }
	};
}