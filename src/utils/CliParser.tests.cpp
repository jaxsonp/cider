#include "unit_testing.hpp"
#include "utils/CliParser.hpp"

using Tokens = std::vector<std::string_view>;
using Values = std::vector<std::string>;

// flags =========================================================================================================

TEST(flag_absent_and_present)
{
	CliParser cli("prog");
	auto &flag = cli.add_flag("verbose");

	cli.parse(Tokens{});
	ASSERT_TRUE(!flag.present());
	ASSERT_TRUE(!flag);

	cli.parse(Tokens{"--verbose"});
	ASSERT_TRUE(flag.present());
	ASSERT_TRUE(bool(flag));
}

TEST(short_flag)
{
	CliParser cli("prog");
	auto &flag = cli.add_flag("verbose").short_name('v');
	cli.parse(Tokens{"-v"});
	ASSERT_TRUE(flag.present());
}

TEST(short_flags_cluster)
{
	CliParser cli("prog");
	auto &a = cli.add_flag("aaa").short_name('a');
	auto &b = cli.add_flag("bbb").short_name('b');
	cli.parse(Tokens{"-ab"});
	ASSERT_TRUE(a.present());
	ASSERT_TRUE(b.present());
}

TEST(repeating_a_flag_is_an_error_unless_allowed)
{
	CliParser cli("prog");
	cli.add_flag("once").short_name('o');
	auto &many = cli.add_flag("many").short_name('m').allow_multi();

	ASSERT_THROWS(cli.parse(Tokens{"--once", "--once"}), CliError);
	ASSERT_THROWS(cli.parse(Tokens{"-oo"}), CliError);

	cli.parse(Tokens{"-mmm"});
	ASSERT_EQ(many.count(), 3);
}

TEST(flags_do_not_take_values)
{
	CliParser cli("prog");
	cli.add_flag("verbose");
	ASSERT_THROWS(cli.parse(Tokens{"--verbose=yes"}), CliError);
}

// options =======================================================================================================

TEST(option_value_forms)
{
	CliParser cli("prog");
	auto &opt = cli.add_flag_arg("out").short_name('o');

	cli.parse(Tokens{"--out", "a"});
	ASSERT_EQ(opt.value(), "a");

	cli.parse(Tokens{"--out=b"});
	ASSERT_EQ(opt.value(), "b");

	cli.parse(Tokens{"-o", "c"});
	ASSERT_EQ(opt.value(), "c");

	cli.parse(Tokens{"-od"});
	ASSERT_EQ(opt.value(), "d");
}

TEST(option_values_can_look_like_anything)
{
	CliParser cli("prog");
	auto &opt = cli.add_flag_arg("out");

	cli.parse(Tokens{"--out=a=b"});
	ASSERT_EQ(opt.value(), "a=b");

	cli.parse(Tokens{"--out="});
	ASSERT_TRUE(opt.present());
	ASSERT_EQ(opt.value(), "");

	cli.parse(Tokens{"--out", "--other"});
	ASSERT_EQ(opt.value(), "--other");
}

TEST(option_needs_a_value)
{
	CliParser cli("prog");
	cli.add_flag_arg("out").short_name('o');
	ASSERT_THROWS(cli.parse(Tokens{"--out"}), CliError);
	ASSERT_THROWS(cli.parse(Tokens{"-o"}), CliError);
}

TEST(short_option_in_a_flag_cluster)
{
	CliParser cli("prog");
	auto &flag = cli.add_flag("verbose").short_name('v');
	auto &opt = cli.add_flag_arg("out").short_name('o');
	cli.parse(Tokens{"-vofile"});
	ASSERT_TRUE(flag.present());
	ASSERT_EQ(opt.value(), "file");
}

TEST(repeating_an_option_is_an_error_unless_allowed)
{
	CliParser cli("prog");
	cli.add_flag_arg("once");
	auto &many = cli.add_flag_arg("many").allow_multi();

	ASSERT_THROWS(cli.parse(Tokens{"--once=1", "--once=2"}), CliError);

	cli.parse(Tokens{"--many=1", "--many=2"});
	ASSERT_TRUE(many.values() == (Values{"1", "2"}));
	ASSERT_EQ(many.value(), "2");
	ASSERT_EQ(many.count(), 2);
}

TEST(split_values)
{
	CliParser cli("prog");
	auto &opt = cli.add_flag_arg("emit").split_on(',').allow_multi();

	cli.parse(Tokens{"--emit=a,b"});
	ASSERT_TRUE(opt.values() == (Values{"a", "b"}));

	cli.parse(Tokens{"--emit=a", "--emit=b,c"});
	ASSERT_TRUE(opt.values() == (Values{"a", "b", "c"}));
}

TEST(split_keeps_empty_pieces)
{
	CliParser cli("prog");
	auto &opt = cli.add_flag_arg("emit").split_on(',');
	cli.parse(Tokens{"--emit=a,,b,"});
	ASSERT_TRUE(opt.values() == (Values{"a", "", "b", ""}));
}

TEST(maybe_value)
{
	CliParser cli("prog");
	auto &opt = cli.add_flag_arg("out");

	cli.parse(Tokens{});
	ASSERT_TRUE(!opt.maybe_value().has_value());
	ASSERT_EQ(opt.value(), "");

	cli.parse(Tokens{"--out=a"});
	ASSERT_EQ(opt.maybe_value().value(), "a");
}

// defaults and required arguments ===============================================================================

TEST(default_value_fills_in_for_an_absent_option)
{
	CliParser cli("prog");
	auto &opt = cli.add_flag_arg("out").default_value("a.out");

	cli.parse(Tokens{});
	ASSERT_EQ(opt.value(), "a.out");
	ASSERT_TRUE(!opt.present());

	cli.parse(Tokens{"--out=b"});
	ASSERT_EQ(opt.value(), "b");
	ASSERT_TRUE(opt.present());
	ASSERT_EQ(opt.count(), 1);
}

TEST(required_argument)
{
	CliParser cli("prog");
	cli.add_flag_arg("out").required();
	ASSERT_THROWS(cli.parse(Tokens{}), CliError);
	cli.parse(Tokens{"--out=a"});
}

TEST(a_default_does_not_satisfy_required)
{
	CliParser cli("prog");
	cli.add_flag_arg("out").required().default_value("a");
	ASSERT_THROWS(cli.parse(Tokens{}), CliError);
}

// positionals ===================================================================================================

TEST(positionals_fill_in_registration_order)
{
	CliParser cli("prog");
	auto &first = cli.add_positional("first");
	auto &second = cli.add_positional("second");
	cli.parse(Tokens{"a", "b"});
	ASSERT_EQ(first.value(), "a");
	ASSERT_EQ(second.value(), "b");
}

TEST(positionals_and_options_mix)
{
	CliParser cli("prog");
	auto &file = cli.add_positional("file");
	auto &flag = cli.add_flag("verbose");
	cli.parse(Tokens{"--verbose", "a"});
	ASSERT_EQ(file.value(), "a");
	ASSERT_TRUE(flag.present());

	cli.parse(Tokens{"a", "--verbose"});
	ASSERT_EQ(file.value(), "a");
	ASSERT_TRUE(flag.present());
}

TEST(too_many_positionals)
{
	CliParser cli("prog");
	cli.add_positional("file");
	ASSERT_THROWS(cli.parse(Tokens{"a", "b"}), CliError);
}

TEST(required_positional)
{
	CliParser cli("prog");
	cli.add_positional("file").required();
	ASSERT_THROWS(cli.parse(Tokens{}), CliError);
}

TEST(lone_dash_is_a_positional)
{
	CliParser cli("prog");
	auto &file = cli.add_positional("file");
	cli.parse(Tokens{"-"});
	ASSERT_EQ(file.value(), "-");
}

TEST(negative_numbers_are_positionals)
{
	CliParser cli("prog");
	auto &number = cli.add_positional("number");
	cli.parse(Tokens{"-5"});
	ASSERT_EQ(number.value(), "-5");
}

TEST(empty_token_is_a_positional)
{
	CliParser cli("prog");
	auto &file = cli.add_positional("file");
	cli.parse(Tokens{""});
	ASSERT_TRUE(file.present());
	ASSERT_EQ(file.value(), "");
}

TEST(double_dash_ends_the_options)
{
	CliParser cli("prog");
	auto &first = cli.add_positional("first");
	auto &second = cli.add_positional("second");
	auto &flag = cli.add_flag("verbose");
	cli.parse(Tokens{"--", "--verbose", "--"});
	ASSERT_EQ(first.value(), "--verbose");
	ASSERT_EQ(second.value(), "--");
	ASSERT_TRUE(!flag.present());
}

// errors ========================================================================================================

TEST(unknown_options)
{
	CliParser cli("prog");
	ASSERT_THROWS(cli.parse(Tokens{"--nope"}), CliError);
	ASSERT_THROWS(cli.parse(Tokens{"--nope=1"}), CliError);
	ASSERT_THROWS(cli.parse(Tokens{"-n"}), CliError);
}

TEST(option_with_no_name)
{
	CliParser cli("prog");
	ASSERT_THROWS(cli.parse(Tokens{"--=value"}), CliError);
}

TEST(registering_a_name_twice)
{
	CliParser cli("prog");
	cli.add_flag("name");
	ASSERT_THROWS(cli.add_flag("name"), CliError);
}

TEST(different_kinds_can_share_a_name)
{
	CliParser cli("prog");
	cli.add_flag("name");
	cli.add_positional("name");
}

TEST(positionals_are_not_options)
{
	CliParser cli("prog");
	cli.add_positional("file");
	ASSERT_THROWS(cli.parse(Tokens{"--file=a"}), CliError);
}

// parsing again =================================================================================================

TEST(parse_resets_results)
{
	CliParser cli("prog");
	auto &flag = cli.add_flag("verbose");
	auto &opt = cli.add_flag_arg("out");
	cli.parse(Tokens{"--verbose", "--out=a"});
	cli.parse(Tokens{});
	ASSERT_TRUE(!flag.present());
	ASSERT_TRUE(!opt.present());
	ASSERT_EQ(opt.value(), "");
}

TEST(argc_argv_skips_the_program_name)
{
	CliParser cli("prog");
	auto &file = cli.add_positional("file");
	const char *argv[] = {"prog", "a"};
	cli.parse(2, argv);
	ASSERT_EQ(file.value(), "a");
}

TEST(argc_of_zero)
{
	CliParser cli("prog");
	cli.parse(0, nullptr);
}

// help and version ==============================================================================================

TEST(help_flag_throws)
{
	CliParser cli("prog");
	cli.add_help_flag();
	ASSERT_THROWS(cli.parse(Tokens{"--help"}), CliHelpRequested);
	ASSERT_THROWS(cli.parse(Tokens{"-h"}), CliHelpRequested);
}

TEST(version_flag_throws)
{
	CliParser cli("prog");
	cli.add_version_flag();
	ASSERT_THROWS(cli.parse(Tokens{"--version"}), CliVersionRequested);
	ASSERT_THROWS(cli.parse(Tokens{"-V"}), CliVersionRequested);
}

TEST(help_wins_over_validation)
{
	CliParser cli("prog");
	cli.add_help_flag();
	cli.add_positional("file").required();
	ASSERT_THROWS(cli.parse(Tokens{"-h"}), CliHelpRequested);
}

TEST(help_text_usage_line)
{
	CliParser cli("prog");
	cli.add_positional("in").required();
	cli.add_positional("out");
	cli.add_flag("verbose");
	ASSERT_TRUE(cli.help_text().starts_with("Usage: prog [options] <in> [out]\n"));
}

TEST(help_text_without_options)
{
	CliParser cli("prog");
	cli.add_positional("in");
	ASSERT_TRUE(cli.help_text().starts_with("Usage: prog [in]\n"));
}

TEST(help_text_description)
{
	CliParser cli("prog", "Does things");
	ASSERT_TRUE(cli.help_text().find("\nDoes things\n") != std::string::npos);
}

TEST(help_text_sections_and_annotations)
{
	CliParser cli("prog");
	cli.add_positional("file", "Input").required();
	cli.add_flag("verbose", "Talk more").short_name('v').allow_multi();
	cli.add_flag_arg("out", "Output").short_name('o').metavar("FILE").default_value("a.out");

	ASSERT_EQ(cli.help_text(),
			  "Usage: prog [options] <file>\n"
			  "\n"
			  "Positional arguments:\n"
			  "  <file>            Input (required)\n"
			  "\n"
			  "Flags:\n"
			  "  -v, --verbose     Talk more (repeatable)\n"
			  "\n"
			  "Options:\n"
			  "  -o, --out <FILE>  Output [default: a.out]\n");
}

TEST(help_text_lines_up_arguments_with_no_short_name)
{
	CliParser cli("prog");
	cli.add_flag("long-flag", "Flag");
	ASSERT_TRUE(cli.help_text().find("      --long-flag  Flag\n") != std::string::npos);
}
