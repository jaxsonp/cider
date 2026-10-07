#pragma once

#include <stdint.h>
#include <stddef.h>
#include <vector>

#include "backend/codegen/riscv/Riscv.hpp"
#include "backend/codegen/riscv/RiscvInstruction.hpp"

namespace codegen::riscv
{
	/// @brief Buffer for emission functions to write to
	///
	/// Instructions are referred to by their position (index) in the buffer, which is what the `write_*`
	/// functions return. Since instructions can be 2 or 4 bytes long (with the C extension), positions are
	/// not byte offsets, use `offset_of` for those
	class Assembler
	{
		struct Entry
		{
			Instruction instr;
			/// Byte offset from the start of the buffer
			size_t offset;
			/// Whether this is to be emitted in its 16 bit compressed form
			bool will_compress;
		};

		std::vector<Entry> buf;

		/// Size in bytes of everything in the buffer
		size_t byte_size = 0;

		/// Register width, 32 or 64
		unsigned int xlen;

		/// Whether to use compressed instructions (C extension) where possible
		bool compress;

	public:
		/// @param xlen Register width of the target, 32 or 64
		/// @param compress Whether to use compressed instructions (C extension) where possible
		Assembler(unsigned int xlen, bool compress)
			: xlen(xlen), compress(compress) {}

		/// @brief Size in bytes of everything written so far, which is also the byte offset of the next instruction
		size_t cur_offset() const { return this->byte_size; }

		/// @brief Byte offset (from the start of this buffer) of the instruction at a position
		size_t offset_of(size_t pos) const { return this->buf.at(pos).offset; }

		/// @brief Replaces the immediate of an already written instruction, for filling in offsets that weren't
		/// known when it was written. Only possible for instructions written with a fixed size
		void backpatch_immediate(size_t pos, int64_t imm);

		/// Copy the contents of this buffer as bytes into a byte vector
		void dump_to_bytes(std::vector<uint8_t> &bytes) const;

		/// @brief Push a new instruction, return its position
		/// @param fixed_size Never compress this instruction. Required if it will be backpatched (compressed
		/// instructions have much smaller immediates, so the final value might not fit)
		size_t write(const Instruction &instr, bool fixed_size = false);

		/// push new add (add register) instruction, return its position
		size_t write_add(Register dest, Register op1, Register op2) { return this->write({Mnemonic::ADD, dest, op1, op2}); }

		/// push new sub (subtract register) instruction, return its position
		size_t write_sub(Register dest, Register op1, Register op2) { return this->write({Mnemonic::SUB, dest, op1, op2}); }

		/// push new xor (bitwise xor) instruction, return its position
		size_t write_xor(Register dest, Register op1, Register op2) { return this->write({Mnemonic::XOR, dest, op1, op2}); }

		/// push new or (bitwise or) instruction, return its position
		size_t write_or(Register dest, Register op1, Register op2) { return this->write({Mnemonic::OR, dest, op1, op2}); }

		/// push new and (bitwise and) instruction, return its position
		size_t write_and(Register dest, Register op1, Register op2) { return this->write({Mnemonic::AND, dest, op1, op2}); }

		/// push new sll (shift left logical) instruction, return its position
		size_t write_sll(Register dest, Register op1, Register op2) { return this->write({Mnemonic::SLL, dest, op1, op2}); }

		/// push new srl (shift right logical) instruction, return its position
		size_t write_srl(Register dest, Register op1, Register op2) { return this->write({Mnemonic::SRL, dest, op1, op2}); }

		/// push new sra (shift right arithmetic) instruction, return its position
		size_t write_sra(Register dest, Register op1, Register op2) { return this->write({Mnemonic::SRA, dest, op1, op2}); }

		/// push new slt (set if less than) instruction, return its position
		size_t write_slt(Register dest, Register op1, Register op2) { return this->write({Mnemonic::SLT, dest, op1, op2}); }

		/// push new slt (set if less than unsigned) instruction, return its position
		size_t write_sltu(Register dest, Register op1, Register op2) { return this->write({Mnemonic::SLTU, dest, op1, op2}); }

		/// push new addi (add immediate) instruction, return its position
		size_t write_addi(Register dest, Register src, int64_t imm) { return this->write({Mnemonic::ADDI, dest, src, Register::zero, imm}); }

		/// push new xori (bitwise xor immediate) instruction, return its position
		size_t write_xori(Register dest, Register src, int64_t imm) { return this->write({Mnemonic::XORI, dest, src, Register::zero, imm}); }

		/// push new ori (bitwise or immediate) instruction, return its position
		size_t write_ori(Register dest, Register src, int64_t imm) { return this->write({Mnemonic::ORI, dest, src, Register::zero, imm}); }

		/// push new andi (bitwise and immediate) instruction, return its position
		size_t write_andi(Register dest, Register src, int64_t imm) { return this->write({Mnemonic::ANDI, dest, src, Register::zero, imm}); }

		/// push new slli (shift left logical immediate) instruction, return its position
		size_t write_slli(Register dest, Register src, int64_t imm) { return this->write({Mnemonic::SLLI, dest, src, Register::zero, imm}); }

		/// push new srli (shift right logical immediate) instruction, return its position
		size_t write_srli(Register dest, Register src, int64_t imm) { return this->write({Mnemonic::SRLI, dest, src, Register::zero, imm}); }

		/// push new srai (shift right arithmetic immediate) instruction, return its position
		size_t write_srai(Register dest, Register src, int64_t imm) { return this->write({Mnemonic::SRAI, dest, src, Register::zero, imm}); }

		/// push new slti (set if less than immediate) instruction, return its position
		size_t write_slti(Register dest, Register src, int64_t imm) { return this->write({Mnemonic::SLTI, dest, src, Register::zero, imm}); }

		/// push new sltiu (set if less than immediate unsigned) instruction, return its position
		size_t write_sltiu(Register dest, Register src, int64_t imm) { return this->write({Mnemonic::SLTIU, dest, src, Register::zero, imm}); }

		/// push new lb (load byte) instruction, return its position
		size_t write_lb(Register dest, Register addr, int64_t addr_offset) { return this->write({Mnemonic::LB, dest, addr, Register::zero, addr_offset}); }

		/// push new lh (load half word) instruction, return its position
		size_t write_lh(Register dest, Register addr, int64_t addr_offset) { return this->write({Mnemonic::LH, dest, addr, Register::zero, addr_offset}); }

		/// push new lw (load word) instruction, return its position
		size_t write_lw(Register dest, Register addr, int64_t addr_offset) { return this->write({Mnemonic::LW, dest, addr, Register::zero, addr_offset}); }

		/// push new lbu (load byte unsigned) instruction, return its position
		size_t write_lbu(Register dest, Register addr, int64_t addr_offset) { return this->write({Mnemonic::LBU, dest, addr, Register::zero, addr_offset}); }

		/// push new lhu (load half word unsigned) instruction, return its position
		size_t write_lhu(Register dest, Register addr, int64_t addr_offset) { return this->write({Mnemonic::LHU, dest, addr, Register::zero, addr_offset}); }

		/// push new sb (store byte) instruction, return its position
		size_t write_sb(Register dest_addr, Register src, int64_t addr_offset) { return this->write({Mnemonic::SB, Register::zero, dest_addr, src, addr_offset}); }

		/// push new sh (store half word) instruction, return its position
		size_t write_sh(Register dest_addr, Register src, int64_t addr_offset) { return this->write({Mnemonic::SH, Register::zero, dest_addr, src, addr_offset}); }

		/// push new sw (store word) instruction, return its position
		size_t write_sw(Register dest_addr, Register src, int64_t addr_offset) { return this->write({Mnemonic::SW, Register::zero, dest_addr, src, addr_offset}); }

		/// push new jal (jump and link) instruction, return its position
		///
		/// Never compressed, so its offset can be backpatched
		size_t write_jal(Register dest, int64_t imm) { return this->write({Mnemonic::JAL, dest, Register::zero, Register::zero, imm}, true); }

		/// push new jalr (jump and link register) instruction, return its position
		size_t write_jalr(Register dest, Register addr, int64_t addr_offset) { return this->write({Mnemonic::JALR, dest, addr, Register::zero, addr_offset}); }

		/// push new lui (load upper immediate) instruction, return its position
		///
		/// Uses the upper 20 bits of the 'imm' argument
		size_t write_lui(Register dest, int64_t imm) { return this->write({Mnemonic::LUI, dest, Register::zero, Register::zero, imm}); }

		/// push new auipc (add upper immediate to pc) instruction, return its position
		///
		/// Uses the upper 20 bits of the 'imm' argument
		size_t write_auipc(Register dest, int64_t imm) { return this->write({Mnemonic::AUIPC, dest, Register::zero, Register::zero, imm}); }

		/// push new ecall (environment call) instruction, return its position
		size_t write_ecall() { return this->write({Mnemonic::ECALL}); }

		/// push new ebreak (environment break) instruction, return its position
		size_t write_ebreak() { return this->write({Mnemonic::EBREAK}); }

		/// push new nop instruction
		size_t write_nop() { return this->write_addi(Register::zero, Register::zero, 0); }

		// RV64I

		/// push new addw (add register, 32 bit) instruction, return its position
		size_t write_addw(Register dest, Register op1, Register op2) { return this->write({Mnemonic::ADDW, dest, op1, op2}); }

		/// push new subw (subtract register, 32 bit) instruction, return its position
		size_t write_subw(Register dest, Register op1, Register op2) { return this->write({Mnemonic::SUBW, dest, op1, op2}); }

		/// push new sllw (shift left logical, 32 bit) instruction, return its position
		size_t write_sllw(Register dest, Register op1, Register op2) { return this->write({Mnemonic::SLLW, dest, op1, op2}); }

		/// push new srlw (shift right logical, 32 bit) instruction, return its position
		size_t write_srlw(Register dest, Register op1, Register op2) { return this->write({Mnemonic::SRLW, dest, op1, op2}); }

		/// push new sraw (shift right arithmetic, 32 bit) instruction, return its position
		size_t write_sraw(Register dest, Register op1, Register op2) { return this->write({Mnemonic::SRAW, dest, op1, op2}); }

		/// push new addiw (add immediate, 32 bit) instruction, return its position
		size_t write_addiw(Register dest, Register src, int64_t imm) { return this->write({Mnemonic::ADDIW, dest, src, Register::zero, imm}); }

		/// push new ld (load double word) instruction, return its position
		size_t write_ld(Register dest, Register addr, int64_t addr_offset) { return this->write({Mnemonic::LD, dest, addr, Register::zero, addr_offset}); }

		/// push new lwu (load word unsigned) instruction, return its position
		size_t write_lwu(Register dest, Register addr, int64_t addr_offset) { return this->write({Mnemonic::LWU, dest, addr, Register::zero, addr_offset}); }

		/// push new sd (store double word) instruction, return its position
		size_t write_sd(Register dest_addr, Register src, int64_t addr_offset) { return this->write({Mnemonic::SD, Register::zero, dest_addr, src, addr_offset}); }

		// M extension

		/// push new mul (multiply lower) instruction, return its position
		size_t write_mul(Register dest, Register op1, Register op2) { return this->write({Mnemonic::MUL, dest, op1, op2}); }

		/// push new mulh (multiply high signed) instruction, return its position
		size_t write_mulh(Register dest, Register op1, Register op2) { return this->write({Mnemonic::MULH, dest, op1, op2}); }

		/// push new mulhsu (multiply high signed*unsigned) instruction, return its position
		size_t write_mulhsu(Register dest, Register op1, Register op2) { return this->write({Mnemonic::MULHSU, dest, op1, op2}); }

		/// push new mulhu (multiply high unsigned) instruction, return its position
		size_t write_mulhu(Register dest, Register op1, Register op2) { return this->write({Mnemonic::MULHU, dest, op1, op2}); }

		/// push new div (signed divide) instruction, return its position
		size_t write_div(Register dest, Register op1, Register op2) { return this->write({Mnemonic::DIV, dest, op1, op2}); }

		/// push new divu (unsigned divide) instruction, return its position
		size_t write_divu(Register dest, Register op1, Register op2) { return this->write({Mnemonic::DIVU, dest, op1, op2}); }

		/// push new rem (signed division remainder) instruction, return its position
		size_t write_rem(Register dest, Register op1, Register op2) { return this->write({Mnemonic::REM, dest, op1, op2}); }

		/// push new remu (unsigned division remainder) instruction, return its position
		size_t write_remu(Register dest, Register op1, Register op2) { return this->write({Mnemonic::REMU, dest, op1, op2}); }

		// M extension, RV64

		/// push new mulw (multiply lower, 32 bit) instruction, return its position
		size_t write_mulw(Register dest, Register op1, Register op2) { return this->write({Mnemonic::MULW, dest, op1, op2}); }

		/// push new divw (signed divide, 32 bit) instruction, return its position
		size_t write_divw(Register dest, Register op1, Register op2) { return this->write({Mnemonic::DIVW, dest, op1, op2}); }

		/// push new divuw (unsigned divide, 32 bit) instruction, return its position
		size_t write_divuw(Register dest, Register op1, Register op2) { return this->write({Mnemonic::DIVUW, dest, op1, op2}); }

		/// push new remw (signed division remainder, 32 bit) instruction, return its position
		size_t write_remw(Register dest, Register op1, Register op2) { return this->write({Mnemonic::REMW, dest, op1, op2}); }

		/// push new remuw (unsigned division remainder, 32 bit) instruction, return its position
		size_t write_remuw(Register dest, Register op1, Register op2) { return this->write({Mnemonic::REMUW, dest, op1, op2}); }

		// multi-instruction sequences

		/// @brief Push the instructions that load a constant into a register
		/// @param value Constant to load. Must be representable in the target's register width (as either
		/// a signed or an unsigned number)
		void load_immediate(Register dest, int64_t value);

		/// @brief Push a call (auipc + jalr pair, which reaches anywhere in the address space) to an address
		/// that isn't known yet. Has to be pointed at its target with `patch_call` once it's been copied out
		/// @param link Register to put the return address in
		/// @return Position of the call
		size_t write_call_placeholder(Register link);

		/// @brief Size in bytes of the call written by `write_call_placeholder`
		static constexpr size_t CALL_SIZE = 8;

		/// @brief Points a call written by `write_call_placeholder` at its target
		/// @param code Bytes that the call was dumped into
		/// @param call_offset Byte offset of the call
		/// @param target_offset Byte offset to call
		static void patch_call(std::vector<uint8_t> &code, size_t call_offset, size_t target_offset);
	};
}
