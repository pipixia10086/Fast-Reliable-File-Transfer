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
#include<pthread.h>
#include<semaphore.h>

#include<signal.h>
#include<sys/time.h>

#include<set>
using namespace std;

#define SERVER_IPADDRESS "192.168.0.100"
#define SERVER_PORT 8000
#define FILE_NAME_MAX_SIZE 512
//#define FILE_NAME "EE542.mkv"

//调整参数
#define BUFFER_SIZE 4096    //1024
#define MAX_QUEUE_LEN  20

//包的类型
#define DATA 1 // 因为是顺序发送，不可能出现id=2在id=1之前收到
#define ACK 2 //server收到直接发送多个包
#define RESEND 3 // server收到只发单包
#define ONCEEND 4 // server收到只发单包
#define ACKLOSS 5 // server收到只发单包

/* 包头 */
typedef struct
{
  int type;
  int id;
  unsigned int buf_size;
}PackInfo;

/* 接收包 */
struct RecvPack
{
  PackInfo head;
  char buf[BUFFER_SIZE];
} data;

sem_t sem_w,sem_r;
pthread_mutex_t clinet_buff_mutex;
// 全局变量
int id;
int End;
int timer = 0; // 控制计时器的开关,发送任何包都需要启动计时器
int datalen = sizeof(data);
PackInfo pack_info;

int ack;
//std::queue<int> Resend_list;
set<int> Resend_list;

RecvPack databuffer[MAX_QUEUE_LEN];
set<int> id_buffer;
int bufferbegin;     // = ACK + 1;
int bufferend;      // = ACK + MAX_QUEUE_LEN;

//客户端socket信息
int client_socket_fd;
struct sockaddr_in server_addr;
socklen_t server_addr_length;

//本地server信息
struct sockaddr_in server_addr1;
struct sockaddr_in client_addr1;
socklen_t client_addr_length1;
int server_socket_fd1;

//超时重传, 客户端失序
struct itimerval opent, closet;
//1用于发resend，2用于发ack
struct sigaction alr1, alr2;


// 发送
void send_packet(int type, int id, unsigned int buffersize){
	pack_info.id = id;
	pack_info.type = type;
	pack_info.buf_size = buffersize;
	/* 重发数据包确认信息 */
	if(sendto(client_socket_fd, (char*)&pack_info, sizeof(pack_info), 0, (struct sockaddr*)&server_addr, server_addr_length) < 0)
	{
	  printf("Send confirm information failed!");
	}
	if(type == RESEND){
		printf("send RESEND id: %d\n", id);
	}
	if(type == ACK){
		printf("send ACK id: %d\n", id);
	}
}


// 超时重传函数
void alarm_handler1(int type)
 {
	 int len;
     printf("RESEND loss\n");
     set<int>::iterator it;
     for(it=Resend_list.begin(); it!=Resend_list.end(); it++){
    	 id = *it;
    	 send_packet(RESEND, id, data.head.buf_size);
    	 send_packet(RESEND, id, data.head.buf_size);
     }
     if(End != 1){
    	 setitimer(ITIMER_REAL, &opent, NULL);
     }
//     Resend_list.clear();
 }


void alarm_handler2(int type)
 {
	 int len;
     printf("ACK loss\n");
     //保证ACK到达
     send_packet(ACKLOSS, ack, BUFFER_SIZE);
//     send_packet(ACKLOSS, ack, BUFFER_SIZE);
//     setitimer(ITIMER_REAL, &opent, NULL);
 }

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

//多个包
void *packets_handle(void *arg){
	int len;
	printf("thread start\n");
	while(1){
		// 一次收多个包
	    // id就是收到的包
			sem_wait(&sem_r);
	    	pthread_mutex_lock(&clinet_buff_mutex); // 利用一次recv等待的时间去写文件
	    	//第一个循环，收一轮包
	    	printf("First Loop\n");
			while(id_buffer.size()!=MAX_QUEUE_LEN){
//			setitimer(ITIMER_REAL, &opent, NULL);
//			sigaction(SIGALRM, &alr2, NULL);
			if((len = recvfrom(server_socket_fd1, (char*)&data, sizeof(data), 0, (struct sockaddr*)&client_addr1,&client_addr_length1)) > 0)
				{
					//收到就关闭计时器
//					setitimer(ITIMER_REAL, &closet, NULL);

					id = data.head.id;
					printf("receive id: %d, type: %d, buf_size: %d\n", data.head.id, data.head.type, data.head.buf_size);

					if(data.head.type == DATA){
						//判断是不是需要的包
						if(id <= bufferend and id >= bufferbegin){
							// 判度id在不在buffer中
							// 如果没有收到过，则存
							if(id_buffer.count(id) == 0){
								int index = id - bufferbegin;  // id = 2, bufferstart = 1, index = 1
								databuffer[index] = data;
								id_buffer.insert(id);
							}
			// 判断是不是最后一个包，如果是则将buffer_end改为最后一个包的id,使得buffer可以恰好装最后一批包（不满20）
							if(data.head.buf_size < BUFFER_SIZE){
								bufferend = data.head.id;
								End = 1; // 收到了全文件最后一个包
								printf("I got last one\n");
								break;
							}

						}
					}
					if(data.head.type == ONCEEND){
						// 收到信号包先跳出循环
						// usleep(200); // 丢弃掉后续的type4
						break;
					}
				}
				else
				{
					printf("RECV fail\n");
				}
			}

			//循环把信号包处理完
//		while(1){
//			recvfrom(client_socket_fd, (char*)&data, sizeof(data), 0, (struct sockaddr*)&server_addr,&server_addr_length);
//			if(data.head.type != ONCEEND){
//				//判断是不是需要的包
//				if(id <= bufferend and id >= bufferbegin){
//					// 判度id在不在buffer中
//					// 如果没有收到过，则存
//					if(id_buffer.count(id) == 0){
//						int index = id - bufferbegin;
//						databuffer[index] = data;
//						id_buffer.insert(id);
//					}
//				}
//				break;
//			}
//		}
		// 收剩下的包
		printf("receive %d packets\n", id_buffer.size());
		//最后一波包，不满20，不能用maxqueuelen判断，需要用bufferbegin-bufferend
//		while(id_buffer.size()!=MAX_QUEUE_LEN){
		while(id_buffer.size()!=(bufferend - bufferbegin + 1)){
				//for循环发送所有的resend
//				timer = 1;
//				setitimer(ITIMER_REAL, &opent, NULL);
				//timeout后，说明堵塞在下面的recv中，timeout产生中断，timeout函数发送resend
//				sigaction(SIGALRM, &alr1, NULL);
				for(int i = bufferbegin; i<= bufferend; i++){
					//不在set中，没有收到的包,发送resend包
					if(id_buffer.count(i) == 0){
						// send resend
						Resend_list.insert(i);
						send_packet(RESEND, i, data.head.buf_size);
					}
				}
				// 发完所有的request，处理data
				// 可能会出现的问题，收到一个包就去再发request，可能会丢掉剩下的包。但是没有关系，他会再发一次

				while(id_buffer.size()!=(bufferend - bufferbegin + 1)){
					setitimer(ITIMER_REAL, &opent, NULL);
					sigaction(SIGALRM, &alr1, NULL);
					if((len = recvfrom(server_socket_fd1, (char*)&data, sizeof(data), 0, (struct sockaddr*)&client_addr1,&client_addr_length1)) > 0)
					{
						//收到就关闭计时器
						setitimer(ITIMER_REAL, &closet, NULL);
						id = data.head.id;
						printf("afrer resending, receive id: %d, type: %d, buf_size: %d\n", data.head.id, data.head.type, data.head.buf_size);

						//如果第一次没收到最后一个包，需要再次判断，是否收到最后一个包
						if(data.head.buf_size < BUFFER_SIZE){
							bufferend = data.head.id;
							End = 1; // 收到了全文件最后一个包
						}
						if(data.head.type == ONCEEND){
							continue;
						}

						if(data.head.type == DATA){
							//判断是不是需要的包
							if(id <= bufferend and id >= bufferbegin){
								// 判度id在不在buffer中
								// 如果没有收到过，则存
								if(id_buffer.count(id) == 0){
									int index = id - bufferbegin;
									databuffer[index] = data;
									id_buffer.insert(id);

									Resend_list.erase(id);
								}
							}
						}
					}
				}

//				if((len = recvfrom(server_socket_fd1, (char*)&data, sizeof(data), 0, (struct sockaddr*)&client_addr1,&client_addr_length1)) > 0)
//				{
//					//收到就关闭计时器
////					setitimer(ITIMER_REAL, &closet, NULL);
//					id = data.head.id;
//					printf("afrer resending, receive id: %d, type: %d, buf_size: %d\n", data.head.id, data.head.type, data.head.buf_size);
//
//					//如果第一次没收到最后一个包，需要再次判断，是否收到最后一个包
//					if(data.head.buf_size < BUFFER_SIZE){
//						bufferend = data.head.id;
//						End = 1; // 收到了全文件最后一个包
//					}
//
//					if(data.head.type == DATA){
//						//判断是不是需要的包
//						if(id <= bufferend and id >= bufferbegin){
//							// 判度id在不在buffer中
//							// 如果没有收到过，则存
//							if(id_buffer.count(id) == 0){
//								int index = id - bufferbegin;
//								databuffer[index] = data;
//								id_buffer.insert(id);
//
//								Resend_list.erase(id);
//							}
//						}
//					}
//				}
			}
//          必须在发完ACK之后解锁，不然切换两次线程导致bufferbegin加了两次导致收不到剩下的包
//			pthread_mutex_unlock(&clinet_buff_mutex);

			//收剩下的包完成，那么就应该先发ACK， 利用recv的时间切换线程去write，并且在write清空buffer
			if(id_buffer.size()==(bufferend - bufferbegin + 1))
			{
				if(End == 1){
	//				告诉服务器收完了;
					send_packet(ACK, -1, BUFFER_SIZE);
					//收完了，终止线程
					printf("thread terminate\n");
				//	pthread_mutex_unlock(&clinet_buff_mutex);
					sem_post(&sem_w);
					pthread_exit(NULL);
					break;
				}
				else{
					//超时重发ACK
//					timer = 1;
//					setitimer(ITIMER_REAL, &opent, NULL);
//					sigaction(SIGALRM, &alr2, NULL);
					ack = bufferend + 1;
					//code bufferend + 1 = 下一个我要的
					send_packet(ACK, bufferend + 1, BUFFER_SIZE);
					send_packet(ACK, bufferend + 1, BUFFER_SIZE);
					send_packet(ACK, bufferend + 1, BUFFER_SIZE);
					send_packet(ACK, bufferend + 1, BUFFER_SIZE);
					send_packet(ACK, bufferend + 1, BUFFER_SIZE);

//					write完成以后更新
//					bufferbegin = bufferend + 1;
//					bufferend = bufferbegin + MAX_QUEUE_LEN - 1;
				}
				//告诉write你去写吧
				sem_post(&sem_w);
				pthread_mutex_unlock(&clinet_buff_mutex);

			}
		} //大loop
	}


int main()
{
	//初始化全局变量
	id = 1;
	End = 0;
	bufferbegin = 1;
	bufferend = MAX_QUEUE_LEN;

	sem_init(&sem_w,0,0);
	sem_init(&sem_r,0,1);

	//计时器相关
	opent.it_value.tv_sec = 0;
	opent.it_value.tv_usec = 500000; // 50ms
	opent.it_interval.tv_sec = 0;
	opent.it_interval.tv_usec = 0;

	closet.it_value.tv_sec = 0;
	closet.it_value.tv_usec = 0; // 0ms
	closet.it_interval.tv_sec = 0;
	closet.it_interval.tv_usec = 0;

	alr1.sa_handler = alarm_handler1;
	alr1.sa_flags = SA_NOMASK;
	alr1.sa_restorer = NULL;

	alr2.sa_handler = alarm_handler2;
	alr2.sa_flags = SA_NOMASK; //返回后在中断重新执行
	alr2.sa_restorer = NULL;

//   setitimer(ITIMER_REAL, &opent, NULL);
//   setitimer(ITIMER_REAL, &closet, NULL);
//   sigaction(SIGALRM,&alr,NULL);

	//socket初始化
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(SERVER_IPADDRESS);
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr_length = sizeof(server_addr);

  /* 创建socket */
	client_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(client_socket_fd < 0)
	{
	perror("Create Socket Failed:");
	exit(1);
	}

	//请求文件信息
  /* 输入文件名到缓冲区 */
	char file_name[FILE_NAME_MAX_SIZE+1];
	bzero(file_name, FILE_NAME_MAX_SIZE+1);
	printf("Please Input File Name On Server: ");
	scanf("%s", file_name);

	char buffer[BUFFER_SIZE];
	bzero(buffer, BUFFER_SIZE);
	strncpy(buffer, file_name, strlen(file_name)>BUFFER_SIZE?BUFFER_SIZE:strlen(file_name));

  /* 发送文件名5次，保证第一个包不丢 */
	int cnt = 5;
	while(cnt > 0){
		cnt --;
		if(sendto(client_socket_fd, buffer, BUFFER_SIZE,0,(struct sockaddr*)&server_addr,server_addr_length) < 0)
		{
		perror("Send File Name Failed:");
		exit(1);
		}
	}

	printf("Ready to receive File:%s\n", file_name);
  /* 打开文件，准备写入 */
	FILE *fp = fopen(file_name, "w");
	if(NULL == fp)
	{
	printf("File:\t%s Can Not Open To Write\n", file_name);
	exit(1);
	}

	  // 创建接受的socket
	  /* 创建UDP套接口 */
	  bzero(&server_addr1, sizeof(server_addr1));
	  server_addr1.sin_family = AF_INET;
	  server_addr1.sin_addr.s_addr = htonl(INADDR_ANY);
	  server_addr1.sin_port = htons(8888);
	  //
	  client_addr_length1 = sizeof(client_addr1);
	  /* 创建socket */
	  server_socket_fd1 = socket(AF_INET, SOCK_DGRAM, 0);
	  if(server_socket_fd1 == -1)
	  {
	    perror("Create Socket Failed:");
	    exit(1);
	  }
	  /* 绑定套接口 */
	  if(-1 == (bind(server_socket_fd1,(struct sockaddr*)&server_addr1,sizeof(server_addr1))))
	  {
	    perror("Server Bind Failed:");
	    exit(1);
	  }
	  printf("server: waiting for connections...\n");
		if(recvfrom(server_socket_fd1, buffer, BUFFER_SIZE,0,(struct sockaddr*)&client_addr1, &client_addr_length1) == -1)
		{
		  perror("Receive Data Failed:");
		  exit(1);
		}
		char* ip;
		ip = inet_ntoa(client_addr1.sin_addr);
		int port;
		port = ntohs(client_addr1.sin_port);
		printf("accept: ip:%s, port:%d\n", ip, port);


//计算传输时间
	clock_t start, finish;
	double  duration;
	start = clock();

 /* 从服务器接收数据，并写入文件 */
  while(1)
  {
    // 使用线程 接受信息并发送包
	/*开辟线程处理接受到的文件信息，一个pthread_create开辟一个线程*/
	pthread_t  tid;
	if(pthread_create(&tid, NULL, packets_handle, NULL) < 0 )
	{
		printf("pthread_create Failed : %s\n",strerror(errno));
	}

    // 主线程仅仅用来将queue中的数据写入到文件中，清空内存
	while(1){
			// 互斥锁定 写入
			sem_wait(&sem_w);
			printf("main thread\n");
		//	pthread_mutex_lock(&clinet_buff_mutex);
			printf("size: %d, bufferend id: %d, bufferbegin: %d\n", id_buffer.size(),bufferend, bufferbegin);
			if(id_buffer.size() != (bufferend - bufferbegin + 1)){
			//	pthread_mutex_unlock(&clinet_buff_mutex);
				continue;
				// sem_post(&sem_r);
			}
			if(End == 1){
				//缓存写文件
				for(int i=0; i<id_buffer.size(); i++){
					data = databuffer[i];
					if(fwrite(data.buf, sizeof(char), data.head.buf_size, fp) < data.head.buf_size)
					{
					  printf("File:\t%s Write Failed\n", file_name);
					  break;
					}
					printf("write id: %d, buf_size: %d\n", data.head.id, data.head.buf_size);
				}
				break;
			}

			pthread_mutex_lock(&clinet_buff_mutex);
			//缓存写文件
			for(int i=0; i<id_buffer.size(); i++){
				data = databuffer[i];
				if(fwrite(data.buf, sizeof(char), data.head.buf_size, fp) < data.head.buf_size)
				{
				  printf("File:\t%s Write Failed\n", file_name);
				  break;
				}
				printf("write id: %d, buf_size: %d\n", data.head.id, data.head.buf_size);
			}
			//清空缓存idbuffer，不需要清空databuffer因为会覆盖
			id_buffer.clear();
			bufferbegin = bufferend + 1;
			bufferend = bufferbegin + MAX_QUEUE_LEN - 1;

			pthread_mutex_unlock(&clinet_buff_mutex);
			sem_post(&sem_r);

	}

	// 循环结束，直接return
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

}




