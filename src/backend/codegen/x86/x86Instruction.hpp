#pragma once

#include <stdint.h>
#include <array>
#include <optional>
#include <variant>
#include <vector>

#include "backend/codegen/x86/x86.hpp"

/// Untyped instruction encoding, used by the `Assembler` to turn its typed `write_*` functions into bytes. Nothing
/// here checks that an instruction makes sense, that's the job of the `Assembler`'s signatures. What is checked
/// here is what depends on the mode: everything that needs a REX prefix is rejected in 32 bit mode
namespace codegen::x86::detail
{
	/// @brief A register in an encoding
	struct RegNum
	{
		/// Hardware number, 0-15
		uint8_t num;
		/// Whether this is an 8 bit access, where 4-7 mean SPL/BPL/SIL/DIL (which need a REX prefix, without one
		/// they'd mean AH/CH/DH/BH)
		bool is_byte;
	};

	/// @brief A memory operand in an encoding
	struct MemRef
	{
		uint8_t base;
		std::optional<uint8_t> index;
		/// Scale encoded as its log2 (0-3)
		uint8_t scale_bits;
		int32_t disp;
	};

	/// @brief The operand that goes in the rm field (and SIB byte) of ModRM
	using RmOperand = std::variant<RegNum, MemRef>;

	/// @brief One encoded instruction, laid out like the machine code it becomes:
	/// `[0x66] [REX] opcode [ModRM [SIB] [disp]] [imm]`
	struct Encoding
	{
		/// Operand size prefix (0x66), for 16 bit operands
		bool operand_size_prefix = false;
		/// REX.W, for 64 bit operands
		bool rex_w = false;

		std::array<uint8_t, 3> opcode = {};
		uint8_t opcode_len = 0;

		/// Register added to the last opcode byte ("+r" in the Intel manual)
		std::optional<RegNum> opcode_reg;

		/// Whether there is a ModRM byte
		bool has_modrm = false;
		/// Value of the reg field: a register, or the opcode extension ("/digit")
		RegNum reg_field = {0, false};
		/// Operand in the rm field
		RmOperand rm = RegNum{0, false};

		/// Immediate, stored little endian in its lower `imm_size` bytes
		int64_t imm = 0;
		uint8_t imm_size = 0;

		/// @brief Appends the encoded bytes to a buffer
		/// @param is_x64 Whether this is assembled for 64 bit mode. In 32 bit mode, an instruction needing a REX
		/// prefix (64 bit operands, r8-r15, SPL/BPL/SIL/DIL) is an internal error
		void encode(std::vector<uint8_t> &buf, bool is_x64) const;
	};
}
