#pragma once

#include <stdint.h>
#include <optional>

#include "backend/codegen/riscv/Riscv.hpp"

namespace codegen::riscv
{
	enum class InstructionFormat : uint8_t
	{
		/// register-register operations
		RType,
		/// register-immediate operations
		IType,
		/// load and store
		SType,
		/// branch instructions
		BType,
		/// upper immediate operators
		UType,
		/// jump instructions
		JType,
	};

	enum class Mnemonic : uint8_t
	{
		// RV32I
		ADD,
		SUB,
		XOR,
		OR,
		AND,
		SLL,
		SRL,
		SRA,
		SLT,
		SLTU,
		ADDI,
		XORI,
		ORI,
		ANDI,
		SLLI,
		SRLI,
		SRAI,
		SLTI,
		SLTIU,
		LB,
		LH,
		LW,
		LBU,
		LHU,
		SB,
		SH,
		SW,
		JAL,
		JALR,
		LUI,
		AUIPC,
		ECALL,
		EBREAK,

		// RV64I
		ADDW,
		SUBW,
		SLLW,
		SRLW,
		SRAW,
		ADDIW,
		SLLIW,
		SRLIW,
		SRAIW,
		LD,
		LWU,
		SD,

		// M extension
		MUL,
		MULH,
		MULHSU,
		MULHU,
		DIV,
		DIVU,
		REM,
		REMU,

		// M extension, RV64
		MULW,
		DIVW,
		DIVUW,
		REMW,
		REMUW,
	};

	/// @brief A single machine instruction, before it's encoded. Operands that an instruction doesn't have
	/// are ignored
	struct Instruction
	{
		Mnemonic mnemonic;
		Register rd = Register::zero;
		Register rs1 = Register::zero;
		Register rs2 = Register::zero;
		/// Immediate operand, only the bits the instruction's format has room for are used:
		/// - I-type, S-type: the lower 12 bits (sign extended by the processor)
		/// - shifts by immediate: the lower 5 bits (6 bits for the non-W shifts, for RV64)
		/// - U-type: bits 12-31, so the value given is the value that ends up in the register
		/// - J-type: bits 1-20
		int64_t imm = 0;

		/// @brief How this instruction's operands are laid out
		InstructionFormat format() const;

		/// @brief Builds the standard 32 bit encoding
		uint32_t encode() const;

		/// @brief Builds the 16 bit encoding from the C extension, for instructions and operands that have one
		/// @param xlen Register width (32 or 64), since a few encodings mean different things in RV32 and RV64
		/// @return The compressed instruction, nullopt if this instruction can't be compressed
		std::optional<uint16_t> try_compress(unsigned int xlen) const;
	};
}
