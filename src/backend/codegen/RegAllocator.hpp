#pragma once

#include <stdint.h>
#include <stddef.h>
#include <unordered_map>
#include <vector>

#include "ir/IR.hpp"

namespace codegen
{
	/// Architecture independent ID of a physical register. Each backend maps these to and from its own registers
	using PhysReg = uint8_t;

	/// @brief Interface the register allocator uses to move values between registers and the stack. Implemented
	/// by each backend, since the instructions used and the frame layout are architecture specific
	class SpillHandler
	{
	public:
		/// @brief Emits code storing a register into a spill slot
		/// @param src Register holding the value
		/// @param vreg Virtual register being spilled (for looking up its type)
		/// @param spill_index Index of the spill slot, counting from 0 in the order slots were first needed
		virtual void store_spilled_vreg(PhysReg src, ir::VRegId vreg, size_t spill_index) = 0;

		/// @brief Emits code loading a spill slot into a register
		/// @param dest Register to load into
		/// @param vreg Virtual register being loaded (for looking up its type)
		/// @param spill_index Index of the spill slot, counting from 0 in the order slots were first needed
		virtual void load_spilled_vreg(PhysReg dest, ir::VRegId vreg, size_t spill_index) = 0;
	};

	/// @brief Architecture independent register allocator
	///
	/// Register allocation strategy:
	/// Local allocation - Per basic-block, assign and track vregs in registers, spill to stack as needed or at end of bb.
	///
	/// Only decides *where* values live. Emitting the loads and stores this requires is left to the backend,
	/// through `SpillHandler`
	class RegAllocator
	{
	public:
		/// @brief Register slot, for use for register allocation
		struct RegSlot
		{
			/// Physical register
			const PhysReg physical;

			/// ID of the current virtual register living here (if there is one)
			ir::VRegId resident;
			/// Whether there is a virtual register loaded in this register
			bool occupied = false;
			/// Whether the virtual register here has been written to
			bool dirty = false;
			/// Whether the current instruction is using this register, so it can't be evicted until the instruction
			/// is done (otherwise loading one operand could evict another, or the destination)
			bool locked = false;

			RegSlot(PhysReg reg)
				: physical(reg) {}
		};

	private:
		/// Emits the loads and stores
		SpillHandler &spill_handler;

		/// Register assignment states, in order of priority
		std::vector<RegSlot> registers;

		/// Map of: spilt vregs -> index of the spill slot that they now live in
		std::unordered_map<ir::VRegId, size_t> spilled_vreg_indices;

		// round robin register spilling
		size_t next_to_spill = 0;

	public:
		/// @param spill_handler Backend to emit loads and stores through, must outlive the allocator
		/// @param pool Registers to allocate from, in order of priority
		RegAllocator(SpillHandler &spill_handler, const std::vector<PhysReg> &pool);

		/// @brief Forgets everything, call when starting a new function
		void start_function();

		/// @brief Empties every register, call when starting a new basic block
		void start_block();

		/// @brief Unlocks every register, call when starting a new instruction (registers are only locked for the
		/// duration of one instruction)
		void start_instruction();

		/// @brief Returns the register slot for a specific physical register
		RegSlot &slot_for(PhysReg reg);

		/// @brief Gets a physical register loaded with the value of a virtual register
		/// @param vreg ID of vreg to put into a register
		/// @return The physical register containing vreg
		RegSlot *load_src_vreg(ir::VRegId vreg);

		/// @brief Gets a physical register to use as a destination of an operation, and marks it as dirty
		/// @param vreg ID of vreg being written to
		/// @return The destination register slot
		RegSlot *load_dest_vreg(ir::VRegId vreg);

		/// @brief Returns an unoccupied register slot by either finding already unoccupied slots, overwriting
		/// non-dirty occupied slots, or spilling dirty slots in a round robin fashion (TODO improve this)
		/// @return Pointer to the now vacant slot
		RegSlot *get_empty_slot();

		/// @brief Spills a register slot's value back onto the stack, marking slot as not dirty
		void spill_slot(RegSlot &slot);

		/// @brief Spills every dirty register and empties all of them, so every live vreg is on the stack. Needed
		/// before anything that clobbers the registers (ie a call)
		void spill_all();

		/// @brief Declares that a physical register already holds the (unsaved) value of a virtual register, for
		/// values that show up in a fixed register (ie arguments, return values)
		void claim(PhysReg reg, ir::VRegId vreg);

		/// @brief Loads a spilled vreg from its stack slot into a physical register. Does not touch register
		/// allocation state
		void load_spilled_vreg(PhysReg dest, ir::VRegId vreg);

		/// @brief Number of spill slots used so far in this function
		size_t spill_count() const { return this->spilled_vreg_indices.size(); }
	};
}
