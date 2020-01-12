#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<netdb.h>
#include<stdarg.h>
#include<string.h>
#include<time.h>

#include<queue>
#include<cstdlib>

#define SERVER_PORT 8000
#define BUFFER_SIZE 1024
#define FILE_NAME_MAX_SIZE 512

#define MAX_QUEUE_LEN   20

/* 包头 */
typedef struct
{
  int id;
  unsigned int buf_size;
}PackInfo;

/* 接收包 */
struct RecvPack
{
  PackInfo head;
  char buf[BUFFER_SIZE];
} data;

// 获取文件大小
#include<sys/stat.h>
long get_file_size(const char *path)
{
	unsigned long filesize = -1;
	struct stat statbuff;
	if(stat(path, &statbuff) < 0){
		return filesize;
	}
	else{
		filesize = statbuff.st_size;
	}
	return filesize;
}

/*保存数据*/
//int save_file(struct udp_datapack file_datas)
//{
//	return -1;
//}

// 使用队列实现  累计确认

int main()
{
  int id = 1;
  int end = 0;
  std::queue<RecvPack> packet_list;

//  std::queue<RecvPack> packet_list;

  /* 服务端地址 */
  struct sockaddr_in server_addr;
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  server_addr.sin_port = htons(SERVER_PORT);
  socklen_t server_addr_length = sizeof(server_addr);

  /* 创建socket */
  int client_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if(client_socket_fd < 0)
  {
    perror("Create Socket Failed:");
    exit(1);
  }

  /* 输入文件名到缓冲区 */
  char file_name[FILE_NAME_MAX_SIZE+1];
  bzero(file_name, FILE_NAME_MAX_SIZE+1);
  printf("Please Input File Name On Server: ");
  scanf("%s", file_name);

  char buffer[BUFFER_SIZE];
  bzero(buffer, BUFFER_SIZE);
  strncpy(buffer, file_name, strlen(file_name)>BUFFER_SIZE?BUFFER_SIZE:strlen(file_name));

  /* 发送文件名 */
  if(sendto(client_socket_fd, buffer, BUFFER_SIZE,0,(struct sockaddr*)&server_addr,server_addr_length) < 0)
  {
    perror("Send File Name Failed:");
    exit(1);
  }

  /* 打开文件，准备写入 */
  FILE *fp = fopen(file_name, "w");
  if(NULL == fp)
  {
    printf("File:\t%s Can Not Open To Write\n", file_name);
    exit(1);
  }

  clock_t start, finish;
  double  duration;
  start = clock();

  /* 从服务器接收数据，并写入文件 */
  int len = 0;
  while(1)
  {
    PackInfo pack_info;

    // 一次收多个包
    // out of order直接返回缺失的序号，并将顺序的queue写进入文件中，然后清空queue
    int cnt = 0;
    while(cnt < MAX_QUEUE_LEN){
        if((len = recvfrom(client_socket_fd, (char*)&data, sizeof(data), 0, (struct sockaddr*)&server_addr,&server_addr_length)) > 0)
        {
          printf("id: %d, buf_size: %d\n", data.head.id, data.head.buf_size);
          cnt++;
          if(data.head.id == id)
          {  //只需要存入queue中
        	packet_list.push(data);
        	++id;
          }
          else /* 不管是重发的包  还是失去序列的包，都直接重发  直接跳出循环，在循环外面完成写入操作。 */
          {
            pack_info.id = id;
            pack_info.buf_size = data.head.buf_size;
            /* 重发数据包确认信息 */
            if(sendto(client_socket_fd, (char*)&pack_info, sizeof(pack_info), 0, (struct sockaddr*)&server_addr, server_addr_length) < 0)
            {
              printf("Send confirm information failed!");
            }
            break;
          }

          //传输完成，先跳一层循环，发送-1终止信息,并将结束标志为设置为1
          if(data.head.buf_size < BUFFER_SIZE){
        	  printf("jump\n");
              pack_info.id = -1;
              pack_info.buf_size = data.head.buf_size;
              /* 重发数据包确认信息 */
              if(sendto(client_socket_fd, (char*)&pack_info, sizeof(pack_info), 0, (struct sockaddr*)&server_addr, server_addr_length) < 0)
              {
                printf("Send confirm information failed!");
              }
              printf("send ack id: %d\n", pack_info.id);
              end = 1;
              break;
          }
        }
    }

    if(cnt==MAX_QUEUE_LEN){
        pack_info.id = id - 1;
        pack_info.buf_size = data.head.buf_size;
        /* 重发数据包确认信息 */
        if(sendto(client_socket_fd, (char*)&pack_info, sizeof(pack_info), 0, (struct sockaddr*)&server_addr, server_addr_length) < 0)
        {
          printf("Send confirm information failed!");
        }
        printf("send ack id: %d\n", pack_info.id);
    }

    // 读完十个包或者发送了out of order包，将queue中的数据写到文件中
    while(!packet_list.empty()){
        data = packet_list.front();
        packet_list.pop();
		if(fwrite(data.buf, sizeof(char), data.head.buf_size, fp) < data.head.buf_size)
		{
		  printf("File:\t%s Write Failed\n", file_name);
		  break;
		}
    }

    if(end == 1){
    	break;
    }


   //  一次收一个包的读写
//    if((len = recvfrom(client_socket_fd, (char*)&data, sizeof(data), 0, (struct sockaddr*)&server_addr,&server_addr_length)) > 0)
//    {
//      printf("id: %d, buf_size: %d\n", data.head.id, data.head.buf_size);
//      if(data.head.id == id)
//      {
//        pack_info.id = data.head.id;
//        pack_info.buf_size = data.head.buf_size;
//        ++id;
//
//        /* 发送数据包确认信息 */
//        if(sendto(client_socket_fd, (char*)&pack_info, sizeof(pack_info), 0, (struct sockaddr*)&server_addr, server_addr_length) < 0)
//        {
//          printf("Send confirm information failed!");
//        }
//        /* 写入文件 */
//        if(fwrite(data.buf, sizeof(char), data.head.buf_size, fp) < data.head.buf_size)
//        {
//          printf("File:\t%s Write Failed\n", file_name);
//          break;
//        }
//      }
//      else if(data.head.id < id) /* 如果是重发的包 */
//      {
//        pack_info.id = data.head.id;
//        pack_info.buf_size = data.head.buf_size;
//        /* 重发数据包确认信息 */
//        if(sendto(client_socket_fd, (char*)&pack_info, sizeof(pack_info), 0, (struct sockaddr*)&server_addr, server_addr_length) < 0)
//        {
//          printf("Send confirm information failed!");
//        }
//      }
//      else
//      {  //out of order直接丢弃
//    	  continue;
//      }
//
//      if(pack_info.buf_size < BUFFER_SIZE){
//    	  break;
//      }
//
//    }

//    else
//    {
//      break;
//    }
  }

  printf("Receive File:\t%s From Server IP Successful!\n", file_name);

  finish = clock();
  duration = (double)(finish - start) / CLOCKS_PER_SEC;
  long file_size;
  file_size = get_file_size(file_name);
  printf( "%f S\n", duration);
  printf( "%ld Byte\n", file_size);
  printf( "%f MBPS\n", file_size/duration/1000000 );

  fclose(fp);
  close(client_socket_fd);
  return 0;
}
