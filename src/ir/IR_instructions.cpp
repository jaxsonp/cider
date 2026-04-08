#include "ir/IR_instructions.hpp"
#include "IR_instructions.hpp"

namespace ir::instr
{

	LoadArgInstruction::LoadArgInstruction(VRegId dest, unsigned short index)
		: Instruction(Op::LoadArg), arg_index(index), dest(dest) {}

	AddInstruction::AddInstruction(VRegId dest, VRegId op1, Operand op2)
		: Instruction(Op::Add), dest(dest), op1(op1), op2(op2) {}

	SubtractInstruction::SubtractInstruction(VRegId dest, VRegId op1, Operand op2)
		: Instruction(Op::Sub), dest(dest), op1(op1), op2(op2) {}

	MultiplyInstruction::MultiplyInstruction(VRegId dest, VRegId op1, Operand op2)
		: Instruction(Op::Mul), dest(dest), op1(op1), op2(op2) {}

	DivideInstruction::DivideInstruction(VRegId dest, VRegId op1, Operand op2)
		: Instruction(Op::Mul), dest(dest), op1(op1), op2(op2) {}

	ModuloInstruction::ModuloInstruction(VRegId dest, VRegId op1, Operand op2)
		: Instruction(Op::Mul), dest(dest), op1(op1), op2(op2) {}

	ReturnInstruction::ReturnInstruction()
		: TerminalInstruction(Op::Return) {}

	ReturnInstruction::ReturnInstruction(Operand _ret_value)
		: TerminalInstruction(Op::Return), ret_value(_ret_value) {}

	LoadImmInstruction::LoadImmInstruction(VRegId _dest, uint64_t _value)
		: Instruction(Op::LoadImm), dest(_dest), value(_value) {}
}