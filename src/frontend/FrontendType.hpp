#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

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
		UNRESOLVED_FLOAT,
		UNKNOWN,
		FUNCTION,
	};

	Variant variant;
	std::string name;

	/// Return type, if variant == FUNCTION. Boxed since FrontendType is recursive here.
	std::shared_ptr<FrontendType> function_return_type;
	std::vector<FrontendType> function_param_types;

	constexpr FrontendType() : variant(Variant::VOID) {}
	constexpr FrontendType(Variant variant) : variant(variant) {}

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
	static FrontendType function(FrontendType return_type, std::vector<FrontendType> param_types);

	bool is_integer() const;
	bool is_signed_integer() const;
	bool is_unsigned_integer() const;
	bool is_float() const;
	bool is_numeric() const;
	bool is_bool() const;
	bool is_function() const;

	/// @brief Return type of this function type, throwing if this is not a function type
	const FrontendType &return_type() const;
	/// @brief Parameter types of this function type, throwing if this is not a function type
	const std::vector<FrontendType> &param_types() const;

	std::string to_string() const;
	static FrontendType from_string(std::string_view s);

	/// @brief Attempts to convert/resolve self into IR type, throwing if not possible
	ir::IrType resolveType() const;

	bool operator==(const FrontendType &other) const;
};
