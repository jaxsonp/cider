#include "CodeGenerator_riscv32.hpp"

#include <format>
#include <vector>
#include <set>

#include "utils/logging.hpp"
#include "utils/error.hpp"

// helpers

constexpr uint32_t bitmask_lower(size_t n)
{
	if (n == 0)
		return 0;
	else if (n >= 32)
		return ~uint32_t(0); // all bits set
	else
		return (uint32_t(1) << n) - 1;
}

namespace codegen
{
	struct CodeGenerator_riscv32::CodeBuffer
	{
		std::vector<MachineInstruction> buf;

		size_t cur_offset() const { return this->buf.size(); }

		/// Copy the contents of this buffer as bytes into a byte vector
		void dump_to_bytes(std::vector<uint8_t> &bytes) const
		{
			bytes.reserve(4 * this->buf.size());
			for (const MachineInstruction &instr : this->buf)
			{
				std::array<uint8_t, 4> instr_bytes = std::bit_cast<std::array<uint8_t, 4>>(instr.encoded);
				bytes.insert(bytes.end(), instr_bytes.begin(), instr_bytes.end());
			}
		}

		/// push new add (add register) instruction, return its position
		size_t write_add(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0u, op1, op2, 0u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}

		/// push new sub (subtract register) instruction, return its position
		size_t write_sub(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0u, op1, op2, 0x20u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}

		/// push new xor (bitwise xor) instruction, return its position
		size_t write_xor(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0x4u, op1, op2, 0u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}

		/// push new or (bitwise or) instruction, return its position
		size_t write_or(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0x6u, op1, op2, 0u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}

		/// push new and (bitwise and) instruction, return its position
		size_t write_and(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0x7u, op1, op2, 0u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}

		/// push new sll (shift left logical) instruction, return its position
		size_t write_sll(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0x1u, op1, op2, 0x0u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}

		/// push new srl (shift right logical) instruction, return its position
		size_t write_srl(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0x5u, op1, op2, 0x0u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}

		/// push new sra (shift right arithmetic) instruction, return its position
		size_t write_sra(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0x5u, op1, op2, 0x20u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}

		/// push new slt (set if less than) instruction, return its position
		size_t write_slt(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0x2u, op1, op2, 0x00u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}

		/// push new slt (set if less than unsigned) instruction, return its position
		size_t write_sltu(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0x3u, op1, op2, 0x00u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}

		/// push new addi (add immediate) instruction, return its position
		size_t write_addi(Register dest, Register src, uint32_t imm)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_i_type(0b0010011u, dest, 0u, src, imm),
				.fmt = InstructionFormat::IType,
			});
			return this->buf.size() - 1;
		}

		/// push new xori (bitwise xor immediate) instruction, return its position
		size_t write_xori(Register dest, Register src, uint32_t imm)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_i_type(0b0010011u, dest, 0x4u, src, imm),
				.fmt = InstructionFormat::IType,
			});
			return this->buf.size() - 1;
		}

		/// push new ori (bitwise or immediate) instruction, return its position
		size_t write_ori(Register dest, Register src, uint32_t imm)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_i_type(0b0010011u, dest, 0x6u, src, imm),
				.fmt = InstructionFormat::IType,
			});
			return this->buf.size() - 1;
		}

		/// push new andi (bitwise and immediate) instruction, return its position
		size_t write_andi(Register dest, Register src, uint32_t imm)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_i_type(0b0010011u, dest, 0x7u, src, imm),
				.fmt = InstructionFormat::IType,
			});
			return this->buf.size() - 1;
		}

		/// push new slli (shift left logical immediate) instruction, return its position
		size_t write_slli(Register dest, Register src, uint32_t imm)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_i_type(0b0010011u, dest, 0x1u, src, imm & bitmask_lower(5)),
				.fmt = InstructionFormat::IType,
			});
			return this->buf.size() - 1;
		}

		/// push new srli (shift right logical immediate) instruction, return its position
		size_t write_srli(Register dest, Register src, uint32_t imm)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_i_type(0b0010011u, dest, 0x5u, src, imm & bitmask_lower(5)),
				.fmt = InstructionFormat::IType,
			});
			return this->buf.size() - 1;
		}

		/// push new srai (shift right arithmetic immediate) instruction, return its position
		size_t write_srai(Register dest, Register src, uint32_t imm)
		{
			this->buf.emplace_back<MachineInstruction>({
				// srai is distinguished from slri by upper bits of immediate
				.encoded = encode_i_type(0b0010011u, dest, 0x5u, src, (imm & bitmask_lower(5)) | 0x400),
				.fmt = InstructionFormat::IType,
			});
			return this->buf.size() - 1;
		}

		/// push new slti (set if less than immediate) instruction, return its position
		size_t write_slti(Register dest, Register src, uint32_t imm)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_i_type(0b0010011u, dest, 0x2u, src, imm),
				.fmt = InstructionFormat::IType,
			});
			return this->buf.size() - 1;
		}

		/// push new sltiu (set if less than immediate unsigned) instruction, return its position
		size_t write_sltiu(Register dest, Register src, uint32_t imm)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_i_type(0b0010011u, dest, 0x3u, src, imm),
				.fmt = InstructionFormat::IType,
			});
			return this->buf.size() - 1;
		}

		/// push new lb (load byte) instruction, return its position
		size_t write_lb(Register dest, Register addr, uint32_t addr_offset)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_i_type(0b0000011u, dest, 0x0u, addr, addr_offset),
				.fmt = InstructionFormat::IType,
			});
			return this->buf.size() - 1;
		}

		/// push new lh (load half word) instruction, return its position
		size_t write_lh(Register dest, Register addr, uint32_t addr_offset)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_i_type(0b0000011u, dest, 0x1u, addr, addr_offset),
				.fmt = InstructionFormat::IType,
			});
			return this->buf.size() - 1;
		}

		/// push new lw (load word) instruction, return its position
		size_t write_lw(Register dest, Register addr, uint32_t addr_offset)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_i_type(0b0000011u, dest, 0x2u, addr, addr_offset),
				.fmt = InstructionFormat::IType,
			});
			return this->buf.size() - 1;
		}

		/// push new lbu (load byte unsigned) instruction, return its position
		size_t write_lbu(Register dest, Register addr, uint32_t addr_offset)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_i_type(0b0000011u, dest, 0x4u, addr, addr_offset),
				.fmt = InstructionFormat::IType,
			});
			return this->buf.size() - 1;
		}

		/// push new lhu (load half word unsigned) instruction, return its position
		size_t write_lhu(Register dest, Register addr, uint32_t addr_offset)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_i_type(0b0000011u, dest, 0x5u, addr, addr_offset),
				.fmt = InstructionFormat::IType,
			});
			return this->buf.size() - 1;
		}

		/// push new sb (store byte) instruction, return its position
		size_t write_sb(Register dest_addr, Register src, uint32_t addr_offset)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_s_type(0b0100011u, 0x0u, dest_addr, src, addr_offset),
				.fmt = InstructionFormat::SType,
			});
			return this->buf.size() - 1;
		}

		/// push new sh (store half word) instruction, return its position
		size_t write_sh(Register dest_addr, Register src, uint32_t addr_offset)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_s_type(0b0100011u, 0x1u, dest_addr, src, addr_offset),
				.fmt = InstructionFormat::SType,
			});
			return this->buf.size() - 1;
		}

		/// push new sw (store word) instruction, return its position
		size_t write_sw(Register dest_addr, Register src, uint32_t addr_offset)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_s_type(0b0100011u, 0x2u, dest_addr, src, addr_offset),
				.fmt = InstructionFormat::SType,
			});
			return this->buf.size() - 1;
		}

		/// push new jal (jump and link) instruction, return its position
		size_t write_jal(Register dest, uint32_t imm)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_j_type(0b1101111u, dest, imm),
				.fmt = InstructionFormat::JType,
			});
			return this->buf.size() - 1;
		}

		/// push new jalr (jump and link register) instruction, return its position
		size_t write_jalr(Register dest, Register addr, uint32_t addr_offset)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_i_type(0b1100111u, dest, 0u, addr, addr_offset),
				.fmt = InstructionFormat::IType,
			});
			return this->buf.size() - 1;
		}

		/// push new lui (load upper immediate) instruction, return its position
		///
		/// Uses the upper 20 bits of the 'imm' argument
		size_t write_lui(Register dest, uint32_t imm)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_u_type(0b0110111u, dest, imm),
				.fmt = InstructionFormat::UType,
			});
			return this->buf.size() - 1;
		}

		/// push new auipc (add upper immediate to pc) instruction, return its position
		///
		/// Uses the upper 20 bits of the 'imm' argument
		size_t write_auipc(Register dest, uint32_t imm)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_u_type(0b0010111u, dest, imm),
				.fmt = InstructionFormat::UType,
			});
			return this->buf.size() - 1;
		}

		/// push new ecall (environment call) instruction, return its position
		size_t write_ecall()
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_i_type(0b1110011u, Register::zero, 0u, Register::zero, 0u),
				.fmt = InstructionFormat::IType,
			});
			return this->buf.size() - 1;
		}

		/// push new ebreak (environment break) instruction, return its position
		size_t write_ebreak()
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_i_type(0b1110011u, Register::zero, 0u, Register::zero, 0x1u),
				.fmt = InstructionFormat::IType,
			});
			return this->buf.size() - 1;
		}

		/// push new nop instruction
		size_t write_nop()
		{
			return this->write_addi(Register::zero, Register::zero, 0u);
		}

		/// push new mul (multiply lower) instruction, return its position
		size_t write_mul(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0u, op1, op2, 0x01u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}

		/// push new mulh (multiply high signed) instruction, return its position
		size_t write_mulh(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0x1u, op1, op2, 0x01u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}

		/// push new mulhsu (multiply high signed*unsigned) instruction, return its position
		size_t write_mulhsu(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0x2u, op1, op2, 0x01u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}

		/// push new mulhu (multiply high unsigned) instruction, return its position
		size_t write_mulhu(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0x3u, op1, op2, 0x01u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}

		/// push new div (signed divide) instruction, return its position
		size_t write_div(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0x4u, op1, op2, 0x01u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}

		/// push new divu (unsigned divide) instruction, return its position
		size_t write_divu(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0x5u, op1, op2, 0x01u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}

		/// push new rem (signed division remainder) instruction, return its position
		size_t write_rem(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0x6u, op1, op2, 0x01u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}

		/// push new remu (unsigned division remainder) instruction, return its position
		size_t write_remu(Register dest, Register op1, Register op2)
		{
			this->buf.emplace_back<MachineInstruction>({
				.encoded = encode_r_type(0b0110011u, dest, 0x7u, op1, op2, 0x01u),
				.fmt = InstructionFormat::RType,
			});
			return this->buf.size() - 1;
		}
	};

	CodeGenerator_riscv32::RegSlot *CodeGenerator_riscv32::load_src_vreg(CodeBuffer &code, ir::VRegId vreg)
	{
		// check if this vreg is already loaded somewhere
		for (RegSlot &slot : this->registers)
		{
			if (slot.occupied && slot.resident == vreg)
				return &slot;
		}

		// load it from stack
		RegSlot *slot = this->get_empty_slot(code);
		uint32_t offset = uint32_t(this->spilled_vreg_fp_offsets[vreg]);
		ir::IrType vreg_type = this->cur_fn->vregs.at(vreg);
		switch (vreg_type.get_size())
		{
		case 1:
			if (vreg_type.is_signed())
				code.write_lb(slot->physical, Register::fp, offset);
			else
				code.write_lbu(slot->physical, Register::fp, offset);
			break;
		case 2:
			if (vreg_type.is_signed())
				code.write_lh(slot->physical, Register::fp, offset);
			else
				code.write_lhu(slot->physical, Register::fp, offset);
			break;
		default:
			code.write_lw(slot->physical, Register::fp, offset);
			break;
		}
		slot->resident = vreg;
		slot->occupied = true;
		return slot;
	}

	CodeGenerator_riscv32::RegSlot *CodeGenerator_riscv32::load_dest_vreg(CodeBuffer &code, ir::VRegId vreg)
	{
		RegSlot *reg = this->get_empty_slot(code);
		reg->resident = vreg;
		reg->occupied = true;
		reg->dirty = true;
		return reg;
	}

	void CodeGenerator_riscv32::spill_slot(CodeBuffer &code, RegSlot &slot)
	{
		int32_t fp_offset;
		ir::VRegId vreg_id = slot.resident;
		ir::IrType vreg_type = this->cur_fn->vregs.at(vreg_id);
		auto preexisting = this->spilled_vreg_fp_offsets.find(vreg_id);
		if (preexisting != this->spilled_vreg_fp_offsets.end())
		{
			fp_offset = preexisting->second;
		}
		else
		{
			fp_offset = -4 * (this->spilled_vreg_fp_offsets.size() + 4);
			this->spilled_vreg_fp_offsets.insert({vreg_id, fp_offset});
		}
		switch (vreg_type.get_size())
		{
		case 1:
			code.write_sb(Register::fp, slot.physical, uint32_t(fp_offset));
			break;
		case 2:
			code.write_sh(Register::fp, slot.physical, uint32_t(fp_offset));
			break;
		default:
			code.write_sw(Register::fp, slot.physical, uint32_t(fp_offset));
			break;
		}
		slot.dirty = false;
	}

	CodeGenerator_riscv32::RegSlot *CodeGenerator_riscv32::get_empty_slot(CodeBuffer &code)
	{
		// first check for non-occupied registers
		for (RegSlot &slot : this->registers)
		{
			if (!slot.occupied)
				return &slot;
		}

		// then check for non-dirty slots and evict
		for (RegSlot &slot : this->registers)
		{
			if (!slot.dirty)
			{
				slot.occupied = false;
				return &slot;
			}
		}

		// worst case: spill register

		// choose a victim >:)
		size_t victim_index = this->next_to_spill;
		this->next_to_spill = (this->next_to_spill + 1) % this->registers.size();
		RegSlot *victim = &this->registers.at(victim_index);

		this->spill_slot(code, *victim);

		victim->occupied = false;
		return victim;
	}

	void CodeGenerator_riscv32::truncate_reg(CodeBuffer &code, RegSlot *slot)
	{
		ir::IrType ir_type = this->cur_fn->vregs.at(slot->resident);
		unsigned int shift = (32u - 8u) * ir_type.get_size();
		if (shift == 0)
			return;
		code.write_slli(slot->physical, slot->physical, static_cast<uint32_t>(shift));
		if (ir_type.is_signed())
			code.write_srai(slot->physical, slot->physical, static_cast<uint32_t>(shift));
		else
			code.write_srli(slot->physical, slot->physical, static_cast<uint32_t>(shift));
	}

	void CodeGenerator_riscv32::lower_function(const ir::Function &fn, Object &obj)
	{
		log_vvv("Lowering function \"{}\"", fn.name);

		// resetting state
		this->cur_fn = &fn;
		this->stack_size = 8; // for saved fp and ra
		this->next_to_spill = 0;
		this->spilled_vreg_fp_offsets.clear();

		CodeBuffer prologue;
		CodeBuffer body;
		CodeBuffer epilogue;

		/// Remember offsets of basic blocks from the beginning of body
		std::unordered_map<ir::BBlockId, size_t> body_bb_offsets;

		/// Tasklist of instructions that need their immediates to be retrofitted with a basic block offset
		std::vector<std::tuple<size_t, ir::BBlockId>> bb_backpatch_list;

		/// Tasklist of instructions that need their immediates to be retrofitted with the epilogue's offset
		std::vector<size_t> epilogue_backpatch_list;

		// prefix traversal of body
		std::set<ir::BBlockId> seen;
		std::vector<ir::BasicBlock *> to_visit;
		to_visit.push_back(fn.entry);
		log_vvvv("Building function body");
		while (!to_visit.empty())
		{
			ir::BasicBlock *bb = to_visit.back();
			to_visit.pop_back();

			// skip basic blocks we have already emitted
			if (seen.contains(bb->id))
				continue;
			seen.insert(bb->id);

			// remember where this bb is
			body_bb_offsets.insert({bb->id, body.cur_offset()});

			// clearing register allocator slots
			for (RegSlot &slot : this->registers)
			{
				slot.occupied = false;
				slot.dirty = false;
			}

			// emit all instructions in basic block
			for (ir::Instruction &instr : bb->instructions)
			{
				switch (instr.opcode)
				{
				case ir::Op::LoadImm:
				{
					// load immmediate
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					uint32_t immediate = static_cast<uint32_t>(instr.data);
					// if 12th bit is 1, the below addi will sign extend the immediate to be negative, we can
					// cancel it out by adding the difference
					if ((immediate & (1 << 11)) != 0)
						immediate += (1 << 12);

					if (immediate > I_TYPE_IMMEDIATE_MAX_SIZE)
					{
						// this immediate is more than 12 bits, needs lui
						body.write_lui(dest->physical, immediate);
						body.write_addi(dest->physical, dest->physical, immediate);
					}
					else
					{
						body.write_addi(dest->physical, Register::zero, immediate);
					}
					break;
				}
				case ir::Op::Add:
				{
					// add register to register
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					RegSlot *op1 = this->load_src_vreg(body, instr.op1);
					RegSlot *op2 = this->load_src_vreg(body, instr.op2);
					body.write_add(dest->physical, op1->physical, op2->physical);
					break;
				}
				case ir::Op::Sub:
				{
					// subtact register from register
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					RegSlot *op1 = this->load_src_vreg(body, instr.op1);
					RegSlot *op2 = this->load_src_vreg(body, instr.op2);
					body.write_sub(dest->physical, op1->physical, op2->physical);
					break;
				}
				case ir::Op::Mul:
				{
					// multiply register to register
					// TODO check for m extension
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					RegSlot *op1 = this->load_src_vreg(body, instr.op1);
					RegSlot *op2 = this->load_src_vreg(body, instr.op2);
					body.write_mul(dest->physical, op1->physical, op2->physical);
					break;
				}
				case ir::Op::Div:
				{
					// divide register to register
					// TODO check for m extension
					ir::IrType op_type = this->cur_fn->vregs.at(instr.op1);
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					RegSlot *op1 = this->load_src_vreg(body, instr.op1);
					RegSlot *op2 = this->load_src_vreg(body, instr.op2);
					// division reads the whole register, so both operands must be truncated first
					this->truncate_reg(body, op1);
					this->truncate_reg(body, op2);
					if (op_type.is_signed())
						body.write_div(dest->physical, op1->physical, op2->physical);
					else
						body.write_divu(dest->physical, op1->physical, op2->physical);
					break;
				}
				case ir::Op::Rem:
				{
					// mod register to register
					// TODO check for m extension
					ir::IrType op_type = this->cur_fn->vregs.at(instr.op1);
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					RegSlot *op1 = this->load_src_vreg(body, instr.op1);
					RegSlot *op2 = this->load_src_vreg(body, instr.op2);
					// division reads the whole register, so both operands must be truncated first
					this->truncate_reg(body, op1);
					this->truncate_reg(body, op2);
					if (op_type.is_signed())
						body.write_rem(dest->physical, op1->physical, op2->physical);
					else
						body.write_remu(dest->physical, op1->physical, op2->physical);
					break;
				}
				case ir::Op::BitAnd:
				{
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					RegSlot *op1 = this->load_src_vreg(body, instr.op1);
					RegSlot *op2 = this->load_src_vreg(body, instr.op2);
					body.write_and(dest->physical, op1->physical, op2->physical);
					break;
				}
				case ir::Op::BitOr:
				{
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					RegSlot *op1 = this->load_src_vreg(body, instr.op1);
					RegSlot *op2 = this->load_src_vreg(body, instr.op2);
					body.write_or(dest->physical, op1->physical, op2->physical);
					break;
				}
				case ir::Op::BitXor:
				{
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					RegSlot *op1 = this->load_src_vreg(body, instr.op1);
					RegSlot *op2 = this->load_src_vreg(body, instr.op2);
					body.write_xor(dest->physical, op1->physical, op2->physical);
					break;
				}
				case ir::Op::BitShl:
				{
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					RegSlot *op1 = this->load_src_vreg(body, instr.op1);
					RegSlot *op2 = this->load_src_vreg(body, instr.op2);
					body.write_sll(dest->physical, op1->physical, op2->physical);
					break;
				}
				case ir::Op::BitShr:
				{
					ir::IrType op_type = this->cur_fn->vregs.at(instr.op1);
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					RegSlot *op1 = this->load_src_vreg(body, instr.op1);
					RegSlot *op2 = this->load_src_vreg(body, instr.op2);
					// the bits shifted in come from the upper bits, so the value must be truncated
					this->truncate_reg(body, op1);
					if (op_type.is_signed())
						body.write_sra(dest->physical, op1->physical, op2->physical);
					else
						body.write_srl(dest->physical, op1->physical, op2->physical);
					break;
				}
				case ir::Op::Neg:
				{
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					RegSlot *src = this->load_src_vreg(body, instr.op1);
					body.write_sub(dest->physical, Register::zero, src->physical);
					break;
				}
				case ir::Op::CmpEq:
				{
					ir::IrType op_type = this->cur_fn->vregs.at(instr.op1);
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					RegSlot *op1 = this->load_src_vreg(body, instr.op1);
					RegSlot *op2 = this->load_src_vreg(body, instr.op2);
					// the xor below is only zero for equal values if both are truncated the same way
					this->truncate_reg(body, op1);
					this->truncate_reg(body, op2);
					// compare the registers (always unsigned check, cus -123 is less than 1 but means not equal)
					body.write_xor(dest->physical, op1->physical, op2->physical);
					body.write_sltiu(dest->physical, dest->physical, 1);
					break;
				}
				case ir::Op::CmpNe:
				{
					ir::IrType op_type = this->cur_fn->vregs.at(instr.op1);
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					RegSlot *op1 = this->load_src_vreg(body, instr.op1);
					RegSlot *op2 = this->load_src_vreg(body, instr.op2);
					// the xor below is only zero for equal values if both are truncated the same way
					this->truncate_reg(body, op1);
					this->truncate_reg(body, op2);
					// compare the registers
					body.write_xor(dest->physical, op1->physical, op2->physical);
					// unequal exactly when the xor is nonzero, always an unsigned test (see CmpEq)
					body.write_sltu(dest->physical, Register::zero, dest->physical);
					break;
				}
				case ir::Op::CmpGt:
				{
					ir::IrType op_type = this->cur_fn->vregs.at(instr.op1);
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					RegSlot *op1 = this->load_src_vreg(body, instr.op1);
					RegSlot *op2 = this->load_src_vreg(body, instr.op2);
					// comparisons read the whole register, so both operands must be truncated first
					this->truncate_reg(body, op1);
					this->truncate_reg(body, op2);
					if (op_type.is_signed())
						body.write_slt(dest->physical, op2->physical, op1->physical);
					else
						body.write_sltu(dest->physical, op2->physical, op1->physical);
					break;
				}
				case ir::Op::CmpGte:
				{
					ir::IrType op_type = this->cur_fn->vregs.at(instr.op1);
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					RegSlot *op1 = this->load_src_vreg(body, instr.op1);
					RegSlot *op2 = this->load_src_vreg(body, instr.op2);
					// comparisons read the whole register, so both operands must be truncated first
					this->truncate_reg(body, op1);
					this->truncate_reg(body, op2);
					// check if less than
					if (op_type.is_signed())
						body.write_slt(dest->physical, op1->physical, op2->physical);
					else
						body.write_sltu(dest->physical, op1->physical, op2->physical);
					// negate
					body.write_xori(dest->physical, dest->physical, 1u);
					break;
				}
				case ir::Op::CmpLt:
				{
					ir::IrType op_type = this->cur_fn->vregs.at(instr.op1);
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					RegSlot *op1 = this->load_src_vreg(body, instr.op1);
					RegSlot *op2 = this->load_src_vreg(body, instr.op2);
					// comparisons read the whole register, so both operands must be truncated first
					this->truncate_reg(body, op1);
					this->truncate_reg(body, op2);
					if (op_type.is_signed())
						body.write_slt(dest->physical, op1->physical, op2->physical);
					else
						body.write_sltu(dest->physical, op1->physical, op2->physical);
					break;
				}
				case ir::Op::CmpLte:
				{
					ir::IrType op_type = this->cur_fn->vregs.at(instr.op1);
					RegSlot *dest = this->load_dest_vreg(body, instr.dest);
					RegSlot *op1 = this->load_src_vreg(body, instr.op1);
					RegSlot *op2 = this->load_src_vreg(body, instr.op2);
					// comparisons read the whole register, so both operands must be truncated first
					this->truncate_reg(body, op1);
					this->truncate_reg(body, op2);
					// check if greater than
					if (op_type.is_signed())
						body.write_slt(dest->physical, op2->physical, op1->physical);
					else
						body.write_sltu(dest->physical, op2->physical, op1->physical);
					// negate
					body.write_xori(dest->physical, dest->physical, 1u);
					break;
				}
				default:
					throw CompilerError::unimplemented("Unhandled instruction variant");
				}
			}

			// emit basic block terminator
			switch (bb->terminator.kind)
			{
			case ir::BasicBlockTerminator::RETURN:
			{
				if (bb->terminator.ret_reg.has_value())
				{
					ir::VRegId ret_vreg = bb->terminator.ret_reg.value();
					RegSlot *ret_value_slot = this->load_src_vreg(body, ret_vreg);
					// the value leaves the compiler here, so it has to be in its canonical form
					this->truncate_reg(body, ret_value_slot);
					body.write_addi(Register::a0, ret_value_slot->physical, 0u);
				}

				size_t pos = body.write_jal(Register::zero, 0u);
				epilogue_backpatch_list.push_back(pos);
				break;
			}
			default:
				throw CompilerError::unimplemented("Unhandled bb terminator kind variant");
			};

			// spilling dirty registers
			for (RegSlot &slot : this->registers)
			{
				if (slot.dirty)
					this->spill_slot(body, slot);
			}
		}

		// TEMP for debugging
		body.write_nop();
		body.write_nop();
		body.write_nop();
		body.write_nop();
		body.write_nop();
		body.write_nop();

		// now go back and fill in basic block offsets for instructions that need it
		log_vvvv("Backpatching offsets in body");

		size_t epilogue_offset = body.cur_offset();
		for (size_t instr_pos : epilogue_backpatch_list)
		{
			MachineInstruction &instr = body.buf.at(instr_pos);

			uint32_t rel_offset = 4 * (epilogue_offset - instr_pos); // always positive

			// backpatching the epilogue's relative offset into the isntruction
			switch (instr.fmt)
			{
			case InstructionFormat::IType:
			{
				// imm[11:0] is at [31:20]
				uint32_t imm_11_0 = rel_offset & bitmask_lower(12);

				instr.encoded &= bitmask_lower(20);
				instr.encoded |= imm_11_0 << 20;
				break;
			}
			case InstructionFormat::SType:
			{
				// imm[11:5] is at [31:25], imm[4:0] is at [11:7]
				uint32_t imm_11_5 = (rel_offset >> 5) & bitmask_lower(7);
				uint32_t imm_4_0 = rel_offset & bitmask_lower(5);

				instr.encoded &= 0b00000001111111111111000001111111u;
				instr.encoded |= imm_11_5 << 25;
				instr.encoded |= imm_4_0 << 7;
				break;
			}
			case InstructionFormat::BType:
			{
				throw CompilerError::unimplemented("TODO B-type instructions");
				break;
			}
			case InstructionFormat::UType:
			{
				throw CompilerError::unimplemented("TODO U-type instructions");
				break;
			}
			case InstructionFormat::JType:
			{
				/// immediate is crazy scrambled, rtfm
				uint32_t imm_20 = (rel_offset >> 20) & 0b1;
				uint32_t imm_19_12 = (rel_offset >> 12) & bitmask_lower(8);
				uint32_t imm_11 = (rel_offset >> 11) & 0b1;
				uint32_t imm_10_1 = (rel_offset >> 1) & bitmask_lower(10);

				instr.encoded &= bitmask_lower(12);
				instr.encoded |= imm_19_12 << 12;
				instr.encoded |= imm_11 << 20;
				instr.encoded |= imm_10_1 << 21;
				instr.encoded |= imm_20 << 31;
				break;
			}
			case InstructionFormat::RType:
			{
				throw CompilerError::internal("Cannot backpatch R-type instruction");
			}
			}

			// TODO handle large immediates
		}

		log_vvvv("Building prologue and epilogue");

		// build prologue -------------

		this->stack_size += (this->spilled_vreg_fp_offsets.size() * 4);
		log_vvvv("calculated stack size: {}", this->stack_size);
		int32_t padded_stack_size = ((this->stack_size + 15) / 16) * 16;
		log_vvvv("padded stack size: {}", padded_stack_size);

		// allocate stack space
		prologue.write_addi(Register::sp, Register::sp, uint32_t(-padded_stack_size));
		// save return address
		prologue.write_sw(Register::sp, Register::ra, uint32_t(padded_stack_size - 4));
		// save caller frame pointer
		prologue.write_sw(Register::sp, Register::fp, uint32_t(padded_stack_size - 8));
		// set new frame pointer
		prologue.write_addi(Register::fp, Register::sp, uint32_t(padded_stack_size));

		// build epilogue -------------

		// restore stack pointer/deallocate stack space
		epilogue.write_addi(Register::sp, Register::fp, 0u);
		// restore caller frame pointer
		epilogue.write_lw(Register::fp, Register::sp, uint32_t(-8));
		// restore return address
		epilogue.write_lw(Register::ra, Register::sp, uint32_t(-4));
		// return
		epilogue.write_jalr(Register::zero, Register::ra, 0u);

		// finalize -------------

		// register this function entry
		obj.functions.push_back(Object::Function{
			.name = fn.name,
			.code_offset = obj.code.size(),
		});

		// write to .text
		prologue.dump_to_bytes(obj.code);
		body.dump_to_bytes(obj.code);
		epilogue.dump_to_bytes(obj.code);

		log_vvv("Function \"{}\" done, object is now {} bytes", fn.name, obj.code.size());
	}

	Object CodeGenerator_riscv32::lower_ir(const ir::Object &ir)
	{
		log_vv("Starting lowering to RV32");
		Object obj;

		for (const auto &[name, fn] : ir.functions)
		{
			this->lower_function(*fn, obj);
		}

		return obj;
	}

	std::vector<uint8_t> CodeGenerator_riscv32::build_runtime_code(uint64_t main_offset, Target t)
	{
		if (t.os == Target::OS::Linux)
		{
			// linux kernel guarantees 16-byte alignment on entry, so no need to align here

			// main will shift 4 * 4 bytes for 4 isntructons
			uint32_t shifted_main_offset = uint32_t(main_offset) + 16;

			CodeBuffer code;
			// call main
			code.write_auipc(Register::ra, shifted_main_offset);
			code.write_jalr(Register::ra, Register::ra, shifted_main_offset);
			// return value still in a0, will leave it there
			// call linux exit syscall
			code.write_addi(Register::a7, Register::zero, 93);
			code.write_ecall();

			std::vector<uint8_t> bytes;
			code.dump_to_bytes(bytes);
			return bytes;
		}
		else
		{
			throw CompilerError::unsupported("Unsupported runtime operating system for RV32");
		}
		return std::vector<uint8_t>();
	}

	uint32_t CodeGenerator_riscv32::encode_r_type(uint32_t opcode, Register rd, uint32_t funct3, Register rs1, Register rs2, uint32_t funct7)
	{
		uint32_t instr = opcode & bitmask_lower(7);
		instr |= (uint32_t(rd) & bitmask_lower(5)) << 7;
		instr |= (funct3 & bitmask_lower(3)) << 12;
		instr |= (uint32_t(rs1) & bitmask_lower(5)) << 15;
		instr |= (uint32_t(rs2) & bitmask_lower(5)) << 20;
		instr |= (funct7 & bitmask_lower(7)) << 25;
		return instr;
	}

	uint32_t CodeGenerator_riscv32::encode_i_type(uint32_t opcode, Register rd, uint32_t funct3, Register rs1, uint32_t imm)
	{
		uint32_t instr = opcode & bitmask_lower(7);
		instr |= (uint32_t(rd) & bitmask_lower(5)) << 7;
		instr |= (funct3 & bitmask_lower(3)) << 12;
		instr |= (uint32_t(rs1) & bitmask_lower(5)) << 15;
		instr |= (imm & bitmask_lower(12)) << 20;
		return instr;
	}

	uint32_t CodeGenerator_riscv32::encode_s_type(uint32_t opcode, uint32_t funct3, Register rs1, Register rs2, uint32_t imm)
	{
		uint32_t imm_4_0 = imm & bitmask_lower(5);
		uint32_t imm_11_5 = (imm >> 5) & bitmask_lower(7);

		uint32_t instr = opcode & bitmask_lower(7);
		instr |= imm_4_0 << 7;
		instr |= (funct3 & bitmask_lower(3)) << 12;
		instr |= (uint32_t(rs1) & bitmask_lower(5)) << 15;
		instr |= (uint32_t(rs2) & bitmask_lower(5)) << 20;
		instr |= imm_11_5 << 25;
		return instr;
	}

	uint32_t CodeGenerator_riscv32::encode_j_type(uint32_t opcode, Register rd, uint32_t imm)
	{
		// scrambling immediate
		uint32_t imm_10_1 = (imm >> 1) & bitmask_lower(10);
		uint32_t imm_11 = (imm >> 11) & 0b1;
		uint32_t imm_19_12 = (imm >> 12) & bitmask_lower(8);
		uint32_t imm_20 = (imm >> 20) & 0b1;

		uint32_t inst = opcode & bitmask_lower(7);
		inst |= (uint32_t(rd) & bitmask_lower(5)) << 7;
		inst |= imm_19_12 << 12;
		inst |= imm_11 << 20;
		inst |= imm_10_1 << 21;
		inst |= imm_20 << 31;

		return inst;
	}

	uint32_t CodeGenerator_riscv32::encode_u_type(uint32_t opcode, Register rd, uint32_t imm)
	{
		uint32_t instr = opcode & bitmask_lower(7);
		instr |= (uint32_t(rd) & bitmask_lower(5)) << 7;
		instr |= imm & ~bitmask_lower(12);
		return instr;
	}

	/*uint32_t CodeGenerator_riscv32::encode_b_type(uint32_t opcode, uint32_t funct3, uint32_t rs1, uint32_t rs2, int32_t imm)
	{
		uint32_t inst = opcode;
		inst |= (funct3 << 12);
		inst |= (rs1 << 15);
		inst |= (rs2 << 20);

		// scramble immediate
		uint32_t b_12 = (imm >> 12) & 0x1;
		uint32_t b_11 = (imm >> 11) & 0x1;
		uint32_t b_10_5 = (imm >> 5) & 0x3F;
		uint32_t b_4_1 = (imm >> 1) & 0xF;

		inst |= (b_12 << 31);
		inst |= (b_10_5 << 25);
		inst |= (b_4_1 << 8);
		inst |= (b_11 << 7);

		return inst;
	}*/
}
