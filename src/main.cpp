#include <iostream>
#include <string>
#include <optional>

#include "utils/CliParser.hpp"
#include "utils/common.hpp"
#include "utils/logging.hpp"
#include "utils/error.hpp"

#include "compile.hpp"

int main(int argc, char **argv)
{
	CliParser cli("sasc-compiler");
	auto &input_filename_arg = cli.add_positional("file", "Input file").required();
	auto &output_filename_arg = cli.add_flag_arg("out", "Output file").short_name('o').default_value("a.out"); // TODO change
	auto &verbosity_flag = cli.add_flag("verbose", "Increase compiler verbosity").short_name('v').allow_multi();
	auto &quiet_flag = cli.add_flag("quiet", "Silence compiler output").short_name('q');
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

	// building compilation settings

	CompileSettings settings;

	std::string filename = input_filename_arg.value();
	if (filename.empty())
	{
		std::cerr << "Missing input file\n";
		return exit_code_as_int(ExitCode::UsageError);
	}
	log_vv("input file: {:s}", filename);

	settings.output_filename = output_filename_arg.value();
	log_vv("output file: {:s}", settings.output_filename);

	if (quiet_flag.present())
		logging::set_global_log_verbosity(-1);
	else
		logging::set_global_log_verbosity(verbosity_flag.count());
	log_vv("quiet: {}", bool_str(quiet_flag.present()));
	log_vv("verbosity: {}", logging::global_log_verbosity());

	settings.debug_print_asm = debug_print_ast_flag.present();
	log_vv("debug print ast: {}", bool_str(settings.debug_print_asm));

	try
	{
		log("{}: compiling...", filename);
		compile(filename, settings);
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
