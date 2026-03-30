#include "ObjectWriter_ELF32.hpp"

#include <stdint.h>
#include <vector>

#include "elf.h"

#include "utils/logging.hpp"

namespace objwriter
{
	/// Not defined in elf.h or the ELF spec im reading
	const Elf32_Half EM_RISCV = 0xf3;

	struct Segment
	{
		/// raw data
		std::vector<uint8_t> data;
		/// Virtual address start
		Elf32_Addr vaddr_start;

		bool read;
		bool write;
		bool execute;
	};

	void ObjectWriter_ELF32::emit(Object &obj, std::ostream &out)
	{
		// finding main
		Elf32_Addr entry_point;
		for (auto &[fn_name, text_offset] : obj.functions)
		{
		}

		// defining segments
		std::vector<Segment> segments;
		segments.emplace_back<Segment>({
			.data = obj.code,
			.vaddr_start = 0x10000,
			.read = true,
			.write = false,
			.execute = true,
		});

		log_vvv("Writing elf header");
		// ELF header
		Elf32_Ehdr ehdr = {};
		ehdr.e_ident[EI_MAG0] = ELFMAG0;
		ehdr.e_ident[EI_MAG1] = ELFMAG1;
		ehdr.e_ident[EI_MAG2] = ELFMAG2;
		ehdr.e_ident[EI_MAG3] = ELFMAG3;
		ehdr.e_ident[EI_CLASS] = ELFCLASS32;
		ehdr.e_ident[EI_DATA] = ELFDATA2LSB; // little endian
		ehdr.e_ident[EI_VERSION] = EV_CURRENT;
		ehdr.e_ident[EI_OSABI] = ELFOSABI_SYSV;
		ehdr.e_type = ET_EXEC; // TODO generalize
		ehdr.e_machine = EM_RISCV;
		ehdr.e_version = EV_CURRENT;
		ehdr.e_entry = entry_point;
		ehdr.e_phoff = sizeof(Elf32_Ehdr); // program headers follow immediately
		ehdr.e_shoff = 0;				   // no section headers for now
		ehdr.e_ehsize = sizeof(Elf32_Ehdr);
		ehdr.e_phentsize = sizeof(Elf32_Phdr);
		ehdr.e_phnum = segments.size();
		ehdr.e_shentsize = 0;
		ehdr.e_shnum = 0;
		ehdr.e_shstrndx = SHN_UNDEF;

		out.write(reinterpret_cast<const char *>(&ehdr), sizeof(ehdr));

		// program headers
		log_vvv("Writing program headers");
		Elf32_Off current_offset = sizeof(ehdr) + (sizeof(Elf32_Phdr) * segments.size());
		for (Segment &seg : segments)
		{
			Elf32_Phdr phdr = {};
			phdr.p_type = PT_LOAD;
			phdr.p_offset = current_offset;
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
			phdr.p_align = 0x1000; // 4kb alignment

			out.write(reinterpret_cast<const char *>(&phdr), sizeof(phdr));

			current_offset += seg.data.size();
		}

		out.flush();
	}

}