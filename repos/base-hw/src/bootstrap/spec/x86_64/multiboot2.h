/*
 * \brief  Multiboot 2 handling
 * \author Alexander Boettcher
 * \date   2017-08-11
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__X86_64__MULTIBOOT2_H_
#define _SRC__BOOTSTRAP__SPEC__X86_64__MULTIBOOT2_H_

/* base includes */
#include <util/mmio.h>

namespace Genode { class Multiboot2_info; }

class Genode::Multiboot2_info : Mmio
{
	private:

		struct Size : Register <0x0, 32> { };

		struct Tag : Genode::Mmio {
			enum { LOG2_SIZE = 3 };

			struct Type : Register <0x00, 32>
			{
				enum { END = 0, MEMORY = 6, ACPI_RSDP = 15 };
			};
			struct Size : Register <0x04, 32> { };

			Tag(addr_t addr) : Mmio(addr) { }
		};

	public:

		enum { MAGIC = 0x36d76289UL };

		struct Memory : Genode::Mmio {
			enum { SIZE = 3 * 8 };

			struct Addr : Register <0x00, 64> { };
			struct Size : Register <0x08, 64> { };
			struct Type : Register <0x10, 32> { enum { MEMORY = 1 }; };

			Memory(addr_t mmap = 0) : Mmio(mmap) { }
		};

		Multiboot2_info(addr_t mbi) : Mmio(mbi) { }

        template <typename FUNC_MEM, typename FUNC_ACPI>
		void for_each_tag(FUNC_MEM mem_fn, FUNC_ACPI acpi_fn)
		{
			addr_t const size = read<Multiboot2_info::Size>();

			for (addr_t tag_addr = base() + (1UL << Tag::LOG2_SIZE);
			     tag_addr < base() + size;)
			{
				Tag tag(tag_addr);

				if (tag.read<Tag::Type>() == Tag::Type::END)
					return;

				if (tag.read<Tag::Type>() == Tag::Type::MEMORY) {
					addr_t mem_start = tag_addr + (1UL << Tag::LOG2_SIZE) + 8;
					addr_t const mem_end = tag_addr + tag.read<Tag::Size>();

					for (; mem_start < mem_end; mem_start += Memory::SIZE) {
						Memory mem(mem_start);
						mem_fn(mem);
					}
				}

				if (tag.read<Tag::Type>() == Tag::Type::ACPI_RSDP) {
					size_t const sizeof_tag = 1UL << Tag::LOG2_SIZE;
					addr_t const rsdp_addr  = tag_addr + sizeof_tag;

					Hw::Acpi_rsdp * rsdp = reinterpret_cast<Hw::Acpi_rsdp *>(rsdp_addr);
					if (rsdp->valid() &&
					    sizeof(*rsdp) >= tag.read<Tag::Size>() - sizeof_tag)
						acpi_fn(*rsdp);
				}

				tag_addr += align_addr(tag.read<Tag::Size>(), Tag::LOG2_SIZE);
			}
		}
};

#endif /* _SRC__BOOTSTRAP__SPEC__X86_64__MULTIBOOT2_H_ */
