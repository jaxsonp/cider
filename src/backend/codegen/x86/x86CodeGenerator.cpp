#include "x86CodeGenerator.hpp"

#include <algorithm>
#include <format>
#include <type_traits>

#include "utils/logging.hpp"
#include "utils/error.hpp"

namespace codegen
{
	using x86::Assembler;
	using x86::Imm;
	using x86::Mem;
	using x86::Reg;
	using x86::Reg32;
	using x86::Reg64;
	using x86::Width;

	namespace x86
	{
		// every value has to be usable as an 8 bit register, so only registers with an 8 bit form are allocated. in
		// 32 bit mode those are a, b, c and d (esi and edi have none)
		// TODO register classes in the allocator, so that esi/edi can hold values that are never used as bytes

		/// Registers handed to the register allocator on x86, in order of priority
		static const std::tuple<const PhysReg, const bool> X86_ALLOCATABLE_REGISTERS[] = {
			{PhysReg(Register::RAX), false},
			{PhysReg(Register::RCX), false},
			{PhysReg(Register::RDX), false},
			{PhysReg(Register::RBX), true},
		};

		/// Registers handed to the register allocator on x86-64, in order of priority. rax, rcx and rdx come last
		/// since division and shifts need them
		static const std::tuple<const PhysReg, const bool> X64_ALLOCATABLE_REGISTERS[] = {
			{PhysReg(Register::R10), false},
			{PhysReg(Register::R11), false},
			{PhysReg(Register::RSI), false},
			{PhysReg(Register::RDI), false},
			{PhysReg(Register::R8), false},
			{PhysReg(Register::R9), false},
			{PhysReg(Register::RAX), false},
			{PhysReg(Register::RCX), false},
			{PhysReg(Register::RDX), false},
			{PhysReg(Register::RBX), true},
			{PhysReg(Register::R12), true},
			{PhysReg(Register::R13), true},
			{PhysReg(Register::R14), true},
			{PhysReg(Register::R15), true},
		};

		/// Registers the first arguments are passed in on x86-64, in order
		static constexpr Register X64_ARG_REGISTERS[] = {
			Register::RDI,
			Register::RSI,
			Register::RDX,
			Register::RCX,
			Register::R8,
			Register::R9,
		};

		/// Stack pointer alignment required (at calls) by both ABIs, in bytes
		static constexpr int64_t STACK_ALIGNMENT = 16;

		static std::span<const std::tuple<const PhysReg, const bool>> get_registers(const Target &target)
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

		/// @brief Calls `f` with the width as a compile time constant (a `std::integral_constant`), so that it can
		/// build operands of that width. `f` is instantiated for every width, so anything that only exists for
		/// some widths has to be behind an `if constexpr`
		template <typename F>
		static void dispatch(Width width, F &&f)
		{
			switch (width)
			{
			case Width::W8:
				f(std::integral_constant<Width, Width::W8>{});
				return;
			case Width::W16:
				f(std::integral_constant<Width, Width::W16>{});
				return;
			case Width::W32:
				f(std::integral_constant<Width, Width::W32>{});
				return;
			case Width::W64:
				f(std::integral_constant<Width, Width::W64>{});
				return;
			}
			throw CompilerError::internal("Uncaught Width variant");
		}

		/// @brief Condition under which a comparison is true
		static Cond condition_of(ir::BinaryOp op, bool is_signed)
		{
			switch (op)
			{
			case ir::BinaryOp::CmpEq:
				return Cond::E;
			case ir::BinaryOp::CmpNe:
				return Cond::NE;
			case ir::BinaryOp::CmpGt:
				return is_signed ? Cond::G : Cond::A;
			case ir::BinaryOp::CmpGte:
				return is_signed ? Cond::GE : Cond::AE;
			case ir::BinaryOp::CmpLt:
				return is_signed ? Cond::L : Cond::B;
			case ir::BinaryOp::CmpLte:
				return is_signed ? Cond::LE : Cond::BE;
			default:
				throw CompilerError::internal("x86 codegen: not a comparison");
			}
		}
	}

	X86CodeGenerator::X86CodeGenerator(const Target &target)
		: target(target),
		  is_x64(target.arch == Target::Arch::X86_64),
		  word_size(target.register_width() / 8),
		  word(target.arch == Target::Arch::X86_64 ? Width::W64 : Width::W32),
		  regalloc(*this, x86::get_registers(target)),
		  body(target.arch == Target::Arch::X86_64)
	{
		if (target.arch != Target::Arch::X86 && target.arch != Target::Arch::X86_64)
			throw CompilerError::internal("Tried to initalize x86 code generator with non-x86 arch");
	}

	Width X86CodeGenerator::width_of(ir::VRegId vreg) const
	{
		return x86::width_of_size(this->cur_fn->vregs.at(vreg).get_size());
	}

	int32_t X86CodeGenerator::spill_bp_offset(size_t spill_index) const
	{
		// spill N lives at bp - word * (N + 1), right below the saved bp
		int64_t offset = -this->word_size * int64_t(spill_index + 1);
		if (offset != int64_t(int32_t(offset)))
			throw CompilerError::unimplemented("x86 codegen: stack frames over 2GiB");
		return int32_t(offset);
	}

	size_t X86CodeGenerator::register_arg_count(size_t arg_count) const
	{
		return this->is_x64 ? std::min(arg_count, MAX_REGISTER_ARGS_X64) : 0;
	}

	void X86CodeGenerator::extend_to_32(Register reg, ir::IrType type)
	{
		switch (type.get_size())
		{
		case 1:
			if (type.is_signed())
				this->body.write_movsx(Reg32(reg), x86::Reg8(reg));
			else
				this->body.write_movzx(Reg32(reg), x86::Reg8(reg));
			break;
		case 2:
			if (type.is_signed())
				this->body.write_movsx(Reg32(reg), x86::Reg16(reg));
			else
				this->body.write_movzx(Reg32(reg), x86::Reg16(reg));
			break;
		default:
			// already (at least) 32 bits
			break;
		}
	}

	void X86CodeGenerator::store_spilled_vreg(PhysReg src, ir::VRegId vreg, size_t spill_index)
	{
		int32_t offset = this->spill_bp_offset(spill_index);
		x86::dispatch(this->width_of(vreg), [&](auto w)
					  {
						  constexpr Width W = decltype(w)::value;
						  this->body.write_mov(Mem<W>(Register::RBP, offset), Reg<W>(Register(src))); });
	}

	void X86CodeGenerator::load_spilled_vreg(PhysReg dest, ir::VRegId vreg, size_t spill_index)
	{
		int32_t offset = this->spill_bp_offset(spill_index);
		x86::dispatch(this->width_of(vreg), [&](auto w)
					  {
						  constexpr Width W = decltype(w)::value;
						  this->body.write_mov(Reg<W>(Register(dest)), Mem<W>(Register::RBP, offset)); });
	}

	void X86CodeGenerator::move_reg(PhysReg dest, PhysReg src, ir::VRegId vreg)
	{
		// narrow values are moved as 32 bit ones, which is shorter and doesn't leave the rest of dest behind
		if (this->width_of(vreg) == Width::W64)
			this->body.write_mov(Reg64(Register(dest)), Reg64(Register(src)));
		else
			this->body.write_mov(Reg32(Register(dest)), Reg32(Register(src)));
	}

	void X86CodeGenerator::begin_function(const ir::Function &fn)
	{
		// values wider than a register would need to be split across two of them
		// TODO implement, so that 64 bit integers work on x86
		for (const auto &[vreg, type] : fn.vregs)
		{
			if (int64_t(type.get_size()) > this->word_size)
				throw CompilerError::unsupported(
					std::format("x86 codegen: type '{}' is wider than a register on this target, which is not supported yet (in function \"{}\")",
								type.to_string(), fn.name));
		}

		// resetting state
		this->stack_passed_args_size = 0;
		this->call_backpatch_list.clear();
		this->body.clear();
		this->body.set_record_text(this->asm_enabled());

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
		Register dest = reg_of(this->regalloc.load_dest_vreg(instr.dest));
		// only the bits that the type has matter
		switch (this->width_of(instr.dest))
		{
		case Width::W8:
			this->body.write_mov(x86::Reg8(dest), Imm<Width::W8>(int8_t(instr.value)));
			break;
		case Width::W16:
			this->body.write_mov(x86::Reg16(dest), Imm<Width::W16>(int16_t(instr.value)));
			break;
		case Width::W32:
			this->body.write_mov(Reg32(dest), Imm<Width::W32>(int32_t(instr.value)));
			break;
		case Width::W64:
			if (instr.value <= UINT32_MAX)
				// writing a 32 bit register clears the upper half, and has the shortest encoding
				this->body.write_mov(Reg32(dest), Imm<Width::W32>(int32_t(uint32_t(instr.value))));
			else if (int64_t(instr.value) >= INT32_MIN && int64_t(instr.value) <= INT32_MAX)
				// sign extended 32 bit immediate
				this->body.write_mov(Reg64(dest), Imm<Width::W64>(int32_t(instr.value)));
			else
				this->body.write_mov(Reg64(dest), x86::Imm64(int64_t(instr.value)));
			break;
		}
	}

	void X86CodeGenerator::lower_binary_instr(const ir::BinaryInstruction &instr)
	{
		// division and shifts take operands in fixed registers, so they allocate their own
		switch (instr.op)
		{
		case ir::BinaryOp::Div:
		case ir::BinaryOp::Rem:
			this->lower_division(instr);
			return;
		case ir::BinaryOp::BitShl:
		case ir::BinaryOp::BitShr:
			this->lower_shift(instr);
			return;
		default:
			break;
		}

		ir::IrType op_type = this->cur_fn->vregs.at(instr.lhs);
		Register dest = reg_of(this->regalloc.load_dest_vreg(instr.dest));
		Register op1 = reg_of(this->regalloc.load_src_vreg(instr.lhs));
		Register op2 = reg_of(this->regalloc.load_src_vreg(instr.rhs));

		// x86 instructions overwrite their first operand, so the result is built in dest, starting from a copy of
		// op1. dest is never the same register as op1 or op2, since they're all locked while being loaded
		x86::dispatch(x86::width_of_size(op_type.get_size()), [&](auto w)
					  {
			constexpr Width W = decltype(w)::value;
			Reg<W> d(dest), a(op1), b(op2);
			switch (instr.op)
			{
			case ir::BinaryOp::Add:
				this->body.write_mov(d, a);
				this->body.write_add(d, b);
				break;
			case ir::BinaryOp::Sub:
				this->body.write_mov(d, a);
				this->body.write_sub(d, b);
				break;
			case ir::BinaryOp::Mul:
				if constexpr (W == Width::W8)
				{
					// imul has no 8 bit form, but the lower 8 bits of a product only depend on the lower 8 bits of
					// its operands, so a 32 bit multiplication does the job
					this->body.write_mov(Reg32(dest), Reg32(op1));
					this->body.write_imul(Reg32(dest), Reg32(op2));
				}
				else
				{
					this->body.write_mov(d, a);
					this->body.write_imul(d, b);
				}
				break;
			case ir::BinaryOp::BitAnd:
				this->body.write_mov(d, a);
				this->body.write_and(d, b);
				break;
			case ir::BinaryOp::BitOr:
				this->body.write_mov(d, a);
				this->body.write_or(d, b);
				break;
			case ir::BinaryOp::BitXor:
				this->body.write_mov(d, a);
				this->body.write_xor(d, b);
				break;
			case ir::BinaryOp::CmpEq:
			case ir::BinaryOp::CmpNe:
			case ir::BinaryOp::CmpGt:
			case ir::BinaryOp::CmpGte:
			case ir::BinaryOp::CmpLt:
			case ir::BinaryOp::CmpLte:
				// the result is a bool, which only owns the lower byte of dest
				this->body.write_cmp(a, b);
				this->body.write_setcc(x86::condition_of(instr.op, op_type.is_signed()), x86::Reg8(dest));
				break;
			default:
				throw CompilerError::internal("Uncaught BinaryOp variant");
			} });
	}

	void X86CodeGenerator::lower_division(const ir::BinaryInstruction &instr)
	{
		ir::IrType op_type = this->cur_fn->vregs.at(instr.lhs);

		// division divides edx:eax by its operand, leaving the quotient in eax and the remainder in edx. so the
		// dividend goes in eax, and both get overwritten
		this->regalloc.evict(PhysReg(Register::RDX));
		this->regalloc.load_src_vreg_into(instr.lhs, PhysReg(Register::RAX), true);
		Register divisor = reg_of(this->regalloc.load_src_vreg(instr.rhs));

		// TODO division by 0 (and MIN / -1 for 32 and 64 bit values) traps, unlike on riscv
		if (op_type.get_size() == 8)
		{
			if (op_type.is_signed())
			{
				this->body.write_cqo();
				this->body.write_idiv(Reg64(divisor));
			}
			else
			{
				this->body.write_xor(Reg32(Register::RDX), Reg32(Register::RDX));
				this->body.write_div(Reg64(divisor));
			}
		}
		else
		{
			// narrower values are divided as 32 bit ones, after being extended (only the extended bits of the
			// divisor change, which are junk anyway). 8 bit division would put the remainder in ah, and both 8 and
			// 16 bit signed division would trap for MIN / -1, whose result doesn't fit
			this->extend_to_32(Register::RAX, op_type);
			this->extend_to_32(divisor, op_type);
			if (op_type.is_signed())
			{
				this->body.write_cdq();
				this->body.write_idiv(Reg32(divisor));
			}
			else
			{
				this->body.write_xor(Reg32(Register::RDX), Reg32(Register::RDX));
				this->body.write_div(Reg32(divisor));
			}
		}

		Register result = instr.op == ir::BinaryOp::Div ? Register::RAX : Register::RDX;
		this->regalloc.claim(PhysReg(result), instr.dest);
	}

	void X86CodeGenerator::lower_shift(const ir::BinaryInstruction &instr)
	{
		ir::IrType op_type = this->cur_fn->vregs.at(instr.lhs);

		// the shift amount has to be in cl. only its lower bits matter, the processor masks it to 5 bits (6 when
		// shifting a 64 bit value), even for 8 and 16 bit values. that's the same as on every other target
		this->regalloc.load_src_vreg_into(instr.rhs, PhysReg(Register::RCX), false);
		Register dest = reg_of(this->regalloc.load_dest_vreg(instr.dest));
		Register value = reg_of(this->regalloc.load_src_vreg(instr.lhs));

		x86::dispatch(x86::width_of_size(op_type.get_size()), [&](auto w)
					  {
			constexpr Width W = decltype(w)::value;
			this->body.write_mov(Reg<W>(dest), Reg<W>(value));
			if (instr.op == ir::BinaryOp::BitShl)
				this->body.write_shl_cl(Reg<W>(dest));
			else if (op_type.is_signed())
				this->body.write_sar_cl(Reg<W>(dest));
			else
				this->body.write_shr_cl(Reg<W>(dest)); });
	}

	void X86CodeGenerator::lower_unary_instr(const ir::UnaryInstruction &instr)
	{
		Register dest = reg_of(this->regalloc.load_dest_vreg(instr.dest));
		Register src = reg_of(this->regalloc.load_src_vreg(instr.src));

		x86::dispatch(this->width_of(instr.src), [&](auto w)
					  {
			constexpr Width W = decltype(w)::value;
			this->body.write_mov(Reg<W>(dest), Reg<W>(src));
			switch (instr.op)
			{
			case ir::UnaryOp::Neg:
				this->body.write_neg(Reg<W>(dest));
				break;
			case ir::UnaryOp::BitNot:
				this->body.write_not(Reg<W>(dest));
				break;
			default:
				throw CompilerError::internal("Uncaught UnaryOp variant");
			} });
	}

	void X86CodeGenerator::lower_load_arg_instr(const ir::LoadArgInstruction &instr)
	{
		if (this->is_x64 && instr.index < MAX_REGISTER_ARGS_X64)
		{
			// the argument is already sitting in its register, so just claim that register for the vreg. this only
			// works while nothing else has been allocated there, ie at the very start of the function
			this->regalloc.claim(PhysReg(x86::X64_ARG_REGISTERS[instr.index]), instr.dest);
			return;
		}

		// stack-passed arguments sit just above the return address. every argument takes a word sized slot,
		// and x86 is little endian, so a narrow argument is the first bytes of its slot
		size_t stack_index = instr.index - this->register_arg_count(instr.index);
		int64_t bp_offset = this->word_size * int64_t(stack_index + 2);
		if (bp_offset > INT32_MAX)
			throw CompilerError::unimplemented("x86 codegen: stack frames over 2GiB");

		Register dest = reg_of(this->regalloc.load_dest_vreg(instr.dest));
		x86::dispatch(this->width_of(instr.dest), [&](auto w)
					  {
						  constexpr Width W = decltype(w)::value;
						  this->body.write_mov(Reg<W>(dest), Mem<W>(Register::RBP, int32_t(bp_offset))); });
	}

	void X86CodeGenerator::lower_call(const ir::CallInstruction &instr)
	{
		// every allocatable register is caller saved, so everything live has to go to the stack first
		this->regalloc.spill_all();

		// now every vreg is on the stack. loading arguments straight from there avoids having to shuffle
		// values between registers

		// stack-passed arguments go to the bottom of our frame, where the callee finds them right above its return
		// address. done first, through eax, since the argument registers are about to be overwritten anyway
		size_t register_args = this->register_arg_count(instr.args.size());
		int64_t stack_args_size = this->word_size * int64_t(instr.args.size() - register_args);
		if (stack_args_size > INT32_MAX)
			throw CompilerError::unimplemented("x86 codegen: stack frames over 2GiB");
		this->stack_passed_args_size = std::max(this->stack_passed_args_size, stack_args_size);
		for (size_t i = register_args; i < instr.args.size(); ++i)
		{
			this->regalloc.load_spilled_vreg(PhysReg(Register::RAX), instr.args[i]);
			int32_t sp_offset = int32_t(this->word_size * int64_t(i - register_args));
			x86::dispatch(this->width_of(instr.args[i]), [&](auto w)
						  {
							  constexpr Width W = decltype(w)::value;
							  this->body.write_mov(Mem<W>(Register::RSP, sp_offset), Reg<W>(Register::RAX)); });
		}
		// then the register arguments
		for (size_t i = 0; i < register_args; ++i)
			this->regalloc.load_spilled_vreg(PhysReg(x86::X64_ARG_REGISTERS[i]), instr.args[i]);

		// call rel32 reaches anywhere within 2GiB, offsets are filled in once every function is lowered
		size_t pos = this->body.write_call_placeholder(asm_symbol(instr.callee));
		this->call_backpatch_list.push_back({pos, instr.callee});

		if (instr.dest.has_value())
			this->regalloc.claim(PhysReg(Register::RAX), instr.dest.value());
	}

	void X86CodeGenerator::lower_return(std::optional<ir::VRegId> ret_reg)
	{
		if (ret_reg.has_value())
		{
			Register value = reg_of(this->regalloc.load_src_vreg(ret_reg.value()));
			ir::IrType type = this->cur_fn->vregs.at(ret_reg.value());
			// the value leaves the compiler here, so it's extended to at least 32 bits like the ABI wants
			switch (type.get_size())
			{
			case 1:
				if (type.is_signed())
					this->body.write_movsx(Reg32(Register::RAX), x86::Reg8(value));
				else
					this->body.write_movzx(Reg32(Register::RAX), x86::Reg8(value));
				break;
			case 2:
				if (type.is_signed())
					this->body.write_movsx(Reg32(Register::RAX), x86::Reg16(value));
				else
					this->body.write_movzx(Reg32(Register::RAX), x86::Reg16(value));
				break;
			case 4:
				this->body.write_mov(Reg32(Register::RAX), Reg32(value));
				break;
			case 8:
				this->body.write_mov(Reg64(Register::RAX), Reg64(value));
				break;
			default:
				throw CompilerError::internal(std::format("x86 codegen: can't return a value of {} bytes", type.get_size()));
			}
		}
		else if (this->cur_fn->name == "main")
		{
			// main's return value becomes the exit code, so a void main must exit cleanly rather than with
			// whatever was left in eax
			this->body.write_xor(Reg32(Register::RAX), Reg32(Register::RAX));
		}

		// no need to spill anything, nothing in this function runs after a return. the epilogue is only 2 bytes,
		// so every return gets its own rather than jumping to a shared one
		this->body.write_leave();
		this->body.write_ret();
	}

	void X86CodeGenerator::finalize_function(const ir::Function &fn, std::vector<uint8_t> &code)
	{
		Assembler prologue = Assembler(this->is_x64, this->asm_enabled());

		// frame size -------------

		// spill slots, then outgoing stack arguments
		int64_t frame_size = int64_t(this->regalloc.spill_count()) * this->word_size + this->stack_passed_args_size;
		log_vvvv("calculated frame size: {}", frame_size);
		// sp is aligned right before the call that got here, then the return address and saved bp are pushed. so
		// the frame plus those two words has to keep it aligned
		int64_t aligned_size = frame_size + 2 * this->word_size;
		aligned_size = ((aligned_size + (x86::STACK_ALIGNMENT - 1)) / x86::STACK_ALIGNMENT) * x86::STACK_ALIGNMENT;
		int64_t padded_frame_size = aligned_size - 2 * this->word_size;
		log_vvvv("padded frame size: {}", padded_frame_size);
		if (padded_frame_size > INT32_MAX)
			throw CompilerError::unimplemented("x86 codegen: stack frames over 2GiB");

		// build prologue -------------

		x86::dispatch(this->word, [&](auto w)
					  {
			constexpr Width W = decltype(w)::value;
			if constexpr (W == Width::W32 || W == Width::W64)
			{
				// save caller base pointer
				prologue.write_push(Reg<W>(Register::RBP));
				// set new base pointer
				prologue.write_mov(Reg<W>(Register::RBP), Reg<W>(Register::RSP));
				// allocate stack space
				if (padded_frame_size != 0)
					prologue.write_sub(Reg<W>(Register::RSP), Imm<W>::checked(padded_frame_size));
			}
			else
				throw CompilerError::internal("x86 codegen: registers are 32 or 64 bits"); });

		// the epilogue (leave, ret) is part of every return

		// finalize -------------

		// call sites are relative to the body, make them relative to the object
		size_t body_start = code.size() + prologue.cur_offset();
		for (const auto &[pos, callee] : this->call_backpatch_list)
			this->add_call_fixup(body_start + pos, callee);

		// write to .text
		prologue.dump_to_bytes(code);
		this->body.dump_to_bytes(code);

		prologue.dump_asm(this->asm_out());
		this->body.dump_asm(this->asm_out());
	}

	void X86CodeGenerator::write_asm_directives(std::ostream &out) const
	{
		out << "\t.intel_syntax noprefix\n";
		out << (this->is_x64 ? "\t.code64\n" : "\t.code32\n");
	}

	void X86CodeGenerator::patch_call(std::vector<uint8_t> &code, size_t call_offset, size_t target_offset)
	{
		Assembler::patch_call(code, call_offset, target_offset);
	}

	std::vector<uint8_t> X86CodeGenerator::build_runtime_code(uint64_t main_offset, Target t)
	{
		if (t.os == Target::OS::Linux)
		{
			// linux kernel guarantees 16-byte alignment on entry, so no need to align here

			Assembler code(this->is_x64, this->asm_enabled());
			// call main
			size_t call_pos = code.write_call_placeholder(asm_symbol("main"));
			// return value is in eax, call linux exit syscall with it
			if (this->is_x64)
			{
				code.write_mov(Reg32(Register::RDI), Reg32(Register::RAX));
				code.write_mov(Reg32(Register::RAX), Imm<Width::W32>(60));
				code.write_syscall();
			}
			else
			{
				code.write_mov(Reg32(Register::RBX), Reg32(Register::RAX));
				code.write_mov(Reg32(Register::RAX), Imm<Width::W32>(1));
				code.write_int(0x80);
			}

			std::vector<uint8_t> bytes;
			code.dump_to_bytes(bytes);
			code.dump_asm(this->runtime_asm_out());

			// main will shift by the size of this code, since it gets prepended
			this->patch_call(bytes, call_pos, bytes.size() + main_offset);
			return bytes;
		}
		else
		{
			throw CompilerError::unsupported("Unsupported runtime operating system for x86");
		}
	}
}
