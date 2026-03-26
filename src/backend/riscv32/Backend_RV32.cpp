#include "Backend_RV32.hpp"

#include <format>
#include <set>
#include <unordered_map>
#include <stdint.h>
#include <array>
#include <utility>
#include <vector>

#include "utils/logging.hpp"
#include "utils/error.hpp"

/* example prologue
# Example: STACK_SIZE = 32
addi sp, sp, -32      # 1. Allocate stack space (grows down)
sw   ra, 28(sp)       # 2. Save Return Address
sw   s0, 24(sp)       # 3. Save caller's Frame Pointer
addi s0, sp, 32       # 4. Set up new Frame Pointer (pointing to start of frame)

# 5. Save any other callee-saved registers your function uses
sw   s1, 20(sp)
sw   s2, 16(sp)
*/

/* example epilogue
# 1. Restore callee-saved registers
lw   s2, 16(sp)
lw   s1, 20(sp)

# 2. Restore FP and RA
lw   s0, 24(sp)
lw   ra, 28(sp)

# 3. Deallocate stack space
addi sp, sp, 32

# 4. Return to caller
ret                   # Pseudo-op for jalr x0, 0(ra)
*/

namespace backends::rv32
{
	/*enum class InstructionFormat
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
	};*/

	struct MachineInstruction
	{
		uint32_t data;
		std::string asm_text = "";
		// InstructionFormat fmt;
	};

	/// @brief Register slot, for use for register allocation
	struct RegSlot
	{
		/// Physical register
		const uint8_t physical;
		/// Human name
		const char *name;

		// const bool clobbered;

		/// ID of the current virtual register living here (if there is one)
		ir::VRegId resident;
		/// Whether there is a virtual register loaded in this register
		bool occupied = false;
		/// Whether the virtual register here has been written to
		bool dirty = false;
		/// Whether this slot has ever been used (don't unset this)
		bool used = false;

		RegSlot(uint8_t physical, const char *name)
			: physical(physical), name(name) {}
	};

	/// Wraps the logic/state required to lower a function from IR to machine code
	///
	/// Stack layout:
	/// ```
	/// | vregs... | saved regs... | saved fp | saved ra |
	/// ^ sp                                             ^ fp
	/// <-- lower addresses      higher addresses -->
	/// <-- stack grows this way
	/// ```
	/// virtual registers will be at: `sp - 4 * vreg_index`
	///
	/// Register allocation strategy:
	/// Local allocation - Per basic-block, assign and track vregs in registers, spill to stack as needed or at end of bb.
	/// Currently only uses caller saved registers first.
	class FunctionHandler
	{
		ir::Function *fn;

		std::vector<MachineInstruction> prologue;
		std::vector<MachineInstruction> body;
		std::vector<MachineInstruction> epilogue;

		std::string prologue_asm;
		std::string body_asm;
		std::string epilogue_asm;

		/// Required space in the stack for this frame. Starts at 8 for return address and saved frame ptr.
		size_t stack_size = 8;

		/// Register assignment states, in order of priority (heuristic = caller saved first (is this good? idk))
		std::array<RegSlot, 15> registers = {
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

		/// @brief Gets a physical register loaded with the value of a virtual register
		/// @param vreg ID of vreg to put into a register
		/// @return The physical register containing vreg
		RegSlot *load_src_vreg(ir::VRegId vreg)
		{
			// check if this vreg is already loaded somewhere
			for (RegSlot &slot : this->registers)
			{
				if (slot.occupied && slot.resident == vreg)
					return &slot;
			}

			// load it
			RegSlot *reg = this->get_empty_slot();
			this->body_asm += std::format("\tlw   \t{}, {}(sp) # loading vreg {}", reg->name, vreg * -4, vreg);
			reg->resident = vreg;
			reg->occupied = true;
			return reg;
		}

		/// @brief Gets a physical register to use as a destination of an operation, and marks it as dirty
		/// @param vreg ID of vreg being written to
		/// @return The destination register slot
		RegSlot *load_dest_vreg(ir::VRegId vreg)
		{
			RegSlot *reg = this->get_empty_slot();
			reg->resident = vreg;
			reg->occupied = true;
			reg->dirty = true;
			return reg;
		}

		// round robin register spilling
		size_t next_to_spill = 0;

		/// @brief Returns an unoccupied register slot by either finding already unoccupied slots, overwriting
		/// non-dirty occupied slots, or spilling dirty slots in a round robin fashion (TODO improve this)
		/// @return Pointer to the now vacant slot
		RegSlot *get_empty_slot()
		{
			// first check for non-occupied registers
			for (RegSlot &slot : this->registers)
			{
				if (!slot.occupied)
				{
					slot.used = true;
					return &slot;
				}
			}

			// then check for non-dirty slots and evict
			for (RegSlot &slot : this->registers)
			{
				if (!slot.dirty)
				{
					slot.occupied = false;
					slot.used = true;
					return &slot;
				}
			}

			// worst case: spill register
			size_t victim_index = this->next_to_spill;
			this->next_to_spill = (this->next_to_spill + 1) % this->registers.size();

			RegSlot *victim = &this->registers.at(victim_index);

			this->body_asm += std::format("\tsw   \t{}, {}(sp) # storing vreg {}", victim->name, victim->resident * -4, victim->resident);

			victim->occupied = false;
			victim->dirty = true;
			victim->used = true;
			return victim;
		};

	public:
		FunctionHandler(ir::Function *function)
			: fn(function) {}

		void lower_to(Object *obj, Backend_RV32 *backend)
		{
			log_vvv("Lowering function \"{}\"", fn->name);

			// register this function in the object
			obj->functions.emplace_back(fn->name);

			// prefix traversal of body
			std::set<ir::BBlockId> seen;
			std::vector<ir::BasicBlock *> to_visit;
			to_visit.push_back(fn->entry);
			while (!to_visit.empty())
			{
				ir::BasicBlock *bb = to_visit.back();
				to_visit.pop_back();

				this->body_asm += std::format("__bb_{}:{}\n", bb->id, (bb->note.empty() ? "" : (" # " + bb->note)));

				ir::Instruction *cur_instr = bb->start;
				while (cur_instr != nullptr)
				{
					if (ir::instr::LoadImmInstruction *instr = dynamic_cast<ir::instr::LoadImmInstruction *>(cur_instr))
					{
						// checking if immediate can fit in 12 bytes
						if (instr->value >= -2048 && instr->value <= 2047)
						{
							RegSlot *dest = this->load_dest_vreg(instr->dest);
							this->body_asm += std::format("\taddi \t{}, zero, {} # load immediate\n", dest->name, instr->value);
						}
						else
						{
							// TODO
							// LUI stuff
							throw UnimplementedError("Cant do large integer immediates yet");
						}
					}
					else if (ir::instr::ReturnInstruction *instr = dynamic_cast<ir::instr::ReturnInstruction *>(cur_instr))
					{
						if (instr->ret_value.has_value())
						{
							RegSlot *ret_value = this->load_src_vreg(instr->ret_value.value());
							this->body_asm += std::format("\taddi \ta0, {}, 0 # return\n", ret_value->name);
							// TODO proper offset
							this->body_asm += std::format("\tjal  \tzero, <epilogue>\n", ret_value->name);
						}
					}
					else
					{
						throw UnimplementedError("Uncaught instruction variant");
					}

					cur_instr = cur_instr->next;
				}
			}

			// build prologue

			// build epilogue

			// finalize function
			this->write_asm();
		}
	};

	/*void Backend_RV32::lowerFunction(ir::Function *fn, Object *obj)
	{

		// labels IN ORDER
		std::unordered_map<std::string, size_t> local_labels;
		std::unordered_map<ir::VRegId, uint32_t> vreg_offsets;

		// prefix traversal of body
		std::set<ir::BBlockId> seen;
		std::vector<ir::BasicBlock *> to_visit;
		to_visit.push_back(fn->entry);
		while (!to_visit.empty())
		{
			ir::BasicBlock *bb = to_visit.back();
			to_visit.pop_back();

			this->asm_buffer += std::format("__bb_{}:{}\n", bb->id, (bb->note.empty() ? "" : (" # note: " + bb->note)));

			ir::Instruction *cur_instr = bb->start;
			while (cur_instr != nullptr)
			{
				if (ir::instr::LoadImmInstruction *instr = dynamic_cast<ir::instr::LoadImmInstruction *>(cur_instr))
				{
					// checking if immediate can fit in 12 bytes
					if (instr->value >= -2048 && instr->value <= 2047)
					{
						RegSlot *dest = this->load_dest_vreg(instr->dest);
						this->asm_buffer += std::format("\taddi\t{}, zero, {}\n", dest->name, instr->value);
					}
					else
					{
						// TODO
						// LUI stuff
						throw UnimplementedError("Cant do large integer immediates yet");
					}
				}
				else if (ir::instr::ReturnInstruction *instr = dynamic_cast<ir::instr::ReturnInstruction *>(cur_instr))
				{
					if (instr->ret_value.has_value())
					{
						RegSlot *ret_value = this->load_src_vreg(instr->ret_value.value());
						this->asm_buffer += std::format("\taddi\ta0, {}, 0\n", ret_value->name);
						// TODO proper offset
						this->asm_buffer += std::format("\tjal \tzero, <epilogue>\n", ret_value->name);
					}
				}
				else
				{
					throw UnimplementedError("Uncaught instruction variant");
				}

				cur_instr = cur_instr->next;
			}
		}
		std::vector<uint8_t> body_machine_code;

		// emit function prologue ---------
		this->write_asm(std::format("\n\n{0}:\n_{0}_prologue:\n", fn->name));
		// TODO check if stack space overflows 12 bits
		this->write_asm(std::format("\taddi\tsp, sp, {}\n", required_stack_space));

		// emit function body ---------
		this->dump_machine_code_buffer(obj->text);
		this->write_asm(std::format("\n_{}_body:\n", fn->name));
		this->write_asm(this->dump_asm_buffer());

		// emit function epilogue ---------
		this->write_asm(std::format("\n_{}_epilogue:\n", fn->name));
	}*/

	Object *Backend_RV32::lowerIr(IrObject *ir)
	{
		log_vv("Starting lowering to RV32");
		Object *obj = new Object();

		for (auto &[name, fn] : ir->functions)
		{
			FunctionHandler(fn).lower_to(obj, &this);
		}

		return obj;
	}
}
