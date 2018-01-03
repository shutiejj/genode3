/*
 * \brief  Report-ROM slave
 * \author Norman Feske
 * \date   2014-02-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__REPORT_ROM_SLAVE_H_
#define _INCLUDE__GEMS__REPORT_ROM_SLAVE_H_

/* Genode includes */
#include <base/lock.h>
#include <os/static_parent_services.h>
#include <os/slave.h>
#include <report_session/connection.h>
#include <rom_session/connection.h>


class Report_rom_slave : public Genode::Noncopyable
{
	private:

		class Policy
		:
			private Genode::Static_parent_services<Genode::Rom_session,
			                                       Genode::Cpu_session,
			                                       Genode::Pd_session,
			                                       Genode::Ram_session,
			                                       Genode::Log_session>,
			public Genode::Slave::Policy
		{
			private:

				Genode::Root_capability _report_root_cap;
				Genode::Root_capability _rom_root_cap;
				bool                    _announced;

			protected:

				static Name              _name()  { return "report_rom"; }
				static Genode::Ram_quota _quota() { return { 1024*1024 }; }
				static Genode::Cap_quota _caps()  { return { 75 }; }

			public:

				Policy(Genode::Rpc_entrypoint        &ep,
				       Genode::Region_map            &rm,
				       Genode::Pd_session            &ref_pd,
				       Genode::Pd_session_capability  ref_pd_cap,
				       Genode::Ram_session           &ref_ram,
				       Genode::Ram_session_capability ref_ram_cap,
				       const char                    *config)
				:
					Genode::Slave::Policy(_name(), _name(), *this, ep, rm,
					                      ref_pd, ref_pd_cap, _caps(), _quota())
				{
					if (config)
						configure(config);
				}
		};

		Genode::size_t   const _ep_stack_size = 4*1024*sizeof(Genode::addr_t);
		Genode::Rpc_entrypoint _ep;
		Policy                 _policy;
		Genode::Child          _child;

	public:

		/**
		 * Constructor
		 *
		 * \param ep   entrypoint used for child thread
		 * \param ram  RAM session used to allocate the configuration
		 *             dataspace
		 */
		Report_rom_slave(Genode::Region_map             &rm,
		                 Genode::Pd_session             &pd,
		                 Genode::Pd_session_capability   pd_cap,
		                 Genode::Ram_session            &ram,
		                 Genode::Ram_session_capability  ram_cap,
		                 char                     const *config)
		:
			_ep(&pd, _ep_stack_size, "report_rom"),
			_policy(_ep, rm, pd, pd_cap, ram, ram_cap, config),
			_child(rm, _ep, _policy)
		{ }

		Genode::Slave::Policy &policy() { return _policy; }
};

#endif /* _INCLUDE__GEMS__REPORT_ROM_SLAVE_H_ */
