#include "RiscvAssembler.hpp"

#include <array>
#include <bit>

#include "utils/common.hpp"
#include "utils/error.hpp"

namespace codegen::riscv
{
	size_t Assembler::write(const Instruction &instr, bool fixed_size)
	{
		bool can_compress = this->compress && !fixed_size && instr.try_compress(this->xlen).has_value();
		this->buf.push_back(Entry{
			.instr = instr,
			.offset = this->byte_size,
			.will_compress = can_compress,
		});
		this->byte_size += can_compress ? 2 : 4;
		return this->buf.size() - 1;
	}

	void Assembler::backpatch_immediate(size_t pos, int64_t imm)
	{
		Entry &entry = this->buf.at(pos);
		if (entry.will_compress)
			throw CompilerError::internal("RISC-V codegen: cannot backpatch a compressed instruction");
		if (entry.instr.format() == InstructionFormat::RType)
			throw CompilerError::internal("RISC-V codegen: cannot backpatch R-type instruction");
		entry.instr.imm = imm;

		// TODO handle large immediates
	}

	void Assembler::dump_to_bytes(std::vector<uint8_t> &bytes) const
	{
		bytes.reserve(bytes.size() + this->byte_size);
		for (const Entry &entry : this->buf)
		{
			if (entry.will_compress)
			{
				// can't fail, since this was already checked when the instruction was written
				uint16_t encoded = entry.instr.try_compress(this->xlen).value();
				std::array<uint8_t, 2> instr_bytes = std::bit_cast<std::array<uint8_t, 2>>(encoded);
				bytes.insert(bytes.end(), instr_bytes.begin(), instr_bytes.end());
			}
			else
			{
				std::array<uint8_t, 4> instr_bytes = std::bit_cast<std::array<uint8_t, 4>>(entry.instr.encode());
				bytes.insert(bytes.end(), instr_bytes.begin(), instr_bytes.end());
			}
		}
	}

	void Assembler::load_immediate(Register dest, int64_t value)
	{
		// on RV32 only the lower 32 bits exist, so 0xFFFFFFFF and -1 are the same thing
		if (this->xlen == 32)
			value = int64_t(int32_t(uint32_t(value)));

		if (value >= -2048 && value <= 2047)
		{
			// fits in a single I-type instruction
			this->write_addi(dest, Register::zero, value);
			return;
		}

		// the lower 12 bits are always added with an addi. it sign extends its immediate, so the rest of
		// the value is rounded to compensate when bit 11 is set
		int64_t lower = int64_t(uint64_t(value) & lower_bitmask<uint64_t>(12));
		if (lower >= 0x800)
			lower -= 0x1000;

		if (value == int64_t(int32_t(value)))
		{
			// more than 12 bits but no more than 32, needs lui for the upper 20
			this->write_lui(dest, int64_t(uint64_t(value) - uint64_t(lower)));
			if (lower != 0)
			{
				// in RV64, lui sign extends to 64 bits. for values just below 2^31 the rounding above makes it load
				// 0x80000000 (negative once sign extended), and only the 32 bit add wraps back around properly
				if (this->xlen == 64)
					this->write_addiw(dest, dest, lower);
				else
					this->write_addi(dest, dest, lower);
			}
			return;
		}

		// more than 32 bits (RV64 only). build the upper part first (recursively, it's now a smaller number),
		// shift it into place, then add the lower 12 bits. any zeroes at the bottom of the upper part are left
		// to the shift, which makes the number to build even smaller
		// the subtraction is done unsigned since it's allowed to wrap, the addi wraps it right back
		int64_t upper = int64_t(uint64_t(value) - uint64_t(lower)) >> 12;
		unsigned int shift = 12 + std::countr_zero(uint64_t(upper));
		upper >>= (shift - 12);

		this->load_immediate(dest, upper);
		this->write_slli(dest, dest, shift);
		if (lower != 0)
			this->write_addi(dest, dest, lower);
	}

	size_t Assembler::write_call_placeholder(Register link)
	{
		// both are fixed size, since the offsets they'll be patched with could be anything
		size_t pos = this->write({Mnemonic::AUIPC, link, Register::zero, Register::zero, 0}, true);
		this->write({Mnemonic::JALR, link, link, Register::zero, 0}, true);
		return pos;
	}

	void Assembler::patch_call(std::vector<uint8_t> &code, size_t call_offset, size_t target_offset)
	{
		auto read_word = [&code](size_t at)
		{
			uint32_t word = 0;
			for (size_t i = 0; i < 4; ++i)
				word |= uint32_t(code.at(at + i)) << (8 * i);
			return word;
		};
		auto write_word = [&code](size_t at, uint32_t word)
		{
			for (size_t i = 0; i < 4; ++i)
				code.at(at + i) = uint8_t(word >> (8 * i));
		};

		// auipc and jalr both sit at call_offset when the address is computed, so the offset is
		// relative to the auipc
		uint32_t rel_offset = uint32_t(target_offset) - uint32_t(call_offset);
		// jalr sign extends its 12 bit immediate, so round the upper part to compensate
		uint32_t hi = (rel_offset + 0x800u) & ~lower_bitmask<uint32_t>(12);
		uint32_t lo = rel_offset & lower_bitmask<uint32_t>(12);

		size_t auipc_at = call_offset;
		size_t jalr_at = call_offset + 4;
		write_word(auipc_at, (read_word(auipc_at) & lower_bitmask<uint32_t>(12)) | hi);
		write_word(jalr_at, (read_word(jalr_at) & lower_bitmask<uint32_t>(20)) | (lo << 20));
	}
}
