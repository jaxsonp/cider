#pragma once

#include <stdint.h>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "backend/codegen/CodeGenerator.hpp"
#include "backend/codegen/RegAllocator.hpp"
#include "backend/codegen/riscv/Riscv.hpp"
#include "backend/codegen/riscv/RiscvAssembler.hpp"

namespace codegen
{
	/// @brief RISC-V backend, for both RV32 and RV64
	///
	/// Everything that depends on the register width is derived from the target, so "word" below means
	/// 4 bytes on RV32 and 8 bytes on RV64
	///
	/// Stack layout:
	/// ```
	/// | outgoing stack args... | spilled vregs... | saved fp | saved ra | incoming stack args... |
	/// ^ sp                                                              ^ fp
	/// <-- lower addresses                                      higher addresses -->
	/// <-- stack grows this way
	/// ```
	/// - spilled vregs will be at fp - (word * (spill number + 3))
	/// - outgoing stack args are the stack-passed arguments (9th onwards) of calls this function makes. The
	///   area is sized for the largest such call, argument i goes at sp + word * (i - 8)
	/// - incoming stack args are the same area in the caller's frame, so argument i is at fp + word * (i - 8)
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
	class RiscvCodeGenerator : public CodeGenerator, private SpillHandler
	{
		using Register = riscv::Register;
		using RegSlot = RegAllocator::RegSlot;

		/// Register reserved as scratch space for the code generator, never handed to the register allocator.
		/// Free to use within the lowering of a single instruction
		static constexpr Register SCRATCH = Register::t6;

		/// Number of arguments that can be passed in registers (a0-a7)
		static constexpr size_t MAX_REGISTER_ARGS = 8;

		/// Target being compiled for, decides the register width and which extensions can be used
		const Target target;

		/// Register width in bits, 32 or 64
		const unsigned int xlen;

		/// Size in bytes of a register, and therefore of every stack slot
		const int64_t word_size;

		const bool enable_compression;

		riscv::Assembler body;

		/// Register assignment states. Registers are in order of priority (heuristic = caller saved first (is this good? idk))
		RegAllocator regalloc;

		/// Bytes at the bottom of the frame (from sp up) reserved for stack-passed arguments of calls made by this function
		int64_t stack_passed_args_size = 0;

		/// Tasklist of instructions (positions in the body) that need their immediates to be retrofitted with
		/// the epilogue's offset
		std::vector<size_t> epilogue_backpatch_list;

		/// Call sites in the body (position of the call, callee name), resolved once every function is lowered
		std::vector<std::tuple<size_t, std::string>> call_backpatch_list;

		/// @brief Physical register of a register slot
		static Register reg_of(const RegSlot *slot) { return Register(slot->physical); }

		/// @brief Offset from the frame pointer of a spill slot (always negative)
		int64_t spill_fp_offset(size_t spill_index) const;

		/// @brief Whether operations on a type should use the 32 bit "W" instructions (RV64 only)
		bool is_word_op(ir::IrType type) const;

		/// @brief Throws if the target doesn't have the M extension
		void require_m_extension(std::string_view operation) const;

		/// @brief Makes `base + offset` usable as the address of a load or store, which only have room for a
		/// signed 12 bit offset. Offsets that fit are left alone, bigger ones get their upper part added to the
		/// base in a scratch register
		/// @param offset Offset to reach, replaced with the offset to use with the returned register
		/// @param scratch Register to clobber if the offset doesn't fit
		/// @return The register to use as the base
		Register reach_offset(riscv::Assembler &code, Register base, int64_t &offset, Register scratch);

		/// @brief Loads a value of `size` bytes from `base + offset`, sign or zero extending it to the whole
		/// register. Any offset is fine
		void write_load(riscv::Assembler &code, Register dest, Register base, int64_t offset, unsigned int size, bool sign_extend);

		/// @brief Stores the lower `size` bytes of a register to `base + offset`. Any offset is fine, but big
		/// ones clobber the scratch register
		void write_store(riscv::Assembler &code, Register base, Register src, int64_t offset, unsigned int size);

		void store_spilled_vreg(PhysReg src, ir::VRegId vreg, size_t spill_index) override;

		/// Sign/zero extends narrow types
		void load_spilled_vreg(PhysReg dest, ir::VRegId vreg, size_t spill_index) override;

		/// @brief Truncates the value in a register to its proper size. Must be called before instructions like
		/// division, where lower bits are affected by upper bits, which may be junk from, say, an add or shift left
		/// @param width How many of the lower bits of the register the instruction reads, values at least this
		/// wide are left alone
		void truncate_reg(RegSlot *slot, unsigned int width);

		/// @brief Truncates the value in a register to its proper size, for instructions that read the whole register
		void truncate_reg(RegSlot *slot) { this->truncate_reg(slot, this->xlen); }

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
		RiscvCodeGenerator(const Target &target);

		virtual std::vector<uint8_t> build_runtime_code(uint64_t main_offset, Target t) override;
	};

}
