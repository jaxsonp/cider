#include "IR.hpp"

#include "utils/error.hpp"

namespace ir
{
	// autoassign id to new bbs
	static unsigned int bb_count = 0;
	BasicBlock::BasicBlock(std::string _note)
		: id(bb_count++), note(_note) {}

	namespace instr
	{
		LoadArgInstruction::LoadArgInstruction(VRegId _dest, unsigned short index)
			: Instruction(Op::LoadArg), arg_index(index), dest(_dest) {}

		ReturnInstruction::ReturnInstruction()
			: TerminalInstruction(Op::Return) {}

		ReturnInstruction::ReturnInstruction(VRegId _ret_value)
			: TerminalInstruction(Op::Return), ret_value(_ret_value) {}

		LoadImmInstruction::LoadImmInstruction(VRegId _dest, uint32_t _value)
			: Instruction(Op::LoadImm), dest(_dest), value(_value) {}
	}

	Function::Function(const std::string &_name)
		: name(_name), vreg_count(0)
	{
		this->entry = new BasicBlock(this->name + " start");
	}

	/*Value Value::new_vreg(VRegId id, Type t)
	{
		Value v;
		v.type = t;
		v.data.vreg_id = id;
		return v;
	}
	Value Value::new_immediate(int32_t val)
	{
		Value v;
		v.type = Type::i32();
		v.data.imm_val = val;
		return v;
	}*/
}

IrWriter::IrWriter()
	: obj(new IrObject()) {}

void IrWriter::new_function(const std::string &name)
{
	this->cur_function = new ir::Function(name);
	this->obj->functions.insert({name, this->cur_function});

	this->cur_bblock = this->cur_function->entry;

	// reset vreg maps
	this->vreg_map_scopes.clear();
	this->vreg_map_scopes.emplace_back();
}

ir::VRegId IrWriter::new_local(const std::string &name)
{
	ir::VRegId id = this->new_vreg();
	this->vreg_map_scopes.back().insert({name, id});
	return id;
}

ir::VRegId IrWriter::get_local(const std::string &name) const
{
	// visit scope stack from top to bottom
	for (std::vector<VRegMap>::const_reverse_iterator vreg_map = this->vreg_map_scopes.rbegin(); vreg_map != this->vreg_map_scopes.rend(); ++vreg_map)
	{
		VRegMap::const_iterator found = vreg_map->find(name);
		if (found != vreg_map->end())
		{
			return found->second;
		}
	}
	throw InternalError(std::format("Failed to find vreg allocation for name \"{}\"", name));
}

void IrWriter::push_scope()
{
	this->vreg_map_scopes.emplace_back();
}

void IrWriter::pop_scope()
{
	this->vreg_map_scopes.pop_back();
}

ir::VRegId IrWriter::new_vreg()
{
	if (this->cur_function == nullptr)
		throw InternalError("Attempted to reserve vreg before a function was created");
	return this->cur_function->vreg_count++;
}

void IrWriter::emit(ir::Instruction *new_instr)
{
	if (this->cur_bblock == nullptr)
		throw InternalError("Attempted to emit IR instruction before a basic block was \"chosen\"");

	this->cur_function->instr_count += 1;
	if (this->cur_instr == nullptr)
	{
		// bblock is empty
		this->cur_bblock->start = new_instr;
		this->cur_bblock->end = new_instr;
		this->cur_instr = new_instr;
	}
	else
	{
		// insert in bblock
		this->cur_instr->next = new_instr;
		new_instr->prev = this->cur_instr;

		if (this->cur_bblock->end == this->cur_instr)
			this->cur_bblock->end = new_instr;
		this->cur_instr = new_instr;
	}
}