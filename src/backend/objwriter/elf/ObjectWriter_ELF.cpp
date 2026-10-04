#include "ObjectWriter_ELF.hpp"

#include <stdint.h>
#include <vector>
#include <cstring>

#include "elf.h"

#include "utils/logging.hpp"
#include "utils/error.hpp"

namespace objwriter
{
	/// RISC-V architechture identifier for elf header (not defined in elf.h or the ELF spec im reading...)
	const Elf32_Half EM_RISCV = 0xf3;

	// elf header flags for risc v

	/// Set if the object contains compressed (C extension) instructions
	const Elf32_Word EF_RISCV_RVC = 0x0001;

	// elf header flags for risc v float type

	const Elf32_Word EF_RISCV_FLOAT_ABI_SOFT = 0x0000;
	const Elf32_Word EF_RISCV_FLOAT_ABI_SINGLE = 0x0002;
	const Elf32_Word EF_RISCV_FLOAT_ABI_DOUBLE = 0x0004;
	const Elf32_Word EF_RISCV_FLOAT_ABI_QUAD = 0x0006;

	struct Elf32
	{
		using Ehdr = Elf32_Ehdr;
		using Phdr = Elf32_Phdr;
		using Addr = Elf32_Addr;
		using Off = Elf32_Off;
		static constexpr unsigned char ident_class = ELFCLASS32;
	};

	struct Elf64
	{
		using Ehdr = Elf64_Ehdr;
		using Phdr = Elf64_Phdr;
		using Addr = Elf64_Addr;
		using Off = Elf64_Off;
		static constexpr unsigned char ident_class = ELFCLASS64;
	};

	template <typename ElfClass>
	struct Segment
	{
		/// raw data
		std::vector<uint8_t> data;
		/// Virtual address start
		typename ElfClass::Addr vaddr_start;

		/// Calculated position in final file
		size_t position_in_file = 0;

		bool read;
		bool write;
		bool execute;
	};

	/// @brief Architecture identifier for the elf header
	static Elf32_Half elf_machine(const Target &target)
	{
		switch (target.arch)
		{
		case Target::Arch::RISCV32:
		case Target::Arch::RISCV64:
			return EM_RISCV;
		}
		throw CompilerError::internal("Uncaught architecture variant");
	}

	/// @brief Processor-specific flags for the elf header
	static Elf32_Word elf_flags(const Target &target)
	{
		Elf32_Word flags = 0;
		switch (target.arch)
		{
		case Target::Arch::RISCV32:
		case Target::Arch::RISCV64:
			if (target.has(Target::RiscvExt::C))
				flags |= EF_RISCV_RVC;

			if (target.abi == Target::ABI::ILP32 || target.abi == Target::ABI::LP64)
				flags |= EF_RISCV_FLOAT_ABI_SOFT;
			else if (target.abi == Target::ABI::ILP32F || target.abi == Target::ABI::LP64F)
				flags |= EF_RISCV_FLOAT_ABI_SINGLE;
			else if (target.abi == Target::ABI::ILP32D || target.abi == Target::ABI::LP64D)
				flags |= EF_RISCV_FLOAT_ABI_DOUBLE;
			break;
		}
		return flags;
	}

	template <typename ElfClass>
	void ObjectWriter_ELF<ElfClass>::emit(const Object &obj, const Target &target, std::ostream &out)
	{
		using Ehdr = typename ElfClass::Ehdr;
		using Phdr = typename ElfClass::Phdr;
		using Off = typename ElfClass::Off;

		const Off alignment = 0x1000u;

		// defining segments ---
		std::vector<Segment<ElfClass>> segments;
		// code segment
		segments.push_back(Segment<ElfClass>{
			.data = obj.code,
			.vaddr_start = 0x10000,
			.read = true,
			.write = false,
			.execute = true,
		});

		log_vvv("Writing elf header");
		// ELF header
		Ehdr ehdr = {};
		memset(ehdr.e_ident, 0, EI_NIDENT); // zeroing ident
		ehdr.e_ident[EI_MAG0] = ELFMAG0;
		ehdr.e_ident[EI_MAG1] = ELFMAG1;
		ehdr.e_ident[EI_MAG2] = ELFMAG2;
		ehdr.e_ident[EI_MAG3] = ELFMAG3;
		ehdr.e_ident[EI_CLASS] = ElfClass::ident_class;
		ehdr.e_ident[EI_DATA] = ELFDATA2LSB; // little endian
		ehdr.e_ident[EI_VERSION] = EV_CURRENT;
		ehdr.e_ident[EI_OSABI] = ELFOSABI_SYSV;
		ehdr.e_type = ET_EXEC; // TODO generalize
		ehdr.e_machine = elf_machine(target);
		ehdr.e_version = EV_CURRENT;
		ehdr.e_entry = 0x10000u;	// enter at start of code section
		ehdr.e_phoff = sizeof(Ehdr); // program headers follow immediately
		ehdr.e_shoff = 0;			// no section headers for now
		ehdr.e_ehsize = sizeof(Ehdr);
		ehdr.e_phentsize = sizeof(Phdr);
		ehdr.e_phnum = segments.size();
		ehdr.e_shentsize = 0;
		ehdr.e_shnum = 0;
		ehdr.e_shstrndx = SHN_UNDEF;
		ehdr.e_flags = elf_flags(target);

		out.write(reinterpret_cast<const char *>(&ehdr), sizeof(ehdr));
		size_t current_file_size = sizeof(ehdr);

		// program headers
		log_vvv("Writing {} program headers", segments.size());

		Off final_file_size = sizeof(ehdr) + (sizeof(Phdr) * segments.size());
		for (Segment<ElfClass> &seg : segments)
		{
			// aligned file offset for this segment to be placed at
			Off file_offset = (final_file_size + (alignment - 1)) & ~(alignment - 1);
			seg.position_in_file = size_t(file_offset);

			Phdr phdr = {};
			phdr.p_type = PT_LOAD;
			phdr.p_offset = file_offset;
			phdr.p_vaddr = seg.vaddr_start;
			phdr.p_paddr = seg.vaddr_start; // possibly unneeded?
			phdr.p_filesz = seg.data.size();
			phdr.p_memsz = seg.data.size();
			phdr.p_flags = 0;
			if (seg.read)
				phdr.p_flags |= PF_R;
			if (seg.write)
				phdr.p_flags |= PF_W;
			if (seg.execute)
				phdr.p_flags |= PF_X;
			phdr.p_align = alignment; // 4kb alignment

			out.write(reinterpret_cast<const char *>(&phdr), sizeof(phdr));
			current_file_size += sizeof(phdr);
			final_file_size = size_t(file_offset) + seg.data.size();
		}

		// writing segments
		log_vvv("Writing data for {} segments", segments.size());
		for (Segment<ElfClass> &seg : segments)
		{
			// writing padding to align segment
			size_t padding_size = seg.position_in_file - current_file_size;
			std::vector<uint8_t> padding(padding_size, 0);
			out.write(reinterpret_cast<const char *>(padding.data()), padding_size);

			// writing segment bytes
			out.write(reinterpret_cast<const char *>(seg.data.data()), seg.data.size());
			current_file_size = seg.position_in_file + seg.data.size();
		}
		out.flush();
	}

	// the only two instantiations, so the template's definition can stay in this file
	template class ObjectWriter_ELF<Elf32>;
	template class ObjectWriter_ELF<Elf64>;
}
