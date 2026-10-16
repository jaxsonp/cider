#pragma once

#include <stdint.h>
#include <stddef.h>
#include <initializer_list>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "backend/codegen/x86/x86.hpp"
#include "backend/codegen/x86/x86Operand.hpp"
#include "backend/codegen/x86/x86Instruction.hpp"

namespace codegen::x86
{
	namespace detail
	{
		template <Width W>
		RegNum to_reg(Reg<W> reg)
		{
			return RegNum{uint8_t(reg.reg()), W == Width::W8};
		}

		template <Width W>
		MemRef to_mem(const Mem<W> &mem)
		{
			MemRef ref{.base = uint8_t(mem.base()), .index = std::nullopt, .scale_bits = 0, .disp = mem.disp()};
			if (mem.index().has_value())
			{
				ref.index = uint8_t(mem.index()->reg());
				ref.scale_bits = uint8_t(mem.index()->scale());
			}
			return ref;
		}

		template <RmOp T>
		RmOperand to_rm(const T &op)
		{
			if constexpr (RegOp<T>)
				return to_reg(op);
			else
				return to_mem(op);
		}

		/// @brief Bytes of the immediate taken by an instruction on values of a width (64 bit instructions take
		/// a sign extended 32 bit one)
		constexpr uint8_t imm_size(Width w)
		{
			return w == Width::W64 ? 4 : uint8_t(w);
		}
	}

	/// @brief Two operands of the same width
	template <typename A, typename B>
	concept SameWidth = A::width == B::width;

	/// @brief Operands that the two operand ALU instructions (add, or, and, sub, xor, cmp) accept: r/m and
	/// register, register and memory, or r/m and immediate. Never memory and memory
	template <typename Dst, typename Src>
	concept AluOperands =
		(RmOp<Dst> && RegOp<Src> && SameWidth<Dst, Src>) ||
		(RegOp<Dst> && MemOp<Src> && SameWidth<Dst, Src>) ||
		(RmOp<Dst> && std::same_as<Src, Imm<Dst::width>>);

	/// @brief Operands of movzx/movsx: a register, and a narrower 8 or 16 bit r/m (there is no 32 to 64 bit
	/// zero extension, writing a 32 bit register already clears the upper half)
	template <typename Dst, typename Src>
	concept ExtendOperands =
		RegOp<Dst> && RmOp<Src> &&
		(Src::width == Width::W8 || Src::width == Width::W16) && (Dst::width > Src::width);

	/// @brief Buffer for emission functions to write to
	///
	/// Every `write_*` function encodes one instruction straight to bytes and returns its byte offset in the
	/// buffer. With `set_record_text`, they also keep its textual assembly (GNU as Intel syntax), for `dump_asm`. Their signatures only accept the operand kinds and widths that the instruction has an encoding for,
	/// so something like `mov [mem], [mem]` or `imul r8, r8` doesn't compile. Whether an instruction is available in
	/// the mode being assembled for (anything needing a REX prefix is 64 bit only) is checked when it's encoded,
	/// and is an internal error
	///
	/// How an x86 instruction is laid out in machine code (everything in brackets is optional):
	///
	/// ```txt
	/// [prefixes] [REX] opcode [ModRM] [SIB] [displacement] [immediate]
	/// ```
	///
	/// The only prefix used is the operand size prefix (0x66), which makes an instruction operate on 16 bits rather
	/// than 32. Most instructions have two opcodes, one for 8 bit operands and one for 16/32/64 bit operands
	///
	/// REX (x64 only) is the byte 0100WRXB, its bits are:
	/// - W: 64 bit operands
	/// - R, X, B: the fourth (top) bit of the ModRM reg field, SIB index field, and ModRM rm field/SIB base field/
	///   register in the opcode respectively, which is how r8-r15 are reached
	/// A REX prefix with no bits set still matters: in 8 bit instructions, register numbers 4-7 mean AH, CH, DH and
	/// BH without one, and SPL, BPL, SIL and DIL with one
	///
	/// ModRM is the byte that says what the operands are. Its fields are:
	///
	/// ```txt
	/// mod (2 bits) | reg (3 bits) | rm (3 bits)
	/// ```
	///
	/// - mod: Describes what the r/m operand of the instruction is. It's values can be:
	///   - 00: memory access, at `[rm]`
	///   - 01: memory access, at `[rm + offset]` where offset is 1 byte in [displacement]
	///   - 10: memory access, at `[rm + offset]` where offset is 4 bytes in [displacement]
	///   - 11: register, specified by `rm`
	///   There are two special cases when mod != 11:
	///   - rm == 100 (sp) means the SIB byte is present and used for memory accesses, rather than rm. Offset in
	///     [displacement] still applies
	///   - mod == 00 and rm == 101 (bp) means there is no base register, only a 4 byte [displacement] (an absolute
	///     address in 32 bit mode, relative to the next instruction in 64 bit mode). So bp as a base always needs
	///     mod 01 or 10, even for an offset of 0
	/// - rm: See MOD notes
	/// - reg: is the other, not r/m operand, which is always a register. Instructions without
	///   a second register (ie when it's an immediate instead) have no use for the field,
	///   so it holds more opcode bits: the opcode extension, written "/digit" in the Intel
	///   manual ("/r" is a normal ModRM, with a register in reg)
	///
	/// SIB byte is the secondary indexing byte, used by some ops to describe non-static memory
	/// indexing/offsetting, eg `[base + index * scale]` (last offset is described in
	/// [displacement]). Fields are:
	///
	/// ```txt
	/// scale (2 bits) | index (3 bits) | base (3 bits)
	/// ```
	///
	/// - scale: scaling factor (1, 2, 4, 8)
	/// - index: register holding index value, 100 (sp) means no index
	/// - base: register holding the base address
	class Assembler
	{
		std::vector<uint8_t> buf;

		bool is_x64;

		/// Whether to keep the textual assembly of every instruction
		bool record_text = false;

		/// Textual assembly of every instruction, if `record_text`
		std::vector<std::string> lines;

		/// @brief The ALU instructions sharing one encoding pattern. The value is the opcode extension of the
		/// immediate forms, and the register forms' opcodes are 8 times it (+0 to +3, +4/+5 for the accumulator)
		enum class AluOp : uint8_t
		{
			ADD = 0,
			OR = 1,
			AND = 4,
			SUB = 5,
			XOR = 6,
			CMP = 7,
		};

		static constexpr std::string_view ALU_MNEMONICS[] = {"add", "or", "", "", "and", "sub", "xor", "cmp"};

		/// @brief Groups of instructions with one r/m operand, distinguished by their opcode extension
		enum class Group : uint8_t
		{
			/// F6/F7 group
			NOT = 2,
			NEG = 3,
			DIV = 6,
			IDIV = 7,
			/// shift group (C0/C1, D0/D1, D2/D3)
			SHL = 4,
			SHR = 5,
			SAR = 7,
		};

		// textual assembly -------------

		template <Width W>
		std::string operand_text(Reg<W> reg) const
		{
			return std::string(register_name(reg.reg(), W));
		}

		template <Width W>
		std::string operand_text(const Mem<W> &mem) const
		{
			static constexpr std::string_view SIZES[] = {"", "byte", "word", "", "dword", "", "", "", "qword"};
			// addresses are always the full registers of the mode
			Width address = this->is_x64 ? Width::W64 : Width::W32;

			std::string text = std::format("{} ptr [{}", SIZES[uint8_t(W)], register_name(mem.base(), address));
			if (mem.index().has_value())
				text += std::format("+{}*{}", register_name(mem.index()->reg(), address), 1 << uint8_t(mem.index()->scale()));
			if (mem.disp() != 0)
				text += std::format("{:+}", mem.disp());
			return text + "]";
		}

		template <Width W>
		std::string operand_text(Imm<W> imm) const
		{
			return std::to_string(int64_t(imm.value));
		}

		std::string operand_text(Imm64 imm) const
		{
			return std::to_string(imm.value);
		}

		/// @brief Textual assembly of an instruction, empty if it's not being recorded
		template <typename... Ops>
		std::string asm_text(std::string_view mnemonic, const Ops &...ops) const
		{
			if (!this->record_text)
				return {};
			std::string text(mnemonic);
			const char *separator = " ";
			((text += separator, text += this->operand_text(ops), separator = ", "), ...);
			return text;
		}

		// encoding -------------

		/// @brief Encodes an instruction into the buffer, returns its offset
		/// @param text Its textual assembly (empty if not recorded)
		size_t emit(const detail::Encoding &enc, std::string text);

		/// @brief Instruction with a ModRM byte
		/// @param size Operand size, picks the 0x66 prefix (16 bit) or REX.W (64 bit). The opcode is up to the caller
		size_t emit_modrm(std::string text, Width size, std::initializer_list<uint8_t> opcode, detail::RegNum reg_field,
						  const detail::RmOperand &rm, int64_t imm = 0, uint8_t imm_size = 0);

		/// @brief Instruction with a ModRM byte holding an opcode extension ("/digit") rather than a register
		size_t emit_modrm_ext(std::string text, Width size, std::initializer_list<uint8_t> opcode, uint8_t ext,
							  const detail::RmOperand &rm, int64_t imm = 0, uint8_t imm_size = 0);

		/// @brief Instruction with a register added to its opcode ("+r")
		size_t emit_opcode_reg(std::string text, Width size, uint8_t opcode, detail::RegNum reg, int64_t imm = 0, uint8_t imm_size = 0);

		/// @brief Instruction with no operands other than an immediate
		size_t emit_plain(std::string text, Width size, std::initializer_list<uint8_t> opcode, int64_t imm = 0, uint8_t imm_size = 0);

		template <RmOp Dst, RegOp Src>
			requires SameWidth<Dst, Src>
		size_t write_alu(AluOp op, const Dst &dst, Src src)
		{
			// op r/m, r
			uint8_t opcode = uint8_t(uint8_t(op) * 8 + (Dst::width == Width::W8 ? 0 : 1));
			return this->emit_modrm(this->asm_text(ALU_MNEMONICS[uint8_t(op)], dst, src),
									Dst::width, {opcode}, detail::to_reg(src), detail::to_rm(dst));
		}

		template <RegOp Dst, MemOp Src>
			requires SameWidth<Dst, Src>
		size_t write_alu(AluOp op, Dst dst, const Src &src)
		{
			// op r, r/m
			uint8_t opcode = uint8_t(uint8_t(op) * 8 + (Dst::width == Width::W8 ? 2 : 3));
			return this->emit_modrm(this->asm_text(ALU_MNEMONICS[uint8_t(op)], dst, src),
									Dst::width, {opcode}, detail::to_reg(dst), detail::to_rm(src));
		}

		template <RmOp Dst>
		size_t write_alu(AluOp op, const Dst &dst, Imm<Dst::width> imm)
		{
			// op r/m, imm. there are shorter forms with a sign extended 8 bit immediate (for wider operands), and
			// with no ModRM when the operand is the accumulator (al/ax/eax/rax)
			std::string text = this->asm_text(ALU_MNEMONICS[uint8_t(op)], dst, imm);
			bool is_accumulator = false;
			if constexpr (RegOp<Dst>)
				is_accumulator = dst.reg() == Register::RAX;

			if constexpr (Dst::width == Width::W8)
			{
				if (is_accumulator)
					return this->emit_plain(std::move(text), Dst::width, {uint8_t(uint8_t(op) * 8 + 4)}, imm.value, 1);
				return this->emit_modrm_ext(std::move(text), Dst::width, {0x80}, uint8_t(op), detail::to_rm(dst), imm.value, 1);
			}
			else
			{
				if (imm.value >= INT8_MIN && imm.value <= INT8_MAX)
					return this->emit_modrm_ext(std::move(text), Dst::width, {0x83}, uint8_t(op), detail::to_rm(dst), imm.value, 1);
				if (is_accumulator)
					return this->emit_plain(std::move(text), Dst::width, {uint8_t(uint8_t(op) * 8 + 5)}, imm.value, detail::imm_size(Dst::width));
				return this->emit_modrm_ext(std::move(text), Dst::width, {0x81}, uint8_t(op), detail::to_rm(dst), imm.value, detail::imm_size(Dst::width));
			}
		}

		template <RmOp Dst>
		size_t write_group_f6(std::string_view mnemonic, Group group, const Dst &dst)
		{
			return this->emit_modrm_ext(this->asm_text(mnemonic, dst),
										Dst::width, {uint8_t(Dst::width == Width::W8 ? 0xF6 : 0xF7)}, uint8_t(group), detail::to_rm(dst));
		}

		template <RmOp Dst>
		size_t write_shift_cl(std::string_view mnemonic, Group group, const Dst &dst)
		{
			std::string text = this->record_text ? this->asm_text(mnemonic, dst) + ", cl" : std::string();
			return this->emit_modrm_ext(std::move(text), Dst::width, {uint8_t(Dst::width == Width::W8 ? 0xD2 : 0xD3)}, uint8_t(group), detail::to_rm(dst));
		}

		template <RmOp Dst>
		size_t write_shift_imm(std::string_view mnemonic, Group group, const Dst &dst, uint8_t count)
		{
			std::string text = this->record_text ? std::format("{}, {}", this->asm_text(mnemonic, dst), count) : std::string();
			// shifting by 1 has its own form with no immediate
			if (count == 1)
				return this->emit_modrm_ext(std::move(text), Dst::width, {uint8_t(Dst::width == Width::W8 ? 0xD0 : 0xD1)}, uint8_t(group), detail::to_rm(dst));
			return this->emit_modrm_ext(std::move(text), Dst::width, {uint8_t(Dst::width == Width::W8 ? 0xC0 : 0xC1)}, uint8_t(group), detail::to_rm(dst), count, 1);
		}

		/// @brief Throws if a register can't be pushed/popped, which only works with full registers of the mode
		template <RegOp R>
		void require_word_reg(const char *mnemonic) const
		{
			if (R::width != (this->is_x64 ? Width::W64 : Width::W32))
				throw CompilerError::internal(std::format("x86 encoding: {} only takes {} bit registers in this mode", mnemonic, this->is_x64 ? 64 : 32));
		}

	public:
		/// @param is_x64 Whether to assemble for 64 bit mode, rather than 32 bit mode
		/// @param record_text Whether to keep the textual assembly of every instruction, for `dump_asm`
		Assembler(bool is_x64, bool record_text = false);

		/// @brief Reset buffer contents
		void clear();

		/// @brief Sets whether to keep the textual assembly of instructions written from now on
		void set_record_text(bool record) { this->record_text = record; }

		/// @brief Size in bytes of everything written so far, which is also the byte offset of the next instruction
		size_t cur_offset() const { return this->buf.size(); }

		/// @brief Copy the contents of this buffer into a byte vector
		void dump_to_bytes(std::vector<uint8_t> &bytes) const;

		/// @brief Writes the recorded textual assembly, one indented instruction per line
		void dump_asm(std::ostream &out) const;

		/// @brief Points a call written by `write_call_placeholder` at its target
		/// @param code Buffer containing the call
		/// @param call_offset Offset of the call in `code`
		/// @param target_offset Offset in `code` of the function being called
		static void patch_call(std::vector<uint8_t> &code, size_t call_offset, size_t target_offset);

		// data movement -------------

		/// mov r/m, r
		template <RmOp Dst, RegOp Src>
			requires SameWidth<Dst, Src>
		size_t write_mov(const Dst &dst, Src src)
		{
			return this->emit_modrm(this->asm_text("mov", dst, src),
									Dst::width, {uint8_t(Dst::width == Width::W8 ? 0x88 : 0x89)}, detail::to_reg(src), detail::to_rm(dst));
		}

		/// mov r, m
		template <RegOp Dst, MemOp Src>
			requires SameWidth<Dst, Src>
		size_t write_mov(Dst dst, const Src &src)
		{
			return this->emit_modrm(this->asm_text("mov", dst, src),
									Dst::width, {uint8_t(Dst::width == Width::W8 ? 0x8A : 0x8B)}, detail::to_reg(dst), detail::to_rm(src));
		}

		/// mov r, imm. On 64 bit registers the immediate is a sign extended 32 bit one, see the `Imm64` overload
		/// for full 64 bit values
		template <RegOp Dst>
		size_t write_mov(Dst dst, Imm<Dst::width> imm)
		{
			std::string text = this->asm_text("mov", dst, imm);
			if constexpr (Dst::width == Width::W64)
				// the +r form takes a full 64 bit immediate, the sign extended one is mov r/m64, imm32
				return this->emit_modrm_ext(std::move(text), Width::W64, {0xC7}, 0, detail::to_rm(dst), imm.value, 4);
			else
				return this->emit_opcode_reg(std::move(text), Dst::width, Dst::width == Width::W8 ? 0xB0 : 0xB8, detail::to_reg(dst), imm.value, detail::imm_size(Dst::width));
		}

		/// mov m, imm
		template <MemOp Dst>
		size_t write_mov(const Dst &dst, Imm<Dst::width> imm)
		{
			return this->emit_modrm_ext(this->asm_text("mov", dst, imm),
										Dst::width, {uint8_t(Dst::width == Width::W8 ? 0xC6 : 0xC7)}, 0, detail::to_rm(dst), imm.value, detail::imm_size(Dst::width));
		}

		/// mov r64, imm64 (aka movabs), the only instruction with a 64 bit immediate
		size_t write_mov(Reg64 dst, Imm64 imm);

		/// movzx r, r/m (move with zero extension)
		template <typename Dst, typename Src>
			requires ExtendOperands<Dst, Src>
		size_t write_movzx(Dst dst, const Src &src)
		{
			return this->emit_modrm(this->asm_text("movzx", dst, src),
									Dst::width, {0x0F, uint8_t(Src::width == Width::W8 ? 0xB6 : 0xB7)}, detail::to_reg(dst), detail::to_rm(src));
		}

		/// movsx r, r/m (move with sign extension)
		template <typename Dst, typename Src>
			requires ExtendOperands<Dst, Src>
		size_t write_movsx(Dst dst, const Src &src)
		{
			return this->emit_modrm(this->asm_text("movsx", dst, src),
									Dst::width, {0x0F, uint8_t(Src::width == Width::W8 ? 0xBE : 0xBF)}, detail::to_reg(dst), detail::to_rm(src));
		}

		// arithmetic -------------

		/// add (dst += src)
		template <typename Dst, typename Src>
			requires AluOperands<Dst, Src>
		size_t write_add(const Dst &dst, const Src &src) { return this->write_alu(AluOp::ADD, dst, src); }

		/// sub (dst -= src)
		template <typename Dst, typename Src>
			requires AluOperands<Dst, Src>
		size_t write_sub(const Dst &dst, const Src &src) { return this->write_alu(AluOp::SUB, dst, src); }

		/// and (dst &= src)
		template <typename Dst, typename Src>
			requires AluOperands<Dst, Src>
		size_t write_and(const Dst &dst, const Src &src) { return this->write_alu(AluOp::AND, dst, src); }

		/// or (dst |= src)
		template <typename Dst, typename Src>
			requires AluOperands<Dst, Src>
		size_t write_or(const Dst &dst, const Src &src) { return this->write_alu(AluOp::OR, dst, src); }

		/// xor (dst ^= src)
		template <typename Dst, typename Src>
			requires AluOperands<Dst, Src>
		size_t write_xor(const Dst &dst, const Src &src) { return this->write_alu(AluOp::XOR, dst, src); }

		/// cmp (sets flags from dst - src)
		template <typename Dst, typename Src>
			requires AluOperands<Dst, Src>
		size_t write_cmp(const Dst &dst, const Src &src) { return this->write_alu(AluOp::CMP, dst, src); }

		/// imul r, r/m (dst *= src). Has no 8 bit form
		template <RegOp Dst, RmOp Src>
			requires SameWidth<Dst, Src> && (Dst::width != Width::W8)
		size_t write_imul(Dst dst, const Src &src)
		{
			return this->emit_modrm(this->asm_text("imul", dst, src),
									Dst::width, {0x0F, 0xAF}, detail::to_reg(dst), detail::to_rm(src));
		}

		/// neg (two's complement negation)
		template <RmOp Dst>
		size_t write_neg(const Dst &dst) { return this->write_group_f6("neg", Group::NEG, dst); }

		/// not (bitwise not)
		template <RmOp Dst>
		size_t write_not(const Dst &dst) { return this->write_group_f6("not", Group::NOT, dst); }

		/// div (unsigned division). Divides edx:eax (or rdx:rax, dx:ax, ax for narrower operands) by the operand,
		/// quotient goes in eax and remainder in edx. Traps if the divisor is 0 or the quotient doesn't fit
		template <RmOp Src>
		size_t write_div(const Src &src) { return this->write_group_f6("div", Group::DIV, src); }

		/// idiv (signed division), same operands as `write_div`
		template <RmOp Src>
		size_t write_idiv(const Src &src) { return this->write_group_f6("idiv", Group::IDIV, src); }

		/// cdq (sign extend eax into edx:eax), for signed 32 bit division
		size_t write_cdq() { return this->emit_plain(this->asm_text("cdq"), Width::W32, {0x99}); }

		/// cqo (sign extend rax into rdx:rax), for signed 64 bit division
		size_t write_cqo() { return this->emit_plain(this->asm_text("cqo"), Width::W64, {0x99}); }

		/// shl r/m, cl (shift left). The processor masks the shift amount to 5 bits (6 for 64 bit operands)
		template <RmOp Dst>
		size_t write_shl_cl(const Dst &dst) { return this->write_shift_cl("shl", Group::SHL, dst); }

		/// shr r/m, cl (logical shift right). The processor masks the shift amount to 5 bits (6 for 64 bit operands)
		template <RmOp Dst>
		size_t write_shr_cl(const Dst &dst) { return this->write_shift_cl("shr", Group::SHR, dst); }

		/// sar r/m, cl (arithmetic shift right). The processor masks the shift amount to 5 bits (6 for 64 bit operands)
		template <RmOp Dst>
		size_t write_sar_cl(const Dst &dst) { return this->write_shift_cl("sar", Group::SAR, dst); }

		/// shl r/m, imm8
		template <RmOp Dst>
		size_t write_shl(const Dst &dst, uint8_t count) { return this->write_shift_imm("shl", Group::SHL, dst, count); }

		/// shr r/m, imm8
		template <RmOp Dst>
		size_t write_shr(const Dst &dst, uint8_t count) { return this->write_shift_imm("shr", Group::SHR, dst, count); }

		/// sar r/m, imm8
		template <RmOp Dst>
		size_t write_sar(const Dst &dst, uint8_t count) { return this->write_shift_imm("sar", Group::SAR, dst, count); }

		/// setcc r/m8 (sets a byte to 1 if the condition holds, 0 otherwise)
		template <RmOp Dst>
			requires(Dst::width == Width::W8)
		size_t write_setcc(Cond cond, const Dst &dst)
		{
			std::string text = this->record_text ? this->asm_text(std::string("set") + std::string(cond_suffix(cond)), dst) : std::string();
			return this->emit_modrm_ext(std::move(text), Width::W8, {0x0F, uint8_t(0x90 + uint8_t(cond))}, 0, detail::to_rm(dst));
		}

		// stack and control flow -------------

		/// push r (only full registers of the mode)
		template <RegOp R>
		size_t write_push(R reg)
		{
			this->require_word_reg<R>("push");
			// operates on 64 bits by default in 64 bit mode, no REX.W
			return this->emit_opcode_reg(this->asm_text("push", reg), Width::W32, 0x50, detail::to_reg(reg));
		}

		/// pop r (only full registers of the mode)
		template <RegOp R>
		size_t write_pop(R reg)
		{
			this->require_word_reg<R>("pop");
			return this->emit_opcode_reg(this->asm_text("pop", reg), Width::W32, 0x58, detail::to_reg(reg));
		}

		/// call rel32, with a zero offset to be filled in by `patch_call`
		/// @param target Symbol being called, for the textual assembly
		size_t write_call_placeholder(std::string_view target)
		{
			return this->emit_plain(this->record_text ? std::format("call {}", target) : std::string(), Width::W32, {0xE8}, 0, 4);
		}

		/// leave (mov sp, bp; pop bp)
		size_t write_leave() { return this->emit_plain(this->asm_text("leave"), Width::W32, {0xC9}); }

		/// ret
		size_t write_ret() { return this->emit_plain(this->asm_text("ret"), Width::W32, {0xC3}); }

		/// int imm8 (software interrupt, ie 0x80 for 32 bit linux syscalls)
		size_t write_int(uint8_t vector)
		{
			return this->emit_plain(this->record_text ? std::format("int {:#x}", vector) : std::string(), Width::W32, {0xCD}, vector, 1);
		}

		/// syscall (64 bit mode only)
		size_t write_syscall();

		/// nop
		size_t write_nop() { return this->emit_plain(this->asm_text("nop"), Width::W32, {0x90}); }
	};
}
