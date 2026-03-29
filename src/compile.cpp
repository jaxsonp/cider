#include "compile.hpp"

#include <fstream>

#include "utils/logging.hpp"
#include "utils/error.hpp"
#include "frontend/AST.hpp"
#include "IR.hpp"
#include "backend/Backend.hpp"
#include "backend/riscv32/Backend_RV32.hpp"
#include "objwriter/ObjectWriter.hpp"
#include "objwriter/elf32/ObjectWriter_ELF32.hpp"

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

	if (settings.debug_print_asm)
	{
		log_vv("Debug printing AST");
		ast.debug_print();
	}

	log_v("Emitting IR");
	IrObject ir = ast.emitIr();

	// TODO generalize this
	log_v("Lowering IR to machine code");
	backends::rv32::Backend_RV32 backend;
	Object obj(backend.lower_ir(ir));

	// TEMP
	log_v("Debug printing computed machine code");

	std::ofstream hex_file("out.hex");
	if (!hex_file.is_open())
		throw FileOpenError(std::format("Failed to open file \"{}\" for writing", "out.hex"));

	std::cout << "      24      16       8       0\n";
	size_t i = 0;
	std::vector<uint8_t> row;
	while (i < obj.text.size())
	{
		row.push_back(obj.text.at(i));
		if (row.size() == 4)
		{
			while (!row.empty())
			{
				hex_file << std::format("{:02x}", row.back());
				std::cout << std::format("{1}{0:08b}", row.back(), (row.size() % 2 == 0) ? "\033[0m" : "\033[90m");
				row.pop_back();
			}
			std::cout << "\033[0m " << (i - 3) << "\n";
			hex_file << "\n";
		}
		++i;
	}
	std::cout << std::endl;
	hex_file.close();

	log_v("Emitting ELF object");

	log_vv("Opening file \"{}\" for writing", settings.output_filename);
	std::ofstream out_file(settings.output_filename);
	if (!out_file.is_open())
		throw FileOpenError(std::format("Failed to open file \"{}\" for writing", settings.output_filename));

	objwriter::elf32::ObjectWriter_ELF32 obj_writer(std::move(obj));
}