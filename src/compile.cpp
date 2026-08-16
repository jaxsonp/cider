#include "compile.hpp"

#include <fstream>
#include <iostream>

#include "utils/logging.hpp"
#include "utils/error.hpp"
#include "frontend/AST.hpp"
#include "ir/IR.hpp"
#include "ir/IrPrinter.hpp"
#include "backend/codegen/CodeGenerator.hpp"
#include "backend/objwriter/ObjectWriter.hpp"

EmitKind CompileSettings::furthest() const
{
	if (this->emits.empty())
		throw CompilerError::internal("No output was requested");
	// std::map keeps EmitKind in declaration order, which is pipeline order
	return this->emits.rbegin()->first;
}

namespace
{
	/// @brief Owns the output streams for every requested textual artifact.
	/// All files are opened up front so a bad path fails before the pipeline runs.
	class EmitStreams
	{
		std::map<EmitKind, std::ofstream> files;

	public:
		explicit EmitStreams(const CompileSettings &settings)
		{
			for (const auto &[kind, target] : settings.emits)
			{
				if (target.to_stdout || kind == EmitKind::Exe)
					continue;

				log_vv("Opening file \"{}\" for writing", target.path);
				std::ofstream file(target.path);
				if (!file.is_open())
					throw CompilerError::file_io_error(
						std::format("Failed to open file \"{}\" for writing", target.path));
				this->files.emplace(kind, std::move(file));
			}
		}

		/// @brief Stream for an artifact. Only valid for kinds that were requested
		std::ostream &get(EmitKind kind)
		{
			auto found = this->files.find(kind);
			return found == this->files.end() ? std::cout : found->second;
		}
	};
}

void compile(const std::string &filename, const CompileSettings &settings)
{
	log_v("Preparing for compilation");

	// open every output up front, so a bad path fails fast
	EmitStreams outputs(settings);

	log_vv("Opening file \"{}\" for reading", filename);
	std::ifstream file(filename);
	if (!file.is_open())
		throw CompilerError::file_io_error(std::format("Failed to open file \"{}\" for reading", filename));
	log_vv("File opened");

	log_v("Building AST");
	AST ast(file);
	file.close();

	if (settings.wants(EmitKind::Ast))
	{
		log_vv("Writing AST");
		ast.print(outputs.get(EmitKind::Ast));
	}
	if (settings.furthest() == EmitKind::Ast)
		return;

	log_v("Emitting IR");
	ir::Object ir = ast.emit_ir();

	if (settings.wants(EmitKind::Ir))
	{
		log_vv("Writing IR");
		ir::print(ir, outputs.get(EmitKind::Ir));
	}
	if (settings.furthest() == EmitKind::Ir)
		return;

	log_v("Lowering IR to machine code");
	auto code_generator = settings.target.get_code_generator();
	Object obj(code_generator->lower_ir(ir));

	log_v("Building executable runtime");
	bool found_main = false;
	size_t main_offset;
	for (const auto &fn : obj.functions)
	{
		if (fn.name == "main")
		{
			main_offset = fn.code_offset;
			found_main = true;
			break;
		}
	}
	if (!found_main)
		throw CompilerError::semantic_error("Missing main function");
	std::vector<uint8_t> runtime_code = code_generator->build_runtime_code(main_offset, settings.target);
	obj.code.reserve(runtime_code.size());
	obj.code.insert(obj.code.begin(), runtime_code.begin(), runtime_code.end()); // is this slow? (eh runtime code isnt big)

	if (!settings.wants(EmitKind::Exe))
		return;

	const std::string &out_path = settings.emits.at(EmitKind::Exe).path;
	log_vv("Opening file \"{}\" for writing", out_path);
	std::ofstream out_file(out_path, std::ios::binary);
	if (!out_file.is_open())
		throw CompilerError::file_io_error(std::format("Failed to open file \"{}\" for writing", out_path));

	log_v("Emitting ELF object");
	auto obj_writer = settings.target.get_object_writer();
	obj_writer->emit(obj, settings.target, out_file);
	out_file.close();
}
