#include "RegAllocator.hpp"

#include <format>

#include "utils/error.hpp"

namespace codegen
{
	RegAllocator::RegAllocator(SpillHandler &spill_handler, const std::vector<PhysReg> &pool)
		: spill_handler(spill_handler)
	{
		this->registers.reserve(pool.size());
		for (PhysReg reg : pool)
			this->registers.emplace_back(reg);
	}

	void RegAllocator::start_function()
	{
		this->next_to_spill = 0;
		this->spilled_vreg_indices.clear();
		for (RegSlot &slot : this->registers)
		{
			slot.occupied = false;
			slot.dirty = false;
			slot.locked = false;
		}
	}

	void RegAllocator::start_block()
	{
		for (RegSlot &slot : this->registers)
		{
			slot.occupied = false;
			slot.dirty = false;
		}
	}

	void RegAllocator::start_instruction()
	{
		for (RegSlot &slot : this->registers)
			slot.locked = false;
	}

	RegAllocator::RegSlot *RegAllocator::load_src_vreg(ir::VRegId vreg)
	{
		// check if this vreg is already loaded somewhere
		for (RegSlot &slot : this->registers)
		{
			if (slot.occupied && slot.resident == vreg)
			{
				slot.locked = true;
				return &slot;
			}
		}

		// load it from stack
		RegSlot *slot = this->get_empty_slot();
		this->load_spilled_vreg(slot->physical, vreg);
		slot->resident = vreg;
		slot->occupied = true;
		slot->locked = true;
		return slot;
	}

	void RegAllocator::load_spilled_vreg(PhysReg dest, ir::VRegId vreg)
	{
		auto found = this->spilled_vreg_indices.find(vreg);
		if (found == this->spilled_vreg_indices.end())
			throw CompilerError::internal(std::format("Codegen: vreg %{} is neither in a register nor on the stack", vreg));
		this->spill_handler.load_spilled_vreg(dest, vreg, found->second);
	}

	RegAllocator::RegSlot &RegAllocator::slot_for(PhysReg reg)
	{
		for (RegSlot &slot : this->registers)
		{
			if (slot.physical == reg)
				return slot;
		}
		throw CompilerError::internal("Codegen: register is not managed by the register allocator");
	}

	RegAllocator::RegSlot *RegAllocator::load_dest_vreg(ir::VRegId vreg)
	{
		RegSlot *reg = this->get_empty_slot();
		reg->resident = vreg;
		reg->occupied = true;
		reg->dirty = true;
		reg->locked = true;
		return reg;
	}

	void RegAllocator::spill_slot(RegSlot &slot)
	{
		size_t spill_index;
		ir::VRegId vreg_id = slot.resident;
		auto preexisting = this->spilled_vreg_indices.find(vreg_id);
		if (preexisting != this->spilled_vreg_indices.end())
		{
			spill_index = preexisting->second;
		}
		else
		{
			// every spilled vreg gets its own slot, numbered in the order they were first spilled
			spill_index = this->spilled_vreg_indices.size();
			this->spilled_vreg_indices.insert({vreg_id, spill_index});
		}
		this->spill_handler.store_spilled_vreg(slot.physical, vreg_id, spill_index);
		slot.dirty = false;
	}

	void RegAllocator::spill_all()
	{
		for (RegSlot &slot : this->registers)
		{
			if (slot.dirty)
				this->spill_slot(slot);
			slot.occupied = false;
		}
	}

	void RegAllocator::claim(PhysReg reg, ir::VRegId vreg)
	{
		RegSlot &slot = this->slot_for(reg);
		if (slot.occupied)
			throw CompilerError::internal("Codegen: claimed register is already holding another value");
		slot.resident = vreg;
		slot.occupied = true;
		slot.dirty = true;
	}

	RegAllocator::RegSlot *RegAllocator::get_empty_slot()
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
			if (!slot.dirty && !slot.locked)
			{
				slot.occupied = false;
				return &slot;
			}
		}

		// worst case: spill register

		// choose a victim >:)
		RegSlot *victim;
		do
		{
			victim = &this->registers.at(this->next_to_spill);
			this->next_to_spill = (this->next_to_spill + 1) % this->registers.size();
		} while (victim->locked);

		this->spill_slot(*victim);

		victim->occupied = false;
		return victim;
	}
}
