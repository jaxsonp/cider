#include "RiscvInstruction.hpp"

#include "utils/common.hpp"
#include "utils/error.hpp"

namespace codegen::riscv
{
	/// @brief The fixed bits of an instruction's encoding
	struct EncodingInfo
	{
		InstructionFormat fmt;
		uint32_t opcode;
		uint32_t funct3 = 0;
		/// For R-type instructions. Shifts by immediate also have one, it sits in the upper bits of their immediate
		uint32_t funct7 = 0;
	};

	// major opcodes
	static constexpr uint32_t OPCODE_LOAD = 0b0000011u;
	static constexpr uint32_t OPCODE_OP_IMM = 0b0010011u;
	static constexpr uint32_t OPCODE_AUIPC = 0b0010111u;
	static constexpr uint32_t OPCODE_OP_IMM_32 = 0b0011011u;
	static constexpr uint32_t OPCODE_STORE = 0b0100011u;
	static constexpr uint32_t OPCODE_OP = 0b0110011u;
	static constexpr uint32_t OPCODE_LUI = 0b0110111u;
	static constexpr uint32_t OPCODE_OP_32 = 0b0111011u;
	static constexpr uint32_t OPCODE_JALR = 0b1100111u;
	static constexpr uint32_t OPCODE_JAL = 0b1101111u;
	static constexpr uint32_t OPCODE_SYSTEM = 0b1110011u;

	static EncodingInfo encoding_info(Mnemonic mnemonic)
	{
		using Fmt = InstructionFormat;
		switch (mnemonic)
		{
		case Mnemonic::ADD:
			return {Fmt::RType, OPCODE_OP, 0x0u, 0x00u};
		case Mnemonic::SUB:
			return {Fmt::RType, OPCODE_OP, 0x0u, 0x20u};
		case Mnemonic::XOR:
			return {Fmt::RType, OPCODE_OP, 0x4u, 0x00u};
		case Mnemonic::OR:
			return {Fmt::RType, OPCODE_OP, 0x6u, 0x00u};
		case Mnemonic::AND:
			return {Fmt::RType, OPCODE_OP, 0x7u, 0x00u};
		case Mnemonic::SLL:
			return {Fmt::RType, OPCODE_OP, 0x1u, 0x00u};
		case Mnemonic::SRL:
			return {Fmt::RType, OPCODE_OP, 0x5u, 0x00u};
		case Mnemonic::SRA:
			return {Fmt::RType, OPCODE_OP, 0x5u, 0x20u};
		case Mnemonic::SLT:
			return {Fmt::RType, OPCODE_OP, 0x2u, 0x00u};
		case Mnemonic::SLTU:
			return {Fmt::RType, OPCODE_OP, 0x3u, 0x00u};
		case Mnemonic::ADDI:
			return {Fmt::IType, OPCODE_OP_IMM, 0x0u};
		case Mnemonic::XORI:
			return {Fmt::IType, OPCODE_OP_IMM, 0x4u};
		case Mnemonic::ORI:
			return {Fmt::IType, OPCODE_OP_IMM, 0x6u};
		case Mnemonic::ANDI:
			return {Fmt::IType, OPCODE_OP_IMM, 0x7u};
		case Mnemonic::SLLI:
			return {Fmt::IType, OPCODE_OP_IMM, 0x1u, 0x00u};
		case Mnemonic::SRLI:
			return {Fmt::IType, OPCODE_OP_IMM, 0x5u, 0x00u};
		case Mnemonic::SRAI:
			// srai is distinguished from slri by upper bits of immediate
			return {Fmt::IType, OPCODE_OP_IMM, 0x5u, 0x20u};
		case Mnemonic::SLTI:
			return {Fmt::IType, OPCODE_OP_IMM, 0x2u};
		case Mnemonic::SLTIU:
			return {Fmt::IType, OPCODE_OP_IMM, 0x3u};
		case Mnemonic::LB:
			return {Fmt::IType, OPCODE_LOAD, 0x0u};
		case Mnemonic::LH:
			return {Fmt::IType, OPCODE_LOAD, 0x1u};
		case Mnemonic::LW:
			return {Fmt::IType, OPCODE_LOAD, 0x2u};
		case Mnemonic::LBU:
			return {Fmt::IType, OPCODE_LOAD, 0x4u};
		case Mnemonic::LHU:
			return {Fmt::IType, OPCODE_LOAD, 0x5u};
		case Mnemonic::SB:
			return {Fmt::SType, OPCODE_STORE, 0x0u};
		case Mnemonic::SH:
			return {Fmt::SType, OPCODE_STORE, 0x1u};
		case Mnemonic::SW:
			return {Fmt::SType, OPCODE_STORE, 0x2u};
		case Mnemonic::JAL:
			return {Fmt::JType, OPCODE_JAL};
		case Mnemonic::JALR:
			return {Fmt::IType, OPCODE_JALR, 0x0u};
		case Mnemonic::LUI:
			return {Fmt::UType, OPCODE_LUI};
		case Mnemonic::AUIPC:
			return {Fmt::UType, OPCODE_AUIPC};
		case Mnemonic::ECALL:
		case Mnemonic::EBREAK:
			// these two are told apart by their immediate
			return {Fmt::IType, OPCODE_SYSTEM, 0x0u};

		case Mnemonic::ADDW:
			return {Fmt::RType, OPCODE_OP_32, 0x0u, 0x00u};
		case Mnemonic::SUBW:
			return {Fmt::RType, OPCODE_OP_32, 0x0u, 0x20u};
		case Mnemonic::SLLW:
			return {Fmt::RType, OPCODE_OP_32, 0x1u, 0x00u};
		case Mnemonic::SRLW:
			return {Fmt::RType, OPCODE_OP_32, 0x5u, 0x00u};
		case Mnemonic::SRAW:
			return {Fmt::RType, OPCODE_OP_32, 0x5u, 0x20u};
		case Mnemonic::ADDIW:
			return {Fmt::IType, OPCODE_OP_IMM_32, 0x0u};
		case Mnemonic::SLLIW:
			return {Fmt::IType, OPCODE_OP_IMM_32, 0x1u, 0x00u};
		case Mnemonic::SRLIW:
			return {Fmt::IType, OPCODE_OP_IMM_32, 0x5u, 0x00u};
		case Mnemonic::SRAIW:
			return {Fmt::IType, OPCODE_OP_IMM_32, 0x5u, 0x20u};
		case Mnemonic::LD:
			return {Fmt::IType, OPCODE_LOAD, 0x3u};
		case Mnemonic::LWU:
			return {Fmt::IType, OPCODE_LOAD, 0x6u};
		case Mnemonic::SD:
			return {Fmt::SType, OPCODE_STORE, 0x3u};

		case Mnemonic::MUL:
			return {Fmt::RType, OPCODE_OP, 0x0u, 0x01u};
		case Mnemonic::MULH:
			return {Fmt::RType, OPCODE_OP, 0x1u, 0x01u};
		case Mnemonic::MULHSU:
			return {Fmt::RType, OPCODE_OP, 0x2u, 0x01u};
		case Mnemonic::MULHU:
			return {Fmt::RType, OPCODE_OP, 0x3u, 0x01u};
		case Mnemonic::DIV:
			return {Fmt::RType, OPCODE_OP, 0x4u, 0x01u};
		case Mnemonic::DIVU:
			return {Fmt::RType, OPCODE_OP, 0x5u, 0x01u};
		case Mnemonic::REM:
			return {Fmt::RType, OPCODE_OP, 0x6u, 0x01u};
		case Mnemonic::REMU:
			return {Fmt::RType, OPCODE_OP, 0x7u, 0x01u};

		case Mnemonic::MULW:
			return {Fmt::RType, OPCODE_OP_32, 0x0u, 0x01u};
		case Mnemonic::DIVW:
			return {Fmt::RType, OPCODE_OP_32, 0x4u, 0x01u};
		case Mnemonic::DIVUW:
			return {Fmt::RType, OPCODE_OP_32, 0x5u, 0x01u};
		case Mnemonic::REMW:
			return {Fmt::RType, OPCODE_OP_32, 0x6u, 0x01u};
		case Mnemonic::REMUW:
			return {Fmt::RType, OPCODE_OP_32, 0x7u, 0x01u};
		}
		throw CompilerError::internal("Uncaught RISC-V mnemonic variant");
	}

	/// @brief Whether this is a shift by an immediate, whose immediate is a shift amount below a funct7
	static bool is_immediate_shift(Mnemonic mnemonic)
	{
		switch (mnemonic)
		{
		case Mnemonic::SLLI:
		case Mnemonic::SRLI:
		case Mnemonic::SRAI:
		case Mnemonic::SLLIW:
		case Mnemonic::SRLIW:
		case Mnemonic::SRAIW:
			return true;
		default:
			return false;
		}
	}

	/// @brief Builds R-type instruction
	/// ```
	/// 0-6   | opcode
	/// 7-11  | rd
	/// 12-14 | funct3
	/// 15-19 | rs1
	/// 20-24 | rs2
	/// 25-31 | funct7
	/// ```
	static uint32_t encode_r_type(uint32_t opcode, Register rd, uint32_t funct3, Register rs1, Register rs2, uint32_t funct7)
	{
		uint32_t instr = opcode & lower_bitmask<uint32_t>(7);
		instr |= (uint32_t(rd) & lower_bitmask<uint32_t>(5)) << 7;
		instr |= (funct3 & lower_bitmask<uint32_t>(3)) << 12;
		instr |= (uint32_t(rs1) & lower_bitmask<uint32_t>(5)) << 15;
		instr |= (uint32_t(rs2) & lower_bitmask<uint32_t>(5)) << 20;
		instr |= (funct7 & lower_bitmask<uint32_t>(7)) << 25;
		return instr;
	}

	/// @brief Builds I-type instruction
	/// ```
	/// 0-6   | opcode
	/// 7-11  | rd
	/// 12-14 | funct3
	/// 15-19 | rs1
	/// 20-31 | imm bits 0-11
	/// ```
	static uint32_t encode_i_type(uint32_t opcode, Register rd, uint32_t funct3, Register rs1, uint32_t imm)
	{
		uint32_t instr = opcode & lower_bitmask<uint32_t>(7);
		instr |= (uint32_t(rd) & lower_bitmask<uint32_t>(5)) << 7;
		instr |= (funct3 & lower_bitmask<uint32_t>(3)) << 12;
		instr |= (uint32_t(rs1) & lower_bitmask<uint32_t>(5)) << 15;
		instr |= (imm & lower_bitmask<uint32_t>(12)) << 20;
		return instr;
	}

	/// @brief Builds S-type instruction
	/// ```
	/// 0-6   | opcode
	/// 7-11  | imm bits 0-4
	/// 12-14 | funct3
	/// 15-19 | rs1
	/// 20-24 | rs2
	/// 25-31 | imm bits 5-11
	/// ```
	static uint32_t encode_s_type(uint32_t opcode, uint32_t funct3, Register rs1, Register rs2, uint32_t imm)
	{
		uint32_t imm_4_0 = imm & lower_bitmask<uint32_t>(5);
		uint32_t imm_11_5 = (imm >> 5) & lower_bitmask<uint32_t>(7);

		uint32_t instr = opcode & lower_bitmask<uint32_t>(7);
		instr |= imm_4_0 << 7;
		instr |= (funct3 & lower_bitmask<uint32_t>(3)) << 12;
		instr |= (uint32_t(rs1) & lower_bitmask<uint32_t>(5)) << 15;
		instr |= (uint32_t(rs2) & lower_bitmask<uint32_t>(5)) << 20;
		instr |= imm_11_5 << 25;
		return instr;
	}

	/// @brief Builds J-type instruction
	/// ```
	/// 0-6   | opcode
	/// 7-11  | rd
	/// 12-19 | imm bits 12-19
	/// 20    | imm bit 11
	/// 21-30 | imm bits 1-10
	/// 31    | imm bit 20
	/// ```
	static uint32_t encode_j_type(uint32_t opcode, Register rd, uint32_t imm)
	{
		// scrambling immediate
		uint32_t imm_10_1 = (imm >> 1) & lower_bitmask<uint32_t>(10);
		uint32_t imm_11 = (imm >> 11) & 0b1;
		uint32_t imm_19_12 = (imm >> 12) & lower_bitmask<uint32_t>(8);
		uint32_t imm_20 = (imm >> 20) & 0b1;

		uint32_t inst = opcode & lower_bitmask<uint32_t>(7);
		inst |= (uint32_t(rd) & lower_bitmask<uint32_t>(5)) << 7;
		inst |= imm_19_12 << 12;
		inst |= imm_11 << 20;
		inst |= imm_10_1 << 21;
		inst |= imm_20 << 31;

		return inst;
	}

	/// @brief Builds U-type instruction
	/// ```
	/// 0-6   | opcode
	/// 7-11  | rd
	/// 12-31 | imm bits 12-31
	/// ```
	static uint32_t encode_u_type(uint32_t opcode, Register rd, uint32_t imm)
	{
		uint32_t instr = opcode & lower_bitmask<uint32_t>(7);
		instr |= (uint32_t(rd) & lower_bitmask<uint32_t>(5)) << 7;
		instr |= imm & ~lower_bitmask<uint32_t>(12);
		return instr;
	}

	// TODO B-type instructions, once the IR has branches to lower
	/*static uint32_t encode_b_type(uint32_t opcode, uint32_t funct3, uint32_t rs1, uint32_t rs2, int32_t imm)
	{
		uint32_t inst = opcode;
		inst |= (funct3 << 12);
		inst |= (rs1 << 15);
		inst |= (rs2 << 20);

		// scramble immediate
		uint32_t b_12 = (imm >> 12) & 0x1;
		uint32_t b_11 = (imm >> 11) & 0x1;
		uint32_t b_10_5 = (imm >> 5) & 0x3F;
		uint32_t b_4_1 = (imm >> 1) & 0xF;

		inst |= (b_12 << 31);
		inst |= (b_10_5 << 25);
		inst |= (b_4_1 << 8);
		inst |= (b_11 << 7);

		return inst;
	}*/

	InstructionFormat Instruction::format() const
	{
		return encoding_info(this->mnemonic).fmt;
	}

	uint32_t Instruction::encode() const
	{
		EncodingInfo info = encoding_info(this->mnemonic);
		uint32_t imm = uint32_t(this->imm);

		switch (info.fmt)
		{
		case InstructionFormat::RType:
			return encode_r_type(info.opcode, this->rd, info.funct3, this->rs1, this->rs2, info.funct7);
		case InstructionFormat::IType:
			if (is_immediate_shift(this->mnemonic))
			{
				// the shift amount is 5 bits wide, or 6 for the RV64 shifts that operate on the whole register.
				// either way the funct7 sits above it (losing its lowest bit to the 6th shift amount bit)
				bool is_word_shift = info.opcode == OPCODE_OP_IMM_32;
				imm = (imm & lower_bitmask<uint32_t>(is_word_shift ? 5 : 6)) | (info.funct7 << 5);
			}
			else if (this->mnemonic == Mnemonic::ECALL)
				imm = 0x0u;
			else if (this->mnemonic == Mnemonic::EBREAK)
				imm = 0x1u;
			return encode_i_type(info.opcode, this->rd, info.funct3, this->rs1, imm);
		case InstructionFormat::SType:
			return encode_s_type(info.opcode, info.funct3, this->rs1, this->rs2, imm);
		case InstructionFormat::BType:
			throw CompilerError::unimplemented("TODO B-type instructions");
		case InstructionFormat::UType:
			return encode_u_type(info.opcode, this->rd, imm);
		case InstructionFormat::JType:
			return encode_j_type(info.opcode, this->rd, imm);
		}
		throw CompilerError::internal("Uncaught RISC-V instruction format variant");
	}

	// compressed instructions =========================

	/// @brief Whether a register is one of x8-x15, the only ones most compressed instructions can address
	static bool is_compressible_reg(Register reg)
	{
		return uint8_t(reg) >= 8 && uint8_t(reg) <= 15;
	}

	/// @brief 3 bit register field used by most compressed instructions (x8-x15 as 0-7)
	static uint16_t creg(Register reg)
	{
		return uint16_t(uint8_t(reg) - 8);
	}

	/// @brief Extracts bits `lo` to `hi` (inclusive) of a value, shifted down to bit 0
	static uint16_t bits(int64_t value, unsigned int hi, unsigned int lo)
	{
		return uint16_t((uint64_t(value) >> lo) & lower_bitmask<uint64_t>(hi - lo + 1));
	}

	/// @brief Sign extends the lower `n` bits of a value
	static int64_t sign_extend(int64_t value, unsigned int n)
	{
		uint64_t sign_bit = uint64_t(1) << (n - 1);
		uint64_t lower = uint64_t(value) & lower_bitmask<uint64_t>(n);
		return int64_t((lower ^ sign_bit) - sign_bit);
	}

	/// @brief Whether a value is representable as a 6 bit signed immediate, like most compressed instructions have
	static bool fits_simm6(int64_t value)
	{
		return value >= -32 && value <= 31;
	}

	/// @brief Builds a compressed instruction in the CI format, with a full 5 bit register field
	/// ```
	/// 0-1   | op
	/// 2-6   | imm bits 0-4
	/// 7-11  | rd/rs1
	/// 12    | imm bit 5
	/// 13-15 | funct3
	/// ```
	static uint16_t encode_ci_type(uint16_t op, uint16_t funct3, Register rd, int64_t imm)
	{
		return uint16_t(op | (bits(imm, 4, 0) << 2) | (uint16_t(rd) << 7) | (bits(imm, 5, 5) << 12) | (funct3 << 13));
	}

	/// @brief Builds a compressed instruction in the CR format
	/// ```
	/// 0-1   | op
	/// 2-6   | rs2
	/// 7-11  | rd/rs1
	/// 12-15 | funct4
	/// ```
	static uint16_t encode_cr_type(uint16_t op, uint16_t funct4, Register rd, Register rs2)
	{
		return uint16_t(op | (uint16_t(rs2) << 2) | (uint16_t(rd) << 7) | (funct4 << 12));
	}

	/// @brief Builds a compressed register-register instruction in the CA format
	/// ```
	/// 0-1   | op
	/// 2-4   | rs2'
	/// 5-6   | funct2
	/// 7-9   | rd'/rs1'
	/// 10-15 | funct6
	/// ```
	static uint16_t encode_ca_type(uint16_t funct6, Register rd, uint16_t funct2, Register rs2)
	{
		return uint16_t(0b01 | (creg(rs2) << 2) | (funct2 << 5) | (creg(rd) << 7) | (funct6 << 10));
	}

	/// @brief Builds a compressed shift/and by immediate instruction in the CB format
	/// ```
	/// 0-1   | op
	/// 2-6   | imm bits 0-4
	/// 7-9   | rd'/rs1'
	/// 10-11 | funct2
	/// 12    | imm bit 5
	/// 13-15 | funct3
	/// ```
	static uint16_t encode_cb_type(uint16_t funct2, Register rd, int64_t imm)
	{
		return uint16_t(0b01 | (bits(imm, 4, 0) << 2) | (creg(rd) << 7) | (funct2 << 10) | (bits(imm, 5, 5) << 12) | (0b100 << 13));
	}

	std::optional<uint16_t> Instruction::try_compress(unsigned int xlen) const
	{
		const Register rd = this->rd;
		const Register rs1 = this->rs1;
		const Register rs2 = this->rs2;
		const Register zero = Register::zero;
		const Register sp = Register::sp;

		switch (this->mnemonic)
		{
		case Mnemonic::ADDI:
		{
			int64_t imm = sign_extend(this->imm, 12);
			// c.nop
			if (rd == zero && rs1 == zero && imm == 0)
				return encode_ci_type(0b01, 0b000, zero, 0);
			if (rd == zero)
				return std::nullopt;
			// c.addi
			if (rd == rs1 && imm != 0 && fits_simm6(imm))
				return encode_ci_type(0b01, 0b000, rd, imm);
			// c.li
			if (rs1 == zero && fits_simm6(imm))
				return encode_ci_type(0b01, 0b010, rd, imm);
			// c.mv
			if (rs1 != zero && imm == 0)
				return encode_cr_type(0b10, 0b1000, rd, rs1);
			// c.addi16sp, immediate is a multiple of 16, scrambled as [9] and [4|6|8:7|5]
			if (rd == sp && rs1 == sp && imm != 0 && imm % 16 == 0 && imm >= -512 && imm <= 496)
				return uint16_t(0b01 | (bits(imm, 5, 5) << 2) | (bits(imm, 8, 7) << 3) | (bits(imm, 6, 6) << 5) | (bits(imm, 4, 4) << 6) |
								(uint16_t(sp) << 7) | (bits(imm, 9, 9) << 12) | (0b011 << 13));
			// c.addi4spn, immediate is an unsigned multiple of 4, scrambled as [5:4|9:6|2|3]
			if (rs1 == sp && is_compressible_reg(rd) && imm > 0 && imm % 4 == 0 && imm <= 1020)
				return uint16_t(0b00 | (creg(rd) << 2) | (bits(imm, 3, 3) << 5) | (bits(imm, 2, 2) << 6) | (bits(imm, 9, 6) << 7) |
								(bits(imm, 5, 4) << 11) | (0b000 << 13));
			return std::nullopt;
		}
		case Mnemonic::ADDIW:
		{
			// c.addiw (this encoding is c.jal in RV32, but addiw doesn't exist there anyway)
			int64_t imm = sign_extend(this->imm, 12);
			if (xlen == 64 && rd != zero && rd == rs1 && fits_simm6(imm))
				return encode_ci_type(0b01, 0b001, rd, imm);
			return std::nullopt;
		}
		case Mnemonic::LUI:
		{
			// c.lui, takes bits 12-17 of the value
			int64_t upper = sign_extend(this->imm >> 12, 20);
			if (rd != zero && rd != sp && upper != 0 && fits_simm6(upper))
				return encode_ci_type(0b01, 0b011, rd, upper);
			return std::nullopt;
		}
		case Mnemonic::SLLI:
		{
			// c.slli
			int64_t shamt = this->imm & lower_bitmask<int64_t>(xlen == 64 ? 6 : 5);
			if (rd != zero && rd == rs1 && shamt != 0)
				return encode_ci_type(0b10, 0b000, rd, shamt);
			return std::nullopt;
		}
		case Mnemonic::SRLI:
		case Mnemonic::SRAI:
		{
			// c.srli, c.srai
			int64_t shamt = this->imm & lower_bitmask<int64_t>(xlen == 64 ? 6 : 5);
			if (is_compressible_reg(rd) && rd == rs1 && shamt != 0)
				return encode_cb_type(this->mnemonic == Mnemonic::SRLI ? 0b00 : 0b01, rd, shamt);
			return std::nullopt;
		}
		case Mnemonic::ANDI:
		{
			// c.andi
			int64_t imm = sign_extend(this->imm, 12);
			if (is_compressible_reg(rd) && rd == rs1 && fits_simm6(imm))
				return encode_cb_type(0b10, rd, imm);
			return std::nullopt;
		}
		case Mnemonic::ADD:
			if (rd == zero)
				return std::nullopt;
			// c.mv
			if (rs1 == zero && rs2 != zero)
				return encode_cr_type(0b10, 0b1000, rd, rs2);
			if (rs2 == zero && rs1 != zero)
				return encode_cr_type(0b10, 0b1000, rd, rs1);
			// c.add (addition is commutative, so the destination can be either operand)
			if (rd == rs1 && rs2 != zero)
				return encode_cr_type(0b10, 0b1001, rd, rs2);
			if (rd == rs2 && rs1 != zero)
				return encode_cr_type(0b10, 0b1001, rd, rs1);
			return std::nullopt;
		case Mnemonic::SUB:
			// c.sub
			if (is_compressible_reg(rd) && rd == rs1 && is_compressible_reg(rs2))
				return encode_ca_type(0b100011, rd, 0b00, rs2);
			return std::nullopt;
		case Mnemonic::XOR:
		case Mnemonic::OR:
		case Mnemonic::AND:
		{
			// c.xor, c.or, c.and (all commutative, so the destination can be either operand)
			uint16_t funct2 = this->mnemonic == Mnemonic::XOR ? 0b01 : (this->mnemonic == Mnemonic::OR ? 0b10 : 0b11);
			if (!is_compressible_reg(rd) || !is_compressible_reg(rs1) || !is_compressible_reg(rs2))
				return std::nullopt;
			if (rd == rs1)
				return encode_ca_type(0b100011, rd, funct2, rs2);
			if (rd == rs2)
				return encode_ca_type(0b100011, rd, funct2, rs1);
			return std::nullopt;
		}
		case Mnemonic::SUBW:
			// c.subw
			if (xlen == 64 && is_compressible_reg(rd) && rd == rs1 && is_compressible_reg(rs2))
				return encode_ca_type(0b100111, rd, 0b00, rs2);
			return std::nullopt;
		case Mnemonic::ADDW:
			// c.addw (commutative, so the destination can be either operand)
			if (xlen != 64 || !is_compressible_reg(rd) || !is_compressible_reg(rs1) || !is_compressible_reg(rs2))
				return std::nullopt;
			if (rd == rs1)
				return encode_ca_type(0b100111, rd, 0b01, rs2);
			if (rd == rs2)
				return encode_ca_type(0b100111, rd, 0b01, rs1);
			return std::nullopt;
		case Mnemonic::LW:
		{
			int64_t offset = sign_extend(this->imm, 12);
			if (offset < 0 || offset % 4 != 0)
				return std::nullopt;
			// c.lwsp, offset scrambled as [5] and [4:2|7:6]
			if (rs1 == sp && rd != zero && offset <= 252)
				return uint16_t(0b10 | (bits(offset, 7, 6) << 2) | (bits(offset, 4, 2) << 4) | (uint16_t(rd) << 7) |
								(bits(offset, 5, 5) << 12) | (0b010 << 13));
			// c.lw, offset scrambled as [5:3] and [2|6]
			if (is_compressible_reg(rs1) && is_compressible_reg(rd) && offset <= 124)
				return uint16_t(0b00 | (creg(rd) << 2) | (bits(offset, 6, 6) << 5) | (bits(offset, 2, 2) << 6) | (creg(rs1) << 7) |
								(bits(offset, 5, 3) << 10) | (0b010 << 13));
			return std::nullopt;
		}
		case Mnemonic::LD:
		{
			// (these encodings are c.flwsp and c.flw in RV32, but ld doesn't exist there anyway)
			int64_t offset = sign_extend(this->imm, 12);
			if (xlen != 64 || offset < 0 || offset % 8 != 0)
				return std::nullopt;
			// c.ldsp, offset scrambled as [5] and [4:3|8:6]
			if (rs1 == sp && rd != zero && offset <= 504)
				return uint16_t(0b10 | (bits(offset, 8, 6) << 2) | (bits(offset, 4, 3) << 5) | (uint16_t(rd) << 7) |
								(bits(offset, 5, 5) << 12) | (0b011 << 13));
			// c.ld, offset scrambled as [5:3] and [7:6]
			if (is_compressible_reg(rs1) && is_compressible_reg(rd) && offset <= 248)
				return uint16_t(0b00 | (creg(rd) << 2) | (bits(offset, 7, 6) << 5) | (creg(rs1) << 7) |
								(bits(offset, 5, 3) << 10) | (0b011 << 13));
			return std::nullopt;
		}
		case Mnemonic::SW:
		{
			int64_t offset = sign_extend(this->imm, 12);
			if (offset < 0 || offset % 4 != 0)
				return std::nullopt;
			// c.swsp, offset scrambled as [5:2|7:6]
			if (rs1 == sp && offset <= 252)
				return uint16_t(0b10 | (uint16_t(rs2) << 2) | (bits(offset, 7, 6) << 7) | (bits(offset, 5, 2) << 9) | (0b110 << 13));
			// c.sw, offset scrambled as [5:3] and [2|6]
			if (is_compressible_reg(rs1) && is_compressible_reg(rs2) && offset <= 124)
				return uint16_t(0b00 | (creg(rs2) << 2) | (bits(offset, 6, 6) << 5) | (bits(offset, 2, 2) << 6) | (creg(rs1) << 7) |
								(bits(offset, 5, 3) << 10) | (0b110 << 13));
			return std::nullopt;
		}
		case Mnemonic::SD:
		{
			// (these encodings are c.fswsp and c.fsw in RV32, but sd doesn't exist there anyway)
			int64_t offset = sign_extend(this->imm, 12);
			if (xlen != 64 || offset < 0 || offset % 8 != 0)
				return std::nullopt;
			// c.sdsp, offset scrambled as [5:3|8:6]
			if (rs1 == sp && offset <= 504)
				return uint16_t(0b10 | (uint16_t(rs2) << 2) | (bits(offset, 8, 6) << 7) | (bits(offset, 5, 3) << 10) | (0b111 << 13));
			// c.sd, offset scrambled as [5:3] and [7:6]
			if (is_compressible_reg(rs1) && is_compressible_reg(rs2) && offset <= 248)
				return uint16_t(0b00 | (creg(rs2) << 2) | (bits(offset, 7, 6) << 5) | (creg(rs1) << 7) |
								(bits(offset, 5, 3) << 10) | (0b111 << 13));
			return std::nullopt;
		}
		case Mnemonic::JALR:
		{
			int64_t offset = sign_extend(this->imm, 12);
			if (rs1 == zero || offset != 0)
				return std::nullopt;
			// c.jr
			if (rd == zero)
				return encode_cr_type(0b10, 0b1000, rs1, zero);
			// c.jalr
			if (rd == Register::ra)
				return encode_cr_type(0b10, 0b1001, rs1, zero);
			return std::nullopt;
		}
		case Mnemonic::EBREAK:
			// c.ebreak
			return encode_cr_type(0b10, 0b1001, zero, zero);
		default:
			// TODO c.j, c.jal, c.beqz and c.bnez. their reach is short, so using them needs jump relaxation
			return std::nullopt;
		}
	}
}
