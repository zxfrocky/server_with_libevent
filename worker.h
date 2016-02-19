/**
	author 445297005@qq.com
*/
#ifndef __WORKER_H__
#define __WORKER_H__
#include "stdio.h"
#include "stdlib.h"
#include "common.h"
#include "session.h"
#include <map>
#include <vector>
#include <queue>

#define TIMEOUT_SECOND 60//超时多久没发数据(心跳)

class CWorker
{
public:
	CWorker();
	~CWorker();
	/**初始化step--表示有多少个worker socket--notifyFd threadNum--worker下面的处理线程数*/
	int Init(unsigned int startId,unsigned int step,int socket,int threadNum);
	/**启动*/
	void Start();
	/**给notifyFd发系统消息*/
	int Send(int *fd,int n);
	/**将受到的buffer添加到输入队列*/
	void AddInBufferList(SBuffer buffer);
	/**将处理完的buffer添加到输出队列*/
	void AddOutBufferList(SBuffer buffer);
	/**添加session至map*/
	void AddSessionMap(CSession *);
	/**添加事件至session*/
	int AddEvent(CSession *session,short events,int fd);
	/**删除某个session*/
	void RemoveSession(CSession *session);
	/**处理输入的buffer*/
	void Handle(SBuffer buffer);
protected:
	/**主事件循环 libevent主事件循环*/
	static void *MainThrRun(void *args);
	void OnMainThrRun();
	/**处理子进程*/
	static void *ChildRun ( void *args );
	void OnChildRun();
	/**_notifyFd事件处理*/
	static void Notify(int sock, short event, void* arg);
	void OnNotify();
	/**_acceptsocket事件处理*/
	static void GetSocket(int sock, short event, void* arg);
	void onGetSocket();
private:
	struct event_base* _base;
	struct event _socketEvent;
	struct event _notifyEvent;
	pthread_cond_t _incond;
	pthread_mutex_t _inmutex;
	pthread_cond_t _outcond;
	pthread_mutex_t _outmutex;
	int _acceptsocket;
	int _notifyFd;
	int _controlFd;
	std::deque<SBuffer> _inBufferList;//单向队列
	std::deque<SBuffer> _outBufferList;//单向队列
	std::map<int,CSession*> _sessionMap;
	std::vector<int>_childThreadIds;
	bool _mainStop;
};
#endif

