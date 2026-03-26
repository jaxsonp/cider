#pragma once

#include <set>
#include <stdint.h>
#include <array>
#include <utility>
#include <unordered_map>
#include <vector>
#include <variant>
#include <string>
#include <string_view>

#include "backend/Backend.hpp"

namespace backends::rv32
{

	/// @brief RISC-V 32bit backend
	///
	/// Stack layout:
	/// ```
	/// | vregs... | saved regs... | saved fp | saved ra |
	/// ^ sp                                             ^ fp
	/// <-- lower addresses      higher addresses -->
	/// <-- stack grows this way
	/// ```
	/// vregs will be at fp - (4 * (spill number + 2))
	///
	/// Register allocation strategy:
	/// Local allocation - Per basic-block, assign and track vregs in registers, spill to stack as needed or at end of bb.
	/// Currently only uses caller saved registers first.
	class Backend_RV32 : public Backend
	{
		/// @brief Register slot, for use for register allocation
		struct RegSlot
		{
			/// Physical register
			const uint8_t physical;
			/// Human name
			const char *name;

			/// ID of the current virtual register living here (if there is one)
			ir::VRegId resident;
			/// Whether there is a virtual register loaded in this register
			bool occupied = false;
			/// Whether the virtual register here has been written to
			bool dirty = false;

			RegSlot(uint8_t physical, const char *name)
				: physical(physical), name(name) {}
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
			/*enum class ReplaceProtocol
			{
				/// No replacement needed
				None,
				/// 12 low bits of immediate
				ITypeImm,
				/// 12 low bits of immediate
				STypeImm,
				/// 13 low bits of immediate (scrambled as hell)
				BTypeImm,
				/// 20 upper bits of immediate
				UTypeImm,
				/// 21 low bits of immediate (also scrambled as hell)
				JTypeImm,
			};*/

			uint32_t data;
			InstructionFormat fmt;

			/// Symbol that needs replacement
			std::string replace_label = "";
		};

		/// @brief Buffer for emission functions to write to
		class CodeBuffer
		{
			std::vector<MachineInstruction> machine_code_buf;
			// std::string assembly_buf;
			std::unordered_map<std::string, size_t> labels;

			size_t cur_offset() const { return this->machine_code_buf.size(); }

		public:
			const std::vector<MachineInstruction> &get_machine_code() const { return this->machine_code_buf; }

			void write_label(std::string_view symbol);

			/// add immediate
			void write_addi(uint8_t dest, uint8_t src, std::variant<uint32_t, std::string_view> imm);

			/// load word
			void write_lw(uint8_t dest, uint8_t addr, uint32_t addr_offset);

			/// store word
			void write_sw(uint8_t addr, uint32_t addr_offset, uint8_t src);

			/// jump and link
			void write_jal(uint8_t dest, std::variant<uint32_t, std::string_view> imm);

			/// Copy the contents of this buffer as bytes into a byte vector
			void dump_to_bytes(std::vector<uint8_t> &bytes) const;
		};

		/// Required space in the stack for this frame. Starts at 8 for return address and saved frame ptr.
		int32_t stack_size;

		/// Register assignment states, in order of priority (heuristic = caller saved first (is this good? idk))
		std::vector<RegSlot> registers = {
			RegSlot(5, "t0"),
			RegSlot(6, "t1"),
			RegSlot(7, "t2"),
			RegSlot(28, "t3"),
			RegSlot(29, "t4"),
			RegSlot(30, "t5"),
			RegSlot(31, "t6"),
			RegSlot(10, "a0"),
			RegSlot(11, "a1"),
			RegSlot(12, "a2"),
			RegSlot(13, "a3"),
			RegSlot(14, "a4"),
			RegSlot(15, "a5"),
			RegSlot(16, "a6"),
			RegSlot(17, "a7"),
			// TODO use s registers
		};

		/// Map of vregs that have been spilled to the offset from frame pointer that they now live (should be negative)
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

		void lower_function(ir::Function *fn, Object *obj);

	public:
		virtual Object *lower_ir(IrObject *ir) override;
	};

	/// @brief Builds I-type instruction
	/// ```
	/// 0-6   | opcode
	/// 7-11  | rd
	/// 12-14 | funct3
	/// 15-19 | rs1
	/// 20-31 | imm bits 0-11
	/// ```
	uint32_t encode_i_type(uint32_t opcode, uint32_t rd, uint32_t funct3, uint32_t rs1, uint32_t imm);

	/// @brief Builds S-type instruction
	/// ```
	/// 0-6   | opcode
	/// 7-11  | imm bits 0-4
	/// 12-14 | funct3
	/// 15-19 | rs1
	/// 20-24 | rs2
	/// 25-31 | imm bits 5-11
	/// ```
	uint32_t encode_s_type(uint32_t opcode, uint32_t funct3, uint32_t rs1, uint32_t rs2, uint32_t imm);

	/// @brief Builds J-type instruction
	/// ```
	/// 0-6   | opcode
	/// 7-11  | rd
	/// 12-19 | imm bits 12-19
	/// 20    | imm bit 11
	/// 21-30 | imm bits 1-10
	/// 31    | imm bit 20
	/// ```
	uint32_t encode_j_type(uint32_t opcode, uint32_t rd, uint32_t imm);

	/// @brief
	/// TODO
	// uint32_t encode_b_type(uint32_t opcode, uint32_t funct3, uint32_t rs1, uint32_t rs2, int32_t imm);

}