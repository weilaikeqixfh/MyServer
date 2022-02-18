#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

#define MAX_FD 65536           //最大文件描述符
#define MAX_EVENT_NUMBER 10000 //epoll监听的最大事件数

//设置信号的处理函数
void addsig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);//将所有信号添加到信号集
    assert(sigaction(sig, &sa, NULL) != -1);//设置信号处理方式；绑定
}


int main(int argc,char* argv[])
{
	if (argc<=1)
          {   
                printf("Using:./server port\nExample:./server 5005\n\n"); exit(-1); 
          }
	int port = atoi(argv[1]);
	 /******设置信号SIGPIPE的处理操作*******/
        //默认读写一个关闭的socket会触发sigpipe信号 该信号的默认操作是关闭进程.我们不希望因为一个错误的读操作关闭进程
        //所以我们需要重新设置sigpipe的信号回调操作函数   比如忽略操作等  使得我们可以防止调用它的默认操作 
        //信号的处理是异步操作  也就是说 在这一条语句以后继续往下执行中如果碰到信号依旧会调用信号的回调处理函数	
	addsig(SIGPIPE, SIG_IGN);//设置信号的处理回调函数 这个SIG_IGN宏代表的操作就是忽略该信号
	threadpool<http_conn> *pool = NULL;
    	try
    	{
        	pool = new threadpool<http_conn>;
   	 }
    	catch (...)
   	{
        	exit(-1);
   	 }

	//创建一个数组用于保存所有的客户端信息
	http_conn *users = new http_conn[MAX_FD];
	assert(users);

	struct sockaddr_in serveraddr;
        bzero(&serveraddr,sizeof(serveraddr));
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);//任意IP地址；
        serveraddr.sin_port = htons(port);//指定通信端口；
	//创建监听socket文件描述符    
        int listenfd = socket(PF_INET,SOCK_STREAM,0);
        assert(listenfd!=-1);
	
	//设置端口复用， 消除bind时"Address already in use"错误
	int flag = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

	//绑定socket和它的地址
        int ret=0;
        ret=bind(listenfd,(struct sockaddr*)&serveraddr,sizeof(serveraddr));
        assert(ret!=-1);
	//创建监听队列以存放待处理的客户连接；
        ret=listen(listenfd,5);
        assert(ret!=-1);

	// 用于存储epoll事件表中就绪事件的event数组
	epoll_event events[MAX_EVENT_NUMBER];
	//创建epoll内核事件表
        int epollfd=epoll_create(20);
	assert(epollfd != -1);
	//将监听的文件描述符添加到epoll对象中
	addfd(epollfd, listenfd, false);
	http_conn::m_epollfd = epollfd;
	
	while(true)
	{
	int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        	if (number < 0 && errno != EINTR){
            		printf("epoll failure\n");
            		break;
      		}
	//循环遍历就绪事件数组
		for (int i = 0; i < number; i++)
        	{
            		int sockfd = events[i].data.fd;

			//处理新到的客户连接
           		 if (sockfd == listenfd){
        	        struct sockaddr_in client_address;
	                socklen_t client_addrlength = sizeof(client_address);
			int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
			//printf("新的客户连接\n");
				if (http_conn::m_user_count >= MAX_FD){
					//目前连接数满了
					//给客户端回复一个信息，服务器正忙
                    			//show_error(connfd, "Internal server busy");
                    			printf("Internal server busy");
				
                    			break;
               			}
				addfd(epollfd,connfd,true);
				//将新的客户数据初始化，并放到客户端数组中保存
				users[connfd].init(connfd, client_address);
			}

			 else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                		//对方异常连接、断开等错误事件，服务器端关闭连接
				 users[sockfd].close_conn();
           		}
			 else if (events[i].events & EPOLLIN){
                		if (users[sockfd].read_once()){
                   		 //若监测到读事件，一次性读完，将该事件放入请求队列
                    		pool->append(users + sockfd);
				}else{
					users[sockfd].close_conn();//读失败，关闭连接
				}
			 }
			 else if (events[i].events & EPOLLOUT){
				 if (!users[sockfd].write())//一次性写完如果写失败，关闭连接
				 {    users[sockfd].close_conn(); }
			 }
		}
	}

	
	close(epollfd);
	close(listenfd);
	delete[] users;
	delete pool;
	return 0;
}
