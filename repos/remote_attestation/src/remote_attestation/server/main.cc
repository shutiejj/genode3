#include <base/component.h>
#include <base/log.h>
#include <base/heap.h>
#include <root/component.h>
#include <remote_attestation_session/remote_attestation_session.h>
#include <base/rpc_server.h>

namespace Remote_attestation {
	struct Session_component;
	struct Root_component;
	struct Main;
}


struct Remote_attestation::Session_component : Genode::Rpc_object<Session>
{
	void say_hello() {
		Genode::log("I am here... Hello."); }

	int add(int a, int b) {
		return a + b; }

	char* create_attestation(char nonce_1, int timeStamp,
		char optional, char code_measurement) {

		char* attestation_report = "Hello world";
//		int i=0; // will point to the location in the report 

		Genode::log("obtaining key now");
		Genode::log(nonce_1);
		//attestation_report = "asfasd";
		return attestation_report;
	}

	struct attestation_report test1(int length){
		struct attestation_report report;
		*report.code_measurement = length;
		return report;
	}
};


class Remote_attestation::Root_component
:
	public Genode::Root_component<Session_component>
{
	protected:

		Session_component *_create_session(const char *args)
		{
			Genode::log("creating remote attestation session");
			return new (md_alloc()) Session_component();
		}

	public:

		Root_component(Genode::Entrypoint &ep,
		               Genode::Allocator &alloc)
		:
			Genode::Root_component<Session_component>(ep, alloc)
		{
			Genode::log("creating root component");
		}
};


struct Remote_attestation::Main
{
	Genode::Env &env;

	/*
	 * A sliced heap is used for allocating session objects - thereby we
	 * can release objects separately.
	 */
	Genode::Sliced_heap sliced_heap { env.ram(), env.rm() };

	Remote_attestation::Root_component root { env.ep(), sliced_heap };

	Main(Genode::Env &env) : env(env)
	{
		/*
		 * Create a RPC object capability for the root interface and
		 * announce the service to our parent.
		 */
		env.parent().announce(env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env)
{
	static Remote_attestation::Main main(env);
}
