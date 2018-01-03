/*
 * \brief  Time source that handles timeouts via a signal handler
 * \author Martin Stein
 * \date   2016-11-04
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SIGNALLED_TIME_SOURCE_H_
#define _SIGNALLED_TIME_SOURCE_H_

/* Genode includes */
#include <base/env.h>
#include <timer/timeout.h>
#include <base/rpc_client.h>

namespace Genode { class Signalled_time_source; }


class Genode::Signalled_time_source : public Time_source
{
	protected:

		using Signal_handler = Genode::Signal_handler<Signalled_time_source>;

		Signal_handler   _signal_handler;
		Timeout_handler *_handler = nullptr;

		void _handle_timeout()
		{
			if (_handler) {
				_handler->handle_timeout(curr_time()); }
		}

	public:

		Signalled_time_source(Env &env)
		:
			_signal_handler(env.ep(), *this,
			                &Signalled_time_source::_handle_timeout)
		{ }
};

#endif /* _SIGNALLED_TIME_SOURCE_H_ */
