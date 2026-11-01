#include "unit_testing.hpp"
#include "backend/Target.hpp"

#include "backend/codegen/CodeGenerator.hpp"
#include "backend/objwriter/ObjectWriter.hpp"
#include "utils/error.hpp"

using Arch = Target::Arch;
using ABI = Target::ABI;
using Format = Target::ObjectFormat;
using Ext = Target::RiscvExt;

TEST(supported_targets_table)
{
	struct Case
	{
		std::string name;
		Arch arch;
		uint32_t features;
		ABI abi;
		Format format;
		unsigned int width;
	};
	const std::vector<Case> cases = {
		{"linux-riscv32g", Arch::RISCV32, Ext::G, ABI::RISCV_ILP32D, Format::ELF32, 32},
		{"linux-riscv32gc", Arch::RISCV32, Ext::G | Ext::C, ABI::RISCV_ILP32D, Format::ELF32, 32},
		{"linux-riscv64g", Arch::RISCV64, Ext::G, ABI::RISCV_LP64D, Format::ELF64, 64},
		{"linux-riscv64gc", Arch::RISCV64, Ext::G | Ext::C, ABI::RISCV_LP64D, Format::ELF64, 64},
		{"linux-x86", Arch::X86, 0, ABI::SYSV_I386, Format::ELF32, 32},
		{"linux-x86_64", Arch::X86_64, 0, ABI::SYSV_AMD64, Format::ELF64, 64},
	};

	ASSERT_EQ(Target::supported_targets.size(), cases.size());
	for (const Case &c : cases)
	{
		Target target(c.name);
		ASSERT_TRUE(target.arch == c.arch);
		ASSERT_EQ(target.features, c.features);
		ASSERT_TRUE(target.abi == c.abi);
		ASSERT_TRUE(target.os == Target::OS::Linux);
		ASSERT_TRUE(target.format == c.format);
		ASSERT_EQ(target.register_width(), c.width);
	}
}

TEST(unknown_target)
{
	for (const std::string &name : {"", "linux", "riscv64g", "linux-arm", "linux-RISCV64G", "macos-x86_64"})
	{
		try
		{
			Target target(name);
			ASSERT_TRUE(false);
		}
		catch (const CompilerError &err)
		{
			ASSERT_EQ(int(err.exit_code()), int(ExitCode::Unsupported));
		}
	}
}

TEST(unknown_target_lists_what_exists)
{
	try
	{
		Target target("nope");
		ASSERT_TRUE(false);
	}
	catch (const CompilerError &err)
	{
		std::string message = err.what();
		for (const auto &[name, target] : Target::supported_targets)
			ASSERT_TRUE(message.find(name) != std::string::npos);
	}
}

TEST(has_checks_every_flag_asked_for)
{
	Target g("linux-riscv64g");
	ASSERT_TRUE(g.has(Ext::M));
	ASSERT_TRUE(g.has(Ext::A));
	ASSERT_TRUE(g.has(Ext::F));
	ASSERT_TRUE(g.has(Ext::D));
	ASSERT_TRUE(g.has(Ext::G));
	ASSERT_TRUE(!g.has(Ext::C));
	ASSERT_TRUE(!g.has(Ext::M | Ext::C));

	Target gc("linux-riscv64gc");
	ASSERT_TRUE(gc.has(Ext::G | Ext::C));
}

TEST(no_flags_are_always_had)
{
	ASSERT_TRUE(Target("linux-x86").has(0));
}

TEST(g_is_imafd)
{
	ASSERT_EQ(Ext::G, Ext::M | Ext::A | Ext::F | Ext::D);
}

TEST(float_precision_of_every_target)
{
	for (const auto &[name, target] : Target::supported_targets)
		ASSERT_EQ(target.abi_float_precision(), 2);
}

TEST(every_target_has_a_backend)
{
	for (const auto &[name, target] : Target::supported_targets)
	{
		ASSERT_TRUE(target.get_code_generator() != nullptr);
		ASSERT_TRUE(target.get_object_writer() != nullptr);
	}
}
