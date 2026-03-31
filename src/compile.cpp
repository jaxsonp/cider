#include "compile.hpp"

#include <fstream>

#include "utils/logging.hpp"
#include "utils/error.hpp"
#include "frontend/AST.hpp"
#include "IR.hpp"
#include "backend/codegen/CodeGenerator.hpp"
#include "backend/objwriter/ObjectWriter.hpp"

void compile(const std::string &filename, const CompileSettings &settings)
{
	log_v("Preparing for compilation");
	log_vv("Opening file \"{}\" for reading", filename);
	std::ifstream file(filename);
	if (!file.is_open())
		throw FileOpenError(std::format("Failed to open file \"{}\" for reading", filename));
	log_vv("File opened");

	log_v("Building AST");
	AST ast(file);
	file.close();

	if (settings.debug_print_asm)
	{
		log_vv("Debug printing AST");
		ast.debug_print();
	}

	log_v("Emitting IR");
	IrObject ir = ast.emitIr();

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
		throw SemanticError("Missing main function");
	std::vector<uint8_t> runtime_code = code_generator->build_runtime_code(main_offset, settings.target);
	obj.code.reserve(runtime_code.size());
	obj.code.insert(obj.code.begin(), runtime_code.begin(), runtime_code.end()); // is this slow? (eh runtime code isnt big)

	log_vv("Opening file \"{}\" for writing", settings.output_filename);
	std::ofstream out_file(settings.output_filename, std::ios::binary);
	if (!out_file.is_open())
		throw FileOpenError(std::format("Failed to open file \"{}\" for writing", settings.output_filename));

	log_v("Emitting ELF object");
	auto obj_writer = settings.target.get_object_writer();
	obj_writer->emit(obj, settings.target, out_file);
	out_file.close();
}