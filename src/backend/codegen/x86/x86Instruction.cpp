#include "x86Instruction.hpp"

#include <format>
#include <string>

#include "utils/error.hpp"

namespace codegen::x86::detail
{
	// REX prefix bits, the prefix is 0100WRXB
	static constexpr uint8_t REX = 0x40;
	/// 64 bit operand size
	static constexpr uint8_t REX_W = 0x08;
	/// Extends ModRM.reg
	static constexpr uint8_t REX_R = 0x04;
	/// Extends SIB.index
	static constexpr uint8_t REX_X = 0x02;
	/// Extends ModRM.rm, SIB.base or the opcode's register
	static constexpr uint8_t REX_B = 0x01;

	/// @brief Collects what REX prefix an instruction needs, and why
	struct RexNeeds
	{
		uint8_t bits = 0;
		bool needed = false;
		/// Reason for the first thing that needed a REX prefix, for the error in 32 bit mode
		std::string reason;

		void need(uint8_t bit, const std::string &why)
		{
			this->bits |= bit;
			if (!this->needed)
				this->reason = why;
			this->needed = true;
		}

		/// @brief Records a register at some position, `extension_bit` being the REX bit that extends it
		void reg(RegNum r, uint8_t extension_bit)
		{
			if (r.num >= 8)
				this->need(extension_bit, std::format("register r{}", r.num));
			else if (r.is_byte && r.num >= 4)
				// a REX prefix (even with no bits set) turns AH/CH/DH/BH into SPL/BPL/SIL/DIL
				this->need(0, "byte register SPL/BPL/SIL/DIL");
		}
	};

	static void encode_int(std::vector<uint8_t> &buf, int64_t value, unsigned int size)
	{
		for (unsigned int i = 0; i < size; ++i)
			buf.push_back(uint8_t(uint64_t(value) >> (8 * i)));
	}

	void Encoding::encode(std::vector<uint8_t> &buf, bool is_x64) const
	{
		// figure out the REX prefix -------------

		RexNeeds rex;
		if (this->rex_w)
			rex.need(REX_W, "64 bit operand");
		if (this->opcode_reg.has_value())
			rex.reg(*this->opcode_reg, REX_B);
		if (this->has_modrm)
		{
			rex.reg(this->reg_field, REX_R);
			if (const RegNum *rm_reg = std::get_if<RegNum>(&this->rm))
			{
				rex.reg(*rm_reg, REX_B);
			}
			else
			{
				const MemRef &mem = std::get<MemRef>(this->rm);
				rex.reg(RegNum{mem.base, false}, REX_B);
				if (mem.index.has_value())
					rex.reg(RegNum{*mem.index, false}, REX_X);
			}
		}

		// the one place where mode matters
		if (rex.needed && !is_x64)
			throw CompilerError::internal(std::format("x86 encoding: {} needs a REX prefix, which doesn't exist in 32 bit mode", rex.reason));

		// prefixes and opcode -------------

		if (this->operand_size_prefix)
			buf.push_back(0x66);
		if (rex.needed)
			buf.push_back(REX | rex.bits);

		for (uint8_t i = 0; i < this->opcode_len; ++i)
		{
			uint8_t byte = this->opcode[i];
			if (i == this->opcode_len - 1 && this->opcode_reg.has_value())
				byte += this->opcode_reg->num & 7;
			buf.push_back(byte);
		}

		// ModRM, SIB, displacement -------------

		if (this->has_modrm)
		{
			uint8_t reg = this->reg_field.num & 7;
			if (const RegNum *rm_reg = std::get_if<RegNum>(&this->rm))
			{
				// mod 11, the rm field is a register rather than memory
				buf.push_back(uint8_t(0b11 << 6 | reg << 3 | (rm_reg->num & 7)));
			}
			else
			{
				const MemRef &mem = std::get<MemRef>(this->rm);
				uint8_t base = mem.base & 7;

				// mod decides the displacement size. mod 00 with a base of 101 (bp/r13) means there's no base at all
				// (absolute or rip-relative addressing), so those always take a displacement, even if it's 0
				uint8_t mod;
				if (mem.disp == 0 && base != 0b101)
					mod = 0b00;
				else if (mem.disp >= INT8_MIN && mem.disp <= INT8_MAX)
					mod = 0b01;
				else
					mod = 0b10;

				// an rm of 100 (sp/r12) means a SIB byte follows, which is also the only way to have an index
				if (mem.index.has_value() || base == 0b100)
				{
					buf.push_back(uint8_t(mod << 6 | reg << 3 | 0b100));
					// an index of 100 means none
					uint8_t index = mem.index.has_value() ? (*mem.index & 7) : 0b100;
					buf.push_back(uint8_t(mem.scale_bits << 6 | index << 3 | base));
				}
				else
				{
					buf.push_back(uint8_t(mod << 6 | reg << 3 | base));
				}

				if (mod == 0b01)
					encode_int(buf, mem.disp, 1);
				else if (mod == 0b10)
					encode_int(buf, mem.disp, 4);
			}
		}

		// immediate -------------

		encode_int(buf, this->imm, this->imm_size);
	}
}
