#include "ir/IrPrinter.hpp"

#include <algorithm>
#include <format>
#include <set>
#include <vector>

#include "utils/error.hpp"

namespace ir
{
	std::string binary_op_mnemonic(BinaryOp op)
	{
		switch (op)
		{
		case BinaryOp::Add:
			return "add";
		case BinaryOp::Sub:
			return "sub";
		case BinaryOp::Mul:
			return "mul";
		case BinaryOp::Div:
			return "div";
		case BinaryOp::Rem:
			return "rem";
		case BinaryOp::BitAnd:
			return "and";
		case BinaryOp::BitOr:
			return "or";
		case BinaryOp::BitXor:
			return "xor";
		case BinaryOp::BitShl:
			return "shl";
		case BinaryOp::BitShr:
			return "shr";
		case BinaryOp::CmpEq:
			return "cmp_eq";
		case BinaryOp::CmpNe:
			return "cmp_ne";
		case BinaryOp::CmpGt:
			return "cmp_gt";
		case BinaryOp::CmpGte:
			return "cmp_gte";
		case BinaryOp::CmpLt:
			return "cmp_lt";
		case BinaryOp::CmpLte:
			return "cmp_lte";
		};
		throw CompilerError::internal("Uncaught IR binary opcode");
	}

	std::string unary_op_mnemonic(UnaryOp op)
	{
		switch (op)
		{
		case UnaryOp::Neg:
			return "neg";
		case UnaryOp::BitNot:
			return "not";
		};
		throw CompilerError::internal("Uncaught IR unary opcode");
	}

	namespace
	{
		std::string vreg_name(VRegId id) { return std::format("%{}", id); }

		void print_binary_instruction(const BinaryInstruction &instr, std::ostream &out)
		{
			out << std::format("  {} = {} {} {}\n", vreg_name(instr.dest), binary_op_mnemonic(instr.op), vreg_name(instr.lhs), vreg_name(instr.rhs));
		}

		void print_unary_instruction(const UnaryInstruction &instr, std::ostream &out)
		{
			out << std::format("  {} = {} {}\n", vreg_name(instr.dest), unary_op_mnemonic(instr.op), vreg_name(instr.src));
		}

		void print_immediate_instruction(const ImmediateInstruction &instr, std::ostream &out)
		{
			out << std::format("  {} = loadimm {}\n", vreg_name(instr.dest), instr.value);
		}

		void print_load_arg_instruction(const LoadArgInstruction &instr, std::ostream &out)
		{
			out << std::format("  {} = loadarg {}\n", vreg_name(instr.dest), instr.index);
		}

		void print_call_instruction(const CallInstruction &instr, std::ostream &out)
		{
			out << "  ";
			if (instr.dest.has_value())
				out << std::format("{} = ", vreg_name(*instr.dest));
			out << std::format("call {}(", instr.callee);
			for (std::size_t i = 0; i < instr.args.size(); ++i)
				out << (i == 0 ? "" : ", ") << vreg_name(instr.args[i]);
			out << ")\n";
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
			out << std::format("fn {}(", fn.name);
			for (std::size_t i = 0; i < fn.argument_types.size(); ++i)
				out << (i == 0 ? "" : ", ") << fn.argument_types[i].to_string();
			out << ')';
			if (fn.return_type.has_value())
				out << std::format(" -> {}", fn.return_type->to_string());
			out << " {\n";

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

				for (const auto &instr : bb->instructions)
				{
					if (const BinaryInstruction *i = std::get_if<BinaryInstruction>(&instr))
						print_binary_instruction(*i, out);
					else if (const UnaryInstruction *i = std::get_if<UnaryInstruction>(&instr))
						print_unary_instruction(*i, out);
					else if (const ImmediateInstruction *i = std::get_if<ImmediateInstruction>(&instr))
						print_immediate_instruction(*i, out);
					else if (const LoadArgInstruction *i = std::get_if<LoadArgInstruction>(&instr))
						print_load_arg_instruction(*i, out);
					else
						print_call_instruction(std::get<CallInstruction>(instr), out);
				}
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
