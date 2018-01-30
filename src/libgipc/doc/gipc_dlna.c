
#include "gipc_command.h"
#include "gipc_process.h"
#include "gipc_message.h"

#include "network.h"


struct dlna_param{
	unsigned char	b_nat;
	unsigned char	b_discovery;
	
	unsigned char	res[2];

	unsigned char	friend_name[32];
	
};

struct dlna_param dlna;

int cb(char* data, int* len, int ctype) {

	printf("restlen = %d  cmd type = %d\n",  *len, ctype);

	if(sizeof(dlna) != *len )
		printf("len is error#########\n");
	
	if( ctype == minidlna_get_param ) 
		memcpy(data, &dlna, sizeof(dlna));

	return 0;
}

#define tcp_server_host "192.168.0.110"
#define tcp_server_port "9993"


int main () {

	int res = -1;
	//res = gipc_init(GIPC_MiniDNLA, local_host_v4, local_port, cb);
	res = gipc_init(GIPC_MiniDNLA, tcp_server_host, tcp_server_port, cb);
	if( res != 0 ) {
		printf("gipc_init failed \n");
		return -1;
	} 

	memset(&dlna, 0, sizeof(struct dlna_param));
	dlna.b_discovery = 2;
	dlna.b_nat = 3;
	memcpy(dlna.friend_name, "dlna_test", strlen("dlna_test"));

	gipc_packet packet;

	packet.desid 	= GIPC_MiniUPNP;
	packet.command	= miniupnp_get_param;
	
	packet.payload = NULL;
	packet.paylen = 0;

	packet.restload = (unsigned char*)&dlna;
	packet.restlen = sizeof(struct dlna_param);

	//res = gipc_call_service(&packet, 2000);


	//printf("dlna b_nat:%d \n", dlna.b_nat);
	//printf("dlna b_discovery:%d \n", dlna.b_discovery);
	//printf("dlna friend_name :%s \n", dlna.friend_name);

	while( 1 ) 
		sleep(2);
	
	gipc_deinit();

	return 0;
}
