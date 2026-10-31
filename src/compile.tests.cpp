#include "unit_testing.hpp"
#include "compile.hpp"

#include "utils/error.hpp"

static CompileSettings settings_for(std::initializer_list<EmitKind> kinds)
{
	CompileSettings settings(Target("linux-x86_64"));
	for (EmitKind kind : kinds)
		settings.emits[kind] = EmitTarget{.path = "", .to_stdout = true};
	return settings;
}

TEST(wants_only_what_was_asked_for)
{
	CompileSettings settings = settings_for({EmitKind::Ir});
	ASSERT_TRUE(settings.wants(EmitKind::Ir));
	ASSERT_TRUE(!settings.wants(EmitKind::Ast));
	ASSERT_TRUE(!settings.wants(EmitKind::Asm));
	ASSERT_TRUE(!settings.wants(EmitKind::Exe));
}

TEST(furthest_of_one_stage)
{
	ASSERT_TRUE(settings_for({EmitKind::Ast}).furthest() == EmitKind::Ast);
	ASSERT_TRUE(settings_for({EmitKind::Ir}).furthest() == EmitKind::Ir);
	ASSERT_TRUE(settings_for({EmitKind::Asm}).furthest() == EmitKind::Asm);
	ASSERT_TRUE(settings_for({EmitKind::Exe}).furthest() == EmitKind::Exe);
}

TEST(furthest_goes_by_pipeline_order_not_request_order)
{
	ASSERT_TRUE(settings_for({EmitKind::Exe, EmitKind::Ast}).furthest() == EmitKind::Exe);
	ASSERT_TRUE(settings_for({EmitKind::Ast, EmitKind::Asm, EmitKind::Ir}).furthest() == EmitKind::Asm);
}

TEST(furthest_of_nothing)
{
	ASSERT_THROWS(settings_for({}).furthest(), CompilerError);
}

TEST(missing_input_file)
{
	try
	{
		compile("/this/file/does/not/exist.cdr", settings_for({EmitKind::Ast}));
		ASSERT_TRUE(false);
	}
	catch (const CompilerError &err)
	{
		ASSERT_EQ(int(err.exit_code()), int(ExitCode::FileIoError));
	}
}

TEST(unwritable_output_path_fails_before_reading_input)
{
	CompileSettings settings(Target("linux-x86_64"));
	settings.emits[EmitKind::Ast] = EmitTarget{.path = "/this/directory/does/not/exist/out.ast", .to_stdout = false};
	try
	{
		compile("/this/file/does/not/exist/either.cdr", settings);
		ASSERT_TRUE(false);
	}
	catch (const CompilerError &err)
	{
		ASSERT_EQ(int(err.exit_code()), int(ExitCode::FileIoError));
		ASSERT_TRUE(std::string(err.what()).find("for writing") != std::string::npos);
	}
}

TEST(version_string)
{
	ASSERT_EQ(cider_version, std::format("{}.{}.{}", cider_version_major, cider_version_minor, cider_version_patch));
}
