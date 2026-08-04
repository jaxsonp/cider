#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>

/// CLI argument parser
class CliParser
{
public:
	/// A registered argument: a boolean flag, a flag that takes a value, or a positional.
	/// Created through CliParser::add_*, which return a reference valid for the parser's lifetime.
	class Arg
	{
	public:
		// configuration (chainable)

		/// Assign a single-character short name (e.g. 'v' → -v). Flags/options only
		Arg &short_name(char c);

		/// Name shown for this argument's value in the help text (defaults to the long name)
		Arg &metavar(std::string name);

		/// Mark as mandatory (parse() throws if absent)
		Arg &required();

		/// Provide a fallback used when the argument is absent
		Arg &default_value(std::string val);

		/// Allow this argument to be passed more than once
		Arg &allow_multi();

		/// Split each value on `sep`, so `--emit=a,b` yields two values. Options only
		Arg &split_on(char sep);

		// results. A default value, if configured, is filled in by parse() when the
		// argument is absent, so the accessors below never need to special-case it

		/// True if the argument was actually passed (false when only a default applied)
		[[nodiscard]] bool present() const { return this->seen; }

		/// Number of times the argument appeared (always 0 or 1 unless allow_multi() set)
		[[nodiscard]] int count() const { return static_cast<int>(this->parsed_values.size()); }

		/// Last value, else ""
		[[nodiscard]] std::string value() const;

		/// Last value, else nullopt
		[[nodiscard]] std::optional<std::string> maybe_value() const;

		/// Every value, in order
		[[nodiscard]] const std::vector<std::string> &values() const { return this->parsed_values; }

		[[nodiscard]] explicit operator bool() const { return this->present(); }

	private:
		friend class CliParser;

		enum class Kind
		{
			Flag,		// --verbose
			Option,		// --out FILE
			Positional, // FILE
		};

		Arg(Kind kind, std::string name, std::string desc)
			: kind(kind), name(std::move(name)), desc(std::move(desc)) {}

		/// Human-readable spelling for error messages, e.g. "--out" or "<file>"
		[[nodiscard]] std::string display_name() const;

		/// Record a value, applying split_on if configured
		void add_value(std::string_view val);

		Kind kind;
		std::string name;
		std::string desc;
		std::optional<char> short_char;
		std::optional<std::string> meta;
		std::optional<std::string> default_val;
		std::optional<char> split_sep;
		bool is_required = false;
		bool multi_allowed = false;

		/// Whether the argument was actually passed, as opposed to filled in from a default
		bool seen = false;
		std::vector<std::string> parsed_values;
	};

	explicit CliParser(std::string program_name, std::string description = {});

	CliParser(const CliParser &) = delete;
	CliParser &operator=(const CliParser &) = delete;

	// argument registration. Each throws CliError if the name or short name is already taken

	/// Register a boolean flag: --name
	Arg &add_flag(std::string long_name, std::string description = {});

	/// Register a flag that takes a value: --name VALUE  or  --name=VALUE
	Arg &add_flag_arg(std::string long_name, std::string description = {});

	/// Register the next positional argument (order of calls = capture order)
	Arg &add_positional(std::string meta_name, std::string description = {});

	/// Register a '--help'/'-h' flag. parse() throws CliHelpRequested when it is passed
	void add_help_flag();

	/// Parse an (argc, argv) pair from main(). argv[0] (program name) is automatically skipped.
	/// Throws CliHelpRequested if the help flag was passed, or CliError with a
	/// human-readable message on any error
	void parse(int argc, const char *const *argv);

	/// Parse an explicit list of tokens (argv[0] NOT included)
	void parse(const std::vector<std::string_view> &tokens);

	/// Returns a formatted help string
	[[nodiscard]] std::string help_text() const;

	/// Prints help to stdout
	void print_help() const;

private:
	/// Register an argument, checking for name collisions
	Arg &add(Arg::Kind kind, std::string name, std::string desc);

	/// Find a registered argument by long name / short name, or nullptr
	[[nodiscard]] Arg *find_long(std::string_view name, Arg::Kind kind);
	[[nodiscard]] Arg *find_short(char c, Arg::Kind kind);

	std::string program_name;
	std::string desc;

	/// Owned via unique_ptr so the references handed out by add_* stay valid as the vector grows
	std::vector<std::unique_ptr<Arg>> args;
	Arg *help_arg = nullptr;
};

/// Wrapper around runtime error for CLI-specific errors
class CliError : public std::runtime_error
{
public:
	explicit CliError(const std::string &s) : std::runtime_error(s) {}
};

/// Thrown by CliParser::parse() when the help flag is passed
class CliHelpRequested
{
};
