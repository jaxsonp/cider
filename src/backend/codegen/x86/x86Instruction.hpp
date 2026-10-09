#pragma once

#include <stdint.h>
#include <vector>
#include <variant>
#include <optional>

#include "backend/codegen/x86/x86.hpp"

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

	enum class Mnemonic : uint8_t
	{
		MOV,
		MOVSX, // Move with sign extension
		MOVZX, // Move with zero extension
			   // ... and plenty more, TODO
	};

	struct RegisterOperand
	{
		Register reg;
		RegisterAccess access;
	};

	struct ImmediateOperand
	{
		int64_t value;
	};

	struct MemoryOperand
	{
		/// Size (in bytes) of the value in memory
		unsigned int size;
		std::optional<Register> base_reg = std::nullopt;
		std::optional<Register> index_reg = std::nullopt;
		uint32_t index_scale = 0; // TODO investigate can this type be smaller?
		int32_t offset = 0;
	};

	class Operand
	{
		/// @brief Operand variant
		std::variant<RegisterOperand, ImmediateOperand, MemoryOperand> op;

	public:
		Operand(ImmediateOperand imm_op) : op(imm_op) {}
		Operand(MemoryOperand mem_op) : op(mem_op) {}
		Operand(RegisterOperand reg_op) : op(reg_op) {}

		/// @brief Checks if this operand is an immediate operand
		bool is_imm() const { return std::holds_alternative<ImmediateOperand>(this->op); }
		/// @brief Checks if this operand is a memory access operand
		bool is_mem() const { return std::holds_alternative<MemoryOperand>(this->op); }
		/// @brief Checks if this operand is a register operand
		bool is_reg() const { return std::holds_alternative<RegisterOperand>(this->op); }

		/// @brief Checks if this operand is an immediate operand, returning a pointer to it if so
		std::optional<ImmediateOperand *> get_imm() { return this->is_reg() ? std::make_optional(std::get_if<ImmediateOperand>(&this->op)) : std::nullopt; }
		/// @brief Checks if this operand is a memory access operand, returning a pointer to it if so
		std::optional<MemoryOperand *> get_mem() { return this->is_reg() ? std::make_optional(std::get_if<MemoryOperand>(&this->op)) : std::nullopt; }
		/// @brief Checks if this operand is a register operand, returning a pointer to it if so
		std::optional<RegisterOperand *> get_reg() { return this->is_reg() ? std::make_optional(std::get_if<RegisterOperand>(&this->op)) : std::nullopt; }

		/// @brief Returns the underlying immediate operand. CALLER MUST CHECK/KNOW THIS IS AN IMMEDIATE OPERAND
		ImmediateOperand &imm() { return std::get<ImmediateOperand>(this->op); }
		/// @brief Returns the underlying immediate operand. CALLER MUST CHECK/KNOW THIS IS AN IMMEDIATE OPERAND
		const ImmediateOperand &imm() const { return std::get<ImmediateOperand>(this->op); }
		/// @brief Returns the underlying memory operand. CALLER MUST CHECK/KNOW THIS IS A MEMORY ACCESS OPERAND
		MemoryOperand &mem() { return std::get<MemoryOperand>(this->op); }
		/// @brief Returns the underlying memory operand. CALLER MUST CHECK/KNOW THIS IS A MEMORY ACCESS OPERAND
		const MemoryOperand &mem() const { return std::get<MemoryOperand>(this->op); }
		/// @brief Returns the underlying register operand. CALLER MUST CHECK/KNOW THIS IS A REGISTER OPERAND
		RegisterOperand &reg() { return std::get<RegisterOperand>(this->op); }
		/// @brief Returns the underlying register operand. CALLER MUST CHECK/KNOW THIS IS A REGISTER OPERAND
		const RegisterOperand &reg() const { return std::get<RegisterOperand>(this->op); }
	};

	/// @brief A single machine instruction before it's encoded
	struct Instruction
	{
		Mnemonic mnemonic;
		std::vector<Operand> operands;

		void encode(std::vector<uint8_t> &buf);
	};
}