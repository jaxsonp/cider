#include "unit_testing.hpp"
#include "backend/codegen/RegAllocator.hpp"

#include <array>
#include <format>
#include <string>
#include <vector>

#include "utils/error.hpp"

using namespace codegen;

namespace
{
	/// Records what the allocator asks of the backend, as a list of readable events
	class RecordingSpillHandler : public SpillHandler
	{
	public:
		std::vector<std::string> events;

		void store_spilled_vreg(PhysReg src, ir::VRegId vreg, size_t spill_index) override
		{
			this->events.push_back(std::format("store r{} %{} slot{}", src, vreg, spill_index));
		}

		void load_spilled_vreg(PhysReg dest, ir::VRegId vreg, size_t spill_index) override
		{
			this->events.push_back(std::format("load r{} %{} slot{}", dest, vreg, spill_index));
		}

		void move_reg(PhysReg dest, PhysReg src, ir::VRegId vreg) override
		{
			this->events.push_back(std::format("move r{} r{} %{}", dest, src, vreg));
		}
	};
}

using Events = std::vector<std::string>;

/// Three caller saved registers (0, 1, 2) and a callee saved one (3)
static constexpr std::array<std::tuple<const PhysReg, const bool>, 4> POOL = {{
	{PhysReg(0), false},
	{PhysReg(1), false},
	{PhysReg(2), false},
	{PhysReg(3), true},
}};

namespace
{
	/// An allocator over `POOL`, ready for a function's first instruction
	struct Fixture
	{
		RecordingSpillHandler spills;
		RegAllocator alloc;

		Fixture() : alloc(spills, POOL)
		{
			this->alloc.start_function();
			this->alloc.start_block();
			this->alloc.start_instruction();
		}

		/// Puts a dirty vreg in the next free register, as its own instruction
		PhysReg define(ir::VRegId vreg)
		{
			this->alloc.start_instruction();
			PhysReg reg = this->alloc.load_dest_vreg(vreg)->physical;
			this->alloc.start_instruction();
			return reg;
		}
	};
}

// destinations ==================================================================================================

TEST(destinations_go_in_priority_order)
{
	Fixture f;
	ASSERT_EQ(int(f.define(0)), 0);
	ASSERT_EQ(int(f.define(1)), 1);
	ASSERT_EQ(int(f.define(2)), 2);
	ASSERT_TRUE(f.spills.events.empty());
}

TEST(destination_slot_state)
{
	Fixture f;
	RegAllocator::RegSlot *slot = f.alloc.load_dest_vreg(7);
	ASSERT_EQ(slot->resident, 7u);
	ASSERT_TRUE(slot->occupied);
	ASSERT_TRUE(slot->dirty);
	ASSERT_TRUE(slot->locked);
}

TEST(callee_saved_registers_are_left_out)
{
	Fixture f;
	ASSERT_THROWS(f.alloc.slot_for(3), CompilerError);
}

// sources =======================================================================================================

TEST(source_already_in_a_register_needs_no_load)
{
	Fixture f;
	PhysReg reg = f.define(5);
	ASSERT_EQ(int(f.alloc.load_src_vreg(5)->physical), int(reg));
	ASSERT_TRUE(f.spills.events.empty());
}

TEST(source_is_locked)
{
	Fixture f;
	f.define(5);
	ASSERT_TRUE(f.alloc.load_src_vreg(5)->locked);
}

TEST(source_nowhere_to_be_found)
{
	Fixture f;
	ASSERT_THROWS(f.alloc.load_src_vreg(5), CompilerError);
}

TEST(spilled_source_is_loaded_back)
{
	Fixture f;
	f.define(0);
	f.alloc.spill_all();
	RegAllocator::RegSlot *slot = f.alloc.load_src_vreg(0);
	ASSERT_TRUE(f.spills.events == (Events{"store r0 %0 slot0", "load r0 %0 slot0"}));
	ASSERT_TRUE(slot->occupied);
	ASSERT_TRUE(!slot->dirty);
}

// when registers run out ========================================================================================

TEST(spilling_starts_once_every_register_is_full)
{
	Fixture f;
	f.define(0);
	f.define(1);
	f.define(2);
	ASSERT_TRUE(f.spills.events.empty());
	f.define(3);
	ASSERT_TRUE(f.spills.events == (Events{"store r0 %0 slot0"}));
}

TEST(spilling_is_round_robin)
{
	Fixture f;
	for (ir::VRegId vreg = 0; vreg < 7; ++vreg)
		f.define(vreg);
	ASSERT_TRUE(f.spills.events == (Events{"store r0 %0 slot0", "store r1 %1 slot1", "store r2 %2 slot2", "store r0 %3 slot3"}));
}

TEST(clean_values_are_replaced_before_spilling_anything)
{
	Fixture f;
	f.define(0);
	f.define(1);
	f.define(2);
	f.alloc.spill_slot(f.alloc.slot_for(1)); // %1 is now clean, and stays put in r1
	f.spills.events.clear();

	ASSERT_EQ(int(f.define(3)), 1);
	ASSERT_TRUE(f.spills.events.empty());
}

TEST(free_registers_are_used_before_clean_ones)
{
	Fixture f;
	f.define(0);
	f.alloc.spill_slot(f.alloc.slot_for(0));
	ASSERT_EQ(int(f.define(1)), 1);
}

TEST(locked_registers_are_not_spilled)
{
	Fixture f;
	f.define(0);
	f.define(1);
	f.define(2);
	f.alloc.load_src_vreg(0);
	f.alloc.load_src_vreg(1);
	RegAllocator::RegSlot *dest = f.alloc.load_dest_vreg(3);
	ASSERT_EQ(int(dest->physical), 2);
	ASSERT_TRUE(f.spills.events == (Events{"store r2 %2 slot0"}));
}

TEST(every_register_locked_is_an_error)
{
	Fixture f;
	f.alloc.load_dest_vreg(0);
	f.alloc.load_dest_vreg(1);
	f.alloc.load_dest_vreg(2);
	ASSERT_THROWS(f.alloc.load_dest_vreg(3), CompilerError);
}

TEST(start_instruction_unlocks)
{
	Fixture f;
	f.alloc.load_dest_vreg(0);
	f.alloc.load_dest_vreg(1);
	f.alloc.load_dest_vreg(2);
	f.alloc.start_instruction();
	f.alloc.load_dest_vreg(3);
	ASSERT_EQ(f.spills.events.size(), 1u);
}

// spill slots ===================================================================================================

TEST(a_vreg_keeps_its_slot)
{
	Fixture f;
	f.define(0);
	f.alloc.spill_all();
	ASSERT_EQ(f.alloc.spill_count(), 1u);

	f.alloc.load_src_vreg(0);
	f.alloc.slot_for(0).dirty = true;
	f.alloc.spill_all();
	ASSERT_EQ(f.alloc.spill_count(), 1u);
	ASSERT_EQ(f.spills.events.back(), "store r0 %0 slot0");
}

TEST(slots_are_numbered_in_first_spill_order)
{
	Fixture f;
	f.define(8);
	f.define(9);
	f.alloc.spill_slot(f.alloc.slot_for(1));
	f.alloc.spill_slot(f.alloc.slot_for(0));
	ASSERT_TRUE(f.spills.events == (Events{"store r1 %9 slot0", "store r0 %8 slot1"}));
	ASSERT_EQ(f.alloc.spill_count(), 2u);
}

TEST(spill_slot_leaves_the_value_in_the_register)
{
	Fixture f;
	f.define(0);
	RegAllocator::RegSlot &slot = f.alloc.slot_for(0);
	f.alloc.spill_slot(slot);
	ASSERT_TRUE(!slot.dirty);
	ASSERT_TRUE(slot.occupied);
}

TEST(spill_all_stores_dirty_values_and_empties_everything)
{
	Fixture f;
	f.define(0);
	f.define(1);
	f.alloc.spill_slot(f.alloc.slot_for(0)); // already clean
	f.spills.events.clear();

	f.alloc.spill_all();
	ASSERT_TRUE(f.spills.events == (Events{"store r1 %1 slot1"}));
	ASSERT_TRUE(!f.alloc.slot_for(0).occupied);
	ASSERT_TRUE(!f.alloc.slot_for(1).occupied);
}

// blocks and functions ==========================================================================================

TEST(start_block_empties_registers_without_spilling)
{
	Fixture f;
	f.define(0);
	f.alloc.start_block();
	ASSERT_TRUE(f.spills.events.empty());
	ASSERT_TRUE(!f.alloc.slot_for(0).occupied);
	ASSERT_THROWS(f.alloc.load_src_vreg(0), CompilerError);
}

TEST(start_block_keeps_spill_slots)
{
	Fixture f;
	f.define(0);
	f.alloc.spill_all();
	f.alloc.start_block();
	ASSERT_EQ(f.alloc.spill_count(), 1u);
	f.alloc.load_src_vreg(0);
	ASSERT_EQ(f.spills.events.back(), "load r0 %0 slot0");
}

TEST(start_function_forgets_everything)
{
	Fixture f;
	for (ir::VRegId vreg = 0; vreg < 4; ++vreg)
		f.define(vreg);
	ASSERT_EQ(f.alloc.spill_count(), 1u);

	f.alloc.start_function();
	ASSERT_EQ(f.alloc.spill_count(), 0u);
	ASSERT_THROWS(f.alloc.load_src_vreg(0), CompilerError);

	// the round robin starts over too
	f.spills.events.clear();
	for (ir::VRegId vreg = 10; vreg < 14; ++vreg)
		f.define(vreg);
	ASSERT_TRUE(f.spills.events == (Events{"store r0 %10 slot0"}));
}

// fixed registers ===============================================================================================

TEST(claim_puts_a_dirty_value_in_a_register)
{
	Fixture f;
	f.alloc.claim(1, 5);
	RegAllocator::RegSlot &slot = f.alloc.slot_for(1);
	ASSERT_EQ(slot.resident, 5u);
	ASSERT_TRUE(slot.occupied);
	ASSERT_TRUE(slot.dirty);
	ASSERT_TRUE(!slot.locked);
}

TEST(claimed_value_can_be_used_as_a_source)
{
	Fixture f;
	f.alloc.claim(2, 5);
	ASSERT_EQ(int(f.alloc.load_src_vreg(5)->physical), 2);
}

TEST(claim_errors)
{
	Fixture f;
	f.define(0);
	ASSERT_THROWS(f.alloc.claim(0, 5), CompilerError);
	ASSERT_THROWS(f.alloc.claim(3, 5), CompilerError);
}

TEST(evict_spills_a_dirty_value)
{
	Fixture f;
	f.define(0);
	f.alloc.evict(0);
	ASSERT_TRUE(f.spills.events == (Events{"store r0 %0 slot0"}));
	RegAllocator::RegSlot &slot = f.alloc.slot_for(0);
	ASSERT_TRUE(!slot.occupied);
	ASSERT_TRUE(slot.locked);
}

TEST(evict_leaves_nothing_to_spill_for_a_clean_value)
{
	Fixture f;
	f.define(0);
	f.alloc.spill_all();
	f.alloc.load_src_vreg(0);
	f.alloc.start_instruction();
	f.spills.events.clear();

	f.alloc.evict(0);
	ASSERT_TRUE(f.spills.events.empty());
}

TEST(evicted_register_is_not_allocated)
{
	Fixture f;
	f.alloc.evict(0);
	ASSERT_EQ(int(f.alloc.load_dest_vreg(1)->physical), 1);
}

TEST(evicted_register_can_be_claimed)
{
	Fixture f;
	f.define(0);
	f.alloc.evict(0);
	f.alloc.claim(0, 9);
	ASSERT_EQ(f.alloc.slot_for(0).resident, 9u);
}

TEST(evict_of_a_locked_register)
{
	Fixture f;
	f.alloc.load_dest_vreg(0);
	ASSERT_THROWS(f.alloc.evict(0), CompilerError);
}

TEST(load_into_when_already_in_place)
{
	Fixture f;
	f.define(0);
	f.alloc.load_src_vreg_into(0, 0, false);
	ASSERT_TRUE(f.spills.events.empty());
	ASSERT_TRUE(f.alloc.slot_for(0).locked);
	ASSERT_TRUE(f.alloc.slot_for(0).occupied);
}

TEST(load_into_when_already_in_place_and_clobbered)
{
	Fixture f;
	f.define(0);
	f.alloc.load_src_vreg_into(0, 0, true);
	ASSERT_TRUE(f.spills.events == (Events{"store r0 %0 slot0"}));
	ASSERT_TRUE(!f.alloc.slot_for(0).occupied);
	ASSERT_TRUE(f.alloc.slot_for(0).locked);
}

TEST(load_into_moves_a_value_from_another_register)
{
	Fixture f;
	f.define(0); // r0
	f.alloc.load_src_vreg_into(0, 1, false);
	ASSERT_TRUE(f.spills.events == (Events{"move r1 r0 %0"}));

	RegAllocator::RegSlot &target = f.alloc.slot_for(1);
	ASSERT_EQ(target.resident, 0u);
	ASSERT_TRUE(target.occupied);
	ASSERT_TRUE(target.dirty);
	ASSERT_TRUE(!f.alloc.slot_for(0).occupied);
	ASSERT_TRUE(!f.alloc.slot_for(0).dirty);
}

TEST(load_into_a_clobbered_copy_leaves_the_original)
{
	Fixture f;
	f.define(0); // r0
	f.alloc.load_src_vreg_into(0, 1, true);
	ASSERT_TRUE(f.spills.events == (Events{"move r1 r0 %0"}));
	ASSERT_TRUE(f.alloc.slot_for(0).occupied);
	ASSERT_TRUE(f.alloc.slot_for(0).dirty);
	ASSERT_TRUE(!f.alloc.slot_for(1).occupied);
	ASSERT_TRUE(f.alloc.slot_for(1).locked);
}

TEST(load_into_from_the_stack)
{
	Fixture f;
	f.define(0);
	f.alloc.spill_all();
	f.spills.events.clear();

	f.alloc.load_src_vreg_into(0, 2, false);
	ASSERT_TRUE(f.spills.events == (Events{"load r2 %0 slot0"}));
	ASSERT_EQ(f.alloc.slot_for(2).resident, 0u);
	ASSERT_TRUE(!f.alloc.slot_for(2).dirty);
}

TEST(load_into_from_the_stack_when_clobbered)
{
	Fixture f;
	f.define(0);
	f.alloc.spill_all();
	f.alloc.load_src_vreg_into(0, 2, true);
	ASSERT_TRUE(!f.alloc.slot_for(2).occupied);
}

TEST(load_into_saves_what_was_there)
{
	Fixture f;
	f.define(0); // r0
	f.define(1); // r1
	f.alloc.load_src_vreg_into(0, 1, false);
	ASSERT_TRUE(f.spills.events == (Events{"store r1 %1 slot0", "move r1 r0 %0"}));
}

TEST(load_into_a_locked_register)
{
	Fixture f;
	f.define(0);
	f.alloc.load_dest_vreg(1);
	ASSERT_THROWS(f.alloc.load_src_vreg_into(0, 1, false), CompilerError);
}

TEST(load_into_from_a_locked_register)
{
	Fixture f;
	f.alloc.load_dest_vreg(0);
	ASSERT_THROWS(f.alloc.load_src_vreg_into(0, 1, false), CompilerError);
}

TEST(load_into_of_a_vreg_that_does_not_exist)
{
	Fixture f;
	ASSERT_THROWS(f.alloc.load_src_vreg_into(5, 0, false), CompilerError);
}

TEST(load_spilled_vreg_does_not_touch_allocation_state)
{
	Fixture f;
	f.define(0);
	f.alloc.spill_all();
	f.spills.events.clear();

	f.alloc.load_spilled_vreg(1, 0);
	ASSERT_TRUE(f.spills.events == (Events{"load r1 %0 slot0"}));
	ASSERT_TRUE(!f.alloc.slot_for(1).occupied);
}
