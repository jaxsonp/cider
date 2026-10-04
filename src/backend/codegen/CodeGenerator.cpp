#include "CodeGenerator.hpp"

#include <format>
#include <iostream>
#include <set>
#include <unordered_map>

#include "utils/logging.hpp"
#include "utils/error.hpp"

// A streambuffer that discards everything
class BlackHoleBuffer : public std::streambuf
{
public:
	int overflow(int c) override { return c; }
};

// An ostream that discards everything
class BlackHoleStream : public std::ostream
{
	BlackHoleBuffer buf;

public:
	BlackHoleStream() : std::ostream(&buf) {}
};

std::ostream &CodeGenerator::asm_out()
{
	static BlackHoleStream null_out = BlackHoleStream();

	if (this->asm_out_ptr != nullptr)
		return *this->asm_out_ptr;
	else
		return null_out;
}

void CodeGenerator::enable_asm_output(std::unique_ptr<std::ostream> out)
{
	this->asm_out_ptr = std::move(out);
}

void CodeGenerator::add_call_fixup(size_t code_offset, const std::string &callee)
{
	this->call_fixups.push_back(CallFixup{.code_offset = code_offset, .callee = callee});
}

void CodeGenerator::lower_function(const ir::Function &fn, Object &obj)
{
	log_vvv("Lowering function \"{}\"", fn.name);

	this->cur_fn = &fn;
	this->begin_function(fn);

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

		this->begin_block(*bb);

		// emit all instructions in basic block
		for (auto &instr_slot : bb->instructions)
		{
			this->begin_instruction();

			if (const ir::ImmediateInstruction *imm_ptr = std::get_if<ir::ImmediateInstruction>(&instr_slot))
				this->lower_immediate_instr(*imm_ptr);
			else if (const ir::BinaryInstruction *bin_ptr = std::get_if<ir::BinaryInstruction>(&instr_slot))
				this->lower_binary_instr(*bin_ptr);
			else if (const ir::UnaryInstruction *un_ptr = std::get_if<ir::UnaryInstruction>(&instr_slot))
				this->lower_unary_instr(*un_ptr);
			else if (const ir::LoadArgInstruction *arg_ptr = std::get_if<ir::LoadArgInstruction>(&instr_slot))
				this->lower_load_arg_instr(*arg_ptr);
			else if (const ir::CallInstruction *call_ptr = std::get_if<ir::CallInstruction>(&instr_slot))
				this->lower_call(*call_ptr);
			else
				throw CompilerError::internal("Codegen: uncaught IR instruction variant");
		}

		// emit basic block terminator
		switch (bb->terminator.kind)
		{
		case ir::BasicBlockTerminator::RETURN:
			this->lower_return(bb->terminator.ret_reg);
			break;
		default:
			// TODO when adding branching terminators, dirty registers must be spilled *before* the jump, since
			// the next bb starts with an empty register file
			throw CompilerError::unimplemented("Unhandled bb terminator kind variant");
		};
	}

	// register this function entry
	obj.functions.push_back(Object::Function{
		.name = fn.name,
		.code_offset = obj.code.size(),
	});

	// write to .text
	this->finalize_function(fn, obj.code);

	this->cur_fn = nullptr;

	log_vvv("Function \"{}\" done, object is now {} bytes", fn.name, obj.code.size());
}

Object CodeGenerator::lower_ir(const ir::Object &ir)
{
	log_vv("Starting lowering to machine code");
	Object obj;

	this->call_fixups.clear();
	for (const auto &[name, fn] : ir.functions)
	{
		this->lower_function(*fn, obj);
	}

	// now every function has a known offset, so calls can be pointed at them
	log_vvv("Resolving {} call site(s)", this->call_fixups.size());
	std::unordered_map<std::string, size_t> fn_offsets;
	for (const Object::Function &fn : obj.functions)
		fn_offsets.insert({fn.name, fn.code_offset});

	for (const CallFixup &fixup : this->call_fixups)
	{
		auto target = fn_offsets.find(fixup.callee);
		if (target == fn_offsets.end())
			throw CompilerError::internal(std::format("Codegen: call to unknown function \"{}\"", fixup.callee));

		this->patch_call(obj.code, fixup.code_offset, target->second);
	}

	return obj;
}
