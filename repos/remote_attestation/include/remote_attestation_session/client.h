#ifndef _INCLUDE__HELLO_SESSION_H__CLIENT_H_
#define _INCLUDE__HELLO_SESSION_H__CLIENT_H_

#include <remote_attestation_session/remote_attestation_session.h>
#include <base/rpc_client.h>
#include <base/log.h>

namespace Remote_attestation { struct Session_client; }


struct Remote_attestation::Session_client : Genode::Rpc_client<Session>
{
	Session_client(Genode::Capability<Session> cap)
	: Genode::Rpc_client<Session>(cap) { }

	void say_hello()
	{
		Genode::log("issue RPC for saying hello");
		call<Rpc_say_hello>();
		Genode::log("returned from 'say_hello' RPC call");
	}

	int add(int a, int b)
	{
		return call<Rpc_add>(a, b);
	}

	char* create_attestation(char nonce_1,
		int timeStamp, char optional, char code_measurement)
	{
		return call<Rpc_create_attestation>(nonce_1,timeStamp,
			optional,code_measurement);
	}
};

#endif /* _INCLUDE__HELLO_SESSION_H__CLIENT_H_ */
