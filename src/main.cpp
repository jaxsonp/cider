#include <fstream>
#include <iostream>
#include <string>
#include <optional>

#include "utils/CliParser.hpp"
#include "utils/common.hpp"
#include "utils/logging.hpp"
#include "utils/error.hpp"

#include "frontend/AST.hpp"
#include "IR.hpp"
#include "Object.hpp"
#include "backend/Backend.hpp"
#include "backend/riscv32/Backend_RV32.hpp"

struct CompileConfig
{
	std::string input_filename;
	bool debug_print_asm = false;
	std::optional<std::string> asm_dump_filename = std::nullopt;

	CompileConfig() = delete;
	CompileConfig(const std::string &filename) : input_filename(filename) {}
};

void compile(const std::string &, CompileConfig &config);

int main(int argc, char **argv)
{
	CliParser cli("sasc-compiler");
	auto &filename_arg = cli.add_positional("file", "Input file to compile").required();
	auto &verbosity_flag = cli.add_flag("verbose", "Increase compiler verbosity").short_name('v').allow_multi();
	auto &quiet_flag = cli.add_flag("quiet", "Silence compiler output").short_name('q');
	auto &dump_asm_arg = cli.add_flag_arg("dump-asm", "Dump debug assembly to the file specified");
	auto &debug_print_ast_flag = cli.add_flag("debug-print-ast", "Print a textual representation of the parsed abstract syntax tree");
	cli.add_help_flag(exit_code_as_int(ExitCode::Success));

	try
	{
		cli.parse(argc, argv);
	}
	catch (const CliError &ex)
	{
		std::cerr << "Error: " << ex.what() << "\n\n";
		cli.print_help();
		return exit_code_as_int(ExitCode::UncaughtInternalError);
	}

	std::string filename = filename_arg.value();
	log_vv("input file: {:s}", filename.c_str());
	if (filename.empty())
	{
		std::cerr << "Missing input file\n";
		return exit_code_as_int(ExitCode::UsageError);
	}

	CompileConfig config(filename);

	log_vv("quiet: {}", bool_str(quiet_flag.present()));
	if (quiet_flag.present())
		logging::set_global_log_verbosity(-1);
	else
		logging::set_global_log_verbosity(verbosity_flag.count());
	log_vv("verbosity: {}", logging::global_log_verbosity());

	config.asm_dump_filename = dump_asm_arg.maybe_value();
	log_vv("dump assembly: \"{}\"", (config.asm_dump_filename.has_value() ? config.asm_dump_filename.value() : "no"));

	config.debug_print_asm = debug_print_ast_flag.present();
	log_vv("debug print ast: {}", bool_str(config.debug_print_asm));

	try
	{
		log("{}: compiling...", filename);
		compile(filename, config);
		log("{}: compilation complete", filename);
	}
	catch (const CompileError &e)
	{
		std::cerr << "\n"
				  << e.what() << "\n";
		log("{}: compilation failed", filename);
		return exit_code_as_int(e.exit_code());
	}
	catch (const std::exception &e)
	{
		std::cerr << "\nUncaught exception occurred: " << e.what() << "\n";
		return exit_code_as_int(ExitCode::UncaughtInternalError);
	}

	return exit_code_as_int(ExitCode::Success);
}

void compile(const std::string &filename, CompileConfig &config)
{
	log_v("Preparing for compilation");
	log_vv("Opening file \"{}\" for reading", filename);
	std::ifstream file(filename);
	if (!file.is_open())
		throw FileOpenError(std::format("Failed to open file \"{}\" for reading", filename));
	log_vv("File opened");

	log_v("Building AST");
	AST ast(file);

	if (config.debug_print_asm)
	{
		log_vv("Debug printing AST");
		ast.debug_print();
	}

	log_v("Emitting IR");
	IrObject *ir = ast.emitIr();

	// TODO generalize this
	log_v("Lowering IR to machine code");
	Backend *backend = new backends::rv32::Backend_RV32();
	if (config.asm_dump_filename.has_value())
	{
		log_vv("Opening file \"{}\" for writing", config.asm_dump_filename.value());
		std::unique_ptr<std::ofstream> asm_dump_file = std::make_unique<std::ofstream>(config.asm_dump_filename.value());
		if (!asm_dump_file->is_open())
			throw FileOpenError(std::format("Failed to open file \"{}\" for writing", filename));
		backend->enable_asm_output(std::move(asm_dump_file));
	}
	Object *obj = backend->lower_ir(ir);

	// TEMP
	log_v("Debug printing computed machine code");

	std::ofstream hex_file("out.hex");
	if (!hex_file.is_open())
		throw FileOpenError(std::format("Failed to open file \"{}\" for writing", "out.hex"));

	std::cout << "      24      16       8       0\n";
	size_t i = 0;
	std::vector<uint8_t> row;
	while (i < obj->text.size())
	{
		row.push_back(obj->text.at(i));
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
}