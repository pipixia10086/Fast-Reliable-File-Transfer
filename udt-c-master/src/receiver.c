#include <string.h>
#include <sys/socket.h>

#include "core.h"
#include "receiver.h"
#include "packet.h"

void receiver_start (void *arg)
{
    conn_t *conn = (conn_t *) arg;
	# packet_t由header和data组成
    packet_t packet;

    memset(&packet, 0, sizeof(packet_t));
	
	# int recvfrom（int sockfd，void *buf，int len，unsigned int lags，struct sockaddr *from，int *fromlen）；  
    while (recvfrom(conn -> sock, &packet, sizeof(packet_t), 0,
           &(conn -> addr), &(conn -> addrlen))) {
        conn -> is_open = 1;
		# 将packet解析并写入buffer中，然后清零
        packet_parse(packet);  
        memset(&packet, 0, sizeof(packet_t));
    }
}
