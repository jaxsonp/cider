#pragma once

#include <stdint.h>
#include <array>
#include <unordered_map>
#include <string>
#include <string_view>

#include "backend/Backend.hpp"

namespace backends::rv32
{
	enum class Register : uint8_t
	{
		zero = 0,
		ra = 1,
		sp = 2,
		gp = 3,
		tp = 4,
		t0 = 5,
		t1 = 6,
		t2 = 7,
		fp = 8, // aka s0
		s1 = 9,
		a0 = 10,
		a1 = 11,
		a2 = 12,
		a3 = 13,
		a4 = 14,
		a5 = 15,
		a6 = 16,
		a7 = 17,
		s2 = 18,
		s3 = 19,
		s4 = 20,
		s5 = 21,
		s6 = 22,
		s7 = 23,
		s8 = 24,
		s9 = 25,
		s10 = 26,
		s11 = 27,
		t3 = 28,
		t4 = 29,
		t5 = 30,
		t6 = 31,
	};

	/// @brief RISC-V 32bit backend
	///
	/// Stack layout:
	/// ```
	/// | vregs... | saved fp | saved ra |
	/// ^ sp                             ^ fp
	/// <-- lower addresses      higher addresses -->
	/// <-- stack grows this way
	/// ```
	/// vregs will be at fp - (4 * (spill number + 3))
	///
	/// Register allocation strategy:
	/// Local allocation - Per basic-block, assign and track vregs in registers, spill to stack as needed or at end of bb.
	/// Currently only uses caller saved registers
	class Backend_RV32 : public Backend
	{
		/// @brief Register slot, for use for register allocation
		struct RegSlot
		{
			/// Physical register
			const Register physical;

			/// ID of the current virtual register living here (if there is one)
			ir::VRegId resident;
			/// Whether there is a virtual register loaded in this register
			bool occupied = false;
			/// Whether the virtual register here has been written to
			bool dirty = false;

			RegSlot(Register reg)
				: physical(reg) {}
		};

		enum class InstructionFormat : uint8_t
		{
			/// register-register operations
			RType,
			/// register-immediate operations
			IType,
			/// load and store
			SType,
			/// branch instructions
			BType,
			/// upper immediate operators
			UType,
			/// jump instructions
			JType,
		};

		struct MachineInstruction
		{
			uint32_t data;
			InstructionFormat fmt;
		};

		/// @brief Buffer for emission functions to write to
		struct CodeBuffer;

		/// Required space in the stack for this frame. Starts at 8 for return address and saved frame ptr.
		int32_t stack_size;

		/// Register assignment states, in order of priority (heuristic = caller saved first (is this good? idk))
		std::array<RegSlot, 15> registers = {
			RegSlot(Register::t0),
			RegSlot(Register::t1),
			RegSlot(Register::t2),
			RegSlot(Register::t3),
			RegSlot(Register::t4),
			RegSlot(Register::t5),
			RegSlot(Register::t6),
			RegSlot(Register::a0),
			RegSlot(Register::a1),
			RegSlot(Register::a2),
			RegSlot(Register::a3),
			RegSlot(Register::a4),
			RegSlot(Register::a5),
			RegSlot(Register::a6),
			RegSlot(Register::a7),
			// TODO use s registers
		};

		/// Map of: spilt vregs -> offset from frame pointer that they now live (should be negative)
		std::unordered_map<ir::VRegId, int32_t> spilled_vreg_fp_offsets;

		/// @brief Gets a physical register loaded with the value of a virtual register
		/// @param vreg ID of vreg to put into a register
		/// @return The physical register containing vreg
		RegSlot *load_src_vreg(CodeBuffer &code, ir::VRegId vreg);

		/// @brief Gets a physical register to use as a destination of an operation, and marks it as dirty
		/// @param vreg ID of vreg being written to
		/// @return The destination register slot
		RegSlot *load_dest_vreg(CodeBuffer &code, ir::VRegId vreg);

		// round robin register spilling
		size_t next_to_spill = 0;

		/// @brief Spills a register slot's value back onto the stack, marking slot as not dirty
		void spill_slot(CodeBuffer &code, RegSlot &slot);

		/// @brief Returns an unoccupied register slot by either finding already unoccupied slots, overwriting
		/// non-dirty occupied slots, or spilling dirty slots in a round robin fashion (TODO improve this)
		/// @return Pointer to the now vacant slot
		RegSlot *get_empty_slot(CodeBuffer &code);

		void lower_function(const ir::Function &fn, Object &obj);

	public:
		Object lower_ir(const IrObject &ir) override;
	};

	/// @brief Builds I-type instruction
	/// ```
	/// 0-6   | opcode
	/// 7-11  | rd
	/// 12-14 | funct3
	/// 15-19 | rs1
	/// 20-31 | imm bits 0-11
	/// ```
	uint32_t encode_i_type(uint32_t opcode, Register rd, uint32_t funct3, Register rs1, uint32_t imm);

	/// @brief Builds S-type instruction
	/// ```
	/// 0-6   | opcode
	/// 7-11  | imm bits 0-4
	/// 12-14 | funct3
	/// 15-19 | rs1
	/// 20-24 | rs2
	/// 25-31 | imm bits 5-11
	/// ```
	uint32_t encode_s_type(uint32_t opcode, uint32_t funct3, Register rs1, Register rs2, uint32_t imm);

	/// @brief Builds J-type instruction
	/// ```
	/// 0-6   | opcode
	/// 7-11  | rd
	/// 12-19 | imm bits 12-19
	/// 20    | imm bit 11
	/// 21-30 | imm bits 1-10
	/// 31    | imm bit 20
	/// ```
	uint32_t encode_j_type(uint32_t opcode, Register rd, uint32_t imm);

	// TODO
	// uint32_t encode_b_type(uint32_t opcode, uint32_t funct3, uint32_t rs1, uint32_t rs2, int32_t imm);

}