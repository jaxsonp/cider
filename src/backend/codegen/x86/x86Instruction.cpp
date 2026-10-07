#include "x86Instruction.hpp"

#include "utils/error.hpp"

namespace codegen::x86
{
	/// @brief Number a register is encoded as in the ModRM byte
	static uint8_t encoding_of(Register reg)
	{
		// r8-r15 need their top bit put in a REX prefix
		if (uint8_t(reg) >= 8)
			throw CompilerError::unimplemented("x86 encoding: registers r8-r15");
		return uint8_t(reg);
	}

	/// @brief Size (in bytes) of the part of a register being accessed
	static unsigned int size_of(RegisterAccess access)
	{
		switch (access)
		{
		case RegisterAccess::LByte:
		case RegisterAccess::HByte:
			return 1;
		case RegisterAccess::Word:
			return 2;
		case RegisterAccess::DWord:
			return 4;
		case RegisterAccess::QWord:
			return 8;
		}
		throw CompilerError::internal("Uncaught RegisterAccess variant");
	}

	/// @brief Writes the ModRM byte addressing a memory operand, plus the SIB byte and displacement it needs
	/// @param reg_field Value of the ModRM reg field, ie the number of the instruction's register operand
	static void encode_modrm(std::vector<uint8_t> &buf, uint8_t reg_field, const MemoryOperand &mem)
	{
		if (mem.index_reg.has_value())
			throw CompilerError::unimplemented("x86 encoding: indexed addressing");
		if (!mem.base_reg.has_value())
			throw CompilerError::unimplemented("x86 encoding: absolute addressing");
		uint8_t base = encoding_of(mem.base_reg.value());

		// mod picks the displacement size. there is no [bp] without one (that encoding means an absolute
		// address instead), so bp always gets at least a zero disp8
		uint8_t mod;
		if (mem.offset == 0 && mem.base_reg != Register::RBP)
			mod = 0b00;
		else if (mem.offset >= INT8_MIN && mem.offset <= INT8_MAX)
			mod = 0b01;
		else
			mod = 0b10;
		buf.push_back(uint8_t(mod << 6 | reg_field << 3 | base));

		// sp's number in the rm field means "a SIB byte follows" instead, so it has to be the base of one
		// (with no index)
		if (mem.base_reg == Register::RSP)
			buf.push_back(0x24);

		// displacements are little endian
		if (mod == 0b01)
			buf.push_back(uint8_t(mem.offset));
		else if (mod == 0b10)
			for (int i = 0; i < 4; ++i)
				buf.push_back(uint8_t(uint32_t(mem.offset) >> (8 * i)));
	}

	void Instruction::encode(std::vector<uint8_t> &buf)
	{
		switch (this->mnemonic)
		{
		case Mnemonic::MOV:
		{
			// only loads (MOV r, m) so far
			const RegisterOperand *dest = this->operands.size() == 2 ? std::get_if<RegisterOperand>(&this->operands[0]) : nullptr;
			const MemoryOperand *src = this->operands.size() == 2 ? std::get_if<MemoryOperand>(&this->operands[1]) : nullptr;
			if (dest == nullptr || src == nullptr)
				throw CompilerError::unimplemented("x86 encoding: MOV other than register <- memory");
			if (src->size != size_of(dest->access))
				throw CompilerError::internal("x86 encoding: MOV operands differ in size");

			uint8_t reg = encoding_of(dest->reg);
			switch (dest->access)
			{
			case RegisterAccess::LByte:
				// without a REX prefix only a, c, d and b have a low byte (4-7 are ah, ch, dh and bh)
				if (reg >= static_cast<uint8_t>(Register::RDX))
					throw CompilerError::internal("x86 encoding: register has no low byte");
				buf.push_back(0x8A);
				break;
			case RegisterAccess::Word:
				// operand size prefix, turning the 32 bit MOV into a 16 bit one
				buf.push_back(0x66);
				buf.push_back(0x8B);
				break;
			case RegisterAccess::DWord:
				buf.push_back(0x8B);
				break;
			default:
				throw CompilerError::unimplemented("x86 encoding: MOV to this part of a register");
			}
			encode_modrm(buf, reg, *src);
			break;
		}
		default:
			throw CompilerError::unimplemented("x86 encoding: this instruction");
		}
	}
}
