#pragma once

#include <stdint.h>
#include <ostream>
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
	std::unique_ptr<std::ostream> asm_out_ptr = nullptr;

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
	/// @brief Access the assembly output stream. Returns a black hole if asm output is not enabled
	std::ostream &asm_out();

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

	/// @brief Enables dumping assembly to an output stream. ostream object must live as long as this object.
	virtual void enable_asm_output(std::unique_ptr<std::ostream> out);
};
