/*
 * \brief  IPv4 peer configuration
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* local includes */
#include <ipv4_config.h>

using namespace Genode;
using namespace Net;

Ipv4_config::Ipv4_config(Ipv4_address_prefix interface,
                         Ipv4_address        gateway)
:
	interface(interface), gateway(gateway)
{
	if (!valid && (interface_valid || gateway_valid)) {
		error("Bad IP configuration");
	}
}
