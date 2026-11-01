#include "unit_testing.hpp"
#include "backend/codegen/riscv/RiscvInstruction.hpp"

#include <format>
#include <vector>

using namespace codegen::riscv;
using enum Register;

// The expected encodings here come from the tables in the RISC-V spec, there's no RISC-V assembler to check against

/// Encoding as hex, so failures are readable
static std::string encoded(const Instruction &instr)
{
	return std::format("{:#010x}", instr.encode());
}

/// Compressed encoding as hex, or "none" if it has none
static std::string compressed(const Instruction &instr, unsigned int xlen)
{
	std::optional<uint16_t> encoding = instr.try_compress(xlen);
	return encoding.has_value() ? std::format("{:#06x}", *encoding) : "none";
}

// standard encodings ============================================================================================

TEST(r_type_encodings)
{
	const std::vector<std::pair<Mnemonic, uint32_t>> cases = {
		{Mnemonic::ADD, 0x00c58533u},
		{Mnemonic::SUB, 0x40c58533u},
		{Mnemonic::XOR, 0x00c5c533u},
		{Mnemonic::OR, 0x00c5e533u},
		{Mnemonic::AND, 0x00c5f533u},
		{Mnemonic::SLL, 0x00c59533u},
		{Mnemonic::SRL, 0x00c5d533u},
		{Mnemonic::SRA, 0x40c5d533u},
		{Mnemonic::SLT, 0x00c5a533u},
		{Mnemonic::SLTU, 0x00c5b533u},
		{Mnemonic::ADDW, 0x00c5853bu},
		{Mnemonic::SUBW, 0x40c5853bu},
		{Mnemonic::SLLW, 0x00c5953bu},
		{Mnemonic::SRLW, 0x00c5d53bu},
		{Mnemonic::SRAW, 0x40c5d53bu},
		{Mnemonic::MUL, 0x02c58533u},
		{Mnemonic::MULH, 0x02c59533u},
		{Mnemonic::MULHSU, 0x02c5a533u},
		{Mnemonic::MULHU, 0x02c5b533u},
		{Mnemonic::DIV, 0x02c5c533u},
		{Mnemonic::DIVU, 0x02c5d533u},
		{Mnemonic::REM, 0x02c5e533u},
		{Mnemonic::REMU, 0x02c5f533u},
		{Mnemonic::MULW, 0x02c5853bu},
		{Mnemonic::DIVW, 0x02c5c53bu},
		{Mnemonic::DIVUW, 0x02c5d53bu},
		{Mnemonic::REMW, 0x02c5e53bu},
		{Mnemonic::REMUW, 0x02c5f53bu},
	};
	for (const auto &[mnemonic, expected] : cases)
	{
		Instruction instr{mnemonic, a0, a1, a2};
		ASSERT_TRUE(instr.format() == InstructionFormat::RType);
		ASSERT_EQ(encoded(instr), std::format("{:#010x}", expected));
	}
}

TEST(i_type_encodings)
{
	const std::vector<std::pair<Mnemonic, uint32_t>> cases = {
		{Mnemonic::ADDI, 0xffb58513u},
		{Mnemonic::XORI, 0xffb5c513u},
		{Mnemonic::ORI, 0xffb5e513u},
		{Mnemonic::ANDI, 0xffb5f513u},
		{Mnemonic::SLTI, 0xffb5a513u},
		{Mnemonic::SLTIU, 0xffb5b513u},
		{Mnemonic::ADDIW, 0xffb5851bu},
		{Mnemonic::LB, 0xffb58503u},
		{Mnemonic::LH, 0xffb59503u},
		{Mnemonic::LW, 0xffb5a503u},
		{Mnemonic::LBU, 0xffb5c503u},
		{Mnemonic::LHU, 0xffb5d503u},
		{Mnemonic::LD, 0xffb5b503u},
		{Mnemonic::LWU, 0xffb5e503u},
		{Mnemonic::JALR, 0xffb58567u},
	};
	for (const auto &[mnemonic, expected] : cases)
	{
		Instruction instr{mnemonic, a0, a1, zero, -5};
		ASSERT_TRUE(instr.format() == InstructionFormat::IType);
		ASSERT_EQ(encoded(instr), std::format("{:#010x}", expected));
	}
}

TEST(s_type_encodings)
{
	const std::vector<std::pair<Mnemonic, uint32_t>> cases = {
		{Mnemonic::SB, 0xfec58da3u},
		{Mnemonic::SH, 0xfec59da3u},
		{Mnemonic::SW, 0xfec5ada3u},
		{Mnemonic::SD, 0xfec5bda3u},
	};
	for (const auto &[mnemonic, expected] : cases)
	{
		Instruction instr{mnemonic, zero, a1, a2, -5};
		ASSERT_TRUE(instr.format() == InstructionFormat::SType);
		ASSERT_EQ(encoded(instr), std::format("{:#010x}", expected));
	}
}

TEST(known_instructions)
{
	// sd ra, 8(sp)
	ASSERT_EQ(encoded({Mnemonic::SD, zero, sp, ra, 8}), "0x00113423");
	// lw a0, 8(sp)
	ASSERT_EQ(encoded({Mnemonic::LW, a0, sp, zero, 8}), "0x00812503");
	// jalr zero, 0(ra), aka ret
	ASSERT_EQ(encoded({Mnemonic::JALR, zero, ra}), "0x00008067");
	// addi zero, zero, 0, aka nop
	ASSERT_EQ(encoded({Mnemonic::ADDI}), "0x00000013");
}

TEST(immediate_shifts_32_bit)
{
	ASSERT_EQ(encoded({Mnemonic::SLLI, a0, a1, zero, 3}), "0x00359513");
	ASSERT_EQ(encoded({Mnemonic::SRLI, a0, a1, zero, 3}), "0x0035d513");
	ASSERT_EQ(encoded({Mnemonic::SRAI, a0, a1, zero, 3}), "0x4035d513");
	ASSERT_EQ(encoded({Mnemonic::SLLIW, a0, a1, zero, 3}), "0x0035951b");
	ASSERT_EQ(encoded({Mnemonic::SRLIW, a0, a1, zero, 3}), "0x0035d51b");
	ASSERT_EQ(encoded({Mnemonic::SRAIW, a0, a1, zero, 3}), "0x4035d51b");
}

TEST(immediate_shifts_use_a_sixth_bit_on_whole_registers)
{
	ASSERT_EQ(encoded({Mnemonic::SLLI, a0, a1, zero, 40}), "0x02859513");
	ASSERT_EQ(encoded({Mnemonic::SRLI, a0, a1, zero, 40}), "0x0285d513");
	ASSERT_EQ(encoded({Mnemonic::SRAI, a0, a1, zero, 40}), "0x4285d513");
}

TEST(word_shifts_only_have_five_bits_of_shift_amount)
{
	Instruction wide{Mnemonic::SLLIW, a0, a1, zero, 35};
	Instruction masked{Mnemonic::SLLIW, a0, a1, zero, 3};
	ASSERT_EQ(wide.encode(), masked.encode());
}

TEST(upper_immediates)
{
	ASSERT_EQ(encoded({Mnemonic::LUI, a0, zero, zero, 0x12345000}), "0x12345537");
	ASSERT_EQ(encoded({Mnemonic::AUIPC, a0, zero, zero, 0x12345000}), "0x12345517");
	ASSERT_EQ(encoded({Mnemonic::LUI, a0, zero, zero, -4096}), "0xfffff537");
}

TEST(upper_immediates_ignore_the_lower_12_bits)
{
	ASSERT_EQ(encoded({Mnemonic::LUI, a0, zero, zero, 0x12345FFF}), "0x12345537");
}

TEST(jump_immediates)
{
	ASSERT_EQ(encoded({Mnemonic::JAL, ra, zero, zero, 16}), "0x010000ef");
	ASSERT_EQ(encoded({Mnemonic::JAL, ra, zero, zero, -16}), "0xff1ff0ef");
	// bit 11 lives in its own spot
	ASSERT_EQ(encoded({Mnemonic::JAL, zero, zero, zero, 0x800}), "0x0010006f");
	ASSERT_EQ(encoded({Mnemonic::JAL, zero, zero, zero, 0xFFFFE}), "0x7ffff06f");
	ASSERT_EQ(encoded({Mnemonic::JAL, zero, zero, zero, -0x100000}), "0x8000006f");
}

TEST(system_instructions)
{
	ASSERT_EQ(encoded({Mnemonic::ECALL}), "0x00000073");
	ASSERT_EQ(encoded({Mnemonic::EBREAK}), "0x00100073");
}

TEST(i_type_immediates_keep_their_lower_12_bits)
{
	ASSERT_EQ(encoded({Mnemonic::ADDI, a0, a1, zero, 2047}), "0x7ff58513");
	ASSERT_EQ(encoded({Mnemonic::ADDI, a0, a1, zero, -2048}), "0x80058513");
	ASSERT_EQ(encoded({Mnemonic::ADDI, a0, a1, zero, 0x1001}), "0x00158513");
}

TEST(every_register_field_reaches_all_32_registers)
{
	ASSERT_EQ(encoded({Mnemonic::ADD, t6, zero, zero}), "0x00000fb3");
	ASSERT_EQ(encoded({Mnemonic::ADD, zero, t6, zero}), "0x000f8033");
	ASSERT_EQ(encoded({Mnemonic::ADD, zero, zero, t6}), "0x01f00033");
}

TEST(formats)
{
	ASSERT_TRUE(Instruction{Mnemonic::LUI}.format() == InstructionFormat::UType);
	ASSERT_TRUE(Instruction{Mnemonic::AUIPC}.format() == InstructionFormat::UType);
	ASSERT_TRUE(Instruction{Mnemonic::JAL}.format() == InstructionFormat::JType);
	ASSERT_TRUE(Instruction{Mnemonic::ECALL}.format() == InstructionFormat::IType);
}

// compressed encodings ==========================================================================================

TEST(c_addi)
{
	ASSERT_EQ(compressed({Mnemonic::ADDI, a0, a0, zero, 1}, 64), "0x0505");
	ASSERT_EQ(compressed({Mnemonic::ADDI, sp, sp, zero, -16}, 64), "0x1141");
	ASSERT_EQ(compressed({Mnemonic::ADDI, sp, sp, zero, 16}, 64), "0x0141");
	// the limits of its immediate
	ASSERT_TRUE(compressed({Mnemonic::ADDI, a0, a0, zero, 31}, 64) != "none");
	ASSERT_TRUE(compressed({Mnemonic::ADDI, a0, a0, zero, -32}, 64) != "none");
	ASSERT_EQ(compressed({Mnemonic::ADDI, a0, a0, zero, 32}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::ADDI, a0, a0, zero, -33}, 64), "none");
}

TEST(c_li)
{
	ASSERT_EQ(compressed({Mnemonic::ADDI, a0, zero, zero, 1}, 64), "0x4505");
	ASSERT_EQ(compressed({Mnemonic::ADDI, a0, zero, zero, 0}, 64), "0x4501");
	ASSERT_EQ(compressed({Mnemonic::ADDI, a0, zero, zero, 32}, 64), "none");
}

TEST(c_mv)
{
	ASSERT_EQ(compressed({Mnemonic::ADDI, a0, a1, zero, 0}, 64), "0x852e");
	ASSERT_EQ(compressed({Mnemonic::ADD, a0, zero, a1}, 64), "0x852e");
	ASSERT_EQ(compressed({Mnemonic::ADD, a0, a1, zero}, 64), "0x852e");
}

TEST(c_nop)
{
	ASSERT_EQ(compressed({Mnemonic::ADDI}, 64), "0x0001");
}

TEST(writing_x0_is_not_compressible)
{
	ASSERT_EQ(compressed({Mnemonic::ADDI, zero, a0, zero, 1}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::ADD, zero, a0, a1}, 64), "none");
}

TEST(c_addi16sp)
{
	ASSERT_EQ(compressed({Mnemonic::ADDI, sp, sp, zero, -32}, 64), "0x1101");
	ASSERT_EQ(compressed({Mnemonic::ADDI, sp, sp, zero, 32}, 64), "0x6105");
	ASSERT_TRUE(compressed({Mnemonic::ADDI, sp, sp, zero, 496}, 64) != "none");
	ASSERT_TRUE(compressed({Mnemonic::ADDI, sp, sp, zero, -512}, 64) != "none");
	ASSERT_EQ(compressed({Mnemonic::ADDI, sp, sp, zero, 512}, 64), "none");
	// has to be a multiple of 16
	ASSERT_EQ(compressed({Mnemonic::ADDI, sp, sp, zero, 40}, 64), "none");
}

TEST(c_addi4spn)
{
	// addi fp, sp, 16
	ASSERT_EQ(compressed({Mnemonic::ADDI, fp, sp, zero, 16}, 64), "0x0800");
	ASSERT_TRUE(compressed({Mnemonic::ADDI, a0, sp, zero, 1020}, 64) != "none");
	ASSERT_EQ(compressed({Mnemonic::ADDI, a0, sp, zero, 1024}, 64), "none");
	// has to be a multiple of 4
	ASSERT_EQ(compressed({Mnemonic::ADDI, a0, sp, zero, 42}, 64), "none");
	// only reaches x8-x15
	ASSERT_EQ(compressed({Mnemonic::ADDI, t0, sp, zero, 40}, 64), "none");
}

TEST(c_addiw)
{
	ASSERT_EQ(compressed({Mnemonic::ADDIW, a0, a0, zero, 1}, 64), "0x2505");
	// aka sext.w
	ASSERT_EQ(compressed({Mnemonic::ADDIW, a0, a0, zero, 0}, 64), "0x2501");
	ASSERT_EQ(compressed({Mnemonic::ADDIW, a0, a1, zero, 1}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::ADDIW, zero, zero, zero, 1}, 64), "none");
}

TEST(rv64_only_compressed_forms)
{
	ASSERT_EQ(compressed({Mnemonic::ADDIW, a0, a0, zero, 1}, 32), "none");
	ASSERT_EQ(compressed({Mnemonic::ADDW, a0, a0, a1}, 32), "none");
	ASSERT_EQ(compressed({Mnemonic::SUBW, a0, a0, a1}, 32), "none");
	ASSERT_EQ(compressed({Mnemonic::LD, a0, a1, zero, 0}, 32), "none");
	ASSERT_EQ(compressed({Mnemonic::SD, zero, a1, a0, 0}, 32), "none");
	ASSERT_EQ(compressed({Mnemonic::LD, ra, sp, zero, 8}, 32), "none");
	ASSERT_EQ(compressed({Mnemonic::SD, zero, sp, ra, 8}, 32), "none");
}

TEST(c_lui)
{
	ASSERT_EQ(compressed({Mnemonic::LUI, a0, zero, zero, 0x1000}, 64), "0x6505");
	ASSERT_EQ(compressed({Mnemonic::LUI, a0, zero, zero, 0}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::LUI, sp, zero, zero, 0x1000}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::LUI, zero, zero, zero, 0x1000}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::LUI, a0, zero, zero, 0x20000}, 64), "none");
}

TEST(c_slli)
{
	ASSERT_EQ(compressed({Mnemonic::SLLI, a0, a0, zero, 2}, 64), "0x050a");
	ASSERT_EQ(compressed({Mnemonic::SLLI, a0, a0, zero, 0}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::SLLI, a0, a1, zero, 2}, 64), "none");
	ASSERT_TRUE(compressed({Mnemonic::SLLI, t0, t0, zero, 40}, 64) != "none");
}

TEST(c_srli_and_c_srai)
{
	ASSERT_EQ(compressed({Mnemonic::SRLI, a0, a0, zero, 1}, 64), "0x8105");
	ASSERT_EQ(compressed({Mnemonic::SRAI, a0, a0, zero, 1}, 64), "0x8505");
	// these only reach x8-x15
	ASSERT_EQ(compressed({Mnemonic::SRLI, t0, t0, zero, 1}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::SRAI, t0, t0, zero, 1}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::SRLI, a0, a1, zero, 1}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::SRLI, a0, a0, zero, 0}, 64), "none");
}

TEST(c_andi)
{
	ASSERT_EQ(compressed({Mnemonic::ANDI, a0, a0, zero, 1}, 64), "0x8905");
	ASSERT_EQ(compressed({Mnemonic::ANDI, a0, a1, zero, 1}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::ANDI, a0, a0, zero, 32}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::ANDI, t0, t0, zero, 1}, 64), "none");
}

TEST(c_add)
{
	ASSERT_EQ(compressed({Mnemonic::ADD, a0, a0, a1}, 64), "0x952e");
	// the destination can be either operand
	ASSERT_EQ(compressed({Mnemonic::ADD, a0, a1, a0}, 64), "0x952e");
	ASSERT_EQ(compressed({Mnemonic::ADD, a0, a1, a2}, 64), "none");
}

TEST(c_sub_is_not_commutative)
{
	ASSERT_EQ(compressed({Mnemonic::SUB, a0, a0, a1}, 64), "0x8d0d");
	ASSERT_EQ(compressed({Mnemonic::SUB, a0, a1, a0}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::SUB, t0, t0, a1}, 64), "none");
}

TEST(c_bitwise)
{
	ASSERT_EQ(compressed({Mnemonic::XOR, a0, a0, a1}, 64), "0x8d2d");
	ASSERT_EQ(compressed({Mnemonic::OR, a0, a0, a1}, 64), "0x8d4d");
	ASSERT_EQ(compressed({Mnemonic::AND, a0, a0, a1}, 64), "0x8d6d");
	// all commutative
	ASSERT_EQ(compressed({Mnemonic::XOR, a0, a1, a0}, 64), "0x8d2d");
	ASSERT_EQ(compressed({Mnemonic::OR, a0, a1, a0}, 64), "0x8d4d");
	ASSERT_EQ(compressed({Mnemonic::AND, a0, a1, a0}, 64), "0x8d6d");

	ASSERT_EQ(compressed({Mnemonic::XOR, a0, a1, a2}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::XOR, t0, t0, a1}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::XOR, a0, a0, t0}, 64), "none");
}

TEST(c_addw_and_c_subw)
{
	ASSERT_EQ(compressed({Mnemonic::ADDW, a0, a0, a1}, 64), "0x9d2d");
	ASSERT_EQ(compressed({Mnemonic::ADDW, a0, a1, a0}, 64), "0x9d2d");
	ASSERT_EQ(compressed({Mnemonic::SUBW, a0, a0, a1}, 64), "0x9d0d");
	ASSERT_EQ(compressed({Mnemonic::SUBW, a0, a1, a0}, 64), "none");
}

TEST(c_lw_and_c_sw)
{
	ASSERT_EQ(compressed({Mnemonic::LW, a0, a1, zero, 0}, 64), "0x4188");
	ASSERT_EQ(compressed({Mnemonic::SW, zero, a1, a0, 0}, 64), "0xc188");
	ASSERT_TRUE(compressed({Mnemonic::LW, a0, a1, zero, 124}, 32) != "none");
	ASSERT_EQ(compressed({Mnemonic::LW, a0, a1, zero, 128}, 32), "none");
	ASSERT_EQ(compressed({Mnemonic::LW, a0, a1, zero, 2}, 32), "none");
	ASSERT_EQ(compressed({Mnemonic::LW, a0, a1, zero, -4}, 32), "none");
	ASSERT_EQ(compressed({Mnemonic::LW, t0, a1, zero, 0}, 32), "none");
	ASSERT_EQ(compressed({Mnemonic::SW, zero, a1, t0, 0}, 32), "none");
}

TEST(c_lwsp_and_c_swsp)
{
	ASSERT_EQ(compressed({Mnemonic::LW, a0, sp, zero, 8}, 32), "0x4522");
	ASSERT_EQ(compressed({Mnemonic::SW, zero, sp, a0, 8}, 32), "0xc42a");
	ASSERT_TRUE(compressed({Mnemonic::LW, a0, sp, zero, 252}, 32) != "none");
	ASSERT_EQ(compressed({Mnemonic::LW, a0, sp, zero, 256}, 32), "none");
	ASSERT_EQ(compressed({Mnemonic::LW, zero, sp, zero, 8}, 32), "none");
}

TEST(c_ld_and_c_sd)
{
	ASSERT_EQ(compressed({Mnemonic::LD, a0, a1, zero, 0}, 64), "0x6188");
	ASSERT_EQ(compressed({Mnemonic::SD, zero, a1, a0, 0}, 64), "0xe188");
	ASSERT_TRUE(compressed({Mnemonic::LD, a0, a1, zero, 248}, 64) != "none");
	ASSERT_EQ(compressed({Mnemonic::LD, a0, a1, zero, 256}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::LD, a0, a1, zero, 4}, 64), "none");
}

TEST(c_ldsp_and_c_sdsp)
{
	ASSERT_EQ(compressed({Mnemonic::LD, ra, sp, zero, 8}, 64), "0x60a2");
	ASSERT_EQ(compressed({Mnemonic::LD, fp, sp, zero, 0}, 64), "0x6402");
	ASSERT_EQ(compressed({Mnemonic::SD, zero, sp, ra, 8}, 64), "0xe406");
	ASSERT_EQ(compressed({Mnemonic::SD, zero, sp, fp, 0}, 64), "0xe022");
	ASSERT_TRUE(compressed({Mnemonic::LD, a0, sp, zero, 504}, 64) != "none");
	ASSERT_EQ(compressed({Mnemonic::LD, a0, sp, zero, 512}, 64), "none");
}

TEST(c_jr_and_c_jalr)
{
	// ret
	ASSERT_EQ(compressed({Mnemonic::JALR, zero, ra}, 64), "0x8082");
	ASSERT_EQ(compressed({Mnemonic::JALR, ra, a0}, 64), "0x9502");
	ASSERT_EQ(compressed({Mnemonic::JALR, a0, a1}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::JALR, zero, ra, zero, 4}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::JALR, zero, zero}, 64), "none");
}

TEST(c_ebreak)
{
	ASSERT_EQ(compressed({Mnemonic::EBREAK}, 64), "0x9002");
}

TEST(instructions_with_no_compressed_form)
{
	ASSERT_EQ(compressed({Mnemonic::MUL, a0, a0, a1}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::DIV, a0, a0, a1}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::JAL, ra, zero, zero, 16}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::ECALL}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::AUIPC, a0}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::LB, a0, a1}, 64), "none");
	ASSERT_EQ(compressed({Mnemonic::SB, zero, a1, a0}, 64), "none");
}

TEST(rv32_keeps_shift_amounts_to_5_bits)
{
	// on RV32 only the lower 5 bits of the shift amount exist
	ASSERT_EQ(compressed({Mnemonic::SLLI, a0, a0, zero, 32}, 32), "none");
	ASSERT_TRUE(compressed({Mnemonic::SLLI, a0, a0, zero, 32}, 64) != "none");
}

// textual assembly ==============================================================================================

TEST(text_of_each_shape)
{
	ASSERT_EQ((Instruction{Mnemonic::ADD, a0, a1, a2}).to_string(), "add a0, a1, a2");
	ASSERT_EQ((Instruction{Mnemonic::ADDI, a0, a1, zero, -5}).to_string(), "addi a0, a1, -5");
	ASSERT_EQ((Instruction{Mnemonic::LW, a0, sp, zero, 8}).to_string(), "lw a0, 8(sp)");
	ASSERT_EQ((Instruction{Mnemonic::SD, zero, sp, ra, 16}).to_string(), "sd ra, 16(sp)");
	ASSERT_EQ((Instruction{Mnemonic::JALR, zero, ra}).to_string(), "jalr zero, 0(ra)");
	ASSERT_EQ((Instruction{Mnemonic::JAL, ra, zero, zero, 16}).to_string(), "jal ra, .+16");
	ASSERT_EQ((Instruction{Mnemonic::JAL, ra, zero, zero, -16}).to_string(), "jal ra, .-16");
	ASSERT_EQ((Instruction{Mnemonic::LUI, a0, zero, zero, 0x12345000}).to_string(), "lui a0, 0x12345");
	ASSERT_EQ((Instruction{Mnemonic::AUIPC, a0, zero, zero, 0x1000}).to_string(), "auipc a0, 0x1");
	ASSERT_EQ((Instruction{Mnemonic::LUI, a0, zero, zero, -4096}).to_string(), "lui a0, 0xfffff");
	ASSERT_EQ((Instruction{Mnemonic::ECALL}).to_string(), "ecall");
	ASSERT_EQ((Instruction{Mnemonic::EBREAK}).to_string(), "ebreak");
}

TEST(text_of_every_mnemonic_group)
{
	ASSERT_EQ((Instruction{Mnemonic::SLLW, a0, a1, a2}).to_string(), "sllw a0, a1, a2");
	ASSERT_EQ((Instruction{Mnemonic::REMUW, a0, a1, a2}).to_string(), "remuw a0, a1, a2");
	ASSERT_EQ((Instruction{Mnemonic::MULHSU, a0, a1, a2}).to_string(), "mulhsu a0, a1, a2");
	ASSERT_EQ((Instruction{Mnemonic::SRAI, a0, a1, zero, 3}).to_string(), "srai a0, a1, 3");
	ASSERT_EQ((Instruction{Mnemonic::ADDIW, a0, a1, zero, 3}).to_string(), "addiw a0, a1, 3");
	ASSERT_EQ((Instruction{Mnemonic::LWU, a0, sp}).to_string(), "lwu a0, 0(sp)");
}

TEST(register_names)
{
	ASSERT_EQ(register_name(zero), "zero");
	ASSERT_EQ(register_name(ra), "ra");
	ASSERT_EQ(register_name(sp), "sp");
	ASSERT_EQ(register_name(fp), "fp");
	ASSERT_EQ(register_name(s1), "s1");
	ASSERT_EQ(register_name(a0), "a0");
	ASSERT_EQ(register_name(a7), "a7");
	ASSERT_EQ(register_name(s2), "s2");
	ASSERT_EQ(register_name(s11), "s11");
	ASSERT_EQ(register_name(t0), "t0");
	ASSERT_EQ(register_name(t2), "t2");
	ASSERT_EQ(register_name(t3), "t3");
	ASSERT_EQ(register_name(t6), "t6");
}
