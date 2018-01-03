#ifndef _INCLUDE__HELLO_SESSION__CONNECTION_H_
#define _INCLUDE__HELLO_SESSION__CONNECTION_H_

#include <remote_attestation_session/client.h>
#include <base/connection.h>

namespace Remote_attestation { struct Connection; }


struct Remote_attestation::Connection : Genode::Connection<Session>, Session_client
{
	Connection(Genode::Env &env)
	:
		/* create session */
		Genode::Connection<Remote_attestation::Session>(env, session(env.parent(),
		                                                "ram_quota=6K, cap_quota=4")),

		/* initialize RPC interface */
		Session_client(cap()) { }
};

#endif /* _INCLUDE__HELLO_SESSION__CONNECTION_H_ */
