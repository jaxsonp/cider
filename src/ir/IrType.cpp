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
}
