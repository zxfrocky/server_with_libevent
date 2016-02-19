/**
	author 445297005@qq.com
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <unistd.h> 
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "workermanager.h"
#include "worker.h"

CWorkerManager::CWorkerManager()
{
	_fd = NULL;
	_stop = false;
}

CWorkerManager::~CWorkerManager()
{
	for (int i = 0; i < _workerCount; ++i)
	{
		delete _workers[i];
		_workers[i] = NULL;
	}

	for (int i = 0; i < _workerCount; ++i)
	{
		delete []_fd[i];
	}

	delete []_fd;

	_stop = true;
}

/**ip--监听ip listenPort--监听端口 workerNum----worker 数量 threadNum--每个worker的处理线程数*/
int CWorkerManager::Init(unsigned int ip,int listenPort,int workerNum,int threadNum)
{
	_workerCount = workerNum;
	_listenIp = ip;
	_listenPort = listenPort;
	_fd = new int*[_workerCount];
	for (int i = 0; i < _workerCount; ++i)
	{
		_fd[i] = new int[2];
		socketpair(AF_UNIX, SOCK_STREAM, 0, _fd[i]);
		CWorker *worker = new CWorker();
		worker->Init(i,_workerCount,_fd[i][1],threadNum);
		_workers.push_back(worker);
	}

	return 0;
}

int CWorkerManager::PutSocket(int idx,int* fd, int n)
{
    char c = '\0';
    struct iovec iov;
    iov.iov_base = &c;
    iov.iov_len = 1;

    char buf[CMSG_SPACE(sizeof(int) * n)];
    struct msghdr msg  = { 0 };
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);
    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * n);
    memcpy(CMSG_DATA(cmsg), fd, sizeof(int) * n);

    //ssize_t s = ::sendmsg(_fd, &msg, 0);
    ssize_t s = ::sendmsg(_fd[idx][0], &msg, 0);
    if (s < 0) {
        printf("sendmsg %d %s", s, strerror(errno));
        return -1;
    }

    return 0;
}

int CWorkerManager::Start()
{
	_listenFd = socket(AF_INET,SOCK_STREAM,0);
	int yes = 1;
	setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	unsigned long ul = 1;
	int sRet = ioctl(_listenFd,FIONBIO,(unsigned long*)&ul);
	struct sockaddr_in my_addr;
	memset(&my_addr, 0, sizeof(my_addr));
	    //实例化对象的属性
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(_listenPort);
    //my_addr.sin_addr.s_addr = INADDR_ANY;
    my_addr.sin_addr.s_addr = _listenIp;

    //将套接字地址和套接字描述符绑定起来
    bind(_listenFd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr));
    //监听该套接字，连接的客户端数量最多为BACKLOG
    listen(_listenFd, 5);

    int ep = epoll_create(8);
    struct epoll_event event;
    event.events = (uint32_t) (EPOLLIN | EPOLLOUT | EPOLLET);
    event.data.fd = _listenFd;
    epoll_ctl(ep, EPOLL_CTL_ADD, _listenFd, &event);
    struct epoll_event eventArr[8];

    socklen_t socketlen;
    int err = -1;
    socketlen = sizeof (err);

    while(!_stop)
    {
	    int n = epoll_wait(ep, eventArr, 8, 1000);//第四个参数是毫秒

	    for (int i = 0; i < n; i++) {
	        epoll_event ev = eventArr[i];
	        if (ev.data.fd == _listenFd && (ev.events & EPOLLIN)) {
	            while(1)
	            {
	            	struct sockaddr_in cli_addr;
				    int newfd;
				    socklen_t sin_size;
				    sin_size = sizeof(struct sockaddr_in);
	            	int newfd1 = accept(_listenFd, (struct sockaddr*)&cli_addr, &sin_size);//指定服务端去接受客户端的连接	
	            	if(newfd1 < 0)
	            		break;
	            	printf("accept a socket :%d\n",newfd1);
	            	static int idx = 0;
	            	PutSocket(idx++%_workerCount,&newfd1,1);
	            }
	        }
        }
    }

     ::close( ep );


	return 0;
}