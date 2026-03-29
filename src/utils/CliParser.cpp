#include "CliParser.hpp"

#include <algorithm>
#include <format>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <unordered_map>
#include <cstdlib>

struct CliParser::FlagSpec::Impl
{
	std::string long_name;
	std::string desc;
	std::optional<char> short_name;
	bool allow_multi = false;

	// results
	int hit_count = 0;
};

struct CliParser::FlagArgSpec::Impl
{
	std::string long_name;
	std::string desc;
	std::optional<char> short_name;
	bool is_required = false;
	std::optional<std::string> default_val;

	// results
	std::optional<std::string> parsed_value;
};

struct CliParser::PositionalSpec::Impl
{
	std::string meta_name;
	std::string desc;
	bool is_required = false;
	std::optional<std::string> default_val;

	// results
	std::optional<std::string> parsed_value;
};

struct CliParser::Impl
{
	std::string program_name;
	std::string desc;

	// specs stored as unique_ptr so addresses stay stable
	std::vector<std::unique_ptr<FlagSpec>> flags;
	std::vector<std::unique_ptr<FlagArgSpec>> flag_args;
	std::vector<std::unique_ptr<PositionalSpec>> positionals;

	// fast lookup maps built once at parse time
	std::unordered_map<std::string, FlagSpec::Impl *> flag_long;
	std::unordered_map<char, FlagSpec::Impl *> flag_short;
	std::unordered_map<std::string, FlagArgSpec::Impl *> farg_long;
	std::unordered_map<char, FlagArgSpec::Impl *> farg_short;

	/// Help flag spec, if one is created
	std::optional<FlagSpec *> help_flag_spec = std::nullopt;
	std::optional<int> help_flag_exit_code = std::nullopt;

	void build_lookup_tables()
	{
		this->flag_long.clear();
		this->flag_short.clear();
		this->farg_long.clear();
		this->farg_short.clear();

		for (auto &fs : this->flags)
		{
			auto *p = fs->impl_.get();
			this->flag_long[p->long_name] = p;
			if (p->short_name)
				this->flag_short[*p->short_name] = p;
		}
		for (auto &fa : this->flag_args)
		{
			auto *p = fa->impl_.get();
			this->farg_long[p->long_name] = p;
			if (p->short_name)
				this->farg_short[*p->short_name] = p;
		}
	}
};

CliParser::CliParser(std::string program_name, std::string desc)
	: impl_(std::make_unique<Impl>())
{
	this->impl_->program_name = std::move(program_name);
	this->impl_->desc = std::move(desc);
}

CliParser::~CliParser() = default;

CliParser::FlagSpec &CliParser::add_flag(std::string long_name, std::string desc)
{
	auto inner = std::make_unique<FlagSpec::Impl>();
	inner->long_name = std::move(long_name);
	inner->desc = std::move(desc);

	auto spec = std::unique_ptr<FlagSpec>(new FlagSpec(std::move(inner)));
	auto &ref = *spec;
	this->impl_->flags.push_back(std::move(spec));
	return ref;
}

CliParser::FlagArgSpec &CliParser::add_flag_arg(std::string long_name, std::string desc)
{
	auto inner = std::make_unique<FlagArgSpec::Impl>();
	inner->long_name = std::move(long_name);
	inner->desc = std::move(desc);

	auto spec = std::unique_ptr<FlagArgSpec>(new FlagArgSpec(std::move(inner)));
	auto &ref = *spec;
	this->impl_->flag_args.push_back(std::move(spec));
	return ref;
}

CliParser::PositionalSpec &CliParser::add_positional(std::string meta_name, std::string desc)
{
	auto inner = std::make_unique<PositionalSpec::Impl>();
	inner->meta_name = std::move(meta_name);
	inner->desc = std::move(desc);

	auto spec = std::unique_ptr<PositionalSpec>(new PositionalSpec(std::move(inner)));
	auto &ref = *spec;
	this->impl_->positionals.push_back(std::move(spec));
	return ref;
}

void CliParser::add_help_flag(std::optional<int> exit_code)
{
	auto inner = std::make_unique<FlagSpec::Impl>();
	inner->long_name = "help";
	inner->short_name = 'h';
	inner->desc = "Show this help text";

	auto spec = std::unique_ptr<FlagSpec>(new FlagSpec(std::move(inner)));
	this->impl_->help_flag_spec = spec.get();
	this->impl_->flags.push_back(std::move(spec));

	this->impl_->help_flag_exit_code = exit_code;
}

void CliParser::parse(int argc, const char *const *argv)
{
	// skip argv[0]
	std::vector<std::string_view> tokens;
	tokens.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
	for (int i = 1; i < argc; ++i)
		tokens.emplace_back(argv[i]);
	parse(tokens);
}

// the magic
void CliParser::parse(const std::vector<std::string_view> &tokens)
{

	this->impl_->build_lookup_tables();

	// reset all previous results
	for (auto &f : this->impl_->flags)
		f->impl_->hit_count = 0;
	for (auto &a : this->impl_->flag_args)
		a->impl_->parsed_value = std::nullopt;
	for (auto &p : this->impl_->positionals)
		p->impl_->parsed_value = std::nullopt;

	std::size_t pos_index = 0; // next positional slot to fill
	bool end_of_flags = false;

	auto error = [&](std::string msg) -> CliError
	{
		return CliError(std::move(msg));
	};

	for (std::size_t i = 0; i < tokens.size(); ++i)
	{
		std::string_view tok = tokens[i];

		// end-of-options sentinel
		if (tok == "--")
		{
			end_of_flags = true;
			continue;
		}

		// treat as positional if end-of-flags or doesn't start with '-'
		if (end_of_flags || tok.empty() || tok[0] != '-')
		{
			if (pos_index >= this->impl_->positionals.size())
				throw error(std::format("unexpected positional argument: '{}'", tok));
			this->impl_->positionals[pos_index++]->impl_->parsed_value = std::string(tok);
			continue;
		}

		// long option: --name or --name=value
		if (tok.size() >= 2 && tok[1] == '-')
		{
			std::string_view body = tok.substr(2); // strip leading --

			// split on equals if present
			std::optional<std::string_view> inline_val;
			if (auto eq = body.find('='); eq != std::string_view::npos)
			{
				inline_val = body.substr(eq + 1);
				body = body.substr(0, eq);
			}

			std::string key(body);

			// is it a boolean flag?
			if (auto it = this->impl_->flag_long.find(key); it != this->impl_->flag_long.end())
			{
				auto *fp = it->second;
				if (inline_val)
					throw error(std::format("flag '--{}' does not take a value", key));
				if (!fp->allow_multi && fp->hit_count > 0)
					throw error(std::format("flag '--{}' passed more than once", key));
				++fp->hit_count;
				continue;
			}

			// is it a flag argument?
			if (auto it = this->impl_->farg_long.find(key); it != this->impl_->farg_long.end())
			{
				auto *ap = it->second;
				if (inline_val)
				{
					ap->parsed_value = std::string(*inline_val);
				}
				else
				{
					if (i + 1 >= tokens.size())
						throw error(std::format("'--{}' requires a value", key));
					ap->parsed_value = std::string(tokens[++i]);
				}
				continue;
			}

			throw error(std::format("unknown option: '--{}'", key));
		}

		// short option(s): -v, -vvv, -o FILE, -oFILE
		{
			std::string_view cluster = tok.substr(1); // strip leading '-'

			for (std::size_t ci = 0; ci < cluster.size(); ++ci)
			{
				char c = cluster[ci];

				// boolean flag?
				if (auto it = this->impl_->flag_short.find(c); it != this->impl_->flag_short.end())
				{
					auto *fp = it->second;
					if (!fp->allow_multi && fp->hit_count > 0)
						throw error(std::format("flag '-{}' passed more than once", c));
					++fp->hit_count;
					continue;
				}

				// flag argument?
				if (auto it = this->impl_->farg_short.find(c); it != this->impl_->farg_short.end())
				{
					auto *ap = it->second;
					// remainder of cluster is the value (e.g. -ofile)
					std::string_view remainder = cluster.substr(ci + 1);
					if (!remainder.empty())
					{
						ap->parsed_value = std::string(remainder);
					}
					else
					{
						if (i + 1 >= tokens.size())
							throw error(std::format("'-{}' requires a value", c));
						ap->parsed_value = std::string(tokens[++i]);
					}
					break; // value consumed the rest of the cluster
				}

				throw error(std::format("unknown option: '-{}'", c));
			}
		}
	}

	// validating required flags
	for (auto &fa : this->impl_->flag_args)
	{
		auto *p = fa->impl_.get();
		if (p->is_required && !p->parsed_value)
			throw CliError(
				std::format("required option '--{}' was not provided", p->long_name));
	}

	// validating required positionals
	for (auto &pos : this->impl_->positionals)
	{
		auto *p = pos->impl_.get();
		if (p->is_required && !p->parsed_value)
			throw CliError(
				std::format("required positional argument <{}> was not provided",
							p->meta_name));
	}

	// printing help if flag present
	if (this->impl_->help_flag_spec.has_value() && this->impl_->help_flag_spec.value()->present())
	{
		this->print_help();
		if (this->impl_->help_flag_exit_code.has_value())
			std::exit(this->impl_->help_flag_exit_code.value());
	}
}

std::string CliParser::help_text() const
{
	std::string out;

	// usage line
	out += std::format("Usage: {}", this->impl_->program_name);
	if (!this->impl_->flags.empty() || !this->impl_->flag_args.empty())
		out += " [options]";
	for (auto &pos : this->impl_->positionals)
	{
		auto *p = pos->impl_.get();
		out += p->is_required ? std::format(" <{}>", p->meta_name) : std::format(" [{}]", p->meta_name);
	}
	out += '\n';

	if (!this->impl_->desc.empty())
		out += '\n' + this->impl_->desc + '\n';

	auto short_str = [](std::optional<char> s) -> std::string
	{
		return s ? std::format("-{}, ", *s) : "    ";
	};

	// flags
	if (!this->impl_->flags.empty())
	{
		out += "\nFlags:\n";
		for (auto &fs : this->impl_->flags)
		{
			auto *p = fs->impl_.get();
			out += std::format("  {}--{:<20} {}{}\n", short_str(p->short_name), p->long_name, p->desc, p->allow_multi ? " (repeatable)" : "");
		}
	}

	// flag arguments
	if (!this->impl_->flag_args.empty())
	{
		out += "\nOptions:\n";
		for (auto &fa : this->impl_->flag_args)
		{
			auto *p = fa->impl_.get();
			std::string lhs = std::format("--{} <{}>", p->long_name, p->long_name);
			std::string rhs = p->desc;
			if (p->is_required)
				rhs += " (required)";
			if (p->default_val)
				rhs += std::format(" [default: {}]", *p->default_val);
			out += std::format("  {}{:<22} {}\n", short_str(p->short_name), lhs, rhs);
		}
	}

	// positionals
	if (!this->impl_->positionals.empty())
	{
		out += "\nPositional arguments:\n";
		for (auto &pos : this->impl_->positionals)
		{
			auto *p = pos->impl_.get();
			std::string rhs = p->desc;
			if (p->is_required)
				rhs += " (required)";
			if (p->default_val)
				rhs += std::format(" [default: {}]", *p->default_val);
			out += std::format("  {:<26} {}\n",
							   std::format("<{}>", p->meta_name), rhs);
		}
	}

	return out;
}

void CliParser::print_help() const { std::cout << help_text(); }

CliParser::FlagSpec::FlagSpec(std::unique_ptr<Impl> i) : impl_(std::move(i)) {}
CliParser::FlagSpec::~FlagSpec() = default;

CliParser::FlagSpec &CliParser::FlagSpec::short_name(char c)
{
	this->impl_->short_name = c;
	return *this;
}

CliParser::FlagSpec &CliParser::FlagSpec::allow_multi()
{
	this->impl_->allow_multi = true;
	return *this;
}

bool CliParser::FlagSpec::present() const { return this->impl_->hit_count > 0; }
int CliParser::FlagSpec::count() const { return this->impl_->hit_count; }

CliParser::FlagArgSpec::FlagArgSpec(std::unique_ptr<Impl> i) : impl_(std::move(i)) {}
CliParser::FlagArgSpec::~FlagArgSpec() = default;

CliParser::FlagArgSpec &CliParser::FlagArgSpec::short_name(char c)
{
	this->impl_->short_name = c;
	return *this;
}

CliParser::FlagArgSpec &CliParser::FlagArgSpec::required()
{
	this->impl_->is_required = true;
	return *this;
}

CliParser::FlagArgSpec &CliParser::FlagArgSpec::default_value(std::string val)
{
	this->impl_->default_val = std::move(val);
	return *this;
}

bool CliParser::FlagArgSpec::present() const { return this->impl_->parsed_value.has_value(); }

std::string CliParser::FlagArgSpec::value() const
{
	if (this->impl_->parsed_value)
		return *this->impl_->parsed_value;
	if (this->impl_->default_val)
		return *this->impl_->default_val;
	return {};
}

std::optional<std::string> CliParser::FlagArgSpec::maybe_value() const
{
	if (this->impl_->parsed_value)
		return this->impl_->parsed_value;
	if (this->impl_->default_val)
		return this->impl_->default_val;
	return std::nullopt;
}

CliParser::PositionalSpec::PositionalSpec(std::unique_ptr<Impl> i) : impl_(std::move(i)) {}
CliParser::PositionalSpec::~PositionalSpec() = default;

CliParser::PositionalSpec &CliParser::PositionalSpec::required()
{
	this->impl_->is_required = true;
	return *this;
}

CliParser::PositionalSpec &CliParser::PositionalSpec::default_value(std::string val)
{
	this->impl_->default_val = std::move(val);
	return *this;
}

bool CliParser::PositionalSpec::present() const { return this->impl_->parsed_value.has_value(); }

std::string CliParser::PositionalSpec::value() const
{
	if (this->impl_->parsed_value)
		return *this->impl_->parsed_value;
	if (this->impl_->default_val)
		return *this->impl_->default_val;
	return {};
}

std::optional<std::string> CliParser::PositionalSpec::maybe_value() const
{
	if (this->impl_->parsed_value)
		return this->impl_->parsed_value;
	if (this->impl_->default_val)
		return this->impl_->default_val;
	return std::nullopt;
}

CliError::CliError(std::string s)
	: std::runtime_error(s) {}
