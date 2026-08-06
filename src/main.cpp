#include <filesystem>
#include <iostream>
#include <format>
#include <map>
#include <string>
#include <vector>

#include "utils/CliParser.hpp"
#include "utils/common.hpp"
#include "utils/logging.hpp"
#include "utils/error.hpp"

#include "compile.hpp"

namespace
{
	struct EmitKindInfo
	{
		std::string_view name;
		EmitKind kind;
		/// Extension for the derived default path, e.g. "foo.cdr" -> "foo.ir"
		std::string_view extension;
		std::string_view description;
	};

	constexpr EmitKindInfo EMIT_KINDS[] = {
		{"ast", EmitKind::Ast, ".ast", "parsed abstract syntax tree"},
		{"ir", EmitKind::Ir, ".ir", "textual intermediate representation"},
		{"asm", EmitKind::Asm, ".s", "textual target assembly"},
		{"exe", EmitKind::Exe, "", "linked executable"},
	};

	/// The value meaning "write to standard output" instead of a file
	constexpr std::string_view STDOUT_SPECIFIER = "stdout";

	std::string emit_kind_list()
	{
		std::string out;
		for (const EmitKindInfo &info : EMIT_KINDS)
			out += std::format("{}{}", out.empty() ? "" : ", ", info.name);
		return out;
	}

	std::string_view emit_kind_name(EmitKind kind)
	{
		for (const EmitKindInfo &info : EMIT_KINDS)
			if (info.kind == kind)
				return info.name;
		throw CompilerError::internal("Uncaught emit kind");
	}

	/// @brief Parse one --emit value: a kind, optionally followed by "=path"
	std::pair<EmitKind, EmitTarget> parse_emit(std::string_view value, const std::filesystem::path &input)
	{
		std::string_view kind_name = value;
		std::optional<std::string_view> path;
		if (std::size_t eq = value.find('='); eq != std::string_view::npos)
		{
			kind_name = value.substr(0, eq);
			path = value.substr(eq + 1);
		}

		const EmitKindInfo *info = nullptr;
		for (const EmitKindInfo &candidate : EMIT_KINDS)
			if (candidate.name == kind_name)
				info = &candidate;
		if (info == nullptr)
			throw CompilerError::unsupported(
				std::format("Unknown emit kind \"{}\" (expected one of: {})", kind_name, emit_kind_list()));

		if (path && path->empty())
			throw CompilerError::unsupported(std::format("Empty output path for emit kind \"{}\"", kind_name));

		EmitTarget target;
		if (!path)
			// no path given, derive one from the input filename
			target.path = std::filesystem::path(input).replace_extension(info->extension).string();
		else if (*path == STDOUT_SPECIFIER)
			target.to_stdout = true;
		else
			target.path = *path;

		return {info->kind, target};
	}
}

int main(int argc, char **argv)
{
	CliParser cli("ciderc", "Compiler for the Cider programming language");

	auto &input_filename_arg = cli.add_positional("file", "Input file").required();
	auto &output_filename_arg = cli.add_flag_arg("out", "Output file path for the executable, if \"--emit exe\"")
									.short_name('o')
									.metavar("FILE")
									.default_value("a.out");
	auto &target_arg = cli.add_flag_arg("target", "Target platform").short_name('t').metavar("TARGET").required();
	auto &emit_arg = cli.add_flag_arg(
							"emit",
							std::format("Artifacts to produce: {}. Append \"=PATH\" to write to file, or \"={}\" for standard output", emit_kind_list(), STDOUT_SPECIFIER))
						 .metavar("KIND")
						 .allow_multi()
						 .split_on(',')
						 .default_value("exe");

	auto &verbosity_flag = cli.add_flag("verbose", "Increase compiler verbosity").short_name('v').allow_multi();
	auto &quiet_flag = cli.add_flag("quiet", "Silence compiler output").short_name('q');
	cli.add_help_flag();

	try
	{
		cli.parse(argc, argv);
	}
	catch (const CliHelpRequested &)
	{
		cli.print_help();
		return exit_code_as_int(ExitCode::Success);
	}
	catch (const CliError &ex)
	{
		std::cerr << "Error: " << ex.what() << "\n\n";
		cli.print_help();
		return exit_code_as_int(ExitCode::UsageError);
	}

	if (quiet_flag.present())
		logging::set_global_log_verbosity(-1);
	else
		logging::set_global_log_verbosity(verbosity_flag.count());
	log_vv("quiet: {}", bool_str(quiet_flag.present()));
	log_vv("verbosity: {}", logging::global_log_verbosity());

	std::string filename = input_filename_arg.value();
	log_vv("input file: {:s}", filename);

	// building compilation settings

	try
	{
		auto found_target = Target::supported_targets.find(target_arg.value());
		if (found_target == Target::supported_targets.end())
		{
			std::string supported;
			for (const auto &[name, target] : Target::supported_targets)
				supported += std::format("{}{}", supported.empty() ? "" : ", ", name);
			throw CompilerError::unsupported(
				std::format("Unknown target \"{}\" (supported targets: {})", target_arg.value(), supported));
		}
		log_vv("target: {:s}", found_target->first);

		CompileSettings settings(found_target->second);

		for (const std::string &value : emit_arg.values())
			settings.emits.insert(parse_emit(value, filename));
		if (settings.emits.empty())
			// only reachable if --emit was passed with an empty value
			throw CompilerError::unsupported("No artifacts to emit");

		// the executable path comes from -o, not from the input filename
		if (auto exe = settings.emits.find(EmitKind::Exe); exe != settings.emits.end() && !exe->second.to_stdout)
			exe->second.path = output_filename_arg.value();

		if (settings.wants(EmitKind::Asm))
			throw CompilerError::unimplemented("Textual assembly output is not implemented yet");

		for (const auto &[kind, target] : settings.emits)
			log_vv("emitting {} to {}", emit_kind_name(kind), target.to_stdout ? STDOUT_SPECIFIER : target.path);

		log("{}: compiling...", filename);
		compile(filename, settings);
		log("{}: compilation complete", filename);
	}
	catch (const CompilerError &e)
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
