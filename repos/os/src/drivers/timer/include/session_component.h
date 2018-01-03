/*
 * \brief  Instance of the timer session interface
 * \author Norman Feske
 * \author Markus Partheymueller
 * \author Martin Stein
 * \date   2006-08-15
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 * Copyright (C) 2012 Intel Corporation
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SESSION_COMPONENT_
#define _SESSION_COMPONENT_

/* Genode includes */
#include <util/list.h>
#include <timer_session/timer_session.h>
#include <base/rpc_server.h>
#include <timer/timeout.h>

namespace Timer {

	using Microseconds = Genode::Microseconds;
	using Duration     = Genode::Duration;
	class Session_component;
}


class Timer::Session_component : public Genode::Rpc_object<Session>,
                                 public Genode::List<Session_component>::Element,
                                 private Genode::Timeout::Handler
{
	private:

		Genode::Timeout                    _timeout;
		Genode::Timeout_scheduler         &_timeout_scheduler;
		Genode::Signal_context_capability  _sigh;

		unsigned long const _init_time_us =
			_timeout_scheduler.curr_time().trunc_to_plain_us().value;

		void handle_timeout(Duration) {
			Genode::Signal_transmitter(_sigh).submit(); }

	public:

		Session_component(Genode::Timeout_scheduler &timeout_scheduler)
		: _timeout(timeout_scheduler), _timeout_scheduler(timeout_scheduler) { }


		/********************
		 ** Timer::Session **
		 ********************/

		void trigger_once(unsigned us) override {
			_timeout.schedule_one_shot(Microseconds(us), *this); }

		void trigger_periodic(unsigned us) override {
			_timeout.schedule_periodic(Microseconds(us), *this); }

		void sigh(Signal_context_capability sigh) override
		{
			_sigh = sigh;
			if (!sigh.valid())
				_timeout.discard();
		}

		unsigned long elapsed_ms() const override {
			return elapsed_us() / 1000; }

		unsigned long elapsed_us() const override {
			return _timeout_scheduler.curr_time().trunc_to_plain_us().value -
			       _init_time_us; }

		void msleep(unsigned) override { /* never called at the server side */ }
		void usleep(unsigned) override { /* never called at the server side */ }
};

#endif /* _SESSION_COMPONENT_ */
