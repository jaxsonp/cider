#include "x86Instruction.hpp"

#include <format>
#include <unordered_map>
#include <vector>

#include "utils/error.hpp"

/******************************************************************************

How an x86 instruction is laid out in machine code (everything in brackets is optional):

  [prefixes] opcode [ModRM] [SIB] [displacement] [immediate]

ModRM is the byte that says what the operands are. Its fields are:

  MOD (2 bits) | reg (3 bits) | rm (3 bits)

- MOD: Describes what the r/m operand of the instruction is. It's values can be:
	- 00: memory access, at `[rm]`
	- 01: memory access, at `[rm + offset]` where offset is 1 byte in [displacement]
	- 10: memory access, at `[rm + offset]` where offset is 4 bytes in [displacement]
	- 11: register, specified by `rm`
  There is a special case, where if mod != 11 and rm == esp, this indicates the SIB byte is present
  and it is used for memory accesses, rather than rm. Offset in [displacement] still applies
- rm: See MOD notes
- reg: is the other, not r/m operand, which is always a register. Instructions without
  a second register (ie when it's an immediate instead) have no use for the field,
  so it holds more opcode bits: the opcode extension, written "/digit" in the Intel
  manual ("/r" is a normal ModRM, with a register in reg)

SIB byte is the secondary indexing byte, used by some ops to describe non-static memory
indexing/offsetting, eg `[base + index * scale]` (last offset is described in
[displacement]). Fields are:

  scale (2 bits) | index (3 bits) | base (3 bits)

- scale: scaling factor (1, 2, 4, 8)
- index: register holding index value
- base: register holding the base address

******************************************************************************/

namespace codegen::x86
{
	/// @brief Where the operands of an instruction go in its machine code. Named after the "Op/En" column of
	/// the instruction tables in the Intel manual
	enum class Layout
	{
		/// `op r/m, reg`: ModRM with operand 1 in rm and operand 2 in reg
		MR,
		/// `op reg, r/m`: ModRM with operand 1 in reg and operand 2 in rm
		RM,
		/// `op r/m, imm`: ModRM with operand 1 in rm and the opcode extension in reg, then the immediate
		MI,
		/// `op reg, imm`: no ModRM, the register's number is added to the opcode ("+r"), then the immediate
		OI,
	};

	/// @brief One way of encoding an instruction, ie one row of its table in the Intel manual
	struct InstrForm
	{
		Layout layout;
		/// Opcode when operating on single bytes
		uint8_t opcode_byte;
		/// Opcode when operating on 16 or 32 bits (they share one, 64 bit operands add a prefix)
		uint8_t opcode;
		/// Opcode extension, for the layouts that have one
		uint8_t extension = 0;
	};

	/// Every valid instruction form (that i've implemented so far).
	/// An instruction uses the first form its operands fit, so when two would work the shorter one goes first
	static const std::unordered_map<Mnemonic, std::vector<InstrForm>> FORMS = {
		{Mnemonic::MOV, {
							// MOV r/m, r (88 /r, 89 /r)
							{Layout::MR, 0x88, 0x89},
							// MOV r, r/m (8A /r, 8B /r)
							{Layout::RM, 0x8A, 0x8B},
							// MOV r, imm (B0+r, B8+r)
							{Layout::OI, 0xB0, 0xB8},
							// MOV r/m, imm (C6 /0, C7 /0)
							{Layout::MI, 0xC6, 0xC7, 0},
						}},
		{Mnemonic::MOVSX, {
							  // MOVSX r, r/m8 (0F BE /r)
							  {Layout::RM, 0x0, 0x0F, 0xBE},
						  }},
		{Mnemonic::MOVZX, {
							  // MOVZX r, r/m8 (0F B6 /r)
							  {Layout::RM, 0x0, 0x0F, 0xB6},
						  }}};

	static bool is_reg(const Operand &op) { return std::holds_alternative<RegisterOperand>(op); }
	static bool is_imm(const Operand &op) { return std::holds_alternative<ImmediateOperand>(op); }

	/// @brief Whether a list of operands can be encoded with a layout
	static bool fits(Layout layout, const std::vector<Operand> &ops)
	{
		// an r/m operand is anything but an immediate
		switch (layout)
		{
		case Layout::MR:
			return ops.size() == 2 && !is_imm(ops[0]) && is_reg(ops[1]);
		case Layout::RM:
			return ops.size() == 2 && is_reg(ops[0]) && !is_imm(ops[1]);
		case Layout::MI:
			return ops.size() == 2 && !is_imm(ops[0]) && is_imm(ops[1]);
		case Layout::OI:
			return ops.size() == 2 && is_reg(ops[0]) && is_imm(ops[1]);
		}
		throw CompilerError::internal("Uncaught Layout variant");
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
			if (const RegisterOperand *reg = std::get_if<RegisterOperand>(&op))
				op_size = size_of(reg->access);
			else if (const MemoryOperand *mem = std::get_if<MemoryOperand>(&op))
				op_size = mem->size;
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
		if (const RegisterOperand *reg = std::get_if<RegisterOperand>(&rm))
		{
			// mod 11, the rm field is a register rather than memory
			buf.push_back(uint8_t(0b11 << 6 | reg_field << 3 | encoding_of(*reg)));
			return;
		}
		else if (const MemoryOperand *mem = std::get_if<MemoryOperand>(&rm))
		{
			// TODO indexed addressing
			if (mem->index_reg.has_value())
				throw CompilerError::unimplemented("x86 encoding: indexed addressing");

			// TODO absolute addressing
			if (!mem->base_reg.has_value())
				throw CompilerError::unimplemented("x86 encoding: absolute addressing");
			uint8_t base_reg = encoding_of(mem->base_reg.value());

			// mod decides the displacement (aka offset) size
			uint8_t mod;
			if (mem->offset == 0 && mem->base_reg != Register::RBP)
				// special case, mod=00 and r/m==BP means absolute addressing
				mod = 0b00; // zero bytes, no offset
			else if (mem->offset >= INT8_MIN && mem->offset <= INT8_MAX)
				mod = 0b01; // one byte offset
			else
				mod = 0b10; // four byte offset

			// build ModRM byte
			buf.push_back(mod << 6 | reg_field << 3 | base_reg);

			// sp's number in the rm field means that a SIB byte follows instead, so it has to be the base of one
			// (with no index)
			if (mem->base_reg == Register::RSP)
				buf.push_back(0x24);

			if (mod == 0b01)
				encode_int(buf, uint64_t(mem->offset), 1);
			else if (mod == 0b10)
				encode_int(buf, uint64_t(mem->offset), 4);
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
		case Layout::MR:
			buf.push_back(opcode);
			encode_modrm(buf, encoding_of(std::get<RegisterOperand>(this->operands[1])), this->operands[0]);
			break;
		case Layout::RM:
			buf.push_back(opcode);
			encode_modrm(buf, encoding_of(std::get<RegisterOperand>(this->operands[0])), this->operands[1]);
			break;
		case Layout::MI:
			buf.push_back(opcode);
			encode_modrm(buf, form->extension, this->operands[0]);
			encode_imm(buf, std::get<ImmediateOperand>(this->operands[1]), size);
			break;
		case Layout::OI:
			buf.push_back(uint8_t(opcode + encoding_of(std::get<RegisterOperand>(this->operands[0]))));
			encode_imm(buf, std::get<ImmediateOperand>(this->operands[1]), size);
			break;
		}
	}
}
