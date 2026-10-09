#include "x86Assembler.hpp"

#include "utils/logging.hpp"

namespace codegen::x86
{
	Assembler::Assembler(bool is_x64) : is_x64(is_x64) {}

	void Assembler::clear()
	{
		this->buf.clear();
	}

	size_t Assembler::write(Instruction inst)
	{
		size_t pos = this->buf.size();
		inst.encode(this->buf);
		return pos;
	}

	void Assembler::write_mov(Operand dest, Operand src, bool signed_type)
	{
		if (!this->is_x64 && dest.is_reg())
		{
			RegisterOperand &dest_reg = dest.reg();
			if (dest_reg.access == RegisterAccess::LByte && (dest_reg.reg == Register::RDI || dest_reg.reg == Register::RSI))
			{
				// special case, without x64, rdi/rsi regs don't have singe byte accessors so use sign/zero extension
				log("MOV required sign/zero extension!!! please check if it worked...");
				dest_reg.access = RegisterAccess::Word;
				if (src.is_imm())
					this->write(
						x86::Instruction{
							.mnemonic = x86::Mnemonic::MOV,
							.operands = {dest, src}});
				else
					this->write(
						x86::Instruction{
							.mnemonic = signed_type ? Mnemonic::MOVSX : Mnemonic::MOVZX,
							.operands = {dest, src}});
				return;
			}
		}

		this->write(
			x86::Instruction{
				.mnemonic = x86::Mnemonic::MOV,
				.operands = {dest, src}});
	}
}