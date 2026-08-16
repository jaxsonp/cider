#include "frontend/FrontendType.hpp"

#include <format>

#include "utils/error.hpp"
#include "FrontendType.hpp"

bool FrontendType::is_integer() const
{
	switch (this->variant)
	{
	case Variant::I8:
	case Variant::I16:
	case Variant::I32:
	case Variant::U8:
	case Variant::U16:
	case Variant::U32:
	case Variant::UNRESOLVED_INT:
		return true;
	default:
		return false;
	}
}

bool FrontendType::is_signed_integer() const
{
	switch (this->variant)
	{
	case Variant::I8:
	case Variant::I16:
	case Variant::I32:
	case Variant::UNRESOLVED_INT:
		return true;
	default:
		return false;
	}
}

bool FrontendType::is_unsigned_integer() const
{
	switch (this->variant)
	{
	case Variant::U8:
	case Variant::U16:
	case Variant::U32:
	case Variant::UNRESOLVED_INT:
		return true;
	default:
		return false;
	}
}

bool FrontendType::is_float() const
{
	switch (this->variant)
	{
	case Variant::UNRESOLVED_FLOAT:
		return true;
	default:
		return false;
	}
}

bool FrontendType::is_numeric() const
{
	switch (this->variant)
	{
	case Variant::I8:
	case Variant::I16:
	case Variant::I32:
	case Variant::U8:
	case Variant::U16:
	case Variant::U32:
	case Variant::UNRESOLVED_INT:
	case Variant::UNRESOLVED_FLOAT:
		return true;
	default:
		return false;
	}
}

bool FrontendType::is_bool() const
{
	return this->variant == Variant::BOOL;
}

bool FrontendType::is_function() const
{
	return this->variant == Variant::FUNCTION;
}

FrontendType FrontendType::function(FrontendType return_type, std::vector<FrontendType> param_types)
{
	FrontendType type(Variant::FUNCTION);
	type.function_return_type = std::make_shared<FrontendType>(std::move(return_type));
	type.function_param_types = std::move(param_types);
	return type;
}

const FrontendType &FrontendType::return_type() const
{
	if (this->variant != Variant::FUNCTION)
		throw CompilerError::internal("Attempted to get return type of a non-function type");
	return *this->function_return_type;
}

const std::vector<FrontendType> &FrontendType::param_types() const
{
	if (this->variant != Variant::FUNCTION)
		throw CompilerError::internal("Attempted to get parameter types of a non-function type");
	return this->function_param_types;
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
	case Variant::FUNCTION:
	{
		std::string result = "fn(";
		for (size_t i = 0; i < this->function_param_types.size(); i++)
		{
			if (i > 0)
				result += ", ";
			result += this->function_param_types[i].to_string();
		}
		result += ") -> " + this->function_return_type->to_string();
		return result;
	}
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
	case FrontendType::Variant::FUNCTION:
		throw CompilerError::internal("Attempted to resolve invalid type");
	default:
		throw CompilerError::internal("Uncaught frontend type variant");
	}
}

bool FrontendType::operator==(const FrontendType &other) const
{
	if (this->variant != other.variant)
		return false;
	if (this->variant == Variant::UNKNOWN)
		return this->name == other.name;
	if (this->variant == Variant::FUNCTION)
		return *this->function_return_type == *other.function_return_type &&
			   this->function_param_types == other.function_param_types;
	return true;
}
