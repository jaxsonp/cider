#pragma once

#include <stdint.h>
#include <concepts>
#include <optional>
#include <format>

#include "backend/codegen/x86/x86.hpp"
#include "utils/error.hpp"

/******************************************************************************

Typed instruction operands. Their types carry the operand's kind (register, memory or immediate) and width,
which is what lets the `Assembler` reject invalid instructions at compile time: every `write_*` function only
accepts the kinds and widths that the instruction has an encoding for.

What can't be known at compile time (which register, the value of an immediate) is checked once, when the
operand is constructed, so that a constructed operand is always encodable. The exception is whether an operand
needs a REX prefix (64 bit operands, r8-r15, SPL/BPL/SIL/DIL), which depends on the mode being assembled for
and so is checked by the encoder instead.

******************************************************************************/
namespace codegen::x86
{
	/// @brief A register, accessed with a width (ie Reg<Width::W32>(Register::RAX) is EAX)
	template <Width W>
	class Reg
	{
		Register r;

	public:
		static constexpr Width width = W;

		constexpr explicit Reg(Register reg) : r(reg) {}

		constexpr Register reg() const { return this->r; }
	};

	using Reg8 = Reg<Width::W8>;
	using Reg16 = Reg<Width::W16>;
	using Reg32 = Reg<Width::W32>;
	using Reg64 = Reg<Width::W64>;

	/// @brief Multiplier of an index register. Only these four exist in the encoding
	enum class Scale : uint8_t
	{
		x1,
		x2,
		x4,
		x8,
	};

	/// @brief The `index * scale` part of a memory address
	class Index
	{
		Register r;
		Scale s;

	public:
		Index(Register reg, Scale scale) : r(reg), s(scale)
		{
			// in the SIB byte, an index of 100 (sp's number) means there is no index
			if (reg == Register::RSP)
				throw CompilerError::internal("x86: the stack pointer can't be an index register");
		}

		Register reg() const { return this->r; }
		Scale scale() const { return this->s; }
	};

	/// @brief A memory access, at `[base + index * scale + disp]`, of a width
	///
	/// Address registers are always the full register of the mode (ie ESP in 32 bit mode, RSP in 64 bit mode), no
	/// address size prefixes
	template <Width W>
	class Mem
	{
		Register b;
		std::optional<Index> i;
		int32_t d;

	public:
		static constexpr Width width = W;

		Mem(Register base, int32_t disp = 0) : b(base), d(disp) {}
		Mem(Register base, Index index, int32_t disp = 0) : b(base), i(index), d(disp) {}

		Register base() const { return this->b; }
		const std::optional<Index> &index() const { return this->i; }
		int32_t disp() const { return this->d; }
	};

	/// @brief Immediate operand of an instruction operating on values of width W. Its C++ type has exactly the
	/// range that the encoding has room for, so a value that doesn't fit can't be built by accident (narrowing in
	/// brace initialization is a compile error, and `checked` throws)
	///
	/// x86 has no 64 bit immediates outside of `mov r64, imm64` (see `Imm64`): instructions on 64 bit values take
	/// a 32 bit immediate, which the processor sign extends
	template <Width W>
	struct Imm
	{
		using value_type =
			std::conditional_t<W == Width::W8, int8_t,
							   std::conditional_t<W == Width::W16, int16_t, int32_t>>;

		value_type value;

		constexpr explicit Imm(value_type value) : value(value) {}

		/// @brief Builds an immediate from any value, throwing if it doesn't fit (as a signed value)
		static Imm checked(int64_t value)
		{
			if (value != int64_t(value_type(value)))
				throw CompilerError::internal(std::format("x86: immediate {} doesn't fit in {} bytes", value, sizeof(value_type)));
			return Imm(value_type(value));
		}
	};

	/// @brief Full 64 bit immediate, only used by `mov r64, imm64`
	struct Imm64
	{
		int64_t value;

		constexpr explicit Imm64(int64_t value) : value(value) {}
	};

	// operand kinds, for constraining the assembler's templates

	template <typename T>
	struct is_reg_op : std::false_type
	{
	};
	template <Width W>
	struct is_reg_op<Reg<W>> : std::true_type
	{
	};

	template <typename T>
	struct is_mem_op : std::false_type
	{
	};
	template <Width W>
	struct is_mem_op<Mem<W>> : std::true_type
	{
	};

	/// @brief A register operand of any width
	template <typename T>
	concept RegOp = is_reg_op<T>::value;

	/// @brief A memory operand of any width
	template <typename T>
	concept MemOp = is_mem_op<T>::value;

	/// @brief A register or memory operand of any width ("r/m" in the Intel manual)
	template <typename T>
	concept RmOp = RegOp<T> || MemOp<T>;
}
