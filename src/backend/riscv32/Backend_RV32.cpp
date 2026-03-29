#include "Backend_RV32.hpp"

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

namespace backends::rv32
{
	struct Backend_RV32::CodeBuffer
	{
		std::vector<MachineInstruction> buf;

		size_t cur_offset() const { return this->buf.size(); }

		/// Copy the contents of this buffer as bytes into a byte vector
		void dump_to_bytes(std::vector<uint8_t> &bytes) const;

		/// push new addi (add immediate) instruction, return its position
		size_t write_addi(Register dest, Register src, uint32_t imm);

		/// push new lw (load word) instruction, return its position
		size_t write_lw(Register dest, Register addr, uint32_t addr_offset);

		/// push new sw (store word) instruction, return its position
		size_t write_sw(Register dest_addr, Register src, uint32_t addr_offset);

		/// push new jal (jump and link) instruction, return its position
		size_t write_jal(Register dest, uint32_t imm);

		/// push new jalr (jump and link register) instruction, return its position
		size_t write_jalr(Register dest, Register addr, uint32_t addr_offset);

		/// push new nop instruction
		size_t write_nop();
	};

	void Backend_RV32::CodeBuffer::dump_to_bytes(std::vector<uint8_t> &bytes) const
	{
		bytes.reserve(4 * this->buf.size());
		for (const MachineInstruction &instr : this->buf)
		{
			std::array<uint8_t, 4> instr_bytes = std::bit_cast<std::array<uint8_t, 4>>(instr.data);
			bytes.insert(bytes.end(), instr_bytes.begin(), instr_bytes.end());
		}
	}

	size_t Backend_RV32::CodeBuffer::write_addi(Register dest, Register src, uint32_t imm)
	{
		size_t pos = this->buf.size();
		this->buf.emplace_back<MachineInstruction>({
			.data = encode_i_type(0b0010011u, dest, 0u, src, imm),
			.fmt = InstructionFormat::IType,
		});
		return pos;
	}

	size_t Backend_RV32::CodeBuffer::write_lw(Register dest, Register addr, uint32_t imm)
	{
		size_t pos = this->buf.size();
		this->buf.emplace_back<MachineInstruction>({
			.data = encode_i_type(0b0000011u, dest, 2u, addr, imm),
			.fmt = InstructionFormat::IType,
		});
		return pos;
	}

	size_t Backend_RV32::CodeBuffer::write_sw(Register dest_addr, Register src, uint32_t addr_offset)
	{
		size_t pos = this->buf.size();
		this->buf.emplace_back<MachineInstruction>({
			.data = encode_s_type(0b0100011u, 2u, dest_addr, src, addr_offset),
			.fmt = InstructionFormat::SType,
		});
		return pos;
	}

	size_t Backend_RV32::CodeBuffer::write_jal(Register dest, uint32_t imm)
	{
		size_t pos = this->buf.size();
		this->buf.emplace_back<MachineInstruction>({
			.data = encode_j_type(0b1101111u, dest, imm),
			.fmt = InstructionFormat::JType,
		});
		return pos;
	}

	size_t Backend_RV32::CodeBuffer::write_jalr(Register dest, Register addr, uint32_t imm)
	{
		size_t pos = this->buf.size();
		this->buf.emplace_back<MachineInstruction>({
			.data = encode_i_type(0b1100111u, dest, 0u, addr, imm),
			.fmt = InstructionFormat::IType,
		});
		return pos;
	}

	size_t Backend_RV32::CodeBuffer::write_nop()
	{
		return this->write_addi(Register::zero, Register::zero, 0u);
	}

	Backend_RV32::RegSlot *Backend_RV32::load_src_vreg(CodeBuffer &code, ir::VRegId vreg)
	{
		// check if this vreg is already loaded somewhere
		for (RegSlot &slot : this->registers)
		{
			if (slot.occupied && slot.resident == vreg)
				return &slot;
		}

		// load it from stack
		RegSlot *slot = this->get_empty_slot(code);
		code.write_lw(slot->physical, Register::fp, uint32_t(this->spilled_vreg_fp_offsets[vreg]));
		slot->resident = vreg;
		slot->occupied = true;
		return slot;
	}

	Backend_RV32::RegSlot *Backend_RV32::load_dest_vreg(CodeBuffer &code, ir::VRegId vreg)
	{
		RegSlot *reg = this->get_empty_slot(code);
		reg->resident = vreg;
		reg->occupied = true;
		reg->dirty = true;
		return reg;
	}

	void Backend_RV32::spill_slot(CodeBuffer &code, RegSlot &slot)
	{
		int32_t fp_offset;
		auto preexisting = this->spilled_vreg_fp_offsets.find(slot.resident);
		if (preexisting != this->spilled_vreg_fp_offsets.end())
		{
			fp_offset = preexisting->second;
		}
		else
		{
			fp_offset = -4 * (this->spilled_vreg_fp_offsets.size() + 4);
			this->spilled_vreg_fp_offsets.insert({slot.resident, fp_offset});
		}
		code.write_sw(Register::fp, slot.physical, uint32_t(fp_offset));
	}

	Backend_RV32::RegSlot *Backend_RV32::get_empty_slot(CodeBuffer &code)
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

	void Backend_RV32::lower_function(const ir::Function &fn, Object &obj)
	{
		log_vvv("Lowering function \"{}\"", fn.name);

		// resetting state
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
		log_vvv("Building function body");
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

			ir::Instruction *cur_instr = bb->start;
			while (cur_instr != nullptr)
			{
				if (ir::instr::LoadImmInstruction *instr = dynamic_cast<ir::instr::LoadImmInstruction *>(cur_instr))
				{
					// load immmediate
					RegSlot *dest = this->load_dest_vreg(body, instr->dest);
					body.write_addi(dest->physical, Register::zero, instr->value);
				}
				else if (ir::instr::ReturnInstruction *instr = dynamic_cast<ir::instr::ReturnInstruction *>(cur_instr))
				{
					// return
					if (instr->ret_value.has_value())
					{
						RegSlot *ret_value = this->load_src_vreg(body, instr->ret_value.value());
						body.write_addi(Register::a0, ret_value->physical, uint32_t(0));
					}
					size_t pos = body.write_jal(Register::zero, 0u);
					epilogue_backpatch_list.push_back(pos);
				}
				else
				{
					throw UnimplementedError("Uncaught instruction variant");
				}

				cur_instr = cur_instr->next;
			}

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
		log_vvv("Backpatching offsets in body");

		// TODO basic block replacements

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

				instr.data &= bitmask_lower(20);
				instr.data |= imm_11_0 << 20;
				break;
			}
			case InstructionFormat::SType:
			{
				// imm[11:5] is at [31:25], imm[4:0] is at [11:7]
				uint32_t imm_11_5 = (rel_offset >> 5) & bitmask_lower(7);
				uint32_t imm_4_0 = rel_offset & bitmask_lower(5);

				instr.data &= 0b00000001111111111111000001111111u;
				instr.data |= imm_11_5 << 25;
				instr.data |= imm_4_0 << 7;
				break;
			}
			case InstructionFormat::BType:
			{
				throw UnimplementedError("TODO B-type instructions");
				break;
			}
			case InstructionFormat::UType:
			{
				throw UnimplementedError("TODO U-type instructions");
				break;
			}
			case InstructionFormat::JType:
			{
				/// immediate is crazy scrambled, rtfm
				uint32_t imm_20 = (rel_offset >> 20) & 0b1;
				uint32_t imm_19_12 = (rel_offset >> 12) & bitmask_lower(8);
				uint32_t imm_11 = (rel_offset >> 11) & 0b1;
				uint32_t imm_10_1 = (rel_offset >> 1) & bitmask_lower(10);

				instr.data &= bitmask_lower(12);
				instr.data |= imm_19_12 << 12;
				instr.data |= imm_11 << 20;
				instr.data |= imm_10_1 << 21;
				instr.data |= imm_20 << 31;
				break;
			}
			case InstructionFormat::RType:
			{
				throw InternalError("Cannot backpatch R-type instruction");
			}
			}

			// TODO handle large immediates
		}

		log_vvv("Building prologue and epilogue");

		// build prologue -------------

		this->stack_size += (this->spilled_vreg_fp_offsets.size() * 4);
		log_vvv("calculated stack size: {}", this->stack_size);
		int32_t padded_stack_size = ((this->stack_size + 15) / 16) * 16;
		log_vvv("padded stack size: {}", padded_stack_size);

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

		prologue.dump_to_bytes(obj.text);
		body.dump_to_bytes(obj.text);
		epilogue.dump_to_bytes(obj.text);

		// register this function
		obj.functions.emplace_back(fn.name);

		log_vvv("Function \"{}\" done, object is now {} bytes", fn.name, obj.text.size());
	}

	Object Backend_RV32::lower_ir(const IrObject &ir)
	{
		log_vv("Starting lowering to RV32");
		Object obj;

		for (const auto &[name, fn] : ir.functions)
		{
			this->lower_function(*fn, obj);
		}

		return obj;
	}

	uint32_t encode_i_type(uint32_t opcode, Register rd, uint32_t funct3, Register rs1, uint32_t imm)
	{
		uint32_t instr = opcode & bitmask_lower(7);
		instr |= (uint32_t(rd) & bitmask_lower(5)) << 7;
		instr |= (funct3 & bitmask_lower(3)) << 12;
		instr |= (uint32_t(rs1) & bitmask_lower(5)) << 15;
		instr |= (imm & bitmask_lower(12)) << 20;
		return instr;
	}

	uint32_t encode_s_type(uint32_t opcode, uint32_t funct3, Register rs1, Register rs2, uint32_t imm)
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

	uint32_t encode_j_type(uint32_t opcode, Register rd, uint32_t imm)
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

	/*uint32_t encode_b_type(uint32_t opcode, uint32_t funct3, uint32_t rs1, uint32_t rs2, int32_t imm)
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
