/**
	author 445297005@qq.com
*/
#include <errno.h>
#include "session.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "worker.h"

#define ONE_PACKET_BUF_LEN 4096
CSession::CSession()
{
	_eventbase = NULL;
	_worker = NULL;
	_writebuffer = NULL;
	_readbuffer = NULL;
	_readbufferlen = 0;
	_realbufferlen = 0;
	_writebufferlen = 0;
}

CSession::~CSession()
{
	printf("func:%s fd:%d\n",__func__,GetFd());
	if(_writebuffer)
		free(_writebuffer);

	if(_readbuffer)
		free(_readbuffer);

	if(_eventbase){
		event_del(&_eventRead);
		event_del(&_eventWrite);
		event_del(&_eventTimeout);
	}

	close(GetFd());
}

int CSession::Init(struct event_base *eventbase,CWorker *worker)
{
	_eventbase = eventbase;
	_worker = worker;
	_readbufferlen = ONE_PACKET_BUF_LEN;
	_readbuffer = (char *)malloc(_readbufferlen);
	memset(_readbuffer,0,_readbufferlen);
	_realbufferlen = 0;
	return 0;
}

void CSession::SetEventBase(struct event_base *eventbase)
{
	_eventbase = eventbase;
}

void CSession::SetWorker(CWorker *worker)
{
	_worker = worker;
}

struct event *CSession::GetReadEvent()
{
	return &_eventRead;
}

struct event *CSession::GetWriteEvent()
{
	return &_eventWrite;
}

struct event *CSession::GetTimeoutEvent()
{
	return &_eventTimeout;
}

struct event_base *CSession::GetEventBase()
{
	return _eventbase;
}

void CSession::ReadProc(int fd,short events,void *arg)
{
	CSession *session = (CSession *)arg;
	session->OnReadProc();
}

void CSession::WriteProc(int fd,short events,void *arg)
{
	CSession *session = (CSession *)arg;
	session->OnWriteProc();
}

void CSession::TimeoutProc(int fd,short events,void *arg)
{
	CSession *session = (CSession *)arg;
	session->OnTimeoutProc();
}

int CSession::GetFd()
{
	int fd = EVENT_FD( GetReadEvent() );
	return fd;
}

bool CSession::IsCompletePacket()
{
	/*todo*/
	return true;
}

void CSession::OnReadProc()
{
	int fd = EVENT_FD( GetReadEvent() );
	int iRecvSize = Recv ( fd, _readbuffer, _readbufferlen - _realbufferlen, 0 ) ;
	printf("func:%s buf:%s iRecvSize:%d\n",__func__,_readbuffer,iRecvSize);
	if(iRecvSize <= 0)
	{
		_worker->RemoveSession(this);
		return;
	}

	_realbufferlen += iRecvSize;
	/*这里要分析是否收到的包是一个完整的包，不完整的话就继续收*/
	if(!IsCompletePacket())
	{
		_readbufferlen += ONE_PACKET_BUF_LEN;
		char *tmp_buf = (char *)malloc(_readbufferlen);
		memset(tmp_buf,0,_readbufferlen);
		memcpy(tmp_buf,_readbuffer,_realbufferlen);
		free(_readbuffer);
		_readbuffer = tmp_buf;
		_worker->AddEvent ( this, EV_READ, GetFd() ) ;
	}

	if(_readbufferlen > 65536)
	{
		printf("packet buf len greater than 65536,maybe something error\n");
		_worker->RemoveSession(this);
		return;
	}
	

	SBuffer buffer;
	buffer.id = fd;
	buffer.socket = fd;
	buffer.len = _realbufferlen;
	buffer.buf = (char *)malloc(_realbufferlen);
	memcpy(buffer.buf,_readbuffer,_realbufferlen);
	_worker->AddInBufferList(buffer);

	_realbufferlen = 0;
	memset(_readbuffer,0,_realbufferlen);
	/*ps 每次都是以4096字节开辟空间，到最后最大可能会达到65536，
		要是长时间没请求字节数没达到65536，可以free掉，重新从4096字节开始申请，
		节省点空间
	*/
	/**还是一问一答吧*/
}


void CSession::AddWriteBuffer(char *buf,int buflen)
{
	_writebuffer = (char *)malloc(buflen);
	_writebufferlen = buflen;
	memcpy(_writebuffer,buf,buflen);
}

void CSession::OnWriteProc()
{
	int fd = EVENT_FD( GetWriteEvent() );
	int ret = Send(fd,_writebuffer,_writebufferlen,0);
	if(ret <= 0)
	{
		_worker->RemoveSession(this);
		return;
	}

	memmove(_writebuffer,_writebuffer+ret,_writebufferlen - ret);
	_writebufferlen -= ret;
	if(_writebufferlen > 0)//没发完，继续发
	{
		_worker->AddEvent ( this, EV_WRITE, GetFd() ) ;
		return;
	}


	free(_writebuffer);
	_writebuffer = NULL;
	_writebufferlen = 0;
	_worker->AddEvent ( this, 0, GetFd() ) ;
	_worker->AddEvent ( this, EV_READ, GetFd() ) ;
}

void CSession::OnTimeoutProc()
{
	printf("func:%s \n",__func__);
	printf("%d seconds no recv any data from client,should close this socket todo\n",TIMEOUT_SECOND);
	//长时间未收到客户端的心跳，则关闭此socket
	//_worker->RemoveSession(this);
}

int CSession::Recv(int s, void *buf, size_t len, int flags)
{
	unsigned int iRead = 0;
	int	iLen = 0,
		iErrno = 0,
		iMaxTry = 0 ;

	timeval tBegin ;
	gettimeofday(&tBegin, NULL) ;

	while (iRead < len)
	{
		iLen = ::recv(s, (char *)buf + iRead, len - iRead, flags);
		if (iLen <= 0)
		{
			iErrno= errno;
			iMaxTry ++ ;
			if ( ((iErrno == EINTR || iErrno == EAGAIN)) && (iMaxTry < 3) && (iLen < 0) )
			{
				continue ;
			}
			else
			{
				/* error */
				printf( " socket %i, errno %i, len %i, iRead %i\n", s, iErrno, len, iRead ) ; 
				return -1;
			}
		}
		iRead += iLen;
		break ;
	}

	return iRead;
}

int CSession::Send(int s, const void *msg, size_t len, int flags)
{
	unsigned int iWrite = 0 ;
	int	iLen = 0,
		iErrno = 0 ;

	//timeval tBegin ;
	//gettimeofday(&tBegin, NULL) ;

	while (iWrite < len)
	{
		iLen = ::send(s, (char *)msg + iWrite, len - iWrite, flags);
		if (iLen <= 0)
		{
			iErrno= errno;
			if (iErrno == EINTR || iErrno == EAGAIN)
			{
				return iWrite;
			}
			else
			{
				/* error */
				printf( "SKEpollIO: socket %i, errno %i, len %i, iWrite %i", s, iErrno, len, iWrite ) ;
				return -1;
			}
		}
		iWrite += iLen;
	}

	return iWrite;
}