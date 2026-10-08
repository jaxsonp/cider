#pragma once

#include <stdint.h>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "backend/codegen/CodeGenerator.hpp"
#include "backend/codegen/RegAllocator.hpp"
#include "backend/codegen/x86/x86Assembler.hpp"

namespace codegen
{

	/// @brief x86 backend
	///
	/// Stack layout (relative to reg_width):
	/// ```
	/// | outgoing stack args... | spilled vregs...  | saved bp | return addr | incoming stack args... |
	/// ^ sp                                         ^ bp
	/// <-- lower addresses                                      higher addresses -->
	/// <-- stack grows this way
	/// ```
	/// - spilled vregs will be at fp - (word * (spill number + 3))
	/// - outgoing stack args are the stack-passed arguments (9th onwards) of calls this function makes. The
	///   area is sized for the largest such call, argument i goes at sp + word * (i - 8)
	///
	/// Register allocation is done by `RegAllocator`. Currently only uses caller saved registers
	///
	/// Values in registers:
	/// A value narrower than a register only owns the lower bits of it, the upper bits can be anything (ie
	/// leftovers from an add that overflowed). Instructions whose result depends on those upper bits (division,
	/// right shifts, comparisons) must call `truncate_reg` on their operands first. On RV64, operations on
	/// types of 32 bits or less use the "W" instructions, which only read the lower 32 bits, so that the result
	/// of every operation is the same as on RV32
	///
	/// Calling convention (subset of the standard ILP32/LP64 ABI):
	/// The first 8 arguments are passed in a0-a7, the rest in word sized stack slots at the caller's sp (the callee's fp). The return value comes
	/// back in a0. Narrow integer arguments and return values are sign/zero extended to 32 bits, and on RV64 32 bit
	/// values are then sign extended to 64 bits. Since every
	/// allocatable register is caller saved, all live values are spilled to the stack before a call.
	class X86CodeGenerator : public CodeGenerator, private SpillHandler
	{
		using RegSlot = RegAllocator::RegSlot;

		/// Target being compiled for, decides the register width and which extensions can be used
		const Target target;

		/// Register width 4 for 32bit, 8 for 64bit
		const uint32_t reg_width;

		/// Whether this arch is x86_64, for convenience
		const bool is_x64;

		/// Register allocator state
		RegAllocator regalloc;

		/// Instruction buffer for function body (not prologue/epilogue)
		x86::Assembler body;

		/// Precomputed offset of incoming stack-passed args from the base pointer
		std::vector<uint32_t> stack_args_bp_offsets;
		/// Precomputed size of all arguments on the stack
		uint64_t stack_args_size = 0;

		void
		store_spilled_vreg(PhysReg src, ir::VRegId vreg, size_t spill_index) override;

		/// Sign/zero extends narrow types
		void load_spilled_vreg(PhysReg dest, ir::VRegId vreg, size_t spill_index) override;

	protected:
		void begin_function(const ir::Function &fn) override;
		void begin_block(const ir::BasicBlock &bb) override;
		void begin_instruction() override;
		void lower_immediate_instr(const ir::ImmediateInstruction &instr) override;
		void lower_binary_instr(const ir::BinaryInstruction &instr) override;
		void lower_unary_instr(const ir::UnaryInstruction &instr) override;
		void lower_load_arg_instr(const ir::LoadArgInstruction &instr) override;
		void lower_call(const ir::CallInstruction &instr) override;
		void lower_return(std::optional<ir::VRegId> ret_reg) override;
		void finalize_function(const ir::Function &fn, std::vector<uint8_t> &code) override;
		void patch_call(std::vector<uint8_t> &code, size_t call_offset, size_t target_offset) override;

	public:
		X86CodeGenerator(const Target &target);

		virtual std::vector<uint8_t> build_runtime_code(uint64_t main_offset, Target t) override;
	};

}
