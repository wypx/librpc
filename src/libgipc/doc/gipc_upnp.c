
#include "gipc_command.h"
#include "gipc_process.h"
#include "gipc_message.h"

#include "network.h"


struct upnp_param{
	unsigned char	b_nat;
	unsigned char	b_discovery;
	
	unsigned char	res[2];

	unsigned char	friend_name[32];
	
};

struct upnp_param upnp;

int cb(char* data, int* len, int ctype) {
	
	printf("cmd type = %d\n", ctype);

	if( ctype == miniupnp_get_param ) 
		memcpy(data, &upnp, sizeof(upnp));

	return 0;
}

struct dlna_param {
	unsigned char	b_nat;
	unsigned char	b_discovery;
	
	unsigned char	res[2];

	unsigned char	friend_name[32];
	
};

#define tcp_server_host "192.168.0.110"
#define tcp_server_port "9993"

int main () {

	int res = -1;
	res = gipc_init(GIPC_MiniUPNP, tcp_server_host, tcp_server_port, cb);
	//res = gipc_init(GIPC_MiniUPNP, local_host_v4, local_port, cb);
	if( res != 0 ) {
		printf("gipc_init failed \n");
		return -1;
	} 
	
	struct dlna_param dlna;
	memset(&dlna, 0, sizeof(struct dlna_param));


	struct gipc_packet_t packet;

	packet.desid = GIPC_MiniDNLA;
	packet.command= minidlna_get_param;
	packet.payload = NULL;
	packet.paylen = 0;

	packet.restload = (unsigned char*)&dlna;
	packet.restlen = sizeof(dlna);

	res = gipc_call_service(&packet, 2000);

	if(res != 0) {
		printf("gipc_call_service err = %d\n", res);
	}
	printf("dlna friend_name = %s\n", dlna.friend_name);
	
	while( 1 ) 
		sleep(2);
	
	gipc_deinit();

	return 0;
}

