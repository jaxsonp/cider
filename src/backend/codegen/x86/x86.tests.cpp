#include "unit_testing.hpp"
#include "backend/codegen/x86/x86.hpp"
#include "backend/codegen/x86/x86Instruction.hpp"
#include "backend/codegen/x86/x86Operand.hpp"

#include "utils/error.hpp"

using namespace codegen::x86;
using enum Register;

TEST(width_of_size)
{
	ASSERT_TRUE(width_of_size(1) == Width::W8);
	ASSERT_TRUE(width_of_size(2) == Width::W16);
	ASSERT_TRUE(width_of_size(4) == Width::W32);
	ASSERT_TRUE(width_of_size(8) == Width::W64);
}

TEST(width_of_invalid_sizes)
{
	for (unsigned int size : {0u, 3u, 5u, 16u})
		ASSERT_THROWS(width_of_size(size), CompilerError);
}

TEST(widths_compare_by_size)
{
	ASSERT_TRUE(Width::W8 < Width::W16);
	ASSERT_TRUE(Width::W16 < Width::W32);
	ASSERT_TRUE(Width::W32 < Width::W64);
}

TEST(registers_are_their_hardware_numbers)
{
	ASSERT_EQ(int(RAX), 0);
	ASSERT_EQ(int(RCX), 1);
	ASSERT_EQ(int(RDX), 2);
	ASSERT_EQ(int(RBX), 3);
	ASSERT_EQ(int(RSP), 4);
	ASSERT_EQ(int(RBP), 5);
	ASSERT_EQ(int(RSI), 6);
	ASSERT_EQ(int(RDI), 7);
	ASSERT_EQ(int(R8), 8);
	ASSERT_EQ(int(R15), 15);
}

TEST(register_names)
{
	ASSERT_EQ(register_name(RAX, Width::W64), "rax");
	ASSERT_EQ(register_name(RAX, Width::W32), "eax");
	ASSERT_EQ(register_name(RAX, Width::W16), "ax");
	ASSERT_EQ(register_name(RAX, Width::W8), "al");

	ASSERT_EQ(register_name(RDI, Width::W64), "rdi");
	ASSERT_EQ(register_name(RDI, Width::W32), "edi");
	ASSERT_EQ(register_name(RDI, Width::W16), "di");
	ASSERT_EQ(register_name(RDI, Width::W8), "dil");

	ASSERT_EQ(register_name(R15, Width::W64), "r15");
	ASSERT_EQ(register_name(R15, Width::W32), "r15d");
	ASSERT_EQ(register_name(R15, Width::W16), "r15w");
	ASSERT_EQ(register_name(R15, Width::W8), "r15b");
}

TEST(register_name_out_of_range)
{
	ASSERT_THROWS(register_name(Register(16), Width::W32), CompilerError);
}

TEST(condition_codes_are_their_encodings)
{
	ASSERT_EQ(int(Cond::O), 0x0);
	ASSERT_EQ(int(Cond::B), 0x2);
	ASSERT_EQ(int(Cond::E), 0x4);
	ASSERT_EQ(int(Cond::A), 0x7);
	ASSERT_EQ(int(Cond::L), 0xC);
	ASSERT_EQ(int(Cond::G), 0xF);
}

TEST(condition_suffixes)
{
	const std::vector<std::pair<Cond, std::string>> cases = {
		{Cond::O, "o"},
		{Cond::NO, "no"},
		{Cond::B, "b"},
		{Cond::AE, "ae"},
		{Cond::E, "e"},
		{Cond::NE, "ne"},
		{Cond::BE, "be"},
		{Cond::A, "a"},
		{Cond::S, "s"},
		{Cond::NS, "ns"},
		{Cond::P, "p"},
		{Cond::NP, "np"},
		{Cond::L, "l"},
		{Cond::GE, "ge"},
		{Cond::LE, "le"},
		{Cond::G, "g"},
	};
	for (const auto &[cond, suffix] : cases)
		ASSERT_EQ(std::string(cond_suffix(cond)), suffix);
}

// operands ======================================================================================================

TEST(checked_immediates_at_their_limits)
{
	ASSERT_EQ(Imm<Width::W8>::checked(127).value, 127);
	ASSERT_EQ(Imm<Width::W8>::checked(-128).value, -128);
	ASSERT_EQ(Imm<Width::W16>::checked(32767).value, 32767);
	ASSERT_EQ(Imm<Width::W16>::checked(-32768).value, -32768);
	ASSERT_EQ(Imm<Width::W32>::checked(INT32_MAX).value, INT32_MAX);
	ASSERT_EQ(Imm<Width::W32>::checked(INT32_MIN).value, INT32_MIN);
}

TEST(checked_immediates_past_their_limits)
{
	ASSERT_THROWS(Imm<Width::W8>::checked(128), CompilerError);
	ASSERT_THROWS(Imm<Width::W8>::checked(-129), CompilerError);
	ASSERT_THROWS(Imm<Width::W16>::checked(32768), CompilerError);
	ASSERT_THROWS(Imm<Width::W16>::checked(-32769), CompilerError);
	ASSERT_THROWS(Imm<Width::W32>::checked(int64_t(INT32_MAX) + 1), CompilerError);
	ASSERT_THROWS(Imm<Width::W32>::checked(int64_t(INT32_MIN) - 1), CompilerError);
}

TEST(sixty_four_bit_operations_take_a_32_bit_immediate)
{
	ASSERT_EQ(Imm<Width::W64>::checked(INT32_MAX).value, INT32_MAX);
	ASSERT_THROWS(Imm<Width::W64>::checked(int64_t(INT32_MAX) + 1), CompilerError);
}

TEST(stack_pointer_cannot_be_an_index)
{
	ASSERT_THROWS(Index(RSP, Scale::x1), CompilerError);
	ASSERT_EQ(int(Index(R12, Scale::x8).reg()), int(R12));
}

TEST(memory_operand_parts)
{
	Mem<Width::W32> simple(RBP, -4);
	ASSERT_EQ(int(simple.base()), int(RBP));
	ASSERT_EQ(simple.disp(), -4);
	ASSERT_TRUE(!simple.index().has_value());

	Mem<Width::W32> indexed(RAX, Index(RCX, Scale::x4), 8);
	ASSERT_TRUE(indexed.index().has_value());
	ASSERT_EQ(int(indexed.index()->reg()), int(RCX));
	ASSERT_TRUE(indexed.index()->scale() == Scale::x4);
	ASSERT_EQ(indexed.disp(), 8);
}

static_assert(RegOp<Reg8> && RegOp<Reg64> && !RegOp<Mem<Width::W8>> && !RegOp<Imm<Width::W8>>);
static_assert(MemOp<Mem<Width::W8>> && !MemOp<Reg8>);
static_assert(RmOp<Reg32> && RmOp<Mem<Width::W32>> && !RmOp<Imm<Width::W32>>);

// raw encoding, for the parts the typed assembler can't reach ===================================================

/// Encoding of a single byte opcode, with no operands
static detail::Encoding plain(uint8_t opcode)
{
	detail::Encoding enc;
	enc.opcode = {opcode, 0, 0};
	enc.opcode_len = 1;
	return enc;
}

TEST(encoding_with_no_prefixes)
{
	std::vector<uint8_t> bytes;
	plain(0x90).encode(bytes, true);
	ASSERT_TRUE(bytes == (std::vector<uint8_t>{0x90}));
}

TEST(encoding_prefix_order)
{
	detail::Encoding enc = plain(0x90);
	enc.operand_size_prefix = true;
	enc.rex_w = true;
	std::vector<uint8_t> bytes;
	enc.encode(bytes, true);
	ASSERT_TRUE(bytes == (std::vector<uint8_t>{0x66, 0x48, 0x90}));
}

TEST(encoding_adds_the_register_to_the_last_opcode_byte)
{
	detail::Encoding enc;
	enc.opcode = {0x0F, 0xC8, 0};
	enc.opcode_len = 2;
	enc.opcode_reg = detail::RegNum{9, false};
	std::vector<uint8_t> bytes;
	enc.encode(bytes, true);
	ASSERT_TRUE(bytes == (std::vector<uint8_t>{0x41, 0x0F, 0xC9}));
}

TEST(encoding_immediates_are_little_endian)
{
	detail::Encoding enc = plain(0x90);
	enc.imm = 0x11223344;
	enc.imm_size = 4;
	std::vector<uint8_t> bytes;
	enc.encode(bytes, true);
	ASSERT_TRUE(bytes == (std::vector<uint8_t>{0x90, 0x44, 0x33, 0x22, 0x11}));
}

TEST(encoding_appends_to_the_buffer)
{
	std::vector<uint8_t> bytes = {0xAA};
	plain(0x90).encode(bytes, false);
	ASSERT_TRUE(bytes == (std::vector<uint8_t>{0xAA, 0x90}));
}

TEST(encoding_rex_bits_for_each_position)
{
	detail::Encoding enc = plain(0x89);
	enc.has_modrm = true;
	enc.reg_field = detail::RegNum{8, false};
	enc.rm = detail::RegNum{1, false};
	std::vector<uint8_t> reg_bytes;
	enc.encode(reg_bytes, true);
	ASSERT_EQ(int(reg_bytes[0]), 0x44); // REX.R

	enc.reg_field = detail::RegNum{1, false};
	enc.rm = detail::RegNum{8, false};
	std::vector<uint8_t> rm_bytes;
	enc.encode(rm_bytes, true);
	ASSERT_EQ(int(rm_bytes[0]), 0x41); // REX.B

	enc.rm = detail::MemRef{.base = 0, .index = uint8_t(8), .scale_bits = 0, .disp = 0};
	std::vector<uint8_t> index_bytes;
	enc.encode(index_bytes, true);
	ASSERT_EQ(int(index_bytes[0]), 0x42); // REX.X
}

TEST(encoding_rex_with_no_bits_for_byte_registers)
{
	detail::Encoding enc = plain(0x88);
	enc.has_modrm = true;
	enc.reg_field = detail::RegNum{6, true};
	enc.rm = detail::RegNum{0, true};
	std::vector<uint8_t> bytes;
	enc.encode(bytes, true);
	ASSERT_EQ(int(bytes[0]), 0x40);
}

TEST(encoding_32_bit_register_numbers_4_to_7_need_no_rex)
{
	detail::Encoding enc = plain(0x89);
	enc.has_modrm = true;
	enc.reg_field = detail::RegNum{6, false};
	enc.rm = detail::RegNum{7, false};
	std::vector<uint8_t> bytes;
	enc.encode(bytes, false);
	ASSERT_EQ(bytes.size(), 2u);
}
