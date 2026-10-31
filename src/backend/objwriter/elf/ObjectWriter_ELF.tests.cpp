#include "unit_testing.hpp"
#include "backend/objwriter/ObjectWriter.hpp"

#include <cstring>
#include <sstream>

#include "backend/objwriter/elf/elf.h"

/// Writes an executable into memory
static std::string write_elf(const std::string &target, std::vector<uint8_t> code)
{
	Object obj;
	obj.code = std::move(code);
	std::ostringstream out(std::ios::binary);
	Target t(target);
	t.get_object_writer()->emit(obj, t, out);
	return out.str();
}

template <typename T>
static T read_struct(const std::string &bytes, size_t offset)
{
	ASSERT_TRUE(offset + sizeof(T) <= bytes.size());
	T value;
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
}

static constexpr Elf32_Half EM_RISCV_VALUE = 0xf3;
static constexpr Elf32_Word RVC_FLAG = 0x0001;
static constexpr Elf32_Word DOUBLE_FLOAT_FLAG = 0x0004;

TEST(identification_bytes)
{
	std::string elf = write_elf("linux-x86_64", {0x90});
	ASSERT_EQ(elf.substr(0, 4), "\x7f" "ELF");
	ASSERT_EQ(int(elf[EI_DATA]), int(ELFDATA2LSB));
	ASSERT_EQ(int(elf[EI_VERSION]), int(EV_CURRENT));
	ASSERT_EQ(int(elf[EI_OSABI]), int(ELFOSABI_SYSV));
}

TEST(elf_class_follows_the_target)
{
	for (const std::string &target : {"linux-x86", "linux-riscv32g", "linux-riscv32gc"})
		ASSERT_EQ(int(write_elf(target, {0})[EI_CLASS]), int(ELFCLASS32));
	for (const std::string &target : {"linux-x86_64", "linux-riscv64g", "linux-riscv64gc"})
		ASSERT_EQ(int(write_elf(target, {0})[EI_CLASS]), int(ELFCLASS64));
}

TEST(machine_follows_the_target)
{
	ASSERT_EQ(read_struct<Elf32_Ehdr>(write_elf("linux-x86", {0}), 0).e_machine, EM_386);
	ASSERT_EQ(read_struct<Elf64_Ehdr>(write_elf("linux-x86_64", {0}), 0).e_machine, EM_X86_64);
	ASSERT_EQ(read_struct<Elf32_Ehdr>(write_elf("linux-riscv32g", {0}), 0).e_machine, EM_RISCV_VALUE);
	ASSERT_EQ(read_struct<Elf64_Ehdr>(write_elf("linux-riscv64g", {0}), 0).e_machine, EM_RISCV_VALUE);
}

TEST(x86_has_no_flags)
{
	ASSERT_EQ(read_struct<Elf32_Ehdr>(write_elf("linux-x86", {0}), 0).e_flags, 0u);
	ASSERT_EQ(read_struct<Elf64_Ehdr>(write_elf("linux-x86_64", {0}), 0).e_flags, 0u);
}

TEST(riscv_flags)
{
	ASSERT_EQ(read_struct<Elf32_Ehdr>(write_elf("linux-riscv32g", {0}), 0).e_flags, DOUBLE_FLOAT_FLAG);
	ASSERT_EQ(read_struct<Elf32_Ehdr>(write_elf("linux-riscv32gc", {0}), 0).e_flags, DOUBLE_FLOAT_FLAG | RVC_FLAG);
	ASSERT_EQ(read_struct<Elf64_Ehdr>(write_elf("linux-riscv64g", {0}), 0).e_flags, DOUBLE_FLOAT_FLAG);
	ASSERT_EQ(read_struct<Elf64_Ehdr>(write_elf("linux-riscv64gc", {0}), 0).e_flags, DOUBLE_FLOAT_FLAG | RVC_FLAG);
}

/// The fields that have the same meaning in the 32 and 64 bit headers
template <typename Ehdr, typename Phdr>
static void check_layout(const std::string &target)
{
	const std::vector<uint8_t> code = {0x11, 0x22, 0x33};
	std::string elf = write_elf(target, code);

	Ehdr ehdr = read_struct<Ehdr>(elf, 0);
	ASSERT_EQ(ehdr.e_type, ET_EXEC);
	ASSERT_EQ(ehdr.e_version, EV_CURRENT);
	ASSERT_EQ(ehdr.e_entry, 0x10000u);
	ASSERT_EQ(ehdr.e_phoff, sizeof(Ehdr));
	ASSERT_EQ(ehdr.e_shoff, 0u);
	ASSERT_EQ(ehdr.e_ehsize, sizeof(Ehdr));
	ASSERT_EQ(ehdr.e_phentsize, sizeof(Phdr));
	ASSERT_EQ(ehdr.e_phnum, 1u);
	ASSERT_EQ(ehdr.e_shnum, 0u);
	ASSERT_EQ(ehdr.e_shstrndx, SHN_UNDEF);

	Phdr phdr = read_struct<Phdr>(elf, sizeof(Ehdr));
	ASSERT_EQ(phdr.p_type, PT_LOAD);
	ASSERT_EQ(phdr.p_offset, 0x1000u);
	ASSERT_EQ(phdr.p_vaddr, 0x10000u);
	ASSERT_EQ(phdr.p_paddr, 0x10000u);
	ASSERT_EQ(phdr.p_filesz, code.size());
	ASSERT_EQ(phdr.p_memsz, code.size());
	ASSERT_EQ(phdr.p_flags, PF_R | PF_X);
	ASSERT_EQ(phdr.p_align, 0x1000u);
}

TEST(layout_32_bit)
{
	check_layout<Elf32_Ehdr, Elf32_Phdr>("linux-x86");
}

TEST(layout_64_bit)
{
	check_layout<Elf64_Ehdr, Elf64_Phdr>("linux-x86_64");
}

TEST(code_follows_the_padding)
{
	std::string elf = write_elf("linux-x86_64", {0x11, 0x22, 0x33});
	ASSERT_EQ(elf.size(), 0x1003u);
	ASSERT_EQ(elf.substr(0x1000), "\x11\x22\x33");
	for (size_t at = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr); at < 0x1000; ++at)
		ASSERT_EQ(int(elf[at]), 0);
}

TEST(empty_object)
{
	std::string elf = write_elf("linux-x86", {});
	ASSERT_EQ(elf.size(), 0x1000u);
	ASSERT_EQ(read_struct<Elf32_Phdr>(elf, sizeof(Elf32_Ehdr)).p_filesz, 0u);
}

TEST(entry_point_is_the_loaded_address_of_the_code)
{
	std::string elf = write_elf("linux-riscv64g", {0});
	Elf64_Ehdr ehdr = read_struct<Elf64_Ehdr>(elf, 0);
	Elf64_Phdr phdr = read_struct<Elf64_Phdr>(elf, ehdr.e_phoff);
	ASSERT_EQ(ehdr.e_entry, phdr.p_vaddr);
}

TEST(segment_offset_and_address_are_congruent)
{
	// the loader refuses a segment whose file offset and address don't line up modulo its alignment
	std::string elf = write_elf("linux-x86_64", {0});
	Elf64_Phdr phdr = read_struct<Elf64_Phdr>(elf, sizeof(Elf64_Ehdr));
	ASSERT_EQ(phdr.p_offset % phdr.p_align, phdr.p_vaddr % phdr.p_align);
}
