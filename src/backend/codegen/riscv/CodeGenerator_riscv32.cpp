#include "CodeGenerator_riscv32.hpp"

#include <format>
#include <vector>
#include <set>

#include "utils/logging.hpp"
#include "utils/error.hpp"
#include "ir/IR_instructions.hpp"

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
				std::array<uint8_t, 4> instr_bytes = std::bit_cast<std::array<uint8_t, 4>>(instr.data);
				bytes.insert(bytes.end(), instr_bytes.begin(), instr_bytes.end());
			}
		}

		/// push new add (add register) instruction, return its position
		size_t write_add(Register dest, Register op1, Register op2)
		{
			size_t pos = this->buf.size();
			this->buf.emplace_back<MachineInstruction>({
				.data = encode_r_type(0b0110011u, dest, 0u, op1, op2, 0u),
				.fmt = InstructionFormat::RType,
			});
			return pos;
		}

		/// push new sub (subtract register) instruction, return its position
		size_t write_sub(Register dest, Register op1, Register op2)
		{
			size_t pos = this->buf.size();
			this->buf.emplace_back<MachineInstruction>({
				.data = encode_r_type(0b0110011u, dest, 0u, op1, op2, 0x20u),
				.fmt = InstructionFormat::RType,
			});
			return pos;
		}

		/// push new mul (multiply lower) instruction, return its position
		size_t write_mul(Register dest, Register op1, Register op2)
		{
			size_t pos = this->buf.size();
			this->buf.emplace_back<MachineInstruction>({
				.data = encode_r_type(0b0110011u, dest, 0u, op1, op2, 0x01u),
				.fmt = InstructionFormat::RType,
			});
			return pos;
		}

		/// push new div (signed divide) instruction, return its position
		size_t write_div(Register dest, Register op1, Register op2)
		{
			size_t pos = this->buf.size();
			this->buf.emplace_back<MachineInstruction>({
				.data = encode_r_type(0b0110011u, dest, 0x4u, op1, op2, 0x01u),
				.fmt = InstructionFormat::RType,
			});
			return pos;
		}

		/// push new divu (unsigned divide) instruction, return its position
		size_t write_divu(Register dest, Register op1, Register op2)
		{
			size_t pos = this->buf.size();
			this->buf.emplace_back<MachineInstruction>({
				.data = encode_r_type(0b0110011u, dest, 0x5u, op1, op2, 0x01u),
				.fmt = InstructionFormat::RType,
			});
			return pos;
		}

		/// push new rem (signed division remainder) instruction, return its position
		size_t write_rem(Register dest, Register op1, Register op2)
		{
			size_t pos = this->buf.size();
			this->buf.emplace_back<MachineInstruction>({
				.data = encode_r_type(0b0110011u, dest, 0x6u, op1, op2, 0x01u),
				.fmt = InstructionFormat::RType,
			});
			return pos;
		}

		/// push new remu (unsigned division remainder) instruction, return its position
		size_t write_remu(Register dest, Register op1, Register op2)
		{
			size_t pos = this->buf.size();
			this->buf.emplace_back<MachineInstruction>({
				.data = encode_r_type(0b0110011u, dest, 0x7u, op1, op2, 0x01u),
				.fmt = InstructionFormat::RType,
			});
			return pos;
		}

		/// push new addi (add immediate) instruction, return its position
		size_t write_addi(Register dest, Register src, uint32_t imm)
		{
			size_t pos = this->buf.size();
			this->buf.emplace_back<MachineInstruction>({
				.data = encode_i_type(0b0010011u, dest, 0u, src, imm),
				.fmt = InstructionFormat::IType,
			});
			return pos;
		}

		/// push new lw (load word) instruction, return its position
		size_t write_lw(Register dest, Register addr, uint32_t addr_offset)
		{
			size_t pos = this->buf.size();
			this->buf.emplace_back<MachineInstruction>({
				.data = encode_i_type(0b0000011u, dest, 2u, addr, addr_offset),
				.fmt = InstructionFormat::IType,
			});
			return pos;
		}

		/// push new sw (store word) instruction, return its position
		size_t write_sw(Register dest_addr, Register src, uint32_t addr_offset)
		{
			size_t pos = this->buf.size();
			this->buf.emplace_back<MachineInstruction>({
				.data = encode_s_type(0b0100011u, 2u, dest_addr, src, addr_offset),
				.fmt = InstructionFormat::SType,
			});
			return pos;
		}

		/// push new jal (jump and link) instruction, return its position
		size_t write_jal(Register dest, uint32_t imm)
		{
			size_t pos = this->buf.size();
			this->buf.emplace_back<MachineInstruction>({
				.data = encode_j_type(0b1101111u, dest, imm),
				.fmt = InstructionFormat::JType,
			});
			return pos;
		}

		/// push new jalr (jump and link register) instruction, return its position
		size_t write_jalr(Register dest, Register addr, uint32_t addr_offset)
		{
			size_t pos = this->buf.size();
			this->buf.emplace_back<MachineInstruction>({
				.data = encode_i_type(0b1100111u, dest, 0u, addr, addr_offset),
				.fmt = InstructionFormat::IType,
			});
			return pos;
		}

		/// push new auipc (add upper immediate to pc) instruction, return its position
		size_t write_auipc(Register dest, uint32_t imm)
		{
			size_t pos = this->buf.size();
			this->buf.emplace_back<MachineInstruction>({
				.data = encode_u_type(0b0010111u, dest, imm),
				.fmt = InstructionFormat::UType,
			});
			return pos;
		}

		/// push new ecall (environment call) instruction, return its position
		size_t write_ecall()
		{
			size_t pos = this->buf.size();
			this->buf.emplace_back<MachineInstruction>({
				.data = encode_i_type(0b1110011u, Register::zero, 0u, Register::zero, 0u),
				.fmt = InstructionFormat::IType,
			});
			return pos;
		}

		/// push new nop instruction
		size_t write_nop()
		{
			return this->write_addi(Register::zero, Register::zero, 0u);
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
		code.write_lw(slot->physical, Register::fp, uint32_t(this->spilled_vreg_fp_offsets[vreg]));
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

	void CodeGenerator_riscv32::lower_function(const ir::Function &fn, Object &obj)
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

			ir::Instruction *cur_instr = bb->start;
			while (cur_instr != nullptr)
			{
				if (ir::instr::LoadImmInstruction *instr = dynamic_cast<ir::instr::LoadImmInstruction *>(cur_instr))
				{
					// load immmediate
					RegSlot *dest = this->load_dest_vreg(body, instr->dest);
					body.write_addi(dest->physical, Register::zero, instr->value);
				}
				else if (ir::instr::AddInstruction *instr = dynamic_cast<ir::instr::AddInstruction *>(cur_instr))
				{
					RegSlot *dest = this->load_dest_vreg(body, instr->dest);
					RegSlot *op1 = this->load_src_vreg(body, instr->op1);
					if (instr->op2.is_vreg())
					{
						// add register to register
						RegSlot *op2 = this->load_src_vreg(body, instr->op2.vreg_id);
						body.write_add(dest->physical, op1->physical, op2->physical);
					}
					else
					{
						// add immediate to register
						// TODO check type here
						uint32_t op2 = uint32_t(instr->op2.imm_value);
						body.write_addi(dest->physical, op1->physical, op2); // UNTESTED
					}
				}
				else if (ir::instr::SubtractInstruction *instr = dynamic_cast<ir::instr::SubtractInstruction *>(cur_instr))
				{
					RegSlot *dest = this->load_dest_vreg(body, instr->dest);
					RegSlot *op1 = this->load_src_vreg(body, instr->op1);
					if (instr->op2.is_vreg())
					{
						// subtact register from register
						RegSlot *op2 = this->load_src_vreg(body, instr->op2.vreg_id);
						body.write_sub(dest->physical, op1->physical, op2->physical);
					}
					else
					{
						// subtract immediate from register
						// TODO check type here
						uint32_t op2 = uint32_t(instr->op2.imm_value);
						// converting to negative to subtract
						op2 = static_cast<uint32_t>(-static_cast<int32_t>(op2));
						body.write_addi(dest->physical, op1->physical, op2); // UNTESTED
					}
				}
				else if (ir::instr::MultiplyInstruction *instr = dynamic_cast<ir::instr::MultiplyInstruction *>(cur_instr))
				{
					// TODO check for m extension
					RegSlot *dest = this->load_dest_vreg(body, instr->dest);
					RegSlot *op1 = this->load_src_vreg(body, instr->op1);
					RegSlot *op2;
					if (instr->op2.is_vreg())
					{
						// register to register
						op2 = this->load_src_vreg(body, instr->op2.vreg_id);
					}
					else
					{
						// immediate to register
						// TODO check type here
						op2 = this->get_empty_slot(body);
						body.write_addi(op2->physical, Register::zero, instr->op2.imm_value);
					}
					body.write_mul(dest->physical, op1->physical, op2->physical);
				}
				else if (ir::instr::DivideInstruction *instr = dynamic_cast<ir::instr::DivideInstruction *>(cur_instr))
				{
					// TODO check for m extension
					RegSlot *dest = this->load_dest_vreg(body, instr->dest);
					RegSlot *op1 = this->load_src_vreg(body, instr->op1);
					RegSlot *op2;
					if (instr->op2.is_vreg())
					{
						// register to register
						op2 = this->load_src_vreg(body, instr->op2.vreg_id);
					}
					else
					{
						// immediate to register
						// TODO check type here
						op2 = this->get_empty_slot(body);
						body.write_addi(op2->physical, Register::zero, instr->op2.imm_value); // UNCHECKED
					}
					// TODO use div if signed type
					body.write_divu(dest->physical, op1->physical, op2->physical);
				}
				else if (ir::instr::ModuloInstruction *instr = dynamic_cast<ir::instr::ModuloInstruction *>(cur_instr))
				{
					// TODO check for m extension
					RegSlot *dest = this->load_dest_vreg(body, instr->dest);
					RegSlot *op1 = this->load_src_vreg(body, instr->op1);
					RegSlot *op2;
					if (instr->op2.is_vreg())
					{
						// register to register
						op2 = this->load_src_vreg(body, instr->op2.vreg_id);
					}
					else
					{
						// immediate to register
						// TODO check type here
						op2 = this->get_empty_slot(body);
						body.write_addi(op2->physical, Register::zero, instr->op2.imm_value); // UNCHECKED
					}
					// TODO use div if signed type
					body.write_remu(dest->physical, op1->physical, op2->physical);
				}
				else if (ir::instr::ReturnInstruction *instr = dynamic_cast<ir::instr::ReturnInstruction *>(cur_instr))
				{
					// return
					if (instr->ret_value.has_value())
					{
						RegSlot *ret_value;
						if (instr->ret_value->type == ir::Operand::VREG)
						{
							ret_value = this->load_src_vreg(body, instr->ret_value->vreg_id);
						}
						else
						{
							ret_value = this->get_empty_slot(body);
							body.write_addi(ret_value->physical, Register::zero, uint32_t(instr->ret_value->imm_value));
						}
						body.write_addi(Register::a0, ret_value->physical, uint32_t(0));
					}
					size_t pos = body.write_jal(Register::zero, 0u);
					epilogue_backpatch_list.push_back(pos);
				}
				else
				{
					throw CompilerError::unimplemented("Uncaught instruction variant");
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
		log_vvvv("Backpatching offsets in body");

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

				instr.data &= bitmask_lower(12);
				instr.data |= imm_19_12 << 12;
				instr.data |= imm_11 << 20;
				instr.data |= imm_10_1 << 21;
				instr.data |= imm_20 << 31;
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

	Object CodeGenerator_riscv32::lower_ir(const IrObject &ir)
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

	uint32_t encode_r_type(uint32_t opcode, Register rd, uint32_t funct3, Register rs1, Register rs2, uint32_t funct7)
	{
		uint32_t instr = opcode & bitmask_lower(7);
		instr |= (uint32_t(rd) & bitmask_lower(5)) << 7;
		instr |= (funct3 & bitmask_lower(3)) << 12;
		instr |= (uint32_t(rs1) & bitmask_lower(5)) << 15;
		instr |= (uint32_t(rs2) & bitmask_lower(5)) << 20;
		instr |= (funct7 & bitmask_lower(7)) << 25;
		return instr;
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

	uint32_t encode_u_type(uint32_t opcode, Register rd, uint32_t imm)
	{
		uint32_t instr = opcode & bitmask_lower(7);
		instr |= (uint32_t(rd) & bitmask_lower(5)) << 7;
		instr |= imm & ~bitmask_lower(12);
		return instr;
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
