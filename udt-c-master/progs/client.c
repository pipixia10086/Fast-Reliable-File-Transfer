#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include "../include/udt.h"

#define HOST "127.0.0.1"
#define PORT "9000"
#define BUFFER_SIZE 10240

int main(int argc, char *argv[])
{
    socket_t        sock;
    int             err;
    struct addrinfo hints,
                    *result;

    udt_startup();    # api.c   中 初始化recv  和  send  buffer 并加锁

	
	# 这里应该用argv中传入  ip：port
    /* get address info */
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    /*hints.ai_socktype = SOCK_STREAM;*/
	
	# int getaddrinfo( const char *hostname, const char *service, const struct addrinfo *hints, struct addrinfo **result );
	# service：服务名可以是十进制的端口号，也可以是已定义的服务名称，如ftp、http等
	# getaddrinfo解决了把主机名和服务名转换成套接口地址结构的问题。  
	# hints：可以是一个空指针，也可以是一个指向某个addrinfo结构体的指针，调用者在这个结构中填入关于期望返回的信息类型的暗示。定制
	# 返回一个指向addrinfo结构体链表的指针
    if ((err = getaddrinfo(NULL, PORT, &hints, &result)) != 0) {
        fprintf(stderr, "Error: %s\n", gai_strerror(err));
        exit(err);
    }

    /* create a socket */
	# api.c 中 udt_socket 用来check并构建socket 
    sock = udt_socket(result -> ai_family,
                      result -> ai_socktype,
                      result -> ai_protocol);
    if (sock == -1) {
        fprintf(stderr, "Could not create socket\n");
        exit(errno);
    }

    /* connect to server */
	# udt_connect 完成连接，并填写状态信息到conn connection中，启用多线程等待接受并解析包，写到
	# 这里应该不会被堵塞，等到server的控制指令，然后sento返回
    if (udt_connect(sock, result -> ai_addr, result -> ai_addrlen) == -1) {
        fprintf(stderr, "Could not connect to socket\n");
        exit(errno);
    } else {
        fprintf(stderr, "Connected\n");
    }

    freeaddrinfo(result);

    /* send file */
    int filefd = open("assets/sendfile", O_RDONLY);  # 只读
    if (filefd < -1) return 2;
	
    if (udt_sendfile(sock, filefd, 0, 10, 0) < 0) return 1;
    close(filefd);

    /* send, recv */
    size_t size;
    char *line;
    udt_send(sock, "\tClient wants to talk", 22, 0);
    while (1) {
        printf("\t\n>> ");
        size = 0;
        size = getline(&line, &size, stdin);
        if (size == 1) break;
        *(line + size - 1) = '\0';
        udt_send(sock, line, size, 0);
        free(line);
    }

    printf("Disconnected\n");

    /* close the connection */
    udt_close(sock);
    return 0;
}
