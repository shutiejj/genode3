/*
 * \brief  I/O memory backend for ACPICA library and lookup code
 *         for initial ACPI RSDP pointer
 * \author Alexander Boettcher
 * \date   2016-11-14
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_io_mem_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <util/misc_math.h>

#include <io_mem_session/connection.h>
#include <rm_session/connection.h>

#include "env.h"

extern "C" {
#include "acpi.h"
#include "acpiosxf.h"
}

#define FAIL(retval) \
	{ \
		Genode::error(__func__, ":", __LINE__, " called - dead"); \
		Genode::Lock lock; \
		while (1) lock.lock(); \
		return retval; \
	}

namespace Acpica {
	class Io_mem;
	class Rsdp;
};


class Acpica::Rsdp
{
	private:

		/* BIOS range to scan for RSDP */
		enum { BIOS_BASE = 0xe0000, BIOS_SIZE = 0x20000 };

		/**
		 * Search for RSDP pointer signature in area
		 */
		Genode::uint8_t *_search_rsdp(Genode::uint8_t *area,
		                              Genode::size_t const area_size)
		{
			for (Genode::addr_t addr = 0; area && addr < area_size; addr += 16)
				/* XXX checksum table */
				if (!Genode::memcmp(area + addr, "RSD PTR ", 8))
					return area + addr;

			return nullptr;
		}

		/**
		 * Return 'Root System Descriptor Pointer' (ACPI spec 5.2.5.1)
		 */
		Genode::uint64_t _rsdp(Genode::Env &env)
		{
			Genode::uint8_t * local = 0;

			/* try BIOS area */
			{
				Genode::Attached_io_mem_dataspace io_mem(env, BIOS_BASE, BIOS_SIZE);
				local = _search_rsdp(io_mem.local_addr<Genode::uint8_t>(), BIOS_SIZE);
				if (local)
					return BIOS_BASE + (local - io_mem.local_addr<Genode::uint8_t>());
			}

			/* search EBDA (BIOS addr + 0x40e) */
			try {
				unsigned short base = 0;
				{
					Genode::Attached_io_mem_dataspace io_mem(env, 0, 0x1000);
					local = io_mem.local_addr<Genode::uint8_t>();
					if (local)
						base = (*reinterpret_cast<unsigned short *>(local + 0x40e)) << 4;
				}

				if (!base)
					return 0;

				Genode::Attached_io_mem_dataspace io_mem(env, base, 1024);
				local = _search_rsdp(io_mem.local_addr<Genode::uint8_t>(), 1024);

				if (local)
					return base + (local - io_mem.local_addr<Genode::uint8_t>());
			} catch (...) {
				Genode::warning("failed to scan EBDA for RSDP root");
			}

			return 0;
		}

	public:

		Rsdp() { }

		Genode::addr_t phys_rsdp(Genode::Env &env)
		{
			Genode::uint64_t phys_rsdp = _rsdp(env);
			return phys_rsdp;
		}
};


class Acpica::Io_mem
{
	private:

		ACPI_PHYSICAL_ADDRESS      _phys   = 0;
		ACPI_SIZE                  _size   = 0;
		Genode::uint8_t           *_virt   = nullptr;
		Genode::Io_mem_connection *_io_mem = nullptr;
		unsigned                   _ref    = 0;

		static Acpica::Io_mem _ios[32];

	public:

		bool unused() const { return _phys == 0 && _size == 0 && _io_mem == nullptr; }
		bool stale() const { return !unused() && _io_mem == nullptr; }
		bool contains_virt (const Genode::uint8_t * v, const ACPI_SIZE s) const
		{
			return _virt <= v && v + s <= _virt + _size;
		}
		bool contains_phys (const ACPI_PHYSICAL_ADDRESS p, const ACPI_SIZE s) const
		{
			return _phys <= p && p + s <= _phys + _size;
		}

		Genode::addr_t to_virt(ACPI_PHYSICAL_ADDRESS p) {
			_ref ++;
			return reinterpret_cast<Genode::addr_t>(_virt + (p - _phys));
		}

		bool ref_dec() { return --_ref; }

		template <typename FUNC>
		static void apply_to_all(FUNC const &func = [] () { } )
		{
			for (unsigned i = 0; i < sizeof(_ios) / sizeof(_ios[0]); i++)
				func(_ios[i]);
		}

		void invalidate()
		{
			if (unused())
				FAIL();

			if (stale()) {
				/**
				 * Look for the larger entry that replaced this one.
				 * Required to decrement ref count.
				 */
				apply_to_all([&] (Acpica::Io_mem &io_mem) {
					if (&io_mem == this)
						return;

					if (!io_mem.contains_phys(_phys, _size))
						return;

					if (io_mem.ref_dec())
						return;

					io_mem._ref++;
					io_mem.invalidate();
				});
			}

			if (ref_dec())
				return;

			if (!stale())
				Genode::destroy(Acpica::heap(), _io_mem);

			_phys   = _size = 0;
			_virt   = nullptr;
			_io_mem = nullptr;
		}

		template <typename FUNC>
		static Genode::addr_t apply_u(FUNC const &func = [] () { } )
		{
			for (unsigned i = 0; i < sizeof(_ios) / sizeof(_ios[0]); i++)
			{
				Genode::addr_t r = func(_ios[i]);
				if (r) return r;
			}
			return 0UL;
		}

		template <typename FUNC>
		static Acpica::Io_mem * apply_p(FUNC const &func = [] () { } )
		{
			for (unsigned i = 0; i < sizeof(_ios) / sizeof(_ios[0]); i++)
			{
				Acpica::Io_mem * r = func(_ios[i]);
				if (r) return r;
			}
			return nullptr;
		}

		static Acpica::Io_mem * allocate(ACPI_PHYSICAL_ADDRESS p, ACPI_SIZE s,
		                                 unsigned r)
		{
			return Acpica::Io_mem::apply_p([&] (Acpica::Io_mem &io_mem) {
				if (!io_mem.unused())
					return reinterpret_cast<Acpica::Io_mem *>(0);

				io_mem._phys = p & ~0xFFFUL;
				io_mem._size = Genode::align_addr(p + s - io_mem._phys, 12);
				io_mem._ref  = r;
				io_mem._virt = 0;

				io_mem._io_mem = new (Acpica::heap())
					Genode::Io_mem_connection(Acpica::env(), io_mem._phys,
					                          io_mem._size);

				return &io_mem;
			});
		}

		static Genode::addr_t insert(ACPI_PHYSICAL_ADDRESS p, ACPI_SIZE s)
		{
			Acpica::Io_mem * io_mem = allocate(p, s, 1);
			if (!io_mem)
				return 0UL;

			io_mem->_virt = Acpica::env().rm().attach(io_mem->_io_mem->dataspace(),
			                                          io_mem->_size);

			return reinterpret_cast<Genode::addr_t>(io_mem->_virt);
		}

		Genode::addr_t pre_expand(ACPI_PHYSICAL_ADDRESS p, ACPI_SIZE s)
		{
			if (_io_mem)
				Genode::destroy(Acpica::heap(), _io_mem);

			Genode::addr_t xsize = _phys - p + _size;
			if (!allocate(p, xsize, _ref))
				FAIL(0)

			return _expand(p, s);
		}

		Genode::addr_t post_expand(ACPI_PHYSICAL_ADDRESS p, ACPI_SIZE s)
		{
			if (_io_mem)
				Genode::destroy(Acpica::heap(), _io_mem);

			ACPI_SIZE xsize = p + s - _phys;
			if (!allocate(_phys, xsize, _ref))
				FAIL(0)

			return _expand(p, s);
		}

		Genode::addr_t _expand(ACPI_PHYSICAL_ADDRESS const p, ACPI_SIZE const s)
		{
			/* mark this element as a stale reference */
			_io_mem = nullptr;

			/* find new created entry */
			Genode::addr_t res = Acpica::Io_mem::apply_u([&] (Acpica::Io_mem &io_mem) {
				if (io_mem.unused() || io_mem.stale() ||
				    !io_mem.contains_phys(p, s))
					return 0UL;

				Genode::Io_mem_dataspace_capability const io_ds = io_mem._io_mem->dataspace();

				/* re-attach mem of stale entries partially using this iomem */
				Acpica::Io_mem::apply_to_all([&] (Acpica::Io_mem &io2) {
					if (io2.unused() || !io2.stale() ||
					    !io_mem.contains_phys(io2._phys, 0))
						return;

					Genode::addr_t off_phys = io2._phys - io_mem._phys;

					Genode::addr_t virt = reinterpret_cast<Genode::addr_t>(io2._virt);

					Acpica::env().rm().attach_at(io_ds, virt, io2._size, off_phys);
				});

				if (io_mem._virt)
					FAIL(0UL);

				/* attach whole memory */
				io_mem._virt = Acpica::env().rm().attach(io_ds);
				return io_mem.to_virt(p);
			});

			/* should never happen */
			if (!res)
				FAIL(0)

			return res;
		}
};

Acpica::Io_mem Acpica::Io_mem::_ios[32];

static ACPI_TABLE_RSDP faked_rsdp;
enum { FAKED_PHYS_RSDP_ADDR = 1 };

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer (void)
{
	Genode::Env &env = Acpica::env();

	/* try platform_info ROM provided by core */
	try {
		Genode::Attached_rom_dataspace info(env, "platform_info");
		Genode::Xml_node xml(info.local_addr<char>(), info.size());
		Genode::Xml_node acpi_node = xml.sub_node("acpi");

		faked_rsdp.Revision = acpi_node.attribute_value("revision", 0U);
		faked_rsdp.RsdtPhysicalAddress = acpi_node.attribute_value("rsdt", 0UL);
		faked_rsdp.XsdtPhysicalAddress = acpi_node.attribute_value("xsdt", 0UL);

		if (faked_rsdp.XsdtPhysicalAddress || faked_rsdp.RsdtPhysicalAddress)
			return FAKED_PHYS_RSDP_ADDR;
	} catch (...) { }

	/* legacy way - search and grep for the pointer */
	Acpica::Rsdp acpi_table;
	ACPI_PHYSICAL_ADDRESS rsdp = acpi_table.phys_rsdp(env);
	return rsdp;
}

void * AcpiOsMapMemory (ACPI_PHYSICAL_ADDRESS phys, ACPI_SIZE size)
{
	if (phys == FAKED_PHYS_RSDP_ADDR)
		return &faked_rsdp;

	Genode::addr_t virt = Acpica::Io_mem::apply_u([&] (Acpica::Io_mem &io_mem) {
		if (io_mem.unused() || io_mem.stale())
			return 0UL;

		if (io_mem.contains_phys(phys, size))
			/* we have already a mapping in which the request fits */
			return io_mem.to_virt(phys);

		if (io_mem.contains_phys(phys + 1, 0))
			/* phys is within region but end is outside region */
			return io_mem.post_expand(phys, size);

		if (io_mem.contains_phys(phys + size - 1, 0))
			/* phys starts before region and end is within region */
			return io_mem.pre_expand(phys, size);

		return 0UL;
	});

	if (virt)
		return reinterpret_cast<void *>(virt);

	virt= Acpica::Io_mem::insert(phys, size);
	if (virt)
		return reinterpret_cast<void *>(virt + (phys & 0xfffU));

	FAIL(nullptr)
}

void AcpiOsUnmapMemory (void * ptr, ACPI_SIZE size)
{
	if (ptr == &faked_rsdp)
		return;

	Genode::uint8_t const * virt = reinterpret_cast<Genode::uint8_t *>(ptr);

	if (Acpica::Io_mem::apply_u([&] (Acpica::Io_mem &io_mem) {
		if (io_mem.unused() || !io_mem.contains_virt(virt, size))
			return 0;

		io_mem.invalidate();
		return 1;
	}))
		return;

	FAIL()
}
