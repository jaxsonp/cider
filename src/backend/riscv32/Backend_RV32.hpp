#pragma once

#include "backend/Backend.hpp"

namespace backends::rv32
{

	/// @brief RISC-V 32bit backend
	class Backend_RV32 : public Backend
	{
	public:
		virtual Object *lowerIr(IrObject *ir) override;
	};
}