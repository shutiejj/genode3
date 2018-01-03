/*
 * \brief  Multiplexing one time source amongst different timeout subjects
 * \author Martin Stein
 * \date   2016-11-04
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <timer/timeout.h>

using namespace Genode;


/*************
 ** Timeout **
 *************/

void Timeout::schedule_periodic(Microseconds duration, Handler &handler)
{
	_alarm.handler = &handler;
	_alarm.periodic = true;
	_alarm.timeout_scheduler._schedule_periodic(*this, duration);
}


void Timeout::schedule_one_shot(Microseconds duration, Handler &handler)
{
	_alarm.handler = &handler;
	_alarm.periodic = false;
	_alarm.timeout_scheduler._schedule_one_shot(*this, duration);
}


void Timeout::discard()
{
	_alarm.timeout_scheduler._discard(*this);
	_alarm.handler = nullptr;
}


/********************
 ** Timeout::Alarm **
 ********************/

bool Timeout::Alarm::on_alarm(unsigned)
{
	if (handler) {
		Handler *current = handler;
		if (!periodic) {
			handler = nullptr;
		}
		current->handle_timeout(timeout_scheduler.curr_time());
	}
	return periodic;
}


/*****************************
 ** Alarm_timeout_scheduler **
 *****************************/

void Alarm_timeout_scheduler::handle_timeout(Duration)
{
	unsigned long const curr_time_us =
		_time_source.curr_time().trunc_to_plain_us().value;

	_alarm_scheduler.handle(curr_time_us);

	/* sleep time is either until the next deadline or the maximum timout */
	unsigned long sleep_time_us;
	Alarm::Time deadline_us;
	if (_alarm_scheduler.next_deadline(&deadline_us)) {
		sleep_time_us = deadline_us - curr_time_us;
	} else {
		sleep_time_us = _time_source.max_timeout().value; }

	/* limit max timeout to a more reasonable value, e.g. 60s */
	if (sleep_time_us > 60000000) {
		sleep_time_us = 60000000;
	} else if (sleep_time_us == 0) {
		sleep_time_us = 1; }

	_time_source.schedule_timeout(Microseconds(sleep_time_us), *this);
}


Alarm_timeout_scheduler::Alarm_timeout_scheduler(Time_source  &time_source,
                                                 Microseconds  min_handle_period)
:
	_time_source(time_source), _alarm_scheduler(min_handle_period.value)
{ }


void Alarm_timeout_scheduler::_enable()
{
	_time_source.schedule_timeout(Microseconds(0), *this);
}


void Alarm_timeout_scheduler::_schedule_one_shot(Timeout      &timeout,
                                                 Microseconds  duration)
{
	unsigned long const curr_time_us =
		_time_source.curr_time().trunc_to_plain_us().value;

	/* ensure that the schedulers time is up-to-date before adding a timeout */
	_alarm_scheduler.handle(curr_time_us);
	_alarm_scheduler.schedule_absolute(&timeout._alarm,
	                                   curr_time_us + duration.value);

	if (_alarm_scheduler.head_timeout(&timeout._alarm)) {
		_time_source.schedule_timeout(Microseconds(0), *this); }
}


void Alarm_timeout_scheduler::_schedule_periodic(Timeout      &timeout,
                                                 Microseconds  duration)
{
	/* ensure that the schedulers time is up-to-date before adding a timeout */
	_alarm_scheduler.handle(_time_source.curr_time().trunc_to_plain_us().value);
	_alarm_scheduler.schedule(&timeout._alarm, duration.value);

	if (_alarm_scheduler.head_timeout(&timeout._alarm)) {
		_time_source.schedule_timeout(Microseconds(0), *this); }
}
