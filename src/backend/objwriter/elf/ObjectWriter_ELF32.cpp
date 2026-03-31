#include "ObjectWriter_ELF32.hpp"

#include <stdint.h>
#include <vector>
#include <cstring>

#include "elf.h"

#include "utils/logging.hpp"

namespace objwriter
{
	/// RISC-V architechture identifier for elf header (not defined in elf.h or the ELF spec im reading...)
	const Elf32_Half EM_RISCV = 0xf3;

	// elf header flags for risc v float type

	const Elf32_Word EF_RISCV_FLOAT_ABI_SOFT = 0x0000;
	const Elf32_Word EF_RISCV_FLOAT_ABI_SINGLE = 0x0002;
	const Elf32_Word EF_RISCV_FLOAT_ABI_DOUBLE = 0x0004;
	const Elf32_Word EF_RISCV_FLOAT_ABI_QUAD = 0x0006;

	struct Segment
	{
		/// raw data
		std::vector<uint8_t> data;
		/// Virtual address start
		Elf32_Addr vaddr_start;

		/// Calculated position in final file
		size_t position_in_file = 0;

		bool read;
		bool write;
		bool execute;
	};

	void ObjectWriter_ELF32::emit(const Object &obj, const Target &target, std::ostream &out)
	{
		const Elf32_Word alignment = 0x1000u;

		// defining segments ---
		std::vector<Segment> segments;
		// code segment
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
		memset(ehdr.e_ident, 0, EI_NIDENT); // zeroing ident
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
		ehdr.e_entry = 0x10000u;		   // enter at start of code section
		ehdr.e_phoff = sizeof(Elf32_Ehdr); // program headers follow immediately
		ehdr.e_shoff = 0;				   // no section headers for now
		ehdr.e_ehsize = sizeof(Elf32_Ehdr);
		ehdr.e_phentsize = sizeof(Elf32_Phdr);
		ehdr.e_phnum = segments.size();
		ehdr.e_shentsize = 0;
		ehdr.e_shnum = 0;
		ehdr.e_shstrndx = SHN_UNDEF;
		ehdr.e_flags = 0;

		if (target.abi == Target::ABI::ILP32)
			ehdr.e_flags |= EF_RISCV_FLOAT_ABI_SOFT;
		else if (target.abi == Target::ABI::ILP32F)
			ehdr.e_flags |= EF_RISCV_FLOAT_ABI_SINGLE;
		else if (target.abi == Target::ABI::ILP32D)
			ehdr.e_flags |= EF_RISCV_FLOAT_ABI_DOUBLE;

		out.write(reinterpret_cast<const char *>(&ehdr), sizeof(ehdr));
		size_t current_file_size = sizeof(ehdr);

		// program headers
		log_vvv("Writing {} program headers", segments.size());

		Elf32_Off final_file_size = sizeof(ehdr) + (sizeof(Elf32_Phdr) * segments.size());
		for (Segment &seg : segments)
		{
			// aligned file offset for this segment to be placed at
			Elf32_Off file_offset = (final_file_size + (alignment - 1)) & ~(alignment - 1);
			seg.position_in_file = size_t(file_offset);

			Elf32_Phdr phdr = {};
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
		for (Segment &seg : segments)
		{
			// writing padding to align segment
			size_t padding_size = seg.position_in_file - current_file_size;
			std::vector<uint8_t> padding(padding_size, 0);
			out.write(reinterpret_cast<const char *>(padding.data()), padding_size);

			// writing segment bytes
			out.write(reinterpret_cast<const char *>(seg.data.data()), seg.data.size());
		}
		out.flush();
	}
}