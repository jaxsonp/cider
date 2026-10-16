#pragma once

#include <stdint.h>
#include <string>
#include <tuple>
#include <vector>

#include "backend/codegen/CodeGenerator.hpp"
#include "backend/codegen/RegAllocator.hpp"
#include "backend/codegen/x86/x86.hpp"
#include "backend/codegen/x86/x86Assembler.hpp"

namespace codegen
{

	/// @brief x86 backend, for both 32 bit x86 (i386) and x86-64
	///
	/// Everything that depends on the register width is derived from the target, so "word" below means
	/// 4 bytes on x86 and 8 bytes on x86-64, and bp/sp are ebp/esp or rbp/rsp
	///
	/// Stack layout:
	/// ```
	/// | outgoing stack args... | spilled vregs... | saved bp | return addr | incoming stack args... |
	/// ^ sp                                        ^ bp
	/// <-- lower addresses                                      higher addresses -->
	/// <-- stack grows this way
	/// ```
	/// - spilled vregs are at bp - word * (spill number + 1)
	/// - outgoing stack args are the stack-passed arguments of calls this function makes. The area is sized for
	///   the largest such call, and stack argument i goes at sp + word * i
	/// - incoming stack args are the same area in the caller's frame, so stack argument i is at bp + word * (i + 2)
	/// - sp is kept 16 byte aligned at calls (the frame is padded so that it is, on both ABIs)
	///
	/// Register allocation is done by `RegAllocator`. Currently only uses caller saved registers
	///
	/// Values in registers:
	/// Every value is operated on at its own width (ie `add al, cl` for 8 bit values), so a value narrower than a
	/// register only owns the lower bits of it and the upper bits can be anything. The exceptions, where values
	/// get extended first, are division (done on 32 bits for narrower types) and returning. Since every value
	/// has to be usable as an 8 bit register, only registers with an 8 bit form are allocated (on x86 that rules
	/// out esi and edi)
	///
	/// Calling convention (subset of the System V i386 and AMD64 ABIs):
	/// - x86: every argument is passed on the stack, in a 4 byte slot
	/// - x86-64: the first 6 arguments are passed in rdi, rsi, rdx, rcx, r8 and r9, the rest on the stack in 8 byte
	///   slots
	/// The return value comes back in eax/rax, extended to at least 32 bits. Since every allocatable register is
	/// caller saved, all live values are spilled to the stack before a call
	class X86CodeGenerator : public CodeGenerator, private SpillHandler
	{
		using Register = x86::Register;
		using RegSlot = RegAllocator::RegSlot;

		/// Number of arguments that can be passed in registers on x86-64
		static constexpr size_t MAX_REGISTER_ARGS_X64 = 6;

		/// Target being compiled for
		const Target target;

		/// Whether this arch is x86_64, for convenience
		const bool is_x64;

		/// Size in bytes of a register, and therefore of every stack slot
		const int64_t word_size;

		/// Width of a register
		const x86::Width word;

		/// Register allocator state
		RegAllocator regalloc;

		/// Instruction buffer for function body (not prologue)
		x86::Assembler body;

		/// Bytes at the bottom of the frame (from sp up) reserved for stack-passed arguments of calls made by this function
		int64_t stack_passed_args_size = 0;

		/// Call sites in the body (byte offset of the call, callee name), resolved once every function is lowered
		std::vector<std::tuple<size_t, std::string>> call_backpatch_list;

		/// @brief Physical register of a register slot
		static Register reg_of(const RegSlot *slot) { return Register(slot->physical); }

		/// @brief Width a virtual register is operated on with
		x86::Width width_of(ir::VRegId vreg) const;

		/// @brief Offset from the base pointer of a spill slot (always negative)
		int32_t spill_bp_offset(size_t spill_index) const;

		/// @brief Number of arguments of a call that are passed in registers
		size_t register_arg_count(size_t arg_count) const;

		/// @brief Sign or zero extends (depending on the type) a value narrower than 32 bits to 32 bits, in place
		void extend_to_32(Register reg, ir::IrType type);

		void store_spilled_vreg(PhysReg src, ir::VRegId vreg, size_t spill_index) override;

		void load_spilled_vreg(PhysReg dest, ir::VRegId vreg, size_t spill_index) override;

		void move_reg(PhysReg dest, PhysReg src, ir::VRegId vreg) override;

		/// @brief Lowers division and remainder, which take their operands in fixed registers
		void lower_division(const ir::BinaryInstruction &instr);

		/// @brief Lowers shifts, which take their shift amount in a fixed register
		void lower_shift(const ir::BinaryInstruction &instr);

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
