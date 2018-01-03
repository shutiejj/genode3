/*
 * \brief  Test CPU-scheduler implementation of the kernel
 * \author Martin Stein
 * \date   2014-09-30
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>

/* core includes */
#include <kernel/cpu_scheduler.h>

/*
 * Utilities
 */

using Genode::size_t;
using Genode::addr_t;
using Kernel::Cpu_share;
using Kernel::Cpu_scheduler;

void * operator new(__SIZE_TYPE__ s, void * p) { return p; }

struct Data
{
	Cpu_share idle;
	Cpu_scheduler scheduler;
	char shares[9][sizeof(Cpu_share)];

	Data() : idle(0, 0), scheduler(&idle, 1000, 100) { }
};

Data * data()
{
	static Data d;
	return &d;
}

void done()
{
	Genode::log("done");
	while (1) ;
}

unsigned share_id(void * const pointer)
{
	addr_t const address = (addr_t)pointer;
	addr_t const base = (addr_t)data()->shares;
	if (address < base || address >= base + sizeof(data()->shares)) {
		return 0; }
	return (address - base) / sizeof(Cpu_share) + 1;
}

Cpu_share * share(unsigned const id)
{
	if (!id) { return &data()->idle; }
	return reinterpret_cast<Cpu_share *>(&data()->shares[id - 1]);
}

void create(unsigned const id)
{
	Cpu_share * const s = share(id);
	void * const p = (void *)s;
	switch (id) {
	case 1: new (p) Cpu_share(2, 230); break;
	case 2: new (p) Cpu_share(0, 170); break;
	case 3: new (p) Cpu_share(3, 110); break;
	case 4: new (p) Cpu_share(1,  90); break;
	case 5: new (p) Cpu_share(3, 120); break;
	case 6: new (p) Cpu_share(3,   0); break;
	case 7: new (p) Cpu_share(2, 180); break;
	case 8: new (p) Cpu_share(2, 100); break;
	case 9: new (p) Cpu_share(2,   0); break;
	default: return;
	}
	data()->scheduler.insert(s);
}

void destroy(unsigned const id)
{
	Cpu_share * const s = share(id);
	data()->scheduler.remove(s);
	s->~Cpu_share();
}

unsigned time()
{
	return data()->scheduler.quota() -
	       data()->scheduler.residual();
}

void update_check(unsigned const l, unsigned const c, unsigned const t,
                  unsigned const s, unsigned const q)
{
	data()->scheduler.update(c);
	unsigned const st = time();
	if (t != st) {
		Genode::log("wrong time ", st, " in line ", l);
		done();
	}
	Cpu_share * const hs = data()->scheduler.head();
	unsigned const hq = data()->scheduler.head_quota();
	if (hs != share(s)) {
		unsigned const hi = share_id(hs);
		Genode::log("wrong share ", hi, " in line ", l);
		done();
	}
	if (hq != q) {
		Genode::log("wrong quota ", hq, " in line ", l);
		done();
	}
}

void ready_check(unsigned const l, unsigned const s, bool const x)
{
	bool const y = data()->scheduler.ready_check(share(s));
	if (y != x) {
		Genode::log("wrong check result ", y, " in line ", l);
		done();
	}
}


/*
 * Shortcuts for all basic operations that the test consists of
 */

#define C(s)       create(s);
#define D(s)       destroy(s);
#define A(s)       data()->scheduler.ready(share(s));
#define I(s)       data()->scheduler.unready(share(s));
#define Y          data()->scheduler.yield();
#define Q(s, q)    data()->scheduler.quota(share(s), q);
#define U(c, t, s, q) update_check(__LINE__, c, t, s, q);
#define O(s)       ready_check(__LINE__, s, true);
#define N(s)       ready_check(__LINE__, s, false);


/**
 * Main routine
 */
void Component::construct(Genode::Env &)
{
	/*
	 * Step-by-step testing
	 *
	 * Every line in this test is structured according to the scheme
	 * '<ops> U(t,c,s,q) <doc>' where the symbols are defined as follows:
	 *
	 * ops  Operations that affect the schedule but not the head of the
	 *      scheduler (which is a buffer to remember the last scheduling
	 *      choice). These operations are:
	 *
	 *      C(s)  construct the context with ID 's' and insert it
	 *      D(s)  remove the context with ID 's' and destruct it
	 *      A(s)  set the context with ID 's' active
	 *      I(s)  set the context with ID 's' inactive
	 *      O(s)  do 'A(s)' and check that this will outdate the head
	 *      N(s)  do 'A(s)' and check that this won't outdate the head
	 *      Y     annotate that the current head wants to yield
	 *
	 * U(c,t,s,q) 
	 *           First update the head and time of the scheduler according to
	 *           the new schedule and the fact that the head consumed a 
	 *           quantum of 'c'. Then, check the consumed time 't' for the
	 *           actual round and if the new head is the context with ID 's'
	 *           that has a quota of 'q'.
	 *
	 * doc  Documents the expected schedule for the point after the head
	 *      update in the corresponding line. First it lists all claims
	 *      via their ID followed by a ' (active) or a ° (inactive) and the
	 *      corresponding quota. So 5°120 is the inactive context 5 that
	 *      has a quota of 120 left for the current round. They are sorted
	 *      decurrent by their priorities and the priority bands are
	 *      separated by dashes. After the lowest priority band there is
	 *      an additional dash followed by the current state of the round-
	 *      robin scheduling that has no quota or priority. Only active
	 *      contexts are listed here and only the head is listed together
	 *      with its remaining quota. So 4'50 1 7 means that the active
	 *      context 4 is the round-robin head with quota 50 remaining and
	 *      that he is followed by the active contexts 1 and 7 both with a
	 *      fresh time-slice.
	 *
	 * The order of operations is the same as in the operative kernel so each
	 * line can be seen as one "kernel pass". If any check in a line fails,
	 * the test prematurely stops and prints out where and why it has stopped.
	 */

	/* first round - idle */
	U( 10,  10, 0, 100) /* - */
	U( 90, 100, 0, 100) /* - */
	U(120, 200, 0, 100) /* - */
	U(130, 300, 0, 100) /* - */
	U(140, 400, 0, 100) /* - */
	U(150, 500, 0, 100) /* - */
	U(160, 600, 0, 100) /* - */
	U(170, 700, 0, 100) /* - */
	U(180, 800, 0, 100) /* - */
	U(190, 900, 0, 100) /* - */
	U(200,   0, 0, 100) /* - */

	/* second round - one claim, one filler */
	C(1) U(111, 100, 0, 100) /* 1°230 - */
	A(1) U(123, 200, 1, 230) /* 1'230 - 1'100 */
	I(1) U(200, 400, 0, 100) /* 1°30 - */
	A(1) U( 10, 410, 1,  30) /* 1'30 - 1'100 */
	     U(100, 440, 1, 100) /* 1'0 - 1'100 */
	     U(200, 540, 1, 100) /* 1'0 - 1'100 */
	I(1) U(200, 640, 0, 100) /* 1°0 - */
	     U(200, 740, 0, 100) /* 1°0 - */
	A(1) U( 10, 750, 1, 100) /* 1'0 - 1'100 */
	     U( 50, 800, 1,  50) /* 1'0 - 1'50 */
	     U( 20, 820, 1,  30) /* 1'0 - 1'30 */
	     U(100, 850, 1, 100) /* 1'0 - 1'100 */
	     U(200, 950, 1,  50) /* 1'0 - 1'100 */
	     U(200,   0, 1, 230) /* 1'230 - 1'100 */

	/* third round - one claim per priority */
	C(2) A(2) U( 50,  50, 1, 180) /* 1'180 - 2'170 - 1'100 2 */
	     I(1) U( 70, 120, 2, 170) /* 1°110 - 2'170 - 2'100 */
	A(1) I(2) U(110, 230, 1, 110) /* 1'110 - 2°60 - 1'100 */
	          U( 90, 320, 1,  20) /* 1'20 - 2°60 - 1'100 */
	A(2) I(1) U( 10, 330, 2,  60) /* 1°10 - 2'60 - 2'100 */
	     C(3) U( 40, 370, 2,  20) /* 3°110 - 1°10 - 2'10 - 2'100 */
	     A(3) U( 10, 380, 3, 110) /* 3'110 - 1°10 - 2'10 - 2'100 3 */
	          U(150, 490, 2,  10) /* 3'0 - 1°10 - 2'10 - 2'100 3 */
	          U( 10, 500, 2, 100) /* 3'0 - 1°10 - 2'0 - 2'100 3 */
	          U( 60, 560, 2,  40) /* 3'0 - 1°10 - 2'0 - 2'40 3 */
	     C(4) U( 60, 600, 3, 100) /* 3'0 - 1°10 - 4°90 - 2'0 - 3'100 2 */
	C(6) A(6) U(120, 700, 2, 100) /* 3'0 - 1°10 - 4°90 - 2'0 - 2'100 6 3*/
	     A(4) U( 80, 780, 4,  90) /* 3'0 - 1°10 - 4'90 - 2'0 - 2'20 6 3 4 */
	I(4) A(1) U( 50, 830, 1,  10) /* 3'0 - 1'10 - 4°40 - 2'0 - 2'20 6 3 1 */
	          U( 50, 840, 2,  20) /* 3'0 - 1'0 - 4°40 - 2'0 - 2'20 6 3 1 */
	          U( 50, 860, 6, 100) /* 3'0 - 1'0 - 4°40 - 2'0 - 6'100 3 1 2 */
	          U(100, 960, 3,  40) /* 3'0 - 1'0 - 4°40 - 2'0 - 3'100 1 2 6 */
	          U( 60,   0, 3, 110) /* 3'110 - 1'230 - 4°40 - 2'170 - 3'60 1 2 6 */

	/* fourth round - multiple claims per priority */
	     C(5) U( 60,  60, 3,  50) /* 3'50 5°120 - 1'230 - 4°90 - 2'170 - 3'60 1 2 6 */
	A(4) I(3) U( 40, 100, 1, 230) /* 3°10 5°120 - 1'230 - 4'90 - 2'170 - 1'100 2 6 4 */
	C(7) A(7) U(200, 300, 7, 180) /* 3°10 5°120 - 7'180 1'30 - 4'90 - 2'170 - 1'100 2 6 4 7 */
	C(8) A(5) U(100, 400, 5, 120) /* 5'120 3°10 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 */
	     A(3) U(100, 500, 3,  10) /* 3'10 5'20 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 3 */
	          U( 30, 510, 5,  20) /* 5'20 3'0 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 3 */
	C(9) A(9) U( 10, 520, 5,  10) /* 5'10 3'0 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 3 9 */
	          U( 50, 530, 7,  80) /* 5'0 3'0 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 3 9 */
	A(8) I(7) U( 10, 540, 8, 100) /* 5'0 3'0 - 8'100 1'30 7°70 - 4'90 - 2'170 - 1'100 2 6 4 5 3 9 8 */
	     I(8) U( 80, 620, 1,  30) /* 5'0 3'0 - 1'30 7°70 8°20 - 4'90 - 2'170 - 1'100 2 6 4 5 3 9 */
	          U(200, 650, 4,  90) /* 5'0 3'0 - 1'0 7°70 8°20 - 4'90 - 2'170 - 1'100 2 6 4 5 3 9 */
	          U(100, 740, 2, 170) /* 5'0 3'0 - 1'0 7°70 8°20 - 4'0 - 2'170 - 1'100 2 6 4 5 3 9 */
	A(8) A(7) U( 10, 750, 7,  70) /* 5'0 3'0 - 7'70 8'20 1'0 - 4'0 - 2'160 - 1'100 2 6 4 5 3 9 8 7 */
	I(7) I(3) U( 10, 760, 8,  20) /* 5'0 3°0 - 8'20 1'0 7°60 - 4'0 - 2'160 - 1'100 2 6 4 5 9 8 */
	     I(8) U( 10, 770, 2, 160) /* 5'0 3°0 - 1'0 7°60 8°10 - 4'0 - 2'160 - 1'100 2 6 4 5 9 */
	     I(2) U( 40, 810, 1, 100) /* 5'0 3°0 - 1'0 7°60 8°10 - 4'0 - 2°120 - 1'100 6 4 5 9 */
	     A(3) U( 30, 840, 1,  70) /* 5'0 3'0 - 1'0 7°60 8°10 - 4'0 - 2°120 - 1'100 6 4 5 9 3 */
	          U( 80, 910, 6,  90) /* 5'0 3'0 - 1'0 7°60 8°10 - 4'0 - 2°120 - 6'100 4 5 9 3 1 */
	A(7) A(8) U( 10, 920, 8,  10) /* 5'0 3'0 - 8'10 7'60 1'0 - 4'0 - 2°120 - 6'90 4 5 9 3 1 7 8 */
	          U( 30, 930, 7,  60) /* 5'0 3'0 - 7'60 1'0 8'0 - 4°0 - 2°120 - 6'90 4 5 9 3 1 7 8 */
	A(2) I(7) U( 10, 940, 2,  60) /* 5'0 3'0 - 1'0 8'0 7°50 - 4'0 - 2'120 - 6'90 4 5 9 3 1 8 2 */
	I(3) I(5) U( 40, 980, 2,  20) /* 5°0 3°0 - 1'0 8'0 7°50 - 4'0 - 2'80 - 6'90 4 9 1 8 2 */
	I(9) I(4) U( 10, 990, 2,  10) /* 5°0 3°0 - 1'0 8'0 7°50 - 4°0 - 2'70 - 6'90 1 8 2 */
	          U( 40,   0, 1, 230) /* 5°120 3°110 - 1'230 8'100 7°180 - 4°90 - 2'170 - 6'90 1 8 2 */

	/* fifth round - yield, ready & check*/
	     I(6) U( 30,  30, 1, 200) /* 5°120 3°110 - 1'200 8'100 7°180 - 4°90 - 2'170 - 1'100 8 2 */
	        Y U( 20,  50, 8, 100) /* 5°120 3°110 - 8'100 1'0 7°180 - 4°90 - 2'170 - 1'100 8 2 */
	          U(200, 150, 2, 170) /* 5°120 3°110 - 1'0 8'0 7°180 - 4°90 - 2'170 - 1'100 8 2 */
	        Y U( 70, 220, 1, 100) /* 5°120 3°110 - 1'0 8'0 7°180 - 4°90 - 2'0 - 1'100 8 2 */
	   I(8) Y U( 40, 260, 2, 100) /* 5°120 3°110 - 1'0 7°180 8°0 - 4°90 - 2'0 - 2'100 1 */
	     I(1) U( 50, 310, 2,  50) /* 5°120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'50 */
	          U( 10, 320, 2,  40) /* 5°120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'40 */
	     N(1) U(200, 360, 1, 100) /* 5°120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 1'100 2 */
	          U( 10, 370, 1,  90) /* 5°120 3°110 - 1'0 7°180 8°0 - 4°90 - 2'0 - 1'90 2 */
	     I(1) U( 10, 380, 2, 100) /* 5°120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'100 */
	     O(5) U( 10, 390, 5, 120) /* 5'120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'90 5 */
	        Y U( 90, 480, 2,  90) /* 5'0 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'90 5 */
	        Y U( 10, 490, 5, 100) /* 5'0 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 5'100 2 */
	     O(7) U( 10, 500, 7, 180) /* 5'0 3°110 - 7'180 1°0 8°0 - 4°90 - 2'0 - 5'90 2 7 */
	        Y U( 10, 510, 5,  90) /* 5'0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 5'90 2 7 */
	        Y U( 10, 520, 2, 100) /* 5'0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 2'100 7 5 */
	        Y U( 10, 530, 7, 100) /* 5'0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 7'100 5 2 */
	     I(5) U( 10, 540, 7,  90) /* 5°0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 7'90 2 */
	I(7) N(5) U( 10, 550, 2, 100) /* 5'0 3°110 - 7°0 1°0 8°0 - 4°90 - 2'0 - 2'100 5 */
	     N(7) U(200, 650, 5, 100) /* 5'0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 5'100 7 2 */
	I(5) I(7) U( 10, 660, 2, 100) /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2'0 - 2'100 */
	     I(2) U( 10, 670, 0, 100) /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - */
	          U( 10, 680, 0, 100) /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - */
	          U(100, 780, 0, 100) /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - */
	     O(9) U( 10, 790, 9, 100) /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - 9'100 */
	     N(6) U( 20, 810, 9,  80) /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - 9'80 6 */
	     N(8) U( 10, 820, 9,  70) /* 5°0 3°110 - 8'0 7°0 1°0 - 4°90 - 2°0 - 9'70 6 8 */
	        Y U( 10, 830, 6, 100) /* 5°0 3°110 - 8'0 7°0 1°0 - 4°90 - 2°0 - 6'100 8 9 */
	        Y U( 10, 840, 8, 100) /* 5°0 3°110 - 8'0 7°0 1°0 - 4°90 - 2°0 - 8'100 9 6 */
	   N(7) Y U( 20, 860, 9, 100) /* 5°0 3°110 - 8'0 7'0 1°0 - 4°90 - 2°0 - 9'100 6 7 8 */
	I(8) I(9) U( 10, 870, 6, 100) /* 5°0 3°110 - 7'0 8°0 1°0 - 4°90 - 2°0 - 6'100 7 */
	I(6) I(7) U( 10, 880, 0, 100) /* 5°0 3°110 - 7°0 8°0 1°0 - 4°90 - 2°0 - */
	     O(4) U( 20, 900, 4,  90) /* 5°0 3°110 - 7°0 8°0 1°0 - 4'90 - 2°0 - 4'100 */
	O(3) N(1) U( 10, 910, 3,  90) /* 3'110 5°0 - 1'0 7°0 8°0 - 4'80 - 2°0 - 4'100 3 1 */
	N(5) I(4) U( 10, 920, 3,  80) /* 3'100 5'0 - 1'0 7°0 8°0 - 4°80 - 2°0 - 3 1 5 */
	     I(3) U( 10, 930, 1,  70) /* 5'0 3°90 - 1'0 7°0 8°0 - 4°80 - 2°0 - 1'100 5 */
	     O(3) U( 10, 940, 3,  60) /* 3'90 5'0 - 1'0 7°0 8°0 - 4°80 - 2°0 - 1'90 5 3 */
	     N(4) U( 10, 950, 3,  50) /* 3'80 5'0 - 1'0 7°0 8°0 - 4'80 - 2°0 - 1'90 5 3 4 */
	     I(4) U( 10, 960, 3,  40) /* 3'70 5'0 - 1'0 7°0 8°0 - 4°80 - 2°0 - 1'90 5 3 */
	I(3) N(4) U( 10, 970, 4,  30) /* 5'0 3°60 - 1'0 7°0 8°0 - 4'80 - 2°0 - 1'90 5 4 */
	     I(4) U( 10, 980, 1,  20) /* 5'0 3°60 - 1'0 7°0 8°0 - 4°70 - 2°0 - 1'90 5 */
	O(3) O(4) U( 10, 990, 3,  10) /* 3'60 5'0 - 1'0 7°0 8°0 - 4'70 - 2°0 - 1'80 5 3 4 */
	        Y U( 10,   0, 5, 120) /* 5'120 3'110 - 1'230 7°180 8°100 - 4'90 - 2°170 - 1'80 5 3 4 */

	/* sixth round - destroy and re-create */
	     D(3) U( 30,  30, 5,  90) /* 5'90 - 1'230 7°180 8°100 - 4'90 - 2°170 - 1'80 5 4 */
	     I(5) U( 30,  60, 1, 230) /* 5°60 - 1'230 7°180 8°100 - 4'90 - 2°170 - 1'80 4 */
	D(4) D(7) U( 20,  80, 1, 210) /* 5°60 - 1'210 8°100 - 2°170 - 1'80 4 */
	I(1) N(9) U( 40, 120, 9, 100) /* 5°60 - 1°170 8°100 - 2°170 - 9'100 */
	A(5) O(8) U( 70, 190, 5,  60) /* 5'60 - 1°170 8'100 - 2°170 - 9'30 5 8 */
	D(8) I(5) U( 10, 200, 9,  30) /* 5°60 - 1°170 - 2°170 - 9'30 */
	N(6) C(4) U( 10, 210, 9,  20) /* 5°60 - 1°170 - 4°90 - 2°170 - 9'20 6 */
	D(5) O(4) U( 10, 220, 4,  90) /* 1°170 - 4'90 - 2°170 - 9'10 6 4 */
	          U(100, 310, 9,  10) /* 1°170 - 4'0 - 2°170 - 9'10 6 4 */
	          U( 10, 320, 6, 100) /* 1°170 - 4'0 - 2°170 - 6'100 4 9 */
	     D(4) U(200, 420, 9, 100) /* 1°170 - 2°170 - 9'100 6 */
	C(5) A(5) U( 10, 430, 5, 120) /* 5'120 - 1°210 - 2°170 - 9'90 6 5 */
	   C(4) Y U( 10, 440, 9,  90) /* 5'0 - 1°170 - 4°90 - 2°170 - 9'90 6 5 */
	   O(4) Y U( 50, 490, 4,  90) /* 5'0 - 1°170 - 4'90 - 2°170 - 6'100 5 4 9 */
	   D(6) Y U( 10, 500, 5, 100) /* 5'0 - 1°170 - 4'0 - 2°170 - 5'100 4 9 */
	     D(9) U(200, 600, 4, 100) /* 5'0 - 1°170 - 4'0 - 2°170 - 4'100 5 */
	C(7) C(8) U(200, 700, 5, 100) /* 5'0 - 1°170 7°180 8°100 - 4'0 - 2°170 - 5'100 4 */
	O(1) O(7) U( 10, 710, 7, 180) /* 5'0 - 7'180 1'170 8°100 - 4'0 - 2°170 - 5'90 4 1 7 */
	     O(8) U( 40, 750, 8, 100) /* 5'0 - 8'100 7'140 1'170 - 4'0 - 2°170 - 5'90 4 1 7 8 */
	     D(7) U(200, 850, 1, 150) /* 5'0 - 1'170 8'0 - 4'0 - 2°170 - 5'90 4 1 8 */
	          U( 50, 900, 1, 100) /* 5'0 - 1'120 8'0 - 4'0 - 2°170 - 5'90 4 1 8 */
	        Y U( 60, 960, 5,  40) /* 5'0 - 8'0 1'0 - 4'0 - 2°170 - 5'90 4 1 8 */
	          U(100,   0, 5, 120) /* 5'120 - 8'100 1'230 - 4'90 - 2°170 - 5'50 4 1 8 */

	/* seventh round - re-configure quota, first part */
	Q(5, 100) C(6) U( 40,  40, 5,  80) /* 5'80 - 8'100 1'230 - 4'90 - 2°170 - 5'50 4 1 8 */
	Q(5,  70) A(6) U( 10,  50, 5,  70) /* 5'70 - 8'100 1'230 - 4'90 - 2°170 - 5'50 4 1 8 6 */
	Q(5,  40) C(9) U( 10,  60, 5,  40) /* 5'40 - 8'100 1'230 - 4'90 - 2°170 - 5'50 4 1 8 6 */
	Q(5,   0) A(9) U( 20,  80, 8, 100) /* 8'100 1'230 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */
	     Q(8, 120) U( 30, 110, 8,  70) /* 8'70 1'230 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */
	     Q(9,  40) U( 10, 120, 8,  60) /* 8'60 1'230 9'0 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */
	     Q(8, 130) U( 10, 130, 8,  50) /* 8'50 1'230 9'0 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */
	     Q(8,  50) U( 20, 150, 8,  30) /* 8'30 1'230 9'0 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */
	     Q(6,  60) U( 10, 160, 8,  20) /* 6'0 - 8'20 1'230 9'0 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */
	          I(8) U( 10, 170, 1, 230) /* 6'0 - 1'230 9'0 8°10 - 4'90 - 2°170 - 5'50 4 1 6 9 */
	          I(1) U(100, 270, 4,  90) /* 6'0 - 9'0 8°10 1°130 - 4'90 - 2°170 - 5'50 4 6 9 */
	               U(100, 360, 5,  50) /* 6'0 - 9'0 8°10 1°130 - 4'0 - 2°170 - 5'50 4 6 9 */
	     Q(1, 110) U( 10, 370, 5,  40) /* 6'0 - 9'0 8°10 1°110 - 4'0 - 2°170 - 5'40 4 6 9 */
	     Q(1, 120) U( 20, 390, 5,  20) /* 6'0 - 9'0 8°10 1°110 - 4'0 - 2°170 - 5'20 4 6 9 */
	     Q(4, 210) U( 10, 400, 5,  10) /* 6'0 - 9'0 8°10 1°110 - 4'0 - 2°170 - 5'10 4 6 9 */
	          A(1) U( 20, 410, 1, 110) /* 6'0 - 1'110 9'0 8°10 - 4'0 - 2°170 - 4'100 6 9 1 5 */
	     Q(1, 100) U( 30, 440, 1,  80) /* 6'0 - 1'80 9'0 8°10 - 4'0 - 2°170 - 4'100 6 9 1 5 */
	          O(8) U( 10, 450, 8,  10) /* 6'0 - 8'10 1'70 9'0 - 4'0 - 2°170 - 4'100 6 9 1 5 8 */
	          N(2) U( 10, 460, 1,  70) /* 6'0 - 1'70 9'0 8'0 - 4'0 - 2'170 - 4'100 6 9 1 5 8 2 */
	     Q(1,  20) U( 30, 490, 1,  20) /* 6'0 - 1'20 9'0 8'0 - 4'0 - 2'170 - 4'100 6 9 1 5 8 2 */
	     Q(1,  50) U( 10, 500, 1,  10) /* 6'0 - 1'10 9'0 8'0 - 4'0 - 2'170 - 4'100 6 9 1 5 8 2 */
	     Q(1,   0) U( 30, 510, 2, 170) /* 6'0 - 9'0 8'0 - 4'0 - 2'170 - 4'100 6 9 1 5 8 2 */
	     Q(2, 180) U( 50, 560, 2, 120) /* 6'0 - 9'0 8'0 - 4'0 - 2'120 - 4'100 6 9 1 5 8 2 */
	I(2) Q(4,  80) U( 70, 630, 4, 100) /* 6'0 - 9'0 8'0 - 4'0 - 2°50 - 4'100 6 9 1 5 8 */
	               U( 50, 680, 4,  50) /* 6'0 - 9'0 8'0 - 4'0 - 2°50 - 4'100 6 9 1 5 8 */
	          I(4) U( 10, 690, 6, 100) /* 6'0 - 9'0 8'0 - 4°0 - 2°50 - 6'100 9 1 5 8 */
	          C(3) U( 30, 720, 6,  70) /* 6'0 3°110 - 9'0 8'0 - 4°0 - 2°50 - 6'70 9 1 5 8 */
	   Q(3, 210) Y U( 20, 740, 9, 100) /* 6'0 3°110 - 9'0 8'0 - 4°0 - 2°50 - 9'100 1 5 8 6 */
	     I(9) N(4) U( 50, 790, 1, 100) /* 6'0 3°110 - 9°0 8'0 - 4'0 - 2°50 - 1'100 5 8 6 4 */
	          O(2) U(170, 890, 2,  50) /* 6'0 3°110 - 9°0 8'0 - 4'0 - 2'50 - 5'100 8 6 4 2 1 */
	        Y N(9) U( 60, 940, 5,  60) /* 6'0 3°110 - 9'0 8'0 - 4'0 - 2'0 - 5'100 8 6 4 2 1 9 */
	          I(6) U( 20, 960, 5,  40) /* 6°0 3°110 - 9'0 8'0 - 4'0 - 2'0 - 5'80 8 4 2 1 9 */
	             Y U( 10, 970, 8,  30) /* 6°0 3°110 - 9'0 8'0 - 4'0 - 2'0 - 8'100 4 2 1 9 5 */
	        I(8) Y U( 10, 980, 4,  20) /* 6°0 3°110 - 9'0 8°0 - 4'0 - 2'0 - 4'100 2 1 9 5 */
	             Y U( 20,   0, 9,  40) /* 6°60 3°210 - 9'40 8°50 - 4'80 - 2'180 - 2'100 1 9 5 4 */

	/* eighth round - re-configure quota, second part */
	          A(3) U( 30,  30, 3, 210) /* 3'210 6°60 - 9'10 8°50 - 4'80 - 2'180 - 2'100 1 9 5 4 3 */
	          I(3) U(110, 140, 9,  10) /* 6°60 3°100 - 9'10 8°50 - 4'80 - 2'180 - 2'100 1 9 5 4 */
	               U( 40, 150, 4,  80) /* 6°60 3°100 - 9'0 8°50 - 4'80 - 2'180 - 2'100 1 9 5 4 */
	               U(100, 230, 2, 180) /* 6°60 3°100 - 9'0 8°50 - 4'0 - 2'180 - 2'100 1 9 5 4 */
	      Q(2, 90) U( 40, 270, 2,  90) /* 6°60 3°100 - 9'0 8°50 - 4'0 - 2'90 - 2'100 1 9 5 4 */
	   Y Q(8, 130) U( 40, 310, 2, 100) /* 6°60 3°100 - 9'0 8°50 - 4'0 - 2'0 - 2'100 1 9 5 4 */
	               U(100, 410, 1, 100) /* 6°60 3°100 - 9'0 8°50 - 4'0 - 2'0 - 1'100 9 5 4 2 */
	 Q(3, 80) O(3) U( 60, 470, 3,  80) /* 3'80 6°60 - 9'0 8°50 - 4'0 - 2'0 - 1'40 9 5 4 2 3*/
	 N(8) Q(8, 50) U(100, 550, 8,  50) /* 3'0 6°60 - 8'50 9'0 - 4'0 - 2'0 - 1'40 9 5 4 2 3 8 */
	               U( 20, 570, 8,  30) /* 3'0 6°60 - 8'30 9'0 - 4'0 - 2'0 - 1'40 9 5 4 2 3 8 */
	 O(6) Q(6, 50) U( 10, 580, 6,  50) /* 6'50 3'0 - 8'20 9'0 - 4'0 - 2'0 - 1'40 9 5 4 2 3 8 6 */
	               U( 70, 630, 8,  20) /* 6'0 3'0 - 9'0 8'20 - 4'0 - 2'0 - 1'40 9 5 4 2 3 8 6 */
	               U( 40, 650, 1,  40) /* 6'0 3'0 - 9'0 8'0 - 4'0 - 2'0 - 1'40 9 5 4 2 3 8 6 */
	               U( 40, 690, 9, 100) /* 6'0 3'0 - 9'0 8'0 - 4'0 - 2'0 - 9'100 5 4 2 3 8 6 1 */
	          D(6) U(200, 790, 5, 100) /* 3'0 - 9'0 8'0 - 4'0 - 2'0 - 5'100 4 2 3 8 1 9 */
	          D(3) U(100, 890, 4, 100) /* 9'0 8'0 - 4'0 - 2'0 - 4'100 2 8 1 9 5 */
	               U(120, 990, 2,  10) /* 9'0 8'0 - 4'0 - 2'0 - 2'100 8 1 9 5 4 */
	               U( 80,   0, 9,  40) /* 9'40 8'50 - 4'80 - 2'90 - 2'90 8 1 9 5 4 */

	done();
}
