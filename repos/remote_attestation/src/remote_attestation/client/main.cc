#include <base/component.h>
#include <base/log.h>
#include <remote_attestation_session/connection.h>
#include <base/rpc_client.h>

void Component::construct(Genode::Env &env)
{
	Remote_attestation::Connection attestation(env);

	attestation.say_hello();

	int const sum = attestation.add(2, 5);
	Genode::log("added 2 + 5 = ", sum);
	
//	int const sum2 = attestation.call<Rpc_add>(3, 5);
	//Genode::log("added 3+5=",sum2);

	Genode::log("hello test completed");

	//char nonce_1[256] = {};
	//char optional[256] = {};
	//char code_measurement[256] = {};
	char nonce_1 = 's';
	char optional = 'b';
	char code_measurement = 'c';
	int timeStamp = 5123;
	//char report[1056];
	char* report;
	report = (char*)Genode::env()->heap()->alloc(1056*sizeof(char));
	report = attestation.create_attestation(nonce_1,timeStamp,optional,code_measurement);
	Genode::log("came back from new function");
	
	//this is how you pass complex variables 
	struct attestation_report rep;
	rep = attestation.test1(123);
	Genode::log("report: ",*rep.code_measurement);
}
