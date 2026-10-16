#include "x86Assembler.hpp"

#include "utils/error.hpp"

namespace codegen::x86
{
	/// @brief Starts an encoding with the prefixes for an operand size
	static detail::Encoding sized(Width size, std::initializer_list<uint8_t> opcode)
	{
		if (opcode.size() > 3 || opcode.size() == 0)
			throw CompilerError::internal("x86 encoding: opcodes are 1 to 3 bytes");

		detail::Encoding enc;
		enc.operand_size_prefix = size == Width::W16;
		enc.rex_w = size == Width::W64;
		for (uint8_t byte : opcode)
			enc.opcode[enc.opcode_len++] = byte;
		return enc;
	}

	Assembler::Assembler(bool is_x64, bool record_text) : is_x64(is_x64), record_text(record_text) {}

	void Assembler::clear()
	{
		this->buf.clear();
		this->lines.clear();
	}

	void Assembler::dump_asm(std::ostream &out) const
	{
		for (const std::string &line : this->lines)
			out << '\t' << line << '\n';
	}

	void Assembler::dump_to_bytes(std::vector<uint8_t> &bytes) const
	{
		bytes.insert(bytes.end(), this->buf.begin(), this->buf.end());
	}

	void Assembler::patch_call(std::vector<uint8_t> &code, size_t call_offset, size_t target_offset)
	{
		if (code.at(call_offset) != 0xE8)
			throw CompilerError::internal("x86 codegen: patched call is not a call rel32");

		// relative to the end of the call, which is 5 bytes long
		int64_t rel = int64_t(target_offset) - int64_t(call_offset + 5);
		if (rel != int64_t(int32_t(rel)))
			throw CompilerError::unimplemented("x86 codegen: call targets more than 2GiB away");
		for (size_t i = 0; i < 4; ++i)
			code.at(call_offset + 1 + i) = uint8_t(uint32_t(rel) >> (8 * i));
	}

	size_t Assembler::emit(const detail::Encoding &enc, std::string text)
	{
		size_t pos = this->buf.size();
		enc.encode(this->buf, this->is_x64);
		if (this->record_text)
			this->lines.push_back(std::move(text));
		return pos;
	}

	size_t Assembler::emit_modrm(std::string text, Width size, std::initializer_list<uint8_t> opcode, detail::RegNum reg_field,
								 const detail::RmOperand &rm, int64_t imm, uint8_t imm_size)
	{
		detail::Encoding enc = sized(size, opcode);
		enc.has_modrm = true;
		enc.reg_field = reg_field;
		enc.rm = rm;
		enc.imm = imm;
		enc.imm_size = imm_size;
		return this->emit(enc, std::move(text));
	}

	size_t Assembler::emit_modrm_ext(std::string text, Width size, std::initializer_list<uint8_t> opcode, uint8_t ext,
									 const detail::RmOperand &rm, int64_t imm, uint8_t imm_size)
	{
		return this->emit_modrm(std::move(text), size, opcode, detail::RegNum{ext, false}, rm, imm, imm_size);
	}

	size_t Assembler::emit_opcode_reg(std::string text, Width size, uint8_t opcode, detail::RegNum reg, int64_t imm, uint8_t imm_size)
	{
		detail::Encoding enc = sized(size, {opcode});
		enc.opcode_reg = reg;
		enc.imm = imm;
		enc.imm_size = imm_size;
		return this->emit(enc, std::move(text));
	}

	size_t Assembler::emit_plain(std::string text, Width size, std::initializer_list<uint8_t> opcode, int64_t imm, uint8_t imm_size)
	{
		detail::Encoding enc = sized(size, opcode);
		enc.imm = imm;
		enc.imm_size = imm_size;
		return this->emit(enc, std::move(text));
	}

	size_t Assembler::write_mov(Reg64 dst, Imm64 imm)
	{
		return this->emit_opcode_reg(this->asm_text("movabs", dst, imm), Width::W64, 0xB8, detail::to_reg(dst), imm.value, 8);
	}

	size_t Assembler::write_syscall()
	{
		// AMD processors have it in 32 bit mode too, Intel ones don't
		if (!this->is_x64)
			throw CompilerError::internal("x86 encoding: syscall is only available in 64 bit mode");
		return this->emit_plain(this->asm_text("syscall"), Width::W32, {0x0F, 0x05});
	}
}
