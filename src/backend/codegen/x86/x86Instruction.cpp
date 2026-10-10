#include "x86Instruction.hpp"

#include <format>
#include <unordered_map>
#include <vector>

#include "utils/error.hpp"

/*
namespace codegen::x86
{
	/// @brief Operator Encoding: where the operands of an instruction go in its machine code. From the "Op/En" column
	/// of the instruction tables in the Intel manual
	enum class OpEn
	{
		/// ModRM byte has operand 1 in rm and operand 2 in reg
		MR,
		/// ModRM byte has operand 2 in rm and operand 1 in reg
		RM,
		/// ModRM byte has operand 1 in rm and the opcode extension in reg, immediate byte(s) at end of instruction
		MI,
		/// No ModRM byte, the register's number is added to the opcode ("+r"), then the immediate
		// OI,
	};

	enum class OpSize
	{
		Byte,
		Word,
		DWord,
		QWord,
	};

	/// @brief One way of encoding an instruction, ie one row of its table in the Intel manual
	struct InstrForm
	{
		OpEn layout;
		/// Opcode when operating on single bytes
		uint8_t opcode_byte;
		/// Opcode when operating on 16 or 32 bits (they share one, 64 bit operands add a prefix)
		uint8_t opcode;
		/// Opcode extension (stored in reg of ModRM byte), for the layouts that have one
		uint8_t extension = 0;
	};

	/// Every valid instruction form (that i've implemented so far).
	/// An instruction uses the first form its operands fit, so when two would work the shorter one goes first
	static const std::unordered_map<Mnemonic, std::vector<InstrForm>> FORMS = {
		{Mnemonic::MOV, {

							// MOV r/m, r (88 /r, 89 /r)
							// {OpEn::MR, 0x88, 0x89},
							// MOV r, r/m (8A /r, 8B /r)
							// {OpEn::RM, 0x8A, 0x8B},
							// MOV r, imm (B0+r, B8+r)
							// {OpEn::OI, 0xB0, 0xB8},
							// MOV r/m, imm (C6 /0, C7 /0)
							// {OpEn::MI, 0xC6, 0xC7, 0},
						}},
		{Mnemonic::MOVSX, {
							  // MOVSX r, r/m8 (0F BE /r)
							  // {OpEn::RM, 0x0, 0x0F, 0xBE},
						  }},
		{Mnemonic::MOVZX, {
							  // MOVZX r, r/m8 (0F B6 /r)
							  // {OpEn::RM, 0x0, 0x0F, 0xB6},
						  }}};

	/// @brief Whether a list of operands can be encoded with a layout
	static bool fits(OpEn layout, const std::vector<Operand> &ops)
	{
		// an r/m operand is anything but an immediate
		switch (layout)
		{
		case OpEn::MR:
			return ops.size() == 2 && !ops[0].is_imm() && ops[1].is_reg();
		case OpEn::RM:
			return ops.size() == 2 && ops[0].is_reg() && !ops[1].is_imm();
		case OpEn::MI:
			return ops.size() == 2 && !ops[0].is_imm() && ops[1].is_imm();
		case OpEn::OI:
			return ops.size() == 2 && ops[0].is_reg() && ops[1].is_imm();
		}
		throw CompilerError::internal("Uncaught OpEn variant");
	}

	/// @brief Number a register is encoded as
	static uint8_t encoding_of(Register reg)
	{
		// r8-r15 need their top bit put in a REX prefix
		if (uint8_t(reg) >= 8)
			throw CompilerError::unimplemented("x86 encoding: registers r8-r15");
		return uint8_t(reg);
	}

	/// @brief Number a register operand is encoded as, which for single bytes depends on which byte it is
	static uint8_t encoding_of(const RegisterOperand &op)
	{
		uint8_t reg = encoding_of(op.reg);
		if (op.access != RegisterAccess::LByte && op.access != RegisterAccess::HByte)
			return reg;

		// only a, c, d and b have bytes that can be accessed. 0-3 are their low bytes (al, cl, dl, bl) and
		// 4-7 their high ones (ah, ch, dh, bh), taking the numbers of sp, bp, si and di
		// TODO x64 can access the low byte of every register, using a REX prefix
		if (reg > uint8_t(Register::RBX))
			throw CompilerError::internal("x86 encoding: register has no byte sized parts");
		return op.access == RegisterAccess::HByte ? reg + 4 : reg;
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

	/// @brief Size (in bytes) of what an instruction operates on. Its register and memory operands all have
	/// to agree on it, and immediates take the same size
	static unsigned int operand_size(const std::vector<Operand> &ops)
	{
		unsigned int size = 0;
		for (const Operand &op : ops)
		{
			unsigned int op_size;
			if (op.is_reg())
				op_size = size_of(op.reg().access);
			else if (op.is_mem())
				op_size = op.mem().size;
			else
				continue;

			if (size != 0 && op_size != size)
				throw CompilerError::internal(std::format("x86 encoding: operands differ in size ({} and {} bytes)", size, op_size));
			size = op_size;
		}

		switch (size)
		{
		case 1:
		case 2:
		case 4:
			return size;
		case 8:
			// TODO needs a REX prefix
			throw CompilerError::unimplemented("x86 encoding: 64 bit operands");
		default:
			throw CompilerError::internal(std::format("x86 encoding: operand size of {} bytes", size));
		}
	}

	/// @brief Writes a value in little endian
	/// @param size Number of bytes to write
	static void encode_int(std::vector<uint8_t> &buf, uint64_t value, unsigned int size)
	{
		for (unsigned int i = 0; i < size; ++i)
			buf.push_back(uint8_t(value >> (8 * i)));
	}

	/// @brief Writes an immediate operand
	/// @param size Number of bytes the immediate takes up
	static void encode_imm(std::vector<uint8_t> &buf, const ImmediateOperand &imm, unsigned int size)
	{
		// signed and unsigned values are both fine, as long as they fit
		int64_t min = -(int64_t(1) << (8 * size - 1));
		int64_t max = (int64_t(1) << (8 * size)) - 1;
		if (imm.value < min || imm.value > max)
			throw CompilerError::internal(std::format("x86 encoding: immediate {} does not fit in {} bytes", imm.value, size));
		encode_int(buf, uint64_t(imm.value), size);
	}

	/// @brief Writes the ModRM byte, plus the SIB byte and displacement of a memory operand if applicable
	/// @param reg_field Value of the reg field: the number of the instruction's register operand, or its
	/// opcode extension
	/// @param rm The r/m operand, a register or memory
	static void encode_modrm(std::vector<uint8_t> &buf, uint8_t reg_field, const Operand &rm)
	{
		if (rm.is_reg())
		{
			// mod 11, the rm field is a register rather than memory
			buf.push_back(uint8_t(0b11 << 6 | reg_field << 3 | encoding_of(rm.reg())));
			return;
		}
		else if (rm.is_mem())
		{
			const MemoryOperand &mem_op = rm.mem();

			// TODO indexed addressing
			if (mem_op.index_reg.has_value())
				throw CompilerError::unimplemented("x86 encoding: indexed addressing");

			// TODO absolute addressing
			if (!mem_op.base_reg.has_value())
				throw CompilerError::unimplemented("x86 encoding: absolute addressing");
			uint8_t base_reg = encoding_of(mem_op.base_reg.value());

			// mod decides the displacement (aka offset) size
			uint8_t mod;
			if (mem_op.offset == 0 && mem_op.base_reg != Register::RBP)
				// special case, mod=00 and r/m==BP means absolute addressing
				mod = 0b00; // zero bytes, no offset
			else if (mem_op.offset >= INT8_MIN && mem_op.offset <= INT8_MAX)
				mod = 0b01; // one byte offset
			else
				mod = 0b10; // four byte offset

			// build ModRM byte
			buf.push_back(mod << 6 | reg_field << 3 | base_reg);

			// sp's number in the rm field means that a SIB byte follows instead, so it has to be the base of one
			// (with no index)
			if (mem_op.base_reg == Register::RSP)
				buf.push_back(0x24);

			if (mod == 0b01)
				encode_int(buf, uint64_t(mem_op.offset), 1);
			else if (mod == 0b10)
				encode_int(buf, uint64_t(mem_op.offset), 4);
		}
		else
			throw CompilerError::internal("x86 codegen: encode_modrm() uncaught operand type");
	}

	void Instruction::encode(std::vector<uint8_t> &buf)
	{
		const InstrForm *form = nullptr;
		if (const auto &mnemonic_forms = FORMS.find(this->mnemonic); mnemonic_forms != FORMS.end())
		{
			for (const InstrForm &candidate : mnemonic_forms->second)
			{
				if (fits(candidate.layout, this->operands))
				{
					form = &candidate;
					break;
				}
			}
		}
		if (form == nullptr)
			throw CompilerError::unimplemented(std::format("x86 encoding: no encoding of instruction {} for these operands", static_cast<int>(this->mnemonic)));

		// 16 bit operands use the 32 bit opcode, behind the operand size prefix
		unsigned int size = operand_size(this->operands);
		if (size == 2)
			buf.push_back(0x66);
		uint8_t opcode = size == 1 ? form->opcode_byte : form->opcode;

		switch (form->layout)
		{
		case OpEn::MR:
			buf.push_back(opcode);
			encode_modrm(buf, encoding_of(this->operands[1].reg().reg), this->operands[0]);
			break;
		case OpEn::RM:
			buf.push_back(opcode);
			encode_modrm(buf, encoding_of(this->operands[0].reg().reg), this->operands[1]);
			break;
		case OpEn::MI:
			buf.push_back(opcode);
			encode_modrm(buf, form->extension, this->operands[0]);
			encode_imm(buf, this->operands[1].imm(), size);
			break;
		case OpEn::OI:
			buf.push_back(uint8_t(opcode + encoding_of(this->operands[0].reg().reg)));
			encode_imm(buf, this->operands[1].imm(), size);
			break;
		}
	}
	void write_mov(std::vector<uint8_t> &buf, operands::rm8 dest, operands::r8 src)
	{
	}
}
*/