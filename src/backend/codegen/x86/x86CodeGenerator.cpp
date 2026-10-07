#include "x86CodeGenerator.hpp"

#include <format>

#include "backend/codegen/x86/x86.hpp"
#include "backend/codegen/x86/x86Instruction.hpp"
#include "utils/error.hpp"

namespace codegen
{

	namespace x86
	{

		static const std::tuple<const PhysReg, const bool> X86_ALLOCATABLE_REGISTERS[] = {
			// TODO reorder based on priority
			{PhysReg(Register::RAX), false},
			{PhysReg(Register::RBX), true},
			{PhysReg(Register::RCX), false},
			{PhysReg(Register::RDX), false},
			{PhysReg(Register::RSI), true},
			{PhysReg(Register::RDI), true},
		};

		static const std::tuple<const PhysReg, const bool> X64_ALLOCATABLE_REGISTERS[] = {
			// TODO reorder based on priority
			{PhysReg(Register::RAX), false},
			{PhysReg(Register::RBX), true},
			{PhysReg(Register::RCX), false},
			{PhysReg(Register::RDX), false},
			{PhysReg(Register::RSI), false},
			{PhysReg(Register::RDI), false},
			{PhysReg(Register::R8), false},
			{PhysReg(Register::R9), false},
			{PhysReg(Register::R10), false},
			{PhysReg(Register::R11), false},
			{PhysReg(Register::R12), true},
			{PhysReg(Register::R13), true},
			{PhysReg(Register::R14), true},
			{PhysReg(Register::R15), true},
		};

		std::span<const std::tuple<const PhysReg, const bool>> get_registers(Target target)
		{
			switch (target.arch)
			{
			case Target::Arch::X86:
				return std::span{X86_ALLOCATABLE_REGISTERS};
			case Target::Arch::X86_64:
				return std::span{X64_ALLOCATABLE_REGISTERS};
			default:
				throw CompilerError::internal("Cannot get x86 registers from non-x86 arch");
			}
		}

	}

	X86CodeGenerator::X86CodeGenerator(const Target &target)
		: target(target), regalloc(*this, x86::get_registers(target)), reg_width(target.arch == Target::Arch::X86 ? 4 : 8)
	{
		if (target.arch != Target::Arch::X86 && target.arch != Target::Arch::X86_64)
			throw CompilerError::internal("Tried to initalize x86 code generator with non-x86 arch");
		if (target.arch == Target::Arch::X86_64)
			throw CompilerError::unimplemented("Have not implemented AMD64 yet");
	}

	void X86CodeGenerator::store_spilled_vreg(PhysReg src, ir::VRegId vreg, size_t spill_index)
	{
		throw CompilerError::unimplemented("TODO x86 register spilling");
	}

	void X86CodeGenerator::load_spilled_vreg(PhysReg dest, ir::VRegId vreg, size_t spill_index)
	{
		throw CompilerError::unimplemented("TODO x86 register spilling");
	}

	void X86CodeGenerator::begin_function(const ir::Function &fn)
	{
		this->body.clear();

		this->regalloc.start_function();
	}

	void X86CodeGenerator::begin_block(const ir::BasicBlock &bb)
	{
		// clearing register allocator slots
		this->regalloc.start_block();
	}

	void X86CodeGenerator::begin_instruction()
	{
		// registers are only locked for the duration of one instruction
		this->regalloc.start_instruction();
	}

	void X86CodeGenerator::lower_immediate_instr(const ir::ImmediateInstruction &instr)
	{
		throw CompilerError::unimplemented("TODO x86 lower imm instr");
	}
	void X86CodeGenerator::lower_binary_instr(const ir::BinaryInstruction &instr)
	{
		throw CompilerError::unimplemented("TODO x86 lower binary instr");
	}
	void X86CodeGenerator::lower_unary_instr(const ir::UnaryInstruction &instr)
	{
		throw CompilerError::unimplemented("TODO x86 lower unary instr");
	}

	void X86CodeGenerator::lower_load_arg_instr(const ir::LoadArgInstruction &instr)
	{
		RegSlot *dest = this->regalloc.load_dest_vreg(instr.dest);
		ir::IrType type = this->cur_fn->vregs.at(instr.dest);
		// every argument takes one word sized slot, so the offset only depends on the index. bp points at the
		// saved bp, with the return address above it
		int32_t bp_offset = int32_t(this->reg_width * (instr.index + 2));
		// x86 is little endian, so a narrow argument is the first bytes of its slot
		x86::MemoryOperand src{.size = type.get_size(), .base_reg = x86::Register::RBP, .offset = bp_offset};

		// TODO 64 bit arguments, once x64 or two register values are supported
		if (type.get_size() > 4)
			throw CompilerError::unimplemented(
				std::format("x86 codegen: arguments of type '{}' (in function \"{}\")", type.to_string(), this->cur_fn->name));

		// narrow arguments are extended while loading, reading only the bytes that belong to them. so
		// whatever the caller left in the rest of the slot doesn't matter
		x86::Mnemonic mnemonic = x86::Mnemonic::MOV;
		if (type.get_size() < 4)
			mnemonic = type.is_signed() ? x86::Mnemonic::MOVSX : x86::Mnemonic::MOVZX;
		this->body.write(x86::Instruction{mnemonic, {x86::RegisterOperand(dest->physical, x86::RegisterAccess::DWord), src}});
	}

	void X86CodeGenerator::lower_call(const ir::CallInstruction &instr)
	{
		throw CompilerError::unimplemented("TODO x86 lower function call instr");
	}
	void X86CodeGenerator::lower_return(std::optional<ir::VRegId> ret_reg)
	{
		throw CompilerError::unimplemented("TODO x86 lower return");
	}
	void X86CodeGenerator::finalize_function(const ir::Function &fn, std::vector<uint8_t> &code)
	{
		throw CompilerError::unimplemented("TODO x86 finalize function");
	}
	void X86CodeGenerator::patch_call(std::vector<uint8_t> &code, size_t call_offset, size_t target_offset)
	{
		throw CompilerError::unimplemented("TODO x86 patch call");
	}

	std::vector<uint8_t> X86CodeGenerator::build_runtime_code(uint64_t main_offset, Target t)
	{
		throw CompilerError::unimplemented("TODO x86 build runtime code");
	}
}
