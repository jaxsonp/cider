#include "RiscvCodeGenerator.hpp"

#include <algorithm>
#include <format>
#include <vector>

#include "utils/logging.hpp"
#include "utils/error.hpp"

namespace codegen
{
	using riscv::Assembler;

	/// Registers handed to the register allocator, in order of priority
	static const std::vector<PhysReg> ALLOCATABLE_REGISTERS = {
		PhysReg(riscv::Register::t0),
		PhysReg(riscv::Register::t1),
		PhysReg(riscv::Register::t2),
		PhysReg(riscv::Register::t3),
		PhysReg(riscv::Register::t4),
		PhysReg(riscv::Register::t5),
		// t6 is reserved as scratch
		PhysReg(riscv::Register::a0),
		PhysReg(riscv::Register::a1),
		PhysReg(riscv::Register::a2),
		PhysReg(riscv::Register::a3),
		PhysReg(riscv::Register::a4),
		PhysReg(riscv::Register::a5),
		PhysReg(riscv::Register::a6),
		PhysReg(riscv::Register::a7),
		// TODO use s registers
	};

	/// Stack pointer alignment required by the ABI, in bytes
	static constexpr int64_t STACK_ALIGNMENT = 16;

	/// @brief Whether a value fits in the signed 12 bit immediate of I-type and S-type instructions
	static bool fits_imm12(int64_t value)
	{
		return value >= -2048 && value <= 2047;
	}

	RiscvCodeGenerator::RiscvCodeGenerator(const Target &target)
		: target(target),
		  enable_compression(target.has(Target::RiscvExt::C)),
		  xlen(target.register_width()),
		  word_size(target.register_width() / 8),
		  body(xlen, target.has(Target::RiscvExt::C)),
		  regalloc(*this, ALLOCATABLE_REGISTERS)
	{
		if (target.arch != Target::Arch::RISCV32 && target.arch != Target::Arch::RISCV64)
			throw CompilerError::internal("Tried to initalize Riscv code generator with non riscv arch");
	}

	int64_t RiscvCodeGenerator::spill_fp_offset(size_t spill_index) const
	{
		// spill N lives at fp - word * (N + 3), right below the saved ra (fp - word) and fp (fp - 2 * word)
		return -this->word_size * int64_t(spill_index + 3);
	}

	bool RiscvCodeGenerator::is_word_op(ir::IrType type) const
	{
		return this->xlen == 64 && type.get_size() <= 4;
	}

	void RiscvCodeGenerator::require_m_extension(std::string_view operation) const
	{
		// TODO lower these to runtime routines instead, for targets without it
		if (!this->target.has(Target::RiscvExt::M))
			throw CompilerError::unsupported(std::format("RISC-V codegen: {} requires the M extension", operation));
	}

	riscv::Register RiscvCodeGenerator::reach_offset(Assembler &code, Register base, int64_t &offset, Register scratch)
	{
		if (fits_imm12(offset))
			return base;

		// the load/store sign extends its 12 bit offset, so round the upper part to compensate
		int64_t upper = (offset + 0x800) & ~int64_t(0xFFF);
		if (upper != int64_t(int32_t(upper)))
			throw CompilerError::unimplemented("RISC-V codegen: stack frames over 2GiB");
		code.write_lui(scratch, upper);
		code.write_add(scratch, scratch, base);
		offset -= upper;
		return scratch;
	}

	void RiscvCodeGenerator::write_load(Assembler &code, Register dest, Register base, int64_t offset, unsigned int size, bool sign_extend)
	{
		// the destination is about to be overwritten anyway, so it can double as the scratch register
		base = this->reach_offset(code, base, offset, dest);
		switch (size)
		{
		case 1:
			if (sign_extend)
				code.write_lb(dest, base, offset);
			else
				code.write_lbu(dest, base, offset);
			break;
		case 2:
			if (sign_extend)
				code.write_lh(dest, base, offset);
			else
				code.write_lhu(dest, base, offset);
			break;
		case 4:
			if (sign_extend || this->xlen == 32)
				code.write_lw(dest, base, offset);
			else
				code.write_lwu(dest, base, offset);
			break;
		case 8:
			code.write_ld(dest, base, offset);
			break;
		default:
			throw CompilerError::internal(std::format("RISC-V codegen: can't load a value of {} bytes", size));
		}
	}

	void RiscvCodeGenerator::write_store(Assembler &code, Register base, Register src, int64_t offset, unsigned int size)
	{
		base = this->reach_offset(code, base, offset, SCRATCH);
		switch (size)
		{
		case 1:
			code.write_sb(base, src, offset);
			break;
		case 2:
			code.write_sh(base, src, offset);
			break;
		case 4:
			code.write_sw(base, src, offset);
			break;
		case 8:
			code.write_sd(base, src, offset);
			break;
		default:
			throw CompilerError::internal(std::format("RISC-V codegen: can't store a value of {} bytes", size));
		}
	}

	void RiscvCodeGenerator::load_spilled_vreg(PhysReg dest, ir::VRegId vreg, size_t spill_index)
	{
		ir::IrType vreg_type = this->cur_fn->vregs.at(vreg);
		// on RV64, 32 bit values are always loaded sign extended (even unsigned ones), which is the form
		// that the ABI wants them passed to other functions in. the upper bits don't matter for anything else
		bool sign_extend = vreg_type.is_signed() || vreg_type.get_size() == 4;
		this->write_load(this->body, Register(dest), Register::fp, this->spill_fp_offset(spill_index), vreg_type.get_size(), sign_extend);
	}

	void RiscvCodeGenerator::store_spilled_vreg(PhysReg src, ir::VRegId vreg, size_t spill_index)
	{
		ir::IrType vreg_type = this->cur_fn->vregs.at(vreg);
		this->write_store(this->body, Register::fp, Register(src), this->spill_fp_offset(spill_index), vreg_type.get_size());
	}

	void RiscvCodeGenerator::truncate_reg(RegSlot *slot, unsigned int width)
	{
		ir::IrType ir_type = this->cur_fn->vregs.at(slot->resident);
		unsigned int type_bits = 8u * ir_type.get_size();
		// values at least as wide as what's being read have nothing to truncate
		if (type_bits >= width)
			return;

		if (this->xlen == 64 && type_bits == 32 && ir_type.is_signed())
		{
			// sign extending a 32 bit value has a dedicated instruction (aka sext.w)
			this->body.write_addiw(reg_of(slot), reg_of(slot), 0);
			return;
		}

		unsigned int shift = this->xlen - type_bits;
		this->body.write_slli(reg_of(slot), reg_of(slot), shift);
		if (ir_type.is_signed())
			this->body.write_srai(reg_of(slot), reg_of(slot), shift);
		else
			this->body.write_srli(reg_of(slot), reg_of(slot), shift);
	}

	void RiscvCodeGenerator::begin_function(const ir::Function &fn)
	{
		// values wider than a register would need to be split across two of them
		// TODO implement, so that 64 bit integers work on RV32
		for (const auto &[vreg, type] : fn.vregs)
		{
			if (8u * type.get_size() > this->xlen)
				throw CompilerError::unsupported(
					std::format("RISC-V codegen: type '{}' is wider than a register on this target, which is not supported yet (in function \"{}\")",
								type.to_string(), fn.name));
		}

		// resetting state
		this->stack_passed_args_size = 0;
		this->regalloc.start_function();
		this->epilogue_backpatch_list.clear();
		this->call_backpatch_list.clear();

		this->body = Assembler(this->xlen, this->enable_compression);
	}

	void RiscvCodeGenerator::begin_block(const ir::BasicBlock &bb)
	{
		// clearing register allocator slots
		this->regalloc.start_block();
	}

	void RiscvCodeGenerator::begin_instruction()
	{
		// registers are only locked for the duration of one instruction
		this->regalloc.start_instruction();
	}

	void RiscvCodeGenerator::lower_immediate_instr(const ir::ImmediateInstruction &instr)
	{
		// load immmediate
		RegSlot *dest = this->regalloc.load_dest_vreg(instr.dest);
		ir::IrType type = this->cur_fn->vregs.at(instr.dest);
		// only the bits that the type has matter. 32 bit and narrower constants are loaded as sign extended
		// 32 bit values, which are the cheapest to build
		int64_t value = type.get_size() <= 4 ? int64_t(int32_t(uint32_t(instr.value))) : int64_t(instr.value);
		this->body.load_immediate(reg_of(dest), value);
	}

	void RiscvCodeGenerator::lower_binary_instr(const ir::BinaryInstruction &instr)
	{
		ir::IrType op_type = this->cur_fn->vregs.at(instr.lhs);
		RegSlot *dest_slot = this->regalloc.load_dest_vreg(instr.dest);
		RegSlot *op1_slot = this->regalloc.load_src_vreg(instr.lhs);
		RegSlot *op2_slot = this->regalloc.load_src_vreg(instr.rhs);
		Register dest = reg_of(dest_slot);
		Register op1 = reg_of(op1_slot);
		Register op2 = reg_of(op2_slot);

		// on RV64, types of 32 bits and narrower are operated on with the 32 bit instructions. those only read
		// the lower 32 bits of their operands, so that's all that has to be valid before using one
		bool word_op = this->is_word_op(op_type);
		unsigned int op_width = word_op ? 32 : this->xlen;

		switch (instr.op)
		{
		case ir::BinaryOp::Add:
			// add register to register
			if (word_op)
				this->body.write_addw(dest, op1, op2);
			else
				this->body.write_add(dest, op1, op2);
			break;
		case ir::BinaryOp::Sub:
			// subtact register from register
			if (word_op)
				this->body.write_subw(dest, op1, op2);
			else
				this->body.write_sub(dest, op1, op2);
			break;
		case ir::BinaryOp::Mul:
			// multiply register to register
			this->require_m_extension("multiplication");
			if (word_op)
				this->body.write_mulw(dest, op1, op2);
			else
				this->body.write_mul(dest, op1, op2);
			break;
		case ir::BinaryOp::Div:
			// divide register to register
			this->require_m_extension("division");
			// division reads the whole register, so both operands must be truncated first
			this->truncate_reg(op1_slot, op_width);
			this->truncate_reg(op2_slot, op_width);
			if (word_op)
			{
				if (op_type.is_signed())
					this->body.write_divw(dest, op1, op2);
				else
					this->body.write_divuw(dest, op1, op2);
			}
			else
			{
				if (op_type.is_signed())
					this->body.write_div(dest, op1, op2);
				else
					this->body.write_divu(dest, op1, op2);
			}
			break;
		case ir::BinaryOp::Rem:
			// mod register to register
			this->require_m_extension("modulo");
			// division reads the whole register, so both operands must be truncated first
			this->truncate_reg(op1_slot, op_width);
			this->truncate_reg(op2_slot, op_width);
			if (word_op)
			{
				if (op_type.is_signed())
					this->body.write_remw(dest, op1, op2);
				else
					this->body.write_remuw(dest, op1, op2);
			}
			else
			{
				if (op_type.is_signed())
					this->body.write_rem(dest, op1, op2);
				else
					this->body.write_remu(dest, op1, op2);
			}
			break;
		case ir::BinaryOp::BitAnd:
			this->body.write_and(dest, op1, op2);
			break;
		case ir::BinaryOp::BitOr:
			this->body.write_or(dest, op1, op2);
			break;
		case ir::BinaryOp::BitXor:
			this->body.write_xor(dest, op1, op2);
			break;
		case ir::BinaryOp::BitShl:
			// the processor only looks at the lower bits of the shift amount, 6 bits when shifting a 64 bit value
			// and 5 bits otherwise. using the 32 bit shifts on RV64 keeps that the same on every target
			if (word_op)
				this->body.write_sllw(dest, op1, op2);
			else
				this->body.write_sll(dest, op1, op2);
			break;
		case ir::BinaryOp::BitShr:
			// the bits shifted in come from the upper bits, so the value must be truncated
			this->truncate_reg(op1_slot, op_width);
			if (word_op)
			{
				if (op_type.is_signed())
					this->body.write_sraw(dest, op1, op2);
				else
					this->body.write_srlw(dest, op1, op2);
			}
			else
			{
				if (op_type.is_signed())
					this->body.write_sra(dest, op1, op2);
				else
					this->body.write_srl(dest, op1, op2);
			}
			break;
		case ir::BinaryOp::CmpEq:
			// the xor below is only zero for equal values if both are truncated the same way
			this->truncate_reg(op1_slot);
			this->truncate_reg(op2_slot);
			// compare the registers (always unsigned check, cus -123 is less than 1 but means not equal)
			this->body.write_xor(dest, op1, op2);
			this->body.write_sltiu(dest, dest, 1);
			break;
		case ir::BinaryOp::CmpNe:
			// the xor below is only zero for equal values if both are truncated the same way
			this->truncate_reg(op1_slot);
			this->truncate_reg(op2_slot);
			// compare the registers
			this->body.write_xor(dest, op1, op2);
			// unequal exactly when the xor is nonzero, always an unsigned test (see CmpEq)
			this->body.write_sltu(dest, Register::zero, dest);
			break;
		case ir::BinaryOp::CmpGt:
			// comparisons read the whole register, so both operands must be truncated first
			this->truncate_reg(op1_slot);
			this->truncate_reg(op2_slot);
			if (op_type.is_signed())
				this->body.write_slt(dest, op2, op1);
			else
				this->body.write_sltu(dest, op2, op1);
			break;
		case ir::BinaryOp::CmpGte:
			// comparisons read the whole register, so both operands must be truncated first
			this->truncate_reg(op1_slot);
			this->truncate_reg(op2_slot);
			// check if less than
			if (op_type.is_signed())
				this->body.write_slt(dest, op1, op2);
			else
				this->body.write_sltu(dest, op1, op2);
			// negate
			this->body.write_xori(dest, dest, 1);
			break;
		case ir::BinaryOp::CmpLt:
			// comparisons read the whole register, so both operands must be truncated first
			this->truncate_reg(op1_slot);
			this->truncate_reg(op2_slot);
			if (op_type.is_signed())
				this->body.write_slt(dest, op1, op2);
			else
				this->body.write_sltu(dest, op1, op2);
			break;
		case ir::BinaryOp::CmpLte:
			// comparisons read the whole register, so both operands must be truncated first
			this->truncate_reg(op1_slot);
			this->truncate_reg(op2_slot);
			// check if greater than
			if (op_type.is_signed())
				this->body.write_slt(dest, op2, op1);
			else
				this->body.write_sltu(dest, op2, op1);
			// negate
			this->body.write_xori(dest, dest, 1);
			break;
		default:
			throw CompilerError::internal("Uncaught BinaryOp variant");
		}
	}

	void RiscvCodeGenerator::lower_unary_instr(const ir::UnaryInstruction &instr)
	{
		switch (instr.op)
		{
		case ir::UnaryOp::Neg:
		{
			RegSlot *dest = this->regalloc.load_dest_vreg(instr.dest);
			RegSlot *src = this->regalloc.load_src_vreg(instr.src);
			this->body.write_sub(reg_of(dest), Register::zero, reg_of(src));
			break;
		}
		case ir::UnaryOp::BitNot:
		{
			RegSlot *dest = this->regalloc.load_dest_vreg(instr.dest);
			RegSlot *op1 = this->regalloc.load_src_vreg(instr.src);
			// XORing with all ones to "flip bits" (the ones above the value's width don't matter)
			this->body.write_xori(reg_of(dest), reg_of(op1), -1);
			break;
		}
		default:
			throw CompilerError::internal("Uncaught UnaryOp variant");
		}
	}

	void RiscvCodeGenerator::lower_load_arg_instr(const ir::LoadArgInstruction &instr)
	{
		if (instr.index >= MAX_REGISTER_ARGS)
		{
			// stack-passed arguments sit just above the caller's sp, which is our fp. every argument takes a
			// word sized slot and was stored already extended by the caller
			RegSlot *dest = this->regalloc.load_dest_vreg(instr.dest);
			int64_t fp_offset = this->word_size * int64_t(instr.index - MAX_REGISTER_ARGS);
			this->write_load(this->body, reg_of(dest), Register::fp, fp_offset, this->word_size, true);
		}
		else
		{
			// the argument is already sitting in its a-register, so just claim that register for the vreg.
			// this only works while nothing else has been allocated there, ie at the very start of the function
			this->regalloc.claim(PhysReg(uint8_t(Register::a0) + instr.index), instr.dest);
		}
	}

	void RiscvCodeGenerator::lower_call(const ir::CallInstruction &instr)
	{
		// every allocatable register is caller saved, so everything live has to go to the stack first
		this->regalloc.spill_all();

		// now every vreg is on the stack. loading arguments straight from there avoids having to shuffle
		// values between registers, and the sized loads also give the callee properly extended values

		// arguments past the eighth go to the bottom of our frame, where the callee finds them at its fp.
		// done first since t0 is free as scratch, and a0-a7 are about to be overwritten anyway
		if (instr.args.size() > MAX_REGISTER_ARGS)
			this->stack_passed_args_size = std::max(this->stack_passed_args_size, this->word_size * int64_t(instr.args.size() - MAX_REGISTER_ARGS));
		for (size_t i = MAX_REGISTER_ARGS; i < instr.args.size(); ++i)
		{
			this->regalloc.load_spilled_vreg(PhysReg(Register::t0), instr.args[i]);
			this->write_store(this->body, Register::sp, Register::t0, this->word_size * int64_t(i - MAX_REGISTER_ARGS), this->word_size);
		}
		// now load
		for (size_t i = 0; i < std::min(instr.args.size(), MAX_REGISTER_ARGS); ++i)
			this->regalloc.load_spilled_vreg(PhysReg(uint8_t(Register::a0) + i), instr.args[i]);

		// auipc + jalr reaches anywhere in the address space, offsets are filled in once every function is lowered
		size_t pos = this->body.write_call_placeholder(Register::ra);
		this->call_backpatch_list.push_back({pos, instr.callee});

		if (instr.dest.has_value())
			this->regalloc.claim(PhysReg(Register::a0), instr.dest.value());
	}

	void RiscvCodeGenerator::lower_return(std::optional<ir::VRegId> ret_reg)
	{
		if (ret_reg.has_value())
		{
			RegSlot *ret_value_slot = this->regalloc.load_src_vreg(ret_reg.value());
			// the value leaves the compiler here, so it has to be in its canonical form
			if (this->xlen == 64 && this->cur_fn->vregs.at(ret_reg.value()).get_size() == 4)
			{
				// which on RV64 is sign extended for 32 bit values, even unsigned ones
				this->body.write_addiw(Register::a0, reg_of(ret_value_slot), 0);
			}
			else
			{
				this->truncate_reg(ret_value_slot);
				this->body.write_addi(Register::a0, reg_of(ret_value_slot), 0);
			}
		}
		else if (this->cur_fn->name == "main")
		{
			// main's return value becomes the exit code, so a void main must exit cleanly rather than with
			// whatever was left in a0
			this->body.write_addi(Register::a0, Register::zero, 0);
		}

		// no need to spill anything, nothing in this function runs after a return
		size_t pos = this->body.write_jal(Register::zero, 0);
		this->epilogue_backpatch_list.push_back(pos);
	}

	void RiscvCodeGenerator::finalize_function(const ir::Function &fn, std::vector<uint8_t> &code)
	{
		Assembler prologue = Assembler(this->xlen, this->enable_compression);
		Assembler epilogue = Assembler(this->xlen, this->enable_compression);

		// TEMP for debugging
		this->body.write_nop();
		this->body.write_nop();
		this->body.write_nop();
		this->body.write_nop();
		this->body.write_nop();
		this->body.write_nop();

		// now go back and fill in the epilogue's offset for instructions that need it
		log_vvvv("Backpatching offsets in body");

		// the epilogue comes right after the body
		size_t epilogue_offset = this->body.cur_offset();
		for (size_t instr_pos : this->epilogue_backpatch_list)
		{
			int64_t rel_offset = int64_t(epilogue_offset - this->body.offset_of(instr_pos)); // always positive
			this->body.backpatch_immediate(instr_pos, rel_offset);
		}

		log_vvvv("Building prologue and epilogue");

		// frame size -------------

		// saved fp and ra, then spill slots, then outgoing stack arguments
		int64_t stack_size = 2 * this->word_size;
		stack_size += int64_t(this->regalloc.spill_count()) * this->word_size;
		stack_size += this->stack_passed_args_size;
		log_vvvv("calculated stack size: {}", stack_size);
		int64_t padded_stack_size = ((stack_size + (STACK_ALIGNMENT - 1)) / STACK_ALIGNMENT) * STACK_ALIGNMENT;
		log_vvvv("padded stack size: {}", padded_stack_size);

		// the frame is allocated in two steps. the first is always the same small size and holds the saved
		// ra and fp, so they can be reached with small offsets no matter how big the whole frame is
		const int64_t save_area_size = STACK_ALIGNMENT;
		int64_t remaining_stack_size = padded_stack_size - save_area_size;

		// build prologue -------------

		// allocate stack space for saved registers
		prologue.write_addi(Register::sp, Register::sp, -save_area_size);
		// save return address
		this->write_store(prologue, Register::sp, Register::ra, save_area_size - this->word_size, this->word_size);
		// save caller frame pointer
		this->write_store(prologue, Register::sp, Register::fp, save_area_size - 2 * this->word_size, this->word_size);
		// set new frame pointer
		prologue.write_addi(Register::fp, Register::sp, save_area_size);
		// allocate the rest of the stack space
		if (fits_imm12(-remaining_stack_size))
		{
			if (remaining_stack_size != 0)
				prologue.write_addi(Register::sp, Register::sp, -remaining_stack_size);
		}
		else
		{
			// too far for an addi, the scratch register is free since nothing has run yet
			if (remaining_stack_size > INT32_MAX)
				throw CompilerError::unimplemented("RISC-V codegen: stack frames over 2GiB");
			prologue.load_immediate(SCRATCH, -remaining_stack_size);
			prologue.write_add(Register::sp, Register::sp, SCRATCH);
		}

		// build epilogue -------------

		// deallocate stack space, down to the saved registers (which have to stay above sp until they've been
		// read, anything below it can be overwritten at any moment, ie by a signal handler)
		epilogue.write_addi(Register::sp, Register::fp, -save_area_size);
		// restore return address
		this->write_load(epilogue, Register::ra, Register::sp, save_area_size - this->word_size, this->word_size, true);
		// restore caller frame pointer
		this->write_load(epilogue, Register::fp, Register::sp, save_area_size - 2 * this->word_size, this->word_size, true);
		// deallocate stack space for saved registers
		epilogue.write_addi(Register::sp, Register::sp, save_area_size);
		// return
		epilogue.write_jalr(Register::zero, Register::ra, 0);

		// finalize -------------

		// call sites are relative to the body, make them relative to the object
		size_t body_start = code.size() + prologue.cur_offset();
		for (const auto &[pos, callee] : this->call_backpatch_list)
			this->add_call_fixup(body_start + this->body.offset_of(pos), callee);

		// write to .text
		prologue.dump_to_bytes(code);
		this->body.dump_to_bytes(code);
		epilogue.dump_to_bytes(code);
	}

	void RiscvCodeGenerator::patch_call(std::vector<uint8_t> &code, size_t call_offset, size_t target_offset)
	{
		Assembler::patch_call(code, call_offset, target_offset);
	}

	std::vector<uint8_t> RiscvCodeGenerator::build_runtime_code(uint64_t main_offset, Target t)
	{
		if (t.os == Target::OS::Linux)
		{
			// linux kernel guarantees 16-byte alignment on entry, so no need to align here

			Assembler code(this->xlen, t.has(Target::RiscvExt::C));
			// call main
			size_t call_pos = code.write_call_placeholder(Register::ra);
			// return value still in a0, will leave it there
			// call linux exit syscall
			code.write_addi(Register::a7, Register::zero, 93);
			code.write_ecall();

			std::vector<uint8_t> bytes;
			code.dump_to_bytes(bytes);

			// main will shift by the size of this code, since it gets prepended
			this->patch_call(bytes, code.offset_of(call_pos), bytes.size() + main_offset);
			return bytes;
		}
		else
		{
			throw CompilerError::unsupported("Unsupported runtime operating system for RISC-V");
		}
		return std::vector<uint8_t>();
	}
}
