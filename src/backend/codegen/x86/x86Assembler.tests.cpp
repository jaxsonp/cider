#include "unit_testing.hpp"
#include "backend/codegen/x86/x86Assembler.hpp"

#include <format>
#include <sstream>

#include "utils/error.hpp"

using namespace codegen::x86;
using enum Register;

using Mem8 = Mem<Width::W8>;
using Mem16 = Mem<Width::W16>;
using Mem32 = Mem<Width::W32>;
using Mem64 = Mem<Width::W64>;
using Imm8 = Imm<Width::W8>;
using Imm16 = Imm<Width::W16>;
using Imm32 = Imm<Width::W32>;
/// The sign extended 32 bit immediate of 64 bit instructions
using ImmS64 = Imm<Width::W64>;

static constexpr Reg64 rax(RAX), rcx(RCX), rsp(RSP), rbp(RBP), r8(R8), r9(R9), r10(R10), r12(R12);
static constexpr Reg32 eax(RAX), ecx(RCX), ebp(RBP), r8d(R8), r9d(R9), r15d(R15);
static constexpr Reg16 ax(RAX), cx(RCX), r8w(R8), r9w(R9);
static constexpr Reg8 al(RAX), cl(RCX), spl(RSP), sil(RSI), r8b(R8), r9b(R9), r10b(R10), r11b(R11);

/// Bytes as space separated hex, like objdump and friends print them
static std::string hex(const std::vector<uint8_t> &bytes)
{
	std::string out;
	for (uint8_t byte : bytes)
		out += std::format("{}{:02x}", out.empty() ? "" : " ", byte);
	return out;
}

static std::string hex(const Assembler &a)
{
	std::vector<uint8_t> bytes;
	a.dump_to_bytes(bytes);
	return hex(bytes);
}

/// Fail the test unless the statement writes exactly these bytes, and exactly this textual assembly
#define ASSERT_ENCODES(is_x64, bytes, text, ...)                     \
	do                                                               \
	{                                                                \
		Assembler a(is_x64, true);                                   \
		__VA_ARGS__;                                                 \
		ASSERT_EQ(hex(a), std::string(bytes));                       \
		std::ostringstream written;                                  \
		a.dump_asm(written);                                         \
		ASSERT_EQ(written.str(), std::format("\t{}\n", text));     \
	} while (0)

#define ASSERT_X64(bytes, text, ...) ASSERT_ENCODES(true, bytes, text, __VA_ARGS__)
#define ASSERT_X86(bytes, text, ...) ASSERT_ENCODES(false, bytes, text, __VA_ARGS__)

// The expected bytes below are what GNU as gives for the text next to them, not what this assembler wrote at the
// time. That's what makes `--emit=asm` assemble to byte identical code

TEST(mov_registers)
{
	ASSERT_X64("89 c8", "mov eax, ecx", a.write_mov(eax, ecx));
	ASSERT_X64("48 89 c8", "mov rax, rcx", a.write_mov(rax, rcx));
	ASSERT_X64("66 89 c8", "mov ax, cx", a.write_mov(ax, cx));
	ASSERT_X64("88 c8", "mov al, cl", a.write_mov(al, cl));
	ASSERT_X64("4d 89 c8", "mov r8, r9", a.write_mov(r8, r9));
	ASSERT_X64("41 89 c0", "mov r8d, eax", a.write_mov(r8d, eax));
	ASSERT_X64("44 89 f8", "mov eax, r15d", a.write_mov(eax, r15d));
	ASSERT_X64("66 45 89 c8", "mov r8w, r9w", a.write_mov(r8w, r9w));
	ASSERT_X64("40 88 c6", "mov sil, al", a.write_mov(sil, al));
	ASSERT_X64("41 88 e2", "mov r10b, spl", a.write_mov(r10b, spl));
}

TEST(mov_memory)
{
	ASSERT_X64("89 45 fc", "mov dword ptr [rbp-4], eax", a.write_mov(Mem32(RBP, -4), eax));
	ASSERT_X64("48 8b 45 f8", "mov rax, qword ptr [rbp-8]", a.write_mov(rax, Mem64(RBP, -8)));
	ASSERT_X64("8a 01", "mov al, byte ptr [rcx]", a.write_mov(al, Mem8(RCX)));
	ASSERT_X64("66 8b 41 02", "mov ax, word ptr [rcx+2]", a.write_mov(ax, Mem16(RCX, 2)));
	ASSERT_X64("88 01", "mov byte ptr [rcx], al", a.write_mov(Mem8(RCX), al));
}

TEST(mov_immediates)
{
	ASSERT_X64("b8 05 00 00 00", "mov eax, 5", a.write_mov(eax, Imm32(5)));
	ASSERT_X64("b0 05", "mov al, 5", a.write_mov(al, Imm8(5)));
	ASSERT_X64("66 b8 e8 03", "mov ax, 1000", a.write_mov(ax, Imm16(1000)));
	ASSERT_X64("41 b9 ff ff ff ff", "mov r9d, -1", a.write_mov(r9d, Imm32(-1)));
	ASSERT_X64("48 c7 c0 ff ff ff ff", "mov rax, -1", a.write_mov(rax, ImmS64(-1)));
	ASSERT_X64("49 c7 c0 07 00 00 00", "mov r8, 7", a.write_mov(r8, ImmS64(7)));
	ASSERT_X64("48 b8 f0 de bc 9a 78 56 34 12", "movabs rax, 1311768467463790320", a.write_mov(rax, Imm64(1311768467463790320)));
	ASSERT_X64("49 ba ff ff ff ff ff ff ff ff", "movabs r10, -1", a.write_mov(r10, Imm64(-1)));
	ASSERT_X64("c7 44 24 08 07 00 00 00", "mov dword ptr [rsp+8], 7", a.write_mov(Mem32(RSP, 8), Imm32(7)));
	ASSERT_X64("c6 45 ff 01", "mov byte ptr [rbp-1], 1", a.write_mov(Mem8(RBP, -1), Imm8(1)));
	ASSERT_X64("66 c7 45 fe 2c 01", "mov word ptr [rbp-2], 300", a.write_mov(Mem16(RBP, -2), Imm16(300)));
	ASSERT_X64("48 c7 45 f8 ff ff ff ff", "mov qword ptr [rbp-8], -1", a.write_mov(Mem64(RBP, -8), ImmS64(-1)));
}

TEST(addressing_modes)
{
	ASSERT_X64("8b 00", "mov eax, dword ptr [rax]", a.write_mov(eax, Mem32(RAX)));
	ASSERT_X64("8b 04 24", "mov eax, dword ptr [rsp]", a.write_mov(eax, Mem32(RSP)));
	ASSERT_X64("8b 45 00", "mov eax, dword ptr [rbp]", a.write_mov(eax, Mem32(RBP)));
	ASSERT_X64("41 8b 04 24", "mov eax, dword ptr [r12]", a.write_mov(eax, Mem32(R12)));
	ASSERT_X64("41 8b 45 00", "mov eax, dword ptr [r13]", a.write_mov(eax, Mem32(R13)));
	ASSERT_X64("8b 40 7f", "mov eax, dword ptr [rax+127]", a.write_mov(eax, Mem32(RAX, 127)));
	ASSERT_X64("8b 80 80 00 00 00", "mov eax, dword ptr [rax+128]", a.write_mov(eax, Mem32(RAX, 128)));
	ASSERT_X64("8b 40 80", "mov eax, dword ptr [rax-128]", a.write_mov(eax, Mem32(RAX, -128)));
	ASSERT_X64("8b 80 7f ff ff ff", "mov eax, dword ptr [rax-129]", a.write_mov(eax, Mem32(RAX, -129)));
	ASSERT_X64("8b 84 24 e8 03 00 00", "mov eax, dword ptr [rsp+1000]", a.write_mov(eax, Mem32(RSP, 1000)));
	ASSERT_X64("8b 04 08", "mov eax, dword ptr [rax+rcx*1]", a.write_mov(eax, Mem32(RAX, Index(RCX, Scale::x1))));
	ASSERT_X64("8b 04 48", "mov eax, dword ptr [rax+rcx*2]", a.write_mov(eax, Mem32(RAX, Index(RCX, Scale::x2))));
	ASSERT_X64("8b 04 88", "mov eax, dword ptr [rax+rcx*4]", a.write_mov(eax, Mem32(RAX, Index(RCX, Scale::x4))));
	ASSERT_X64("8b 44 c8 10", "mov eax, dword ptr [rax+rcx*8+16]", a.write_mov(eax, Mem32(RAX, Index(RCX, Scale::x8), 16)));
	ASSERT_X64("8b 44 8d 00", "mov eax, dword ptr [rbp+rcx*4]", a.write_mov(eax, Mem32(RBP, Index(RCX, Scale::x4))));
	ASSERT_X64("47 8b 04 51", "mov r8d, dword ptr [r9+r10*2]", a.write_mov(r8d, Mem32(R9, Index(R10, Scale::x2))));
}

TEST(zero_and_sign_extension)
{
	ASSERT_X64("0f b6 c1", "movzx eax, cl", a.write_movzx(eax, cl));
	ASSERT_X64("0f b7 c1", "movzx eax, cx", a.write_movzx(eax, cx));
	ASSERT_X64("48 0f b6 c1", "movzx rax, cl", a.write_movzx(rax, cl));
	ASSERT_X64("66 0f b6 c1", "movzx ax, cl", a.write_movzx(ax, cl));
	ASSERT_X64("0f b6 45 ff", "movzx eax, byte ptr [rbp-1]", a.write_movzx(eax, Mem8(RBP, -1)));
	ASSERT_X64("45 0f b6 c1", "movzx r8d, r9b", a.write_movzx(r8d, r9b));
	ASSERT_X64("40 0f b6 c6", "movzx eax, sil", a.write_movzx(eax, sil));
	ASSERT_X64("0f be c1", "movsx eax, cl", a.write_movsx(eax, cl));
	ASSERT_X64("0f bf c1", "movsx eax, cx", a.write_movsx(eax, cx));
	ASSERT_X64("48 0f be c1", "movsx rax, cl", a.write_movsx(rax, cl));
	ASSERT_X64("48 0f bf c1", "movsx rax, cx", a.write_movsx(rax, cx));
	ASSERT_X64("0f bf 45 fe", "movsx eax, word ptr [rbp-2]", a.write_movsx(eax, Mem16(RBP, -2)));
}

TEST(alu_instructions)
{
	ASSERT_X64("01 c8", "add eax, ecx", a.write_add(eax, ecx));
	ASSERT_X64("09 c8", "or eax, ecx", a.write_or(eax, ecx));
	ASSERT_X64("21 c8", "and eax, ecx", a.write_and(eax, ecx));
	ASSERT_X64("29 c8", "sub eax, ecx", a.write_sub(eax, ecx));
	ASSERT_X64("31 c8", "xor eax, ecx", a.write_xor(eax, ecx));
	ASSERT_X64("39 c8", "cmp eax, ecx", a.write_cmp(eax, ecx));
	ASSERT_X64("00 c8", "add al, cl", a.write_add(al, cl));
	ASSERT_X64("66 01 c8", "add ax, cx", a.write_add(ax, cx));
	ASSERT_X64("48 01 c8", "add rax, rcx", a.write_add(rax, rcx));
	ASSERT_X64("45 30 d8", "xor r8b, r11b", a.write_xor(r8b, r11b));
	ASSERT_X64("03 45 fc", "add eax, dword ptr [rbp-4]", a.write_add(eax, Mem32(RBP, -4)));
	ASSERT_X64("01 45 fc", "add dword ptr [rbp-4], eax", a.write_add(Mem32(RBP, -4), eax));
	ASSERT_X64("3a 01", "cmp al, byte ptr [rcx]", a.write_cmp(al, Mem8(RCX)));
	ASSERT_X64("83 c1 01", "add ecx, 1", a.write_add(ecx, Imm32(1)));
	ASSERT_X64("83 c1 7f", "add ecx, 127", a.write_add(ecx, Imm32(127)));
	ASSERT_X64("83 c1 80", "add ecx, -128", a.write_add(ecx, Imm32(-128)));
	ASSERT_X64("81 c1 80 00 00 00", "add ecx, 128", a.write_add(ecx, Imm32(128)));
	ASSERT_X64("81 c1 7f ff ff ff", "add ecx, -129", a.write_add(ecx, Imm32(-129)));
	ASSERT_X64("83 c0 01", "add eax, 1", a.write_add(eax, Imm32(1)));
	ASSERT_X64("05 e8 03 00 00", "add eax, 1000", a.write_add(eax, Imm32(1000)));
	ASSERT_X64("66 05 e8 03", "add ax, 1000", a.write_add(ax, Imm16(1000)));
	ASSERT_X64("48 05 e8 03 00 00", "add rax, 1000", a.write_add(rax, ImmS64(1000)));
	ASSERT_X64("48 83 ec 10", "sub rsp, 16", a.write_sub(rsp, ImmS64(16)));
	ASSERT_X64("48 81 ec 00 10 00 00", "sub rsp, 4096", a.write_sub(rsp, ImmS64(4096)));
	ASSERT_X64("04 05", "add al, 5", a.write_add(al, Imm8(5)));
	ASSERT_X64("80 c1 05", "add cl, 5", a.write_add(cl, Imm8(5)));
	ASSERT_X64("3c 05", "cmp al, 5", a.write_cmp(al, Imm8(5)));
	ASSERT_X64("83 7d fc 05", "cmp dword ptr [rbp-4], 5", a.write_cmp(Mem32(RBP, -4), Imm32(5)));
	ASSERT_X64("49 83 e1 f0", "and r9, -16", a.write_and(r9, ImmS64(-16)));
}

TEST(multiply_negate_divide)
{
	ASSERT_X64("0f af c1", "imul eax, ecx", a.write_imul(eax, ecx));
	ASSERT_X64("66 0f af c1", "imul ax, cx", a.write_imul(ax, cx));
	ASSERT_X64("48 0f af c1", "imul rax, rcx", a.write_imul(rax, rcx));
	ASSERT_X64("45 0f af c1", "imul r8d, r9d", a.write_imul(r8d, r9d));
	ASSERT_X64("0f af 45 fc", "imul eax, dword ptr [rbp-4]", a.write_imul(eax, Mem32(RBP, -4)));
	ASSERT_X64("f7 d8", "neg eax", a.write_neg(eax));
	ASSERT_X64("f6 d8", "neg al", a.write_neg(al));
	ASSERT_X64("66 f7 d8", "neg ax", a.write_neg(ax));
	ASSERT_X64("48 f7 d8", "neg rax", a.write_neg(rax));
	ASSERT_X64("f7 d0", "not eax", a.write_not(eax));
	ASSERT_X64("f6 d1", "not cl", a.write_not(cl));
	ASSERT_X64("49 f7 d2", "not r10", a.write_not(r10));
	ASSERT_X64("f7 f1", "div ecx", a.write_div(ecx));
	ASSERT_X64("48 f7 f1", "div rcx", a.write_div(rcx));
	ASSERT_X64("f7 f9", "idiv ecx", a.write_idiv(ecx));
	ASSERT_X64("48 f7 f9", "idiv rcx", a.write_idiv(rcx));
	ASSERT_X64("41 f7 f8", "idiv r8d", a.write_idiv(r8d));
	ASSERT_X64("99", "cdq", a.write_cdq());
	ASSERT_X64("48 99", "cqo", a.write_cqo());
}

TEST(shifts)
{
	ASSERT_X64("d3 e0", "shl eax, cl", a.write_shl_cl(eax));
	ASSERT_X64("d3 e8", "shr eax, cl", a.write_shr_cl(eax));
	ASSERT_X64("d3 f8", "sar eax, cl", a.write_sar_cl(eax));
	ASSERT_X64("48 d3 e0", "shl rax, cl", a.write_shl_cl(rax));
	ASSERT_X64("d2 e0", "shl al, cl", a.write_shl_cl(al));
	ASSERT_X64("41 d3 f9", "sar r9d, cl", a.write_sar_cl(r9d));
	ASSERT_X64("d1 e0", "shl eax, 1", a.write_shl(eax, 1));
	ASSERT_X64("d1 e8", "shr eax, 1", a.write_shr(eax, 1));
	ASSERT_X64("d1 f8", "sar eax, 1", a.write_sar(eax, 1));
	ASSERT_X64("c1 e0 05", "shl eax, 5", a.write_shl(eax, 5));
	ASSERT_X64("c1 e8 05", "shr eax, 5", a.write_shr(eax, 5));
	ASSERT_X64("c1 f8 05", "sar eax, 5", a.write_sar(eax, 5));
	ASSERT_X64("48 c1 e0 28", "shl rax, 40", a.write_shl(rax, 40));
	ASSERT_X64("c0 e0 03", "shl al, 3", a.write_shl(al, 3));
	ASSERT_X64("66 c1 f8 03", "sar ax, 3", a.write_sar(ax, 3));
}

TEST(set_on_condition)
{
	ASSERT_X64("0f 90 c0", "seto al", a.write_setcc(Cond::O, al));
	ASSERT_X64("0f 91 c0", "setno al", a.write_setcc(Cond::NO, al));
	ASSERT_X64("0f 92 c0", "setb al", a.write_setcc(Cond::B, al));
	ASSERT_X64("0f 93 c0", "setae al", a.write_setcc(Cond::AE, al));
	ASSERT_X64("0f 94 c0", "sete al", a.write_setcc(Cond::E, al));
	ASSERT_X64("0f 95 c0", "setne al", a.write_setcc(Cond::NE, al));
	ASSERT_X64("0f 96 c0", "setbe al", a.write_setcc(Cond::BE, al));
	ASSERT_X64("0f 97 c0", "seta al", a.write_setcc(Cond::A, al));
	ASSERT_X64("0f 98 c0", "sets al", a.write_setcc(Cond::S, al));
	ASSERT_X64("0f 99 c0", "setns al", a.write_setcc(Cond::NS, al));
	ASSERT_X64("0f 9a c0", "setp al", a.write_setcc(Cond::P, al));
	ASSERT_X64("0f 9b c0", "setnp al", a.write_setcc(Cond::NP, al));
	ASSERT_X64("0f 9c c0", "setl al", a.write_setcc(Cond::L, al));
	ASSERT_X64("0f 9d c0", "setge al", a.write_setcc(Cond::GE, al));
	ASSERT_X64("0f 9e c0", "setle al", a.write_setcc(Cond::LE, al));
	ASSERT_X64("0f 9f c0", "setg al", a.write_setcc(Cond::G, al));
	ASSERT_X64("41 0f 94 c1", "sete r9b", a.write_setcc(Cond::E, r9b));
	ASSERT_X64("40 0f 94 c6", "sete sil", a.write_setcc(Cond::E, sil));
	ASSERT_X64("0f 94 45 ff", "sete byte ptr [rbp-1]", a.write_setcc(Cond::E, Mem8(RBP, -1)));
}

TEST(stack_and_control_flow)
{
	ASSERT_X64("55", "push rbp", a.write_push(rbp));
	ASSERT_X64("41 54", "push r12", a.write_push(r12));
	ASSERT_X64("5d", "pop rbp", a.write_pop(rbp));
	ASSERT_X64("41 5c", "pop r12", a.write_pop(r12));
	ASSERT_X64("c9", "leave", a.write_leave());
	ASSERT_X64("c3", "ret", a.write_ret());
	ASSERT_X64("90", "nop", a.write_nop());
	ASSERT_X64("0f 05", "syscall", a.write_syscall());
}

TEST(thirty_two_bit_mode)
{
	ASSERT_X86("8b 44 24 08", "mov eax, dword ptr [esp+8]", a.write_mov(eax, Mem32(RSP, 8)));
	ASSERT_X86("89 45 fc", "mov dword ptr [ebp-4], eax", a.write_mov(Mem32(RBP, -4), eax));
	ASSERT_X86("8b 45 00", "mov eax, dword ptr [ebp]", a.write_mov(eax, Mem32(RBP)));
	ASSERT_X86("66 8b 01", "mov ax, word ptr [ecx]", a.write_mov(ax, Mem16(RCX)));
	ASSERT_X86("8a 41 01", "mov al, byte ptr [ecx+1]", a.write_mov(al, Mem8(RCX, 1)));
	ASSERT_X86("89 c8", "mov eax, ecx", a.write_mov(eax, ecx));
	ASSERT_X86("b0 05", "mov al, 5", a.write_mov(al, Imm8(5)));
	ASSERT_X86("b8 05 00 00 00", "mov eax, 5", a.write_mov(eax, Imm32(5)));
	ASSERT_X86("55", "push ebp", a.write_push(ebp));
	ASSERT_X86("5d", "pop ebp", a.write_pop(ebp));
	ASSERT_X86("cd 80", "int 0x80", a.write_int(0x80));
	ASSERT_X86("c9", "leave", a.write_leave());
	ASSERT_X86("c3", "ret", a.write_ret());
}

// what isn't encodable =========================================================================================

TEST(rex_needs_are_rejected_in_32_bit_mode)
{
	Assembler a(false);
	ASSERT_THROWS(a.write_mov(rax, rcx), CompilerError);
	ASSERT_THROWS(a.write_mov(r8d, eax), CompilerError);
	ASSERT_THROWS(a.write_mov(eax, r8d), CompilerError);
	ASSERT_THROWS(a.write_mov(sil, al), CompilerError);
	ASSERT_THROWS(a.write_mov(al, spl), CompilerError);
	ASSERT_THROWS(a.write_mov(eax, Mem32(R9)), CompilerError);
	ASSERT_THROWS(a.write_mov(eax, Mem32(RAX, Index(R9, Scale::x2))), CompilerError);
	ASSERT_THROWS(a.write_neg(rax), CompilerError);
	ASSERT_THROWS(a.write_cqo(), CompilerError);
	ASSERT_THROWS(a.write_push(r8d), CompilerError);
	ASSERT_THROWS(a.write_setcc(Cond::E, sil), CompilerError);
}

TEST(rejected_instructions_write_nothing)
{
	Assembler a(false);
	ASSERT_THROWS(a.write_mov(rax, rcx), CompilerError);
	ASSERT_EQ(a.cur_offset(), 0u);
}

TEST(low_byte_registers_need_no_rex_in_32_bit_mode)
{
	Assembler a(false);
	a.write_mov(al, cl);
	a.write_mov(Reg8(RDX), Reg8(RBX));
	ASSERT_EQ(hex(a), "88 c8 88 da");
}

TEST(push_and_pop_only_take_full_registers)
{
	Assembler x64(true), x86(false);
	ASSERT_THROWS(x64.write_push(eax), CompilerError);
	ASSERT_THROWS(x64.write_pop(eax), CompilerError);
	ASSERT_THROWS(x64.write_push(ax), CompilerError);
	ASSERT_THROWS(x86.write_push(rax), CompilerError);
	ASSERT_THROWS(x86.write_pop(ax), CompilerError);
}

TEST(syscall_is_64_bit_only)
{
	Assembler a(false);
	ASSERT_THROWS(a.write_syscall(), CompilerError);
}

// the assembler's signatures are what reject these, so they're checked at compile time

static_assert(!AluOperands<Mem32, Mem32>, "no memory to memory instructions");
static_assert(!AluOperands<Reg32, Reg64>, "operand widths have to match");
static_assert(!AluOperands<Reg8, Mem32>, "operand widths have to match");
static_assert(!AluOperands<Mem32, Reg8>, "operand widths have to match");
static_assert(!AluOperands<Reg32, Imm8>, "immediate width has to match the operand");
static_assert(AluOperands<Reg32, Reg32> && AluOperands<Mem32, Reg32> && AluOperands<Reg32, Mem32>);
static_assert(AluOperands<Reg32, Imm32> && AluOperands<Mem32, Imm32> && AluOperands<Reg64, ImmS64>);
static_assert(!AluOperands<Reg64, Imm32>, "64 bit instructions take their own immediate type");

static_assert(!ExtendOperands<Reg8, Reg8>, "extending has to widen");
static_assert(!ExtendOperands<Reg16, Reg16>, "extending has to widen");
static_assert(!ExtendOperands<Reg64, Reg32>, "32 to 64 bit extension is implicit in writing the register");
static_assert(!ExtendOperands<Mem32, Reg8>, "extending needs a register destination");
static_assert(ExtendOperands<Reg32, Reg8> && ExtendOperands<Reg64, Reg16> && ExtendOperands<Reg16, Mem8>);

// buffer ========================================================================================================

TEST(write_returns_the_offset_of_the_instruction)
{
	Assembler a(true);
	ASSERT_EQ(a.write_nop(), 0u);
	ASSERT_EQ(a.write_mov(rax, rcx), 1u);
	ASSERT_EQ(a.write_ret(), 4u);
	ASSERT_EQ(a.cur_offset(), 5u);
}

TEST(clear_empties_the_buffer)
{
	Assembler a(true, true);
	a.write_nop();
	a.clear();
	ASSERT_EQ(a.cur_offset(), 0u);
	ASSERT_EQ(hex(a), "");

	std::ostringstream written;
	a.dump_asm(written);
	ASSERT_EQ(written.str(), "");
}

TEST(dump_to_bytes_appends)
{
	Assembler a(true);
	a.write_nop();
	std::vector<uint8_t> bytes = {0xAA};
	a.dump_to_bytes(bytes);
	ASSERT_EQ(hex(bytes), "aa 90");
}

TEST(text_is_only_recorded_when_asked_to)
{
	Assembler a(true);
	a.write_nop();
	a.set_record_text(true);
	a.write_ret();
	a.set_record_text(false);
	a.write_leave();

	std::ostringstream written;
	a.dump_asm(written);
	ASSERT_EQ(written.str(), "\tret\n");
}

// calls =========================================================================================================

TEST(call_placeholder_has_a_zero_offset)
{
	ASSERT_X64("e8 00 00 00 00", "call foo", a.write_call_placeholder("foo"));
	ASSERT_X86("e8 00 00 00 00", "call foo", a.write_call_placeholder("foo"));
}

TEST(patch_call_is_relative_to_the_end_of_the_call)
{
	std::vector<uint8_t> code(32, 0x90);

	code[8] = 0xE8;
	Assembler::patch_call(code, 8, 13);
	ASSERT_EQ(hex({code.begin() + 8, code.begin() + 13}), "e8 00 00 00 00");

	Assembler::patch_call(code, 8, 20);
	ASSERT_EQ(hex({code.begin() + 8, code.begin() + 13}), "e8 07 00 00 00");
}

TEST(patch_call_backwards)
{
	std::vector<uint8_t> code(32, 0x90);
	code[20] = 0xE8;
	Assembler::patch_call(code, 20, 0);
	ASSERT_EQ(hex({code.begin() + 20, code.begin() + 25}), "e8 e7 ff ff ff");
}

TEST(patch_call_leaves_the_rest_alone)
{
	std::vector<uint8_t> code = {0x11, 0xE8, 0, 0, 0, 0, 0x22};
	Assembler::patch_call(code, 1, 6);
	ASSERT_EQ(hex(code), "11 e8 00 00 00 00 22");
}

TEST(patch_call_only_patches_calls)
{
	std::vector<uint8_t> code(8, 0x90);
	ASSERT_THROWS(Assembler::patch_call(code, 0, 4), CompilerError);
}

TEST(patch_call_out_of_range)
{
	std::vector<uint8_t> code(8, 0x90);
	code[0] = 0xE8;
	ASSERT_THROWS(Assembler::patch_call(code, 0, size_t(1) << 32), CompilerError);
}
