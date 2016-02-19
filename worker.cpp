/**
	author 445297005@qq.com
*/
#include "worker.h"
#include "session.h"
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include "common.h"

CWorker::CWorker()
{
	_base = NULL;
	pthread_mutex_init( &_inmutex, NULL );
	pthread_cond_init( &_incond, NULL );

	pthread_mutex_init( &_outmutex, NULL );
	pthread_cond_init( &_outcond, NULL );
	_mainStop = false;
}

CWorker::~CWorker()
{
	_mainStop = true;
	for (int i = 0; i < _childThreadIds.size(); ++i)
	{
		pthread_kill(_childThreadIds[i],-1);
	}

	std::map<int,CSession*>::const_iterator iter;
	for (iter = _sessionMap.begin(); iter != _sessionMap.end(); ++iter)
	{
		CSession *session = iter->second;
		delete session;
		session = NULL;
	}

	std::deque<SBuffer>::const_iterator iter1;
	for (iter1 = _inBufferList.begin(); iter1 != _inBufferList.end(); ++iter1)
	{
		SBuffer buffer = *iter1;
		free(buffer.buf);
	}

	event_del(&_socketEvent);
	event_del(&_notifyEvent);
	event_base_free(_base);
	close(_notifyFd);
	close(_controlFd);

	std::deque<SBuffer>::const_iterator iter2;
	for (iter2 = _outBufferList.begin(); iter2 != _outBufferList.end(); ++iter2)
	{
		SBuffer buffer = *iter2;
		free(buffer.buf);
	}

	_sessionMap.clear();

	pthread_mutex_destroy( &_inmutex );
	pthread_cond_destroy( &_incond );

	pthread_mutex_destroy( &_outmutex );
	pthread_cond_destroy( &_outcond );
}

int CWorker::AddEvent(CSession *session,short events,int fd)
{
	int ret = 0;

	if( events & EV_WRITE )
	{
		if( fd < 0 ) fd = EVENT_FD( session->GetWriteEvent() );

        event_del ( session->GetWriteEvent() ) ;
		event_set( session->GetWriteEvent(), fd, events, CSession::WriteProc, session );
		//event_base_set( session->GetEventBase(), session->GetWriteEvent() );
		event_base_set( _base, session->GetWriteEvent() );

		struct timeval timeout;
		memset( &timeout, 0, sizeof( timeout ) );
		timeout.tv_sec = 5;//eventArg->GetTimeout();
		ret = event_add( session->GetWriteEvent(), &timeout );
		if ( (0 > fd) || (0 != ret) )
		{
			printf( "ADDEVENT: fd %i, write event add failed %i errno %d(%s) ", fd, ret, errno, strerror(errno) ) ;
		}
	}

	if( events & EV_READ )
	{
		if( fd < 0 ) fd = EVENT_FD( session->GetReadEvent() );

        event_del ( session->GetReadEvent() ) ;
		event_set( session->GetReadEvent(), fd, events, CSession::ReadProc, session );
		//event_base_set( session->GetEventBase(), session->GetReadEvent() );
		event_base_set( _base, session->GetReadEvent() );

		struct timeval timeout;
		memset( &timeout, 0, sizeof( timeout ) );
		timeout.tv_sec = 5;//eventArg->GetTimeout();
		ret = event_add( session->GetReadEvent(), &timeout );
		if ( (0 > fd) || (0 != ret) )
		{
			printf( "ADDEVENT: fd %i, read event add failed %i errno %d(%s) ", fd, ret, errno, strerror(errno)  ) ;
		}
	}

	/* proc time out event */
	if ( 0 == events )
	{
		//定时器
		if( fd < 0 ) fd = EVENT_FD( session->GetReadEvent() );

        event_del ( session->GetTimeoutEvent() ) ;
		event_set( session->GetTimeoutEvent(), -1, events, CSession::TimeoutProc, session );
		//event_base_set( session->GetEventBase(), session->GetTimeoutEvent() );
		event_base_set( _base, session->GetTimeoutEvent() );

		struct timeval timeout;
		memset( &timeout, 0, sizeof( timeout ) );
		timeout.tv_sec = TIMEOUT_SECOND;//eventArg->GetTimeout();//300秒
		ret = event_add( session->GetTimeoutEvent(), &timeout );
		if ( (0 > fd) || (0 != ret) )
		{
			printf( "ADDEVENT: fd %i timeout event add failed %i errno %d(%s) ", fd, ret, errno, strerror(errno)  ) ;
		}
	}

	return ret;
}

void CWorker::GetSocket(int sock, short event, void* args)
{
	CWorker *worker = (CWorker *)args;
	worker->onGetSocket();
}

void CWorker::onGetSocket()
{
    // 获取连接 fd
#if 1
    char c = '\0';
    struct iovec iov;
    iov.iov_base = &c;
    iov.iov_len = 1;
#endif

#if 0
    char c[100];
    struct iovec iov;
    iov.iov_base = c;
    iov.iov_len = 100;
#endif
    char _buf[2048];
    struct msghdr msg = { 0 };
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_controllen = sizeof(_buf);
    msg.msg_control = _buf;

    ssize_t s = ::recvmsg(_acceptsocket, &msg, msg.msg_flags);	

    if (s < 0) {
        printf("recvmsg %d %s", s, strerror(errno));
        return ;
    }

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == 0) {
        printf("cmsghdr null");
        return ;
    }

    if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
        printf("ERR: invalid cmsg");
        return ;
    }

    int n = (cmsg->cmsg_len - ((char*)CMSG_DATA(cmsg) - (char*)cmsg));
    if (n % sizeof(int) != 0) {
        printf("ERR: invalid cmsg len");
        return ;
    }

   printf("CWorker::handleRead cmsg->cmsg_len %d , n %d ",cmsg->cmsg_len,n);

    n /= sizeof(int);

    // 生成客户端处理器
    int* fds = (int*)CMSG_DATA(cmsg);
    for (int i = 0; i < n; ++i)
    {
    	CSession *session = new CSession();
    	session->Init(_base,this);
    	int iRet = AddEvent(session,EV_READ,fds[i]) ;
		if ( 0 != iRet )
		{
			delete session, session = NULL ;
			close ( fds[i] ) ;
			fds[i] = -1 ;
		}
		else
		{
			AddSessionMap(session);
		}
    }
}

void CWorker::Notify(int sock, short event, void* args)
{
	CWorker *worker = (CWorker *)args;
	worker->OnNotify();
}

void CWorker::OnNotify()
{
	printf("come func:%s\n",__func__);
	int ret=0;
	char data[1024];
	ret = read ( _notifyFd, (char *)data, sizeof(data) );
	if ( (0 > ret) && (errno != EAGAIN) )
	{
		printf ( "NOTIFY: read data failed fd %i size %i errno %i '%s'", _notifyFd, ret, errno, strerror(errno) );
	}
	printf("func:%s recv %d bytes _outBufferList.size:%d\n",__func__,ret,_outBufferList.size());

	if ( _outBufferList.size() == 0)
	{
		return ;
	}
	
	
	SBuffer buffer = _outBufferList.front();
	_outBufferList.pop_front();
		
	CSession *session = NULL;
	//从_sessionMap找到session
	std::map<int,CSession *>::iterator iter ;
	printf("func:%s id:%d socket:%d buffer.len:%d \n",__func__,buffer.id,buffer.socket,buffer.len);
	iter = _sessionMap.find(buffer.id);
	if(iter != _sessionMap.end())
	{
		session = iter->second;
		printf("find it\n");
		session->AddWriteBuffer(buffer.buf,buffer.len);
		free(buffer.buf);
	    int iRet = AddEvent ( session, EV_WRITE, session->GetFd() ) ;
    }
    else
    	printf("not find it\n");

}

void CWorker::RemoveSession(CSession *session)
{
	std::map<int,CSession *>::iterator iter ;
	printf("func:%s \n",__func__);
	iter = _sessionMap.find(session->GetFd());
	if(iter != _sessionMap.end())
	{
		session = iter->second;
		delete session;
		printf("find it\n");
		_sessionMap.erase(iter);
    }}

void CWorker::OnMainThrRun ()
{
		/* main application loop */
	{
		while ( !_mainStop  )
		{
			if(_mainStop)
			{
				printf("EXIT:%s:%s:%d receive signal",__FILE__,__func__,__LINE__);
				break;
			}
			event_base_loop ( _base, EVLOOP_ONCE );
		}
	}

	/* release */
	{
		pthread_exit ( NULL ) ;
	}
}

void *CWorker::MainThrRun ( void *args )
{
	CWorker *worker = (CWorker *)args;
	worker->OnMainThrRun();
}

void *CWorker::ChildRun ( void *args )
{
	CWorker *worker = (CWorker *)args;
	worker->OnChildRun();
}

void CWorker::OnChildRun ( )
{
	while(1)
	{
		pthread_mutex_lock( &_inmutex );

		if/*while*/ ( _inBufferList.size() == 0 )
		{		
			pthread_cond_wait( &_incond, &_inmutex );	
		}

		if ( _inBufferList.size() == 0)
		{
			pthread_mutex_unlock( &_inmutex );
			return ;
		}
		
		
		SBuffer buffer = _inBufferList.front();
		_inBufferList.pop_front();
		//queueinfo();
			
		pthread_mutex_unlock( &_inmutex );

		printf("get buf：len:%d buf:%s\n",buffer.len,buffer);
		//handle
		Handle(buffer);

		char buf = 'Z';
		int ret = write(_controlFd,&buf,1);
		if(ret != 1)
		{
			if ( errno != EAGAIN )
			{
				printf ( "NOTIFY: write data failed fd %i size %i errno %i '%s'",
					_controlFd, ret, errno, strerror(errno) );
			}
		}
	}
}

void CWorker::Handle(SBuffer buffer)
{
	//这里就是处理输入的buffer，处理完后就塞进输出队列
	/***
		handle--------handle
	****/
	AddOutBufferList(buffer);
}

int CWorker::Init(unsigned int startId,unsigned int step,int socket,int threadNum)
{
	//unsigned long ul = 1;
	//int sRet = ioctlsocket(_listenFd,FIONBIO,(unsigned long*)&ul);
	//设置非阻塞
	_acceptsocket = socket;
	::fcntl(_acceptsocket, F_SETFL, O_NONBLOCK);
	//创建基事件
	_base = (struct event_base*)event_init();
	printf("_base:%X startId:%d\n",_base,startId);

	//设置回调函数.将event对象监听的socket托管给event_base,指定要监听的事件类型，并绑上相应的回调函数
    event_set(&_socketEvent, socket, EV_READ|EV_PERSIST, CWorker::GetSocket, this);//
    event_base_set(_base,&_socketEvent);
    event_add(&_socketEvent, NULL );

    int fd[2];
    if ( 0 != socketpair(AF_UNIX, SOCK_STREAM, 0, fd) )
	{
		printf ( "NOTIFY: create socketpair failed errno %i '%s'", errno, strerror(errno) );
		return -1 ;
	}

	int n = 65536;
	setsockopt(fd[0], SOL_SOCKET, SO_RCVBUF, &n, sizeof(n));
	fcntl(fd[0], F_SETFL, O_RDWR|O_NONBLOCK);

	n = 65536;
	setsockopt(fd[1], SOL_SOCKET, SO_RCVBUF, &n, sizeof(n));
	fcntl(fd[1], F_SETFL, O_RDWR|O_NONBLOCK);

	_notifyFd = fd[0]; /* fd used at epoll process side */
	_controlFd = fd[1]; /* fd used at working processes side */

	event_set(&_notifyEvent, _notifyFd, EV_READ|EV_PERSIST, CWorker::Notify, this);//
	event_base_set(_base,&_notifyEvent);
	event_add(&_notifyEvent, NULL );

	// worker threads
	for ( int j = 0; j < threadNum; j++ )
	{
		pthread_t tTid ;
		pthread_attr_t attr;
		pthread_attr_init(&attr);

		int iRet = pthread_attr_setstacksize(&attr, (size_t)2*1024*1024);
		if ( 0 != iRet )
		{
			printf ( "FATAL: pthread_attr_setstacksize failed for %s", strerror(errno) ) ;
		}

		iRet = pthread_create ( &tTid, &attr, CWorker::ChildRun, this ) ;
		if ( 0 != iRet )
		{
			printf ( "FATAL: Create %i thread failed, aborted...", j + 1 ) ;
			return -1;
		}

		pthread_detach ( tTid ) ;
		_childThreadIds.push_back(tTid);
	}


    pthread_t tThread ;
	pthread_attr_t attr;
	pthread_attr_init(&attr);

	pthread_attr_setstacksize(&attr, (size_t)2*1024*1024);
	pthread_create ( &tThread, &attr, MainThrRun, this ) ;
	pthread_detach ( tThread ) ;
	printf ( "#%lu --> main epoll thread %d begins to run ...\n", tThread, startId ) ;
}

//塞进输入队列，并触发信号，使处理线程进行处理
void CWorker::AddInBufferList(SBuffer buffer)
{
	_inBufferList.push_back(buffer);
	pthread_cond_signal( &_incond );
}

void CWorker::AddOutBufferList(SBuffer buffer)
{
	//唤醒处理线程
	pthread_mutex_lock( &_outmutex );
	_outBufferList.push_back(buffer);

	pthread_mutex_unlock( &_outmutex );

	pthread_cond_signal( &_outcond );
}

void CWorker::AddSessionMap(CSession * session)
{	
	printf("func:%s add session fd:%d\n",__func__,session->GetFd());
	_sessionMap.insert(std::map<int,CSession*>::value_type(session->GetFd(),session));
}
