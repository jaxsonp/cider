#pragma once

namespace codegen::x86
{
	/// @brief Buffer for emission functions to write to
	///
	/// Instructions are referred to by their position (index) in the buffer, which is what the `write_*`
	/// functions return. Since instructions can be 2 or 4 bytes long (with the C extension), positions are
	/// not byte offsets, use `offset_of` for those
	class Assembler
	{
	};
}