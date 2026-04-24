#include "frontend/FrontendType.hpp"

#include <format>

#include "utils/error.hpp"
#include "FrontendType.hpp"

bool FrontendType::is_integer() const
{
	return this->variant == Variant::I8 ||
		   this->variant == Variant::I16 ||
		   this->variant == Variant::I32 ||
		   this->variant == Variant::U8 ||
		   this->variant == Variant::U16 ||
		   this->variant == Variant::U32;
}

std::string FrontendType::to_string() const
{
	switch (this->variant)
	{
	case Variant::VOID:
		return "void";
	case Variant::BOOL:
		return "bool";
	case Variant::I8:
		return "i8";
	case Variant::I16:
		return "i16";
	case Variant::I32:
		return "i32";
	case Variant::U8:
		return "u8";
	case Variant::U16:
		return "u16";
	case Variant::U32:
		return "u32";
	default:
		throw CompilerError::internal("Uncaught frontend type variant");
	}
}

FrontendType FrontendType::from_string(std::string_view s)
{
	if (s.empty())
		return FrontendType::unresolved();
	else if (s == "void")
		return FrontendType::void_type();
	else if (s == "bool")
		return FrontendType::boolean();
	else if (s == "u8")
		return FrontendType::u8();
	else if (s == "u16")
		return FrontendType::u16();
	else if (s == "u32")
		return FrontendType::u32();
	else if (s == "i8")
		return FrontendType::i8();
	else if (s == "i16")
		return FrontendType::i16();
	else if (s == "i32")
		return FrontendType::i32();
	else
		return FrontendType::unknown();
}

ir::IrType FrontendType::resolveType() const
{
	switch (this->variant)
	{
	case FrontendType::Variant::BOOL:
		return ir::IrType::boolean();
	case FrontendType::Variant::I8:
		return ir::IrType::i8();
	case FrontendType::Variant::I16:
		return ir::IrType::i16();
	case FrontendType::Variant::I32:
		return ir::IrType::i32();
	case FrontendType::Variant::U8:
		return ir::IrType::u8();
	case FrontendType::Variant::U16:
		return ir::IrType::u16();
	case FrontendType::Variant::U32:
		return ir::IrType::u32();
	case FrontendType::Variant::VOID:
	case FrontendType::Variant::UNRESOLVED:
	case FrontendType::Variant::UNRESOLVED_INT:
	case FrontendType::Variant::UNKNOWN:
		throw CompilerError::internal("Attempted to resolve invalid type");
	default:
		throw CompilerError::internal("Uncaught frontend type variant");
	}
}

bool FrontendType::operator==(const FrontendType &other) const
{
	return (this->variant == other.variant && (this->variant != Variant::UNKNOWN || this->name == other.name));
}
