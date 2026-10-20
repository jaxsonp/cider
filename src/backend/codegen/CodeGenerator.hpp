#pragma once

#include <stdint.h>
#include <ostream>
#include <sstream>
#include <memory>
#include <string>
#include <vector>

#include "ir/IR.hpp"
#include "backend/objwriter/ObjectWriter.hpp"
#include "backend/Target.hpp"

class Target;

/// @brief Base of every backend. Owns the architecture independent parts of lowering IR to machine code
/// (walking functions, basic blocks and instructions, and pointing calls at their targets), and hands
/// each IR construct to the backend through the `lower_*` hooks
///
/// A backend implements the hooks, emitting into buffers of its own. `finalize_function` is where it assembles
/// everything it emitted for the function (plus prologue and epilogue) into the object's code
class CodeGenerator
{
	/// Whether textual assembly is being collected
	bool emit_asm = false;
	/// Textual assembly of every function lowered so far
	std::ostringstream function_asm;
	/// Textual assembly of the runtime code
	std::ostringstream runtime_asm;

	/// @brief A call site whose target offset isn't known until every function has been lowered
	struct CallFixup
	{
		/// Byte offset into the object's code of the call
		size_t code_offset;
		/// Name of the function being called
		std::string callee;
	};

	/// Call sites across the whole object, patched once every function has a known offset
	std::vector<CallFixup> call_fixups;

	void lower_function(const ir::Function &fn, Object &obj);

protected:
	/// @brief Whether textual assembly is being collected, for backends to skip building it otherwise
	bool asm_enabled() const { return this->emit_asm; }

	/// @brief Stream for the textual assembly of the function being lowered, meant to be written to from
	/// `finalize_function` (the function's label is already written). Returns a black hole if asm output is not
	/// enabled
	std::ostream &asm_out();

	/// @brief Stream for the textual assembly of the runtime code, meant to be written to from `build_runtime_code`.
	/// Returns a black hole if asm output is not enabled
	std::ostream &runtime_asm_out();

	/// @brief Writes the assembler directives that go at the top of the textual assembly (ie syntax, ISA options)
	virtual void write_asm_directives(std::ostream &out) const = 0;

	/// @brief How a function's name appears in textual assembly. Always quoted, so that a function named like a
	/// register or a keyword of the assembler's syntax is still a symbol
	static std::string asm_symbol(const std::string &name) { return "\"" + name + "\""; }

	/// Function currently being lowered, for looking up vreg types (spill width, div/rem signedness, etc.)
	const ir::Function *cur_fn = nullptr;

	/// @brief Registers a call site to be pointed at its callee once every function has been lowered, by
	/// `patch_call`. Meant to be called from `finalize_function`, when the call's final position is known
	/// @param code_offset Byte offset into the object's code of the call
	/// @param callee Name of the function being called
	void add_call_fixup(size_t code_offset, const std::string &callee);

	// lowering hooks, in the order they're called for each function

	/// @brief Called before anything in a function is lowered, to reset per-function state
	virtual void begin_function(const ir::Function &fn) = 0;

	/// @brief Called before the instructions of a basic block are lowered
	virtual void begin_block(const ir::BasicBlock &bb) = 0;

	/// @brief Called before each instruction (and each terminator) is lowered
	virtual void begin_instruction() = 0;

	virtual void lower_immediate_instr(const ir::ImmediateInstruction &instr) = 0;
	virtual void lower_binary_instr(const ir::BinaryInstruction &instr) = 0;
	virtual void lower_unary_instr(const ir::UnaryInstruction &instr) = 0;
	virtual void lower_load_arg_instr(const ir::LoadArgInstruction &instr) = 0;
	virtual void lower_call(const ir::CallInstruction &instr) = 0;

	/// @brief Lowers a return terminator
	/// @param ret_reg Value being returned, nullopt when returning nothing
	virtual void lower_return(std::optional<ir::VRegId> ret_reg) = 0;

	/// @brief Called once the whole body is lowered. Appends the function's finished machine code to `code`,
	/// and registers its call sites with `add_call_fixup`
	virtual void finalize_function(const ir::Function &fn, std::vector<uint8_t> &code) = 0;

	/// @brief Points an already emitted call at its target
	/// @param code The object's code
	/// @param call_offset Byte offset of the call, as given to `add_call_fixup`
	/// @param target_offset Byte offset of the function being called
	virtual void patch_call(std::vector<uint8_t> &code, size_t call_offset, size_t target_offset) = 0;

public:
	virtual ~CodeGenerator() = default;

	/// @brief Lowers a whole IR object to machine code
	Object lower_ir(const ir::Object &ir);

	/// @brief Creates the small chunk of entry code for an executable
	/// @param main_offset Offset of the main function BEFORE prepending this runtime code
	/// @param t Target
	/// @return Runtime code in a byte vector
	virtual std::vector<uint8_t> build_runtime_code(uint64_t main_offset, Target t) = 0;

	/// @brief Enables collecting textual assembly while lowering, written out with `write_asm`
	void enable_asm_output() { this->emit_asm = true; }

	/// @brief Writes the textual assembly (GNU as syntax) of everything lowered, laid out like the executable's code:
	/// the runtime code (at `_start`) first, then every function. Requires `enable_asm_output` before lowering
	void write_asm(std::ostream &out) const;
};
