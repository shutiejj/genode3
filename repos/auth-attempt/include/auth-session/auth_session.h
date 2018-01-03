#include <session/session.h>
#include <base/rpc.h>

namespace Hello { struct Session;}

struct Hello::Session : Genode::Session {
	static const char* service_name() {return "Hello";}

	enum { CAP_QUOTA = 2};
	
	virtual void initiate_auth_protocol() =0;
	virtual int add(int a, int b) =0;

	GENODE_RPC(Rpc_initiate_auth_protocol, void, initiate_auth_protocol);
	GENODE_RPC(Rpc_add,int,add,int,int);
	GENODE_RPC_INTERFACE(Rpc_initiate_auth_protocol,Rpc_add);
};
