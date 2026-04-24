#pragma once

#include <string>
#include <string_view>
#include <variant>

#include "ir/IrType.hpp"
#include "utils/common.hpp"

/// @brief A type in the context of the source code
struct FrontendType
{
	enum class Variant
	{
		VOID,
		BOOL,
		U8,
		U16,
		U32,
		I8,
		I16,
		I32,
		UNRESOLVED,
		UNRESOLVED_INT,
		UNKNOWN,
	};

	Variant variant;
	/// Name, if an unknown type
	std::string name;

	FrontendType() : variant(Variant::VOID) {}
	FrontendType(Variant variant) : variant(variant) {}

	// convenience factories
	static FrontendType void_type() { return FrontendType(Variant::VOID); }
	static FrontendType i8() { return FrontendType(Variant::I8); }
	static FrontendType i16() { return FrontendType(Variant::I16); }
	static FrontendType i32() { return FrontendType(Variant::I32); }
	static FrontendType u8() { return FrontendType(Variant::U8); }
	static FrontendType u16() { return FrontendType(Variant::U16); }
	static FrontendType u32() { return FrontendType(Variant::U32); }
	static FrontendType boolean() { return FrontendType(Variant::BOOL); }
	static FrontendType unresolved() { return FrontendType(Variant::UNRESOLVED); }
	static FrontendType unresolved_int() { return FrontendType(Variant::UNRESOLVED_INT); }
	static FrontendType unknown() { return FrontendType(Variant::UNKNOWN); }

	bool is_integer() const;

	std::string to_string() const;
	static FrontendType from_string(std::string_view s);

	/// @brief Attempts to convert/resolve self into IR type, throwing if not possible
	ir::IrType resolveType() const;

	bool operator==(const FrontendType &other) const;
};
