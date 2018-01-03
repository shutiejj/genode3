#ifndef _INCLUDE__HELLO_SESSION__HELLO_SESSION_H_
#define _INCLUDE__HELLO_SESSION__HELLO_SESSION_H_

#include <session/session.h>
#include <base/rpc.h>

namespace Remote_attestation { struct Session; }


struct Remote_attestation::Session : Genode::Session
{
	static const char *service_name() { return "Remote_attestation"; }

	enum { CAP_QUOTA = 3 };

	virtual void say_hello() = 0;
	virtual int add(int a, int b) = 0;
	virtual char* create_attestation(
		char nonce_1, int timeStamp,
		char optional, char code_measurement)=0;

	/*******************
	 ** RPC interface **
	 *******************/

	GENODE_RPC(Rpc_say_hello, void, say_hello);
	GENODE_RPC(Rpc_add, int, add, int, int);
	GENODE_RPC(Rpc_create_attestation, char*, create_attestation,
		char, int, char, char);

	GENODE_RPC_INTERFACE(Rpc_say_hello, Rpc_add,Rpc_create_attestation);
};

#endif /* _INCLUDE__HELLO_SESSION__HELLO_SESSION_H_ */
