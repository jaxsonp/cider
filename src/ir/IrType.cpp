#include "ir/IrType.hpp"

#include "utils/error.hpp"
#include "IrType.hpp"

namespace ir
{
	unsigned int ir::IrType::get_size() const
	{
		switch (this->variant)
		{
		case Variant::BOOL:
			return 1;
		case Variant::U8:
			return 1;
		case Variant::U16:
			return 2;
		case Variant::U32:
			return 4;
		case Variant::I8:
			return 1;
		case Variant::I16:
			return 2;
		case Variant::I32:
			return 4;
		};
		throw CompilerError::internal("Uncaught IR type variant");
	}

	unsigned int IrType::get_alignment() const
	{
		switch (this->variant)
		{
		case Variant::BOOL:
			return 1;
		case Variant::U8:
			return 1;
		case Variant::U16:
			return 2;
		case Variant::I8:
			return 1;
		case Variant::I16:
			return 2;
		};
		return 4;
	}

	std::string IrType::to_string() const
	{
		switch (this->variant)
		{
		case Variant::BOOL:
			return "bool";
		case Variant::U8:
			return "u8";
		case Variant::U16:
			return "u16";
		case Variant::U32:
			return "u32";
		case Variant::I8:
			return "i8";
		case Variant::I16:
			return "i16";
		case Variant::I32:
			return "i32";
		};
		throw CompilerError::internal("Uncaught IR type variant");
	}

	bool IrType::is_signed() const
	{
		switch (this->variant)
		{
		case Variant::I8:
		case Variant::I16:
		case Variant::I32:
			return true;
		case Variant::BOOL:
		case Variant::U8:
		case Variant::U16:
		case Variant::U32:
			return false;
		};
		throw CompilerError::internal("Uncaught IR type variant");
	}
}
