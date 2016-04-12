#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/epoll.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<fcntl.h>
#define MAX_NUM 64
int startup(int port)
{
	int sock=socket(AF_INET,SOCK_STREAM,0);
	if(sock<0){
		perror("socket");
		exit(1);
	}
	struct sockaddr_in local;
	local.sin_family=AF_INET;
	local.sin_port=htons(port);
	local.sin_addr.s_addr=htonl(INADDR_ANY);
	if(bind(sock,(struct sockaddr*)&local,sizeof(local))<0){
		perror("bind");
		exit(2);
	}
	if(listen(sock,5)<0){
		perror("listen");
		exit(3);
	}
	return sock;
}
//set file descriptor for no blocking mode
void set_nonoblock(int _fd)
{
	int fl=fcntl(_fd,F_GETFL);
	if(fl<0){
		perror("fcntl");
		return;
	}
	fcntl(_fd,fl | O_NONBLOCK);
	return;
}
//recv data
int read_data(int _fd,char* buf,int len)
{
	ssize_t _size=-1;
	int total=0;
	while((_size=recv(_fd,buf+total,len-1,MSG_DONTWAIT))){
		if(_size>0){
			total+=_size;
		}else if(_size==0){//file end
			return 0;
		}else{
			return -1;
		}
	}
}
int main()
{
	short port=8080;
	int listen_sock=startup(port);
	struct sockaddr_in client;
	socklen_t len=sizeof(client);
	int epoll_fd=epoll_create(256);//create epoll handle
	int timeout=1000;
	if(epoll_fd<0){
		perror("epoll_create");
		exit(1);
	}
	struct epoll_event _ev;//save care fd
	_ev.events=EPOLLIN;
	_ev.data.fd=listen_sock;
	if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,listen_sock,&_ev)<0){
		perror("epoll_ctl");
		goto LABLE;
	}
	struct epoll_event _ev_out[MAX_NUM];//save ready fd
	char buf[1024*5];
	memset(buf,'\0',sizeof(buf));
	int ready_num=-1;//save ready_fd num
	while(1){
		switch(ready_num=epoll_wait(epoll_fd,_ev_out,MAX_NUM,timeout)){
			case 0:
				printf("timeout\n");
				break;
			case -1:
				perror("epoll_wait");
				break;
			default:
				{
					int i=0;
					for(;i<ready_num;i++){
						int _fd=_ev_out[i].data.fd;
						//listen_sock
						if(_fd==listen_sock && (_ev_out[i].events & EPOLLIN)){
							int new_sock=accept(_fd,(struct sockaddr*)&client,&len);
							if(new_sock<0){
								perror("accept");
								continue;
							}
							printf("get a new connect...\n");
							set_nonoblock(new_sock);
							_ev.events=EPOLLIN | EPOLLET;
							_ev.data.fd=new_sock;
							if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,new_sock,&_ev)<0){
								perror("epoll_ctl");
								close(new_sock);
								continue;
							}
							continue;
						}
						//data_sock
						if(_ev_out[i].events & EPOLLIN){
							if(read_data(_fd,buf,sizeof(buf))==0){
								printf("client close...");
								epoll_ctl(epoll_fd,EPOLL_CTL_DEL,_fd,NULL);
							}
							printf("%s\n",buf);
						}
					}
				}
				break;
		}
	}
LABLE:
	close(epoll_fd);
	return 0;
}
