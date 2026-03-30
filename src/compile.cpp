#include "compile.hpp"

#include <fstream>

#include "utils/logging.hpp"
#include "utils/error.hpp"
#include "frontend/AST.hpp"
#include "IR.hpp"

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

	log_vv("Opening file \"{}\" for writing", settings.output_filename);
	std::ofstream out_file(settings.output_filename, std::ios::binary);
	if (!out_file.is_open())
		throw FileOpenError(std::format("Failed to open file \"{}\" for writing", settings.output_filename));

	log_v("Emitting ELF object");
	auto obj_writer = settings.target.get_object_writer();
	obj_writer->emit(obj, out_file);
	out_file.close();
}