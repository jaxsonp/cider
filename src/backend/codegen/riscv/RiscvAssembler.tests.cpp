#include "unit_testing.hpp"
#include "backend/codegen/riscv/RiscvAssembler.hpp"

#include <format>
#include <sstream>
#include <vector>

#include "utils/error.hpp"

using namespace codegen::riscv;
using enum Register;

static std::string text_of(const Assembler &a)
{
	std::ostringstream out;
	a.dump_asm(out);
	return out.str();
}

static uint32_t read_word(const std::vector<uint8_t> &bytes, size_t at)
{
	uint32_t word = 0;
	for (size_t i = 0; i < 4; ++i)
		word |= uint32_t(bytes.at(at + i)) << (8 * i);
	return word;
}

/// Runs the instructions `load_immediate` can pick on a single register, by decoding their 32 bit encodings
static int64_t run_load_immediate(unsigned int xlen, int64_t value)
{
	Assembler a(xlen, false);
	a.load_immediate(a0, value);
	std::vector<uint8_t> bytes;
	a.dump_to_bytes(bytes);

	int64_t reg = 0x5555555555555555;
	for (size_t at = 0; at < bytes.size(); at += 4)
	{
		uint32_t word = read_word(bytes, at);
		uint32_t opcode = word & 0x7F;
		uint32_t funct3 = (word >> 12) & 7;
		int64_t imm_i = int32_t(word) >> 20;
		int64_t rs1_value = ((word >> 15) & 0x1F) == 0 ? 0 : reg;

		if (opcode == 0b0110111) // lui
			reg = int32_t(word & 0xFFFFF000u);
		else if (opcode == 0b0010011 && funct3 == 0) // addi
			reg = rs1_value + imm_i;
		else if (opcode == 0b0011011 && funct3 == 0) // addiw
			reg = int32_t(uint32_t(rs1_value) + uint32_t(imm_i));
		else if (opcode == 0b0010011 && funct3 == 1) // slli
			reg = int64_t(uint64_t(rs1_value) << ((word >> 20) & 0x3F));
		else
			throw std::runtime_error(std::format("load_immediate wrote something unexpected: {:#x}", word));

		if (xlen == 32)
			reg = int32_t(reg);
	}
	return reg;
}

// load_immediate ================================================================================================

TEST(load_immediate_small_values_are_one_addi)
{
	for (int64_t value : {int64_t(0), int64_t(1), int64_t(-1), int64_t(2047), int64_t(-2048)})
	{
		Assembler a(64, false);
		a.load_immediate(a0, value);
		ASSERT_EQ(a.cur_offset(), 4u);
		ASSERT_EQ(text_of(a), std::format("\taddi a0, zero, {}\n", value));
	}
}

TEST(load_immediate_past_12_bits_needs_lui)
{
	Assembler a(64, false);
	a.load_immediate(a0, 2048);
	ASSERT_EQ(a.cur_offset(), 8u);
}

TEST(load_immediate_with_no_lower_bits_is_only_a_lui)
{
	Assembler a(64, false);
	a.load_immediate(a0, 0x1000);
	ASSERT_EQ(text_of(a), "\tlui a0, 0x1\n");
}

TEST(load_immediate_uses_addiw_on_rv64)
{
	Assembler rv64(64, false);
	rv64.load_immediate(a0, 0x12345678);
	ASSERT_EQ(text_of(rv64), "\tlui a0, 0x12345\n\taddiw a0, a0, 1656\n");

	Assembler rv32(32, false);
	rv32.load_immediate(a0, 0x12345678);
	ASSERT_EQ(text_of(rv32), "\tlui a0, 0x12345\n\taddi a0, a0, 1656\n");
}

TEST(load_immediate_rv32_values)
{
	const std::vector<int64_t> values = {
		0,
		1,
		-1,
		2047,
		2048,
		-2048,
		-2049,
		4095,
		4096,
		0x7FF,
		0x800,
		0xFFF,
		0x1000,
		0x12345678,
		0x7FFFF7FF,
		0x7FFFF800,
		0x7FFFFFFF,
		INT32_MIN,
		INT32_MIN + 1,
		-0x12345678,
	};
	for (int64_t value : values)
		ASSERT_EQ(run_load_immediate(32, value), value);
}

TEST(load_immediate_rv32_treats_unsigned_and_signed_alike)
{
	ASSERT_EQ(run_load_immediate(32, 0xFFFFFFFFll), -1);
	ASSERT_EQ(run_load_immediate(32, 0x80000000ll), int64_t(INT32_MIN));
}

TEST(load_immediate_rv64_values)
{
	const std::vector<int64_t> values = {
		0,
		1,
		-1,
		2047,
		2048,
		-2048,
		-2049,
		0x7FF,
		0x800,
		0xFFF,
		0x1000,
		0x12345678,
		// the rounding of the lower bits pushes lui past 2^31
		0x7FFFF7FF,
		0x7FFFF800,
		0x7FFFFFFF,
		INT32_MIN,
		0x80000000ll,
		0xFFFFFFFFll,
		0x100000000ll,
		0x100000001ll,
		0x1234567890ABCDEFll,
		0x123456789ABCDEF0ll,
		0x0F0F0F0F0F0F0F0Fll,
		0x7FFFFFFFFFFFFFFFll,
		INT64_MIN,
		INT64_MIN + 1,
		-0x100000000ll,
		-0x1234567890ABCDEFll,
		// bit 11 set all the way up, so the rounding carries through every step
		0x7FFFFFFFFFFFF800ll,
		0xFFFFFFFFll << 12,
		int64_t(0xFFFFFFFFFFFFF7FFull),
		int64_t(0x8000000000000800ull),
	};
	for (int64_t value : values)
		ASSERT_EQ(run_load_immediate(64, value), value);
}

TEST(load_immediate_rv64_every_single_bit)
{
	for (unsigned int bit = 0; bit < 64; ++bit)
	{
		int64_t value = int64_t(uint64_t(1) << bit);
		ASSERT_EQ(run_load_immediate(64, value), value);
		ASSERT_EQ(run_load_immediate(64, ~value), ~value);
		ASSERT_EQ(run_load_immediate(64, value - 1), value - 1);
	}
}

// compression ===================================================================================================

TEST(nothing_is_compressed_without_the_c_extension)
{
	Assembler a(64, false);
	a.write_addi(a0, a0, 1);
	ASSERT_EQ(a.cur_offset(), 4u);
	std::vector<uint8_t> bytes;
	a.dump_to_bytes(bytes);
	ASSERT_EQ(read_word(bytes, 0), 0x00150513u);
}

TEST(compressible_instructions_take_2_bytes)
{
	Assembler a(64, true);
	a.write_addi(a0, a0, 1);
	ASSERT_EQ(a.cur_offset(), 2u);
	std::vector<uint8_t> bytes;
	a.dump_to_bytes(bytes);
	ASSERT_TRUE(bytes == (std::vector<uint8_t>{0x05, 0x05}));
}

TEST(incompressible_instructions_still_take_4_bytes)
{
	Assembler a(64, true);
	a.write_mul(a0, a1, a2);
	ASSERT_EQ(a.cur_offset(), 4u);
}

TEST(xlen_decides_what_compresses)
{
	Assembler rv64(64, true);
	rv64.write_ld(ra, sp, 8);
	ASSERT_EQ(rv64.cur_offset(), 2u);

	Assembler rv32(32, true);
	rv32.write_lw(ra, sp, 8);
	ASSERT_EQ(rv32.cur_offset(), 2u);
}

TEST(fixed_size_instructions_are_never_compressed)
{
	Assembler a(64, true);
	a.write({Mnemonic::ADDI, a0, a0, zero, 1}, true);
	ASSERT_EQ(a.cur_offset(), 4u);
}

TEST(jal_is_always_full_size)
{
	Assembler a(64, true);
	a.write_jal(zero, 8);
	ASSERT_EQ(a.cur_offset(), 4u);
}

// the buffer ====================================================================================================

TEST(positions_and_offsets)
{
	Assembler a(64, true);
	size_t first = a.write_addi(a0, a0, 1);  // 2 bytes
	size_t second = a.write_mul(a0, a1, a2); // 4 bytes
	size_t third = a.write_addi(a0, a0, 1);
	ASSERT_EQ(first, 0u);
	ASSERT_EQ(second, 1u);
	ASSERT_EQ(third, 2u);
	ASSERT_EQ(a.offset_of(first), 0u);
	ASSERT_EQ(a.offset_of(second), 2u);
	ASSERT_EQ(a.offset_of(third), 6u);
	ASSERT_EQ(a.cur_offset(), 8u);
}

TEST(clear_empties_the_buffer)
{
	Assembler a(64, false);
	a.write_nop();
	a.clear();
	ASSERT_EQ(a.cur_offset(), 0u);
	ASSERT_EQ(a.write_nop(), 0u);
}

TEST(dump_to_bytes_is_little_endian_and_appends)
{
	Assembler a(32, false);
	a.write_ecall();
	std::vector<uint8_t> bytes = {0xAA};
	a.dump_to_bytes(bytes);
	ASSERT_TRUE(bytes == (std::vector<uint8_t>{0xAA, 0x73, 0x00, 0x00, 0x00}));
}

TEST(nop_is_addi_zero_zero_zero)
{
	Assembler a(32, false);
	a.write_nop();
	std::vector<uint8_t> bytes;
	a.dump_to_bytes(bytes);
	ASSERT_EQ(read_word(bytes, 0), 0x00000013u);
}

TEST(backpatch_immediate)
{
	Assembler a(32, false);
	size_t pos = a.write_jal(ra, 0);
	a.backpatch_immediate(pos, 16);
	std::vector<uint8_t> bytes;
	a.dump_to_bytes(bytes);
	ASSERT_EQ(read_word(bytes, 0), 0x010000efu);
}

TEST(backpatch_rejects_what_has_no_room)
{
	Assembler a(64, true);
	size_t compressed = a.write_addi(a0, a0, 1);
	size_t r_type = a.write_mul(a0, a1, a2);
	ASSERT_THROWS(a.backpatch_immediate(compressed, 4), CompilerError);
	ASSERT_THROWS(a.backpatch_immediate(r_type, 4), CompilerError);
}

TEST(write_functions_put_operands_in_the_right_fields)
{
	Assembler a(64, false);
	a.write_sw(a1, a2, 4);
	a.write_lbu(a0, a1, 4);
	a.write_sraw(a0, a1, a2);
	ASSERT_EQ(text_of(a), "\tsw a2, 4(a1)\n\tlbu a0, 4(a1)\n\tsraw a0, a1, a2\n");
}

// textual assembly ==============================================================================================

TEST(dump_asm_is_one_indented_instruction_per_line)
{
	Assembler a(64, false);
	a.write_add(a0, a1, a2);
	a.write_ecall();
	ASSERT_EQ(text_of(a), "\tadd a0, a1, a2\n\tecall\n");
}

TEST(rvc_is_switched_off_around_full_size_instructions)
{
	Assembler a(64, true);
	a.write_addi(a0, a0, 1);
	a.write_jal(zero, 8);
	a.write_addi(a0, a0, 1);
	ASSERT_EQ(text_of(a), "\taddi a0, a0, 1\n\t.option norvc\n\tjal zero, .+8\n\t.option rvc\n\taddi a0, a0, 1\n");
}

TEST(rvc_is_back_on_at_the_end)
{
	Assembler a(64, true);
	a.write_jal(zero, 8);
	ASSERT_EQ(text_of(a), "\t.option norvc\n\tjal zero, .+8\n\t.option rvc\n");
}

TEST(no_rvc_options_without_the_c_extension)
{
	Assembler a(64, false);
	a.write_jal(zero, 8);
	a.write_addi(a0, a0, 1);
	ASSERT_EQ(text_of(a), "\tjal zero, .+8\n\taddi a0, a0, 1\n");
}

// calls =========================================================================================================

TEST(call_placeholder_is_a_full_size_auipc_and_jalr)
{
	Assembler a(64, true);
	size_t pos = a.write_call_placeholder(ra, "foo");
	ASSERT_EQ(pos, 0u);
	ASSERT_EQ(a.cur_offset(), Assembler::CALL_SIZE);
}

TEST(call_is_written_as_one_pseudo_instruction)
{
	Assembler a(64, false);
	a.write_call_placeholder(ra, "foo");
	a.write_call_placeholder(t0, "bar");
	ASSERT_EQ(text_of(a), "\tcall foo\n\tcall t0, bar\n");
}

/// Writes a call at `call_offset`, patches it to `target_offset`, and returns the address it computes
static int64_t patched_target(size_t call_offset, size_t target_offset)
{
	Assembler a(64, false);
	a.write_call_placeholder(ra, "foo");
	std::vector<uint8_t> call;
	a.dump_to_bytes(call);

	std::vector<uint8_t> code(std::max(call_offset, target_offset) + Assembler::CALL_SIZE, 0);
	std::copy(call.begin(), call.end(), code.begin() + call_offset);
	Assembler::patch_call(code, call_offset, target_offset);

	uint32_t auipc = read_word(code, call_offset);
	uint32_t jalr = read_word(code, call_offset + 4);
	ASSERT_EQ(auipc & 0xFFF, 0x097u);
	ASSERT_EQ(jalr & 0xFFFFF, 0x080E7u);

	return int64_t(call_offset) + int32_t(auipc & 0xFFFFF000u) + (int32_t(jalr) >> 20);
}

TEST(patch_call_reaches_its_target)
{
	const std::vector<std::pair<size_t, size_t>> cases = {
		{0, 0},
		{0, 8},
		{100, 0},
		{0, 0x7FF},
		// the lower 12 bits are sign extended by jalr, so these need the upper part rounded up
		{0, 0x800},
		{0, 0xFFF},
		{0, 0x1000},
		{0x1000, 0},
		{0x800, 0},
		{0x801, 0},
		{0, 0x123456},
		{0x123456, 8},
	};
	for (const auto &[call_offset, target_offset] : cases)
		ASSERT_EQ(patched_target(call_offset, target_offset), int64_t(target_offset));
}

TEST(patch_call_keeps_the_register_fields)
{
	Assembler a(64, false);
	a.write_call_placeholder(t0, "foo");
	std::vector<uint8_t> code;
	a.dump_to_bytes(code);
	Assembler::patch_call(code, 0, 0x1234);
	ASSERT_EQ(read_word(code, 0) & 0xFFF, 0x297u);
	ASSERT_EQ(read_word(code, 4) & 0xFFFFF, 0x282E7u);
}
