#include "x86CodeGenerator.hpp"

#include <format>

#include "backend/codegen/x86/x86.hpp"
// #include "backend/codegen/x86/x86Instruction.hpp"
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

		/// @brief Get the required register size to house a specific type (returns low byte of word if size is 1 byte)
		/// @param type IR type
		/// @return Required register size
		RegisterAccess reg_size_from_type(ir::IrType type)
		{
			switch (type.get_size())
			{
			case 1:
				return RegisterAccess::LByte;
			case 2:
				return RegisterAccess::Word;
			case 4:
				return RegisterAccess::DWord;
			case 8:
				return RegisterAccess::QWord;
			}
			throw CompilerError::internal(std::format("Weird size in function reg_size_from_type(): {} bytes", type.get_size()));
		}
	}

	X86CodeGenerator::X86CodeGenerator(const Target &target)
		: target(target), regalloc(*this, x86::get_registers(target)),
		  reg_width(target.arch == Target::Arch::X86_64 ? 8 : 4),
		  is_x64(target.arch == Target::Arch::X86_64), body(target.arch == Target::Arch::X86_64)
	{
		if (target.arch != Target::Arch::X86 && target.arch != Target::Arch::X86_64)
			throw CompilerError::internal("Tried to initalize x86 code generator with non-x86 arch");
		if (is_x64)
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
		// clear data
		this->body.clear();

		// calculate incoming stack-passed argument offsets
		this->stack_args_bp_offsets.clear();
		uint32_t cumulative_bp_offset = 2 * this->reg_width;
		for (size_t i = 0; i < fn.argument_types.size(); i++)
		{
			this->stack_args_bp_offsets.push_back(cumulative_bp_offset);
			cumulative_bp_offset += fn.argument_types[i].get_size();
		}
		this->stack_args_size = cumulative_bp_offset - (2 * this->reg_width);

		// prepare register allocator
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
		// RegSlot *dest = this->regalloc.load_dest_vreg(instr.dest);
		// ir::IrType imm_type = this->cur_fn->vregs.at(instr.dest);
		// unsigned int imm_size = imm_type.get_size();
		// x86::RegisterAccess dest_reg_size = x86::reg_size_from_type(imm_type);

		// // if (instr.value > UINT64_MAX)
		// x86::ImmediateOperand src_operand{
		// 	.value = instr.value,
		// };
		// x86::RegisterOperand dest_operand{.reg = static_cast<x86::Register>(dest->physical), .access = dest_reg_size};

		// this->body.write_mov(dest_operand, src_operand, imm_type.is_signed());
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
		ir::IrType arg_type = this->cur_fn->vregs.at(instr.dest);
		unsigned int arg_size = arg_type.get_size();

		if (instr.index >= this->stack_args_bp_offsets.size())
			throw CompilerError::internal(std::format("Load arg instruction has index {}, while precomputed stack_args_bp_offset has size {}", instr.index, this->stack_args_bp_offsets.size()));
		uint32_t bp_offset = this->stack_args_bp_offsets[instr.index];

		switch (arg_size)
		{
		case 1:
			this->body.write_mov(x86::operands::rm8());
			break;
		default:
			break;
		}
		// x86::RegisterAccess dest_reg_size = x86::reg_size_from_type(arg_type);

		// x86::MemoryOperand src_operand{.size = arg_size, .base_reg = x86::Register::RBP, .offset = static_cast<int32_t>(bp_offset)};
		// x86::RegisterOperand dest_operand{.reg = static_cast<x86::Register>(dest->physical), .access = dest_reg_size};

		// this->body.write_mov(dest_operand, src_operand, arg_type.is_signed());
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
