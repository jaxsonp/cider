#include "ir/IrPrinter.hpp"

#include <algorithm>
#include <format>
#include <set>
#include <vector>

#include "utils/error.hpp"

namespace ir
{
	std::string op_mnemonic(Op opcode)
	{
		switch (opcode)
		{
		case Op::Add:
			return "add";
		case Op::Sub:
			return "sub";
		case Op::Mul:
			return "mul";
		case Op::Div:
			return "div";
		case Op::Rem:
			return "rem";
		case Op::BitAnd:
			return "and";
		case Op::BitOr:
			return "or";
		case Op::BitXor:
			return "xor";
		case Op::BitShl:
			return "shl";
		case Op::BitShr:
			return "shr";
		case Op::Neg:
			return "neg";
		case Op::CmpEq:
			return "cmp_eq";
		case Op::CmpNe:
			return "cmp_ne";
		case Op::CmpGt:
			return "cmp_gt";
		case Op::CmpGte:
			return "cmp_gte";
		case Op::CmpLt:
			return "cmp_lt";
		case Op::CmpLte:
			return "cmp_lte";
		case Op::Load:
			return "load";
		case Op::Store:
			return "store";
		case Op::Move:
			return "move";
		case Op::LoadImm:
			return "loadimm";
		case Op::LoadArg:
			return "loadarg";
		};
		throw CompilerError::internal("Uncaught IR opcode");
	}

	namespace
	{
		std::string vreg_name(VRegId id) { return std::format("%{}", id); }

		/// @brief Whether an instruction's result register is meaningful
		bool has_dest(Op opcode)
		{
			return opcode != Op::Store;
		}

		/// @brief Whether an instruction carries an immediate in its `data` field
		bool has_immediate(Op opcode)
		{
			return opcode == Op::LoadImm || opcode == Op::LoadArg;
		}

		void print_instruction(const Instruction &instr, std::ostream &out)
		{
			out << "  ";
			if (has_dest(instr.opcode))
				out << std::format("{} = ", vreg_name(instr.dest));
			out << op_mnemonic(instr.opcode);

			// unused operands are marked NO_VREG by the emitter, and are not printed
			if (!has_dest(instr.opcode))
				out << std::format(" {}", vreg_name(instr.dest));
			if (instr.op1 != NO_VREG)
				out << std::format(" {}", vreg_name(instr.op1));
			if (instr.op2 != NO_VREG)
				out << std::format(" {}", vreg_name(instr.op2));
			if (has_immediate(instr.opcode))
				out << std::format(" {}", instr.data);

			out << '\n';
		}

		void print_block_args(const BasicBlockTerminator::Successor &succ, std::ostream &out)
		{
			if (succ.args.empty())
				return;
			out << '(';
			for (std::size_t i = 0; i < succ.args.size(); ++i)
				out << (i == 0 ? "" : ", ") << vreg_name(succ.args[i]);
			out << ')';
		}

		void print_terminator(const BasicBlockTerminator &term, std::ostream &out)
		{
			switch (term.kind)
			{
			case BasicBlockTerminator::RETURN:
				out << "  ret";
				if (term.ret_reg)
					out << std::format(" {}", vreg_name(*term.ret_reg));
				out << '\n';
				return;

			case BasicBlockTerminator::JUMP:
				out << std::format("  jump bb{}", term.successors[0].bb->id);
				print_block_args(term.successors[0], out);
				out << '\n';
				return;

			case BasicBlockTerminator::BRANCH:
				out << std::format("  branch bb{}", term.successors[0].bb->id);
				print_block_args(term.successors[0], out);
				out << std::format(", bb{}", term.successors[1].bb->id);
				print_block_args(term.successors[1], out);
				out << '\n';
				return;
			};
			throw CompilerError::internal("Uncaught IR terminator kind");
		}

		/// @brief Collect a function's reachable blocks, entry first.
		/// Blocks are only reachable through the entry and terminator successors — blocks
		/// allocated by Function::new_bb() but never jumped to (such as the block opened
		/// after a return) have an uninitialized terminator and must not be visited.
		std::vector<const BasicBlock *> reachable_blocks(const Function &fn)
		{
			std::vector<const BasicBlock *> ordered;
			if (fn.entry == nullptr)
				return ordered;

			std::set<BBlockId> seen;
			std::vector<const BasicBlock *> to_visit{fn.entry};
			while (!to_visit.empty())
			{
				const BasicBlock *bb = to_visit.back();
				to_visit.pop_back();
				if (!seen.insert(bb->id).second)
					continue;
				ordered.push_back(bb);

				const BasicBlockTerminator &term = bb->terminator;
				unsigned int successor_count = term.kind == BasicBlockTerminator::JUMP ? 1
											   : term.kind == BasicBlockTerminator::BRANCH
												   ? 2
												   : 0;
				for (unsigned int i = successor_count; i > 0; --i)
					to_visit.push_back(term.successors[i - 1].bb);
			}
			return ordered;
		}

		void print_function(const Function &fn, std::ostream &out)
		{
			out << std::format("fn {} {{\n", fn.name);

			// vreg declarations, ordered by id
			std::vector<VRegId> vreg_ids;
			vreg_ids.reserve(fn.vregs.size());
			for (const auto &[id, type] : fn.vregs)
				vreg_ids.push_back(id);
			std::sort(vreg_ids.begin(), vreg_ids.end());
			for (VRegId id : vreg_ids)
				out << std::format("  {}: {}\n", vreg_name(id), fn.vregs.at(id).to_string());

			for (const BasicBlock *bb : reachable_blocks(fn))
			{
				out << std::format("bb{}:", bb->id);
				if (!bb->note.empty())
					out << std::format(" // {}", bb->note);
				out << '\n';

				for (const Instruction &instr : bb->instructions)
					print_instruction(instr, out);
				print_terminator(bb->terminator, out);
			}

			out << "}\n";
		}
	}

	void print(const Object &obj, std::ostream &out)
	{
		// functions live in an unordered map, so sort by name to keep output stable
		std::vector<const Function *> functions;
		functions.reserve(obj.functions.size());
		for (const auto &[name, fn] : obj.functions)
			functions.push_back(fn);
		std::sort(functions.begin(), functions.end(),
				  [](const Function *a, const Function *b)
				  { return a->name < b->name; });

		for (std::size_t i = 0; i < functions.size(); ++i)
		{
			if (i > 0)
				out << '\n';
			print_function(*functions[i], out);
		}
	}
}
