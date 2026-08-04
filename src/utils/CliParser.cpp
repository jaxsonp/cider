#include "CliParser.hpp"

#include <algorithm>
#include <format>
#include <iostream>

// Arg ========================================================================

CliParser::Arg &CliParser::Arg::short_name(char c)
{
	this->short_char = c;
	return *this;
}

CliParser::Arg &CliParser::Arg::metavar(std::string name)
{
	this->meta = std::move(name);
	return *this;
}

CliParser::Arg &CliParser::Arg::required()
{
	this->is_required = true;
	return *this;
}

CliParser::Arg &CliParser::Arg::default_value(std::string val)
{
	this->default_val = std::move(val);
	return *this;
}

CliParser::Arg &CliParser::Arg::allow_multi()
{
	this->multi_allowed = true;
	return *this;
}

CliParser::Arg &CliParser::Arg::split_on(char sep)
{
	this->split_sep = sep;
	return *this;
}

std::string CliParser::Arg::value() const
{
	return this->parsed_values.empty() ? std::string{} : this->parsed_values.back();
}

std::optional<std::string> CliParser::Arg::maybe_value() const
{
	if (this->parsed_values.empty())
		return std::nullopt;
	return this->parsed_values.back();
}

std::string CliParser::Arg::display_name() const
{
	if (this->kind == Kind::Positional)
		return std::format("<{}>", this->name);
	return std::format("--{}", this->name);
}

void CliParser::Arg::add_value(std::string_view val)
{
	if (!this->split_sep)
	{
		this->parsed_values.emplace_back(val);
		return;
	}

	// split on the separator, keeping empty pieces so `--emit=a,,b` is caught downstream
	std::size_t start = 0;
	while (true)
	{
		std::size_t sep = val.find(*this->split_sep, start);
		if (sep == std::string_view::npos)
		{
			this->parsed_values.emplace_back(val.substr(start));
			return;
		}
		this->parsed_values.emplace_back(val.substr(start, sep - start));
		start = sep + 1;
	}
}

// registration ===============================================================

CliParser::CliParser(std::string program_name, std::string desc)
	: program_name(std::move(program_name)), desc(std::move(desc)) {}

CliParser::Arg &CliParser::add(Arg::Kind kind, std::string name, std::string desc)
{
	for (const auto &existing : this->args)
		if (existing->name == name && existing->kind == kind)
			throw CliError(std::format("argument '{}' registered more than once", name));

	this->args.push_back(std::unique_ptr<Arg>(new Arg(kind, std::move(name), std::move(desc))));
	return *this->args.back();
}

CliParser::Arg &CliParser::add_flag(std::string long_name, std::string description)
{
	return this->add(Arg::Kind::Flag, std::move(long_name), std::move(description));
}

CliParser::Arg &CliParser::add_flag_arg(std::string long_name, std::string description)
{
	return this->add(Arg::Kind::Option, std::move(long_name), std::move(description));
}

CliParser::Arg &CliParser::add_positional(std::string meta_name, std::string description)
{
	return this->add(Arg::Kind::Positional, std::move(meta_name), std::move(description));
}

void CliParser::add_help_flag()
{
	this->help_arg = &this->add_flag("help", "Show this help text").short_name('h');
}

CliParser::Arg *CliParser::find_long(std::string_view name, Arg::Kind kind)
{
	for (const auto &arg : this->args)
		if (arg->kind == kind && arg->name == name)
			return arg.get();
	return nullptr;
}

CliParser::Arg *CliParser::find_short(char c, Arg::Kind kind)
{
	for (const auto &arg : this->args)
		if (arg->kind == kind && arg->short_char == c)
			return arg.get();
	return nullptr;
}

// parsing ====================================================================

void CliParser::parse(int argc, const char *const *argv)
{
	// skip argv[0]
	std::vector<std::string_view> tokens;
	tokens.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
	for (int i = 1; i < argc; ++i)
		tokens.emplace_back(argv[i]);
	this->parse(tokens);
}

namespace
{
	/// Whether a '-'-prefixed token is a negative number rather than an option cluster
	bool looks_like_negative_number(std::string_view tok)
	{
		if (tok.size() < 2 || tok[0] != '-')
			return false;
		return tok[1] >= '0' && tok[1] <= '9';
	}
}

void CliParser::parse(const std::vector<std::string_view> &tokens)
{
	// reset previous results
	for (auto &arg : this->args)
	{
		arg->seen = false;
		arg->parsed_values.clear();
	}

	// positional slots, in registration order
	std::vector<Arg *> positionals;
	for (auto &arg : this->args)
		if (arg->kind == Arg::Kind::Positional)
			positionals.push_back(arg.get());

	std::size_t pos_index = 0; // next positional slot to fill
	bool end_of_flags = false;

	auto take_positional = [&](std::string_view tok)
	{
		if (pos_index >= positionals.size())
			throw CliError(std::format("unexpected positional argument: '{}'", tok));
		Arg *pos = positionals[pos_index++];
		pos->seen = true;
		pos->add_value(tok);
	};

	// records a hit on a boolean flag, rejecting a repeat unless allowed
	auto hit_flag = [&](Arg *flag, std::string_view spelling)
	{
		if (!flag->multi_allowed && flag->seen)
			throw CliError(std::format("flag '{}' passed more than once", spelling));
		flag->seen = true;
		flag->add_value("");
	};

	// records a value on an option, rejecting a repeat unless allowed
	auto set_option = [&](Arg *opt, std::string_view spelling, std::string_view val)
	{
		if (!opt->multi_allowed && opt->seen)
			throw CliError(std::format("option '{}' passed more than once", spelling));
		opt->seen = true;
		opt->add_value(val);
	};

	for (std::size_t i = 0; i < tokens.size(); ++i)
	{
		std::string_view tok = tokens[i];

		// end-of-options sentinel — only the first one is a sentinel, later ones are positionals
		if (tok == "--" && !end_of_flags)
		{
			end_of_flags = true;
			continue;
		}

		// a bare '-' or a negative number is never an option
		if (end_of_flags || tok.empty() || tok[0] != '-' || tok == "-" || looks_like_negative_number(tok))
		{
			take_positional(tok);
			continue;
		}

		// long option: --name or --name=value
		if (tok.size() >= 2 && tok[1] == '-')
		{
			std::string_view body = tok.substr(2); // strip leading --

			// split on equals if present
			std::optional<std::string_view> inline_val;
			if (std::size_t eq = body.find('='); eq != std::string_view::npos)
			{
				inline_val = body.substr(eq + 1);
				body = body.substr(0, eq);
			}

			if (body.empty())
				throw CliError(std::format("malformed option: '{}'", tok));

			if (Arg *flag = this->find_long(body, Arg::Kind::Flag); flag != nullptr)
			{
				if (inline_val)
					throw CliError(std::format("flag '--{}' does not take a value", body));
				hit_flag(flag, tok);

				// report help immediately, before any validation can reject the command line
				if (flag == this->help_arg)
					throw CliHelpRequested();
				continue;
			}

			if (Arg *opt = this->find_long(body, Arg::Kind::Option); opt != nullptr)
			{
				if (!inline_val)
				{
					if (i + 1 >= tokens.size())
						throw CliError(std::format("option '--{}' requires a value", body));
					inline_val = tokens[++i];
				}
				set_option(opt, std::format("--{}", body), *inline_val);
				continue;
			}

			throw CliError(std::format("unknown option: '{}'", tok));
		}

		// short option(s): -v, -vvv, -o FILE, -oFILE
		std::string_view cluster = tok.substr(1); // strip leading '-'
		for (std::size_t ci = 0; ci < cluster.size(); ++ci)
		{
			char c = cluster[ci];

			if (Arg *flag = this->find_short(c, Arg::Kind::Flag); flag != nullptr)
			{
				hit_flag(flag, std::format("-{}", c));

				if (flag == this->help_arg)
					throw CliHelpRequested();
				continue;
			}

			if (Arg *opt = this->find_short(c, Arg::Kind::Option); opt != nullptr)
			{
				// remainder of the cluster is the value (e.g. -ofile), else the next token
				std::string_view remainder = cluster.substr(ci + 1);
				if (remainder.empty())
				{
					if (i + 1 >= tokens.size())
						throw CliError(std::format("option '-{}' requires a value", c));
					remainder = tokens[++i];
				}
				set_option(opt, std::format("-{}", c), remainder);
				break; // the value consumed the rest of the cluster
			}

			throw CliError(std::format("unknown option: '-{}' (in '{}')", c, tok));
		}
	}

	// fill in defaults for anything that wasn't passed, so callers never have to check
	for (auto &arg : this->args)
		if (!arg->seen && arg->default_val)
			arg->add_value(*arg->default_val);

	// validate required arguments
	for (const auto &arg : this->args)
		if (arg->is_required && !arg->seen)
			throw CliError(std::format("required argument {} was not provided", arg->display_name()));
}

// help text ==================================================================

std::string CliParser::help_text() const
{
	// left-hand column for one argument, e.g. "-o, --out <FILE>" or "<file>"
	auto lhs_for = [](const Arg &arg) -> std::string
	{
		if (arg.kind == Arg::Kind::Positional)
			return std::format("<{}>", arg.name);

		std::string out = arg.short_char ? std::format("-{}, ", *arg.short_char) : "    ";
		out += std::format("--{}", arg.name);
		if (arg.kind == Arg::Kind::Option)
			out += std::format(" <{}>", arg.meta ? *arg.meta : arg.name);
		return out;
	};

	// right-hand column: description plus any annotations
	auto rhs_for = [](const Arg &arg) -> std::string
	{
		std::string out = arg.desc;
		if (arg.is_required)
			out += " (required)";
		if (arg.multi_allowed)
			out += " (repeatable)";
		if (arg.default_val)
			out += std::format(" [default: {}]", *arg.default_val);
		return out;
	};

	// align every description to the same column, sized to the widest entry
	std::size_t width = 0;
	for (const auto &arg : this->args)
		width = std::max(width, lhs_for(*arg).size());

	std::string out;

	// usage line
	out += std::format("Usage: {}", this->program_name);
	bool has_options = false;
	for (const auto &arg : this->args)
		has_options |= arg->kind != Arg::Kind::Positional;
	if (has_options)
		out += " [options]";
	for (const auto &arg : this->args)
	{
		if (arg->kind != Arg::Kind::Positional)
			continue;
		out += arg->is_required ? std::format(" <{}>", arg->name) : std::format(" [{}]", arg->name);
	}
	out += '\n';

	if (!this->desc.empty())
		out += '\n' + this->desc + '\n';

	// one section per kind, in a fixed order
	struct Section
	{
		Arg::Kind kind;
		std::string_view heading;
	};
	static constexpr Section sections[] = {
		{Arg::Kind::Positional, "Positional arguments"},
		{Arg::Kind::Flag, "Flags"},
		{Arg::Kind::Option, "Options"},
	};

	for (const Section &section : sections)
	{
		bool printed_heading = false;
		for (const auto &arg : this->args)
		{
			if (arg->kind != section.kind)
				continue;
			if (!printed_heading)
			{
				out += std::format("\n{}:\n", section.heading);
				printed_heading = true;
			}
			out += std::format("  {:<{}}  {}\n", lhs_for(*arg), width, rhs_for(*arg));
		}
	}

	return out;
}

void CliParser::print_help() const { std::cout << this->help_text(); }
