#include "Backend_RV32.hpp"

#include <format>

#include "utils/logging.hpp"
#include "utils/error.hpp"

namespace backends::rv32
{
	const uint8_t REGISTER_zero = 0;
	const uint8_t REGISTER_ra = 1;
	const uint8_t REGISTER_sp = 2;
	const uint8_t REGISTER_fp = 8;

	void Backend_RV32::CodeBuffer::write_label(std::string_view symbol)
	{
		this->labels.insert({std::string(symbol), this->cur_offset()});
	}

	void Backend_RV32::CodeBuffer::write_addi(uint8_t dest, uint8_t src, std::variant<uint32_t, std::string_view> imm)
	{
		MachineInstruction instr;
		instr.fmt = InstructionFormat::IType;

		uint32_t imm_value = 0;
		if (std::holds_alternative<uint32_t>(imm))
			imm_value = std::get<std::uint32_t>(imm);
		else
			instr.replace_label = std::string(std::get<std::string_view>(imm));

		instr.data = encode_i_type(0b0010011u, dest, 0x0u, src, imm_value);

		this->machine_code_buf.push_back(instr);
	}

	void Backend_RV32::CodeBuffer::write_lw(uint8_t dest, uint8_t addr, uint32_t addr_offset)
	{
		MachineInstruction instr;
		instr.fmt = InstructionFormat::IType;
		instr.data = encode_i_type(0b0000011u, dest, 0x2u, addr, addr_offset);
	}

	void Backend_RV32::CodeBuffer::write_sw(uint8_t addr, uint32_t addr_offset, uint8_t src)
	{
		MachineInstruction instr;
		instr.fmt = InstructionFormat::SType;
		instr.data = encode_s_type(0b0100011u, 0x2u, addr, src, addr_offset);
		this->machine_code_buf.push_back(instr);
	}

	void Backend_RV32::CodeBuffer::write_jal(uint8_t dest, std::variant<uint32_t, std::string_view> imm)
	{
		MachineInstruction instr;
		instr.fmt = InstructionFormat::JType;

		uint32_t imm_value = 0;
		if (std::holds_alternative<uint32_t>(imm))
			imm_value = std::get<std::uint32_t>(imm);
		else
			instr.replace_label = std::string(std::get<std::string_view>(imm));

		instr.data = encode_j_type(0b1101111u, dest, imm_value);

		this->machine_code_buf.push_back(instr);
	}

	void Backend_RV32::CodeBuffer::dump_to_bytes(std::vector<uint8_t> &bytes) const
	{
		bytes.reserve(4 * this->machine_code_buf.size());
		for (const MachineInstruction &instr : this->machine_code_buf)
		{
			std::array<uint8_t, 4> instr_bytes = std::bit_cast<std::array<uint8_t, 4>>(instr.data);
			bytes.insert(bytes.end(), instr_bytes.begin(), instr_bytes.end());
		}
	}

	Backend_RV32::RegSlot *Backend_RV32::load_src_vreg(CodeBuffer &code, ir::VRegId vreg)
	{
		// check if this vreg is already loaded somewhere
		for (RegSlot &slot : this->registers)
		{
			if (slot.occupied && slot.resident == vreg)
				return &slot;
		}

		// load it
		RegSlot *slot = this->get_empty_slot(code);
		code.write_lw(slot->physical, REGISTER_fp, uint32_t(this->spilled_vreg_fp_offsets[vreg]));
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
			int32_t fp_offset = preexisting->second;
		}
		else
		{
			int32_t fp_offset = -4 * (this->spilled_vreg_fp_offsets.size() + 2);
			this->spilled_vreg_fp_offsets.insert({slot.resident, fp_offset});
		}
		code.write_sw(REGISTER_fp, uint32_t(fp_offset), slot.physical);
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
		size_t victim_index = this->next_to_spill;
		this->next_to_spill = (this->next_to_spill + 1) % this->registers.size();

		RegSlot *victim = &this->registers.at(victim_index);

		// code.write_asm(std::format("\tsw   \t{}, {}(sp) # storing vreg {}", victim->name, victim->resident * -4, victim->resident));
		this->spill_slot(code, *victim);

		victim->occupied = false;
		victim->dirty = true;
		return victim;
	}

	void Backend_RV32::lower_function(ir::Function *fn, Object *obj)
	{
		log_vvv("Lowering function \"{}\"", fn->name);

		// resetting state
		this->stack_size = 8; // for saved fp and ra
		this->next_to_spill = 0;
		this->spilled_vreg_fp_offsets.clear();

		CodeBuffer prologue;
		CodeBuffer body;
		CodeBuffer epilogue;

		// this->body_asm += std::format("\n__{}_body:\n", fn->name);

		// prefix traversal of body
		const std::string epilogue_label(std::format("__{}_epilogue", fn->name));
		std::set<ir::BBlockId> seen;
		std::vector<ir::BasicBlock *> to_visit;
		to_visit.push_back(fn->entry);
		while (!to_visit.empty())
		{
			ir::BasicBlock *bb = to_visit.back();
			to_visit.pop_back();

			// this->body_asm += std::format("__{}_bb{}:{}\n", fn->name, bb->id, (bb->note.empty() ? "" : (" # " + bb->note)));

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
					RegSlot *dest = this->load_dest_vreg(body, instr->dest);
					body.write_addi(dest->physical, 0, instr->value);
				}
				else if (ir::instr::ReturnInstruction *instr = dynamic_cast<ir::instr::ReturnInstruction *>(cur_instr))
				{
					if (instr->ret_value.has_value())
					{
						RegSlot *ret_value = this->load_src_vreg(body, instr->ret_value.value());
						body.write_addi(REGISTER_ra, ret_value->physical, uint32_t(0));
						// TODO proper offset
						body.write_jal(REGISTER_zero, epilogue_label);
					}
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

		// build prologue -------------

		this->stack_size += (this->spilled_vreg_fp_offsets.size() * 4);
		log_vvv("calculated stack size: {}", this->stack_size);
		int32_t padded_stack_size = ((this->stack_size + 15) / 16) * 16;
		log_vvv("padded stack size: {}", padded_stack_size);

		// allocate stack space
		prologue.write_addi(REGISTER_sp, REGISTER_sp, uint32_t(-padded_stack_size));
		// save return address
		prologue.write_sw(REGISTER_sp, uint32_t(padded_stack_size - 4), REGISTER_ra);
		// save caller frame pointer
		prologue.write_sw(REGISTER_sp, uint32_t(padded_stack_size - 8), REGISTER_fp);
		// set new frame pointer
		prologue.write_addi(REGISTER_fp, REGISTER_sp, uint32_t(padded_stack_size));

		// build epilogue -------------

		// restore caller frame pointer
		epilogue.write_lw(REGISTER_fp, uint32_t(padded_stack_size - 8), REGISTER_sp);
		// restore return address
		epilogue.write_lw(REGISTER_ra, uint32_t(padded_stack_size - 4), REGISTER_sp);

		// finalize function -------------

		prologue.dump_to_bytes(obj->text);
		body.dump_to_bytes(obj->text);
		epilogue.dump_to_bytes(obj->text);

		// register this function
		obj->functions.emplace_back(fn->name);
	}

	Object *Backend_RV32::lower_ir(IrObject *ir)
	{
		log_vv("Starting lowering to RV32");
		Object *obj = new Object();

		for (auto &[name, fn] : ir->functions)
		{
			this->lower_function(fn, obj);
		}

		return obj;
	}

	constexpr uint32_t bitmask_lower(size_t n)
	{
		if (n == 0)
			return 0;
		else if (n >= 32)
			return ~uint32_t(0); // all bits set
		else
			return (uint32_t(1) << n) - 1;
	}

	uint32_t encode_i_type(uint32_t opcode, uint32_t rd, uint32_t funct3, uint32_t rs1, uint32_t imm)
	{
		uint32_t instr = opcode & bitmask_lower(7);
		instr |= (rd & bitmask_lower(5)) << 7;
		instr |= (funct3 & bitmask_lower(3)) << 12;
		instr |= (rs1 & bitmask_lower(5)) << 15;
		instr |= (imm & bitmask_lower(12)) << 20;
		return instr;
	}

	uint32_t encode_s_type(uint32_t opcode, uint32_t funct3, uint32_t rs1, uint32_t rs2, uint32_t imm)
	{
		uint32_t imm_4_0 = imm & bitmask_lower(5);
		uint32_t imm_11_5 = (imm >> 5) & bitmask_lower(7);

		uint32_t instr = opcode & bitmask_lower(7);
		instr |= imm_4_0 << 7;
		instr |= (funct3 & bitmask_lower(3)) << 12;
		instr |= (rs1 & bitmask_lower(5)) << 15;
		instr |= (rs2 & bitmask_lower(5)) << 15;
		instr |= imm_11_5 << 25;
		return instr;
	}

	uint32_t encode_j_type(uint32_t opcode, uint32_t rd, uint32_t imm)
	{
		// scrambling immediate
		uint32_t imm_19_12 = (imm >> 12) & bitmask_lower(8);
		uint32_t imm_11 = (imm >> 11) & 0b1;
		uint32_t imm_10_1 = (imm >> 1) & bitmask_lower(10);
		uint32_t imm_20 = (imm >> 20) & 0b1;

		uint32_t inst = opcode & bitmask_lower(7);
		inst |= (rd & bitmask_lower(5)) << 7;
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
