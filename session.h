#ifndef __SESSION_H__
#define __SESSION_H__
#include <event.h>

class CWorker;

class CSession
{
public:
	CSession();
	~CSession();
	struct event *GetReadEvent();
	struct event *GetWriteEvent();
	struct event *GetTimeoutEvent();
	struct event_base *GetEventBase();
	void AddWriteBuffer(char *buf,int buflen);
	int GetFd();
	void SetEventBase(struct event_base *eventbase);

	static void ReadProc(int fd,short events,void *arg);
	static void WriteProc(int fd,short events,void *arg);
	static void TimeoutProc(int fd,short events,void *arg);
	void OnReadProc();
	void OnWriteProc();
	void OnTimeoutProc();
	void SetWorker(CWorker *worker);
	int Send(int s, const void *msg, size_t len, int flags);
	int Recv(int s, void *buf, size_t len, int flags);
	int Init(struct event_base *eventbase,CWorker *worker);
	bool IsCompletePacket();
private:
	char *_readbuffer;
	int _readbufferlen;
	int _realbufferlen;
	char *_writebuffer;
	int _writebufferlen;
	struct event _eventRead ;
	struct event _eventWrite ;
	struct event _eventTimeout ;
	struct event_base *_eventbase; 
	CWorker * _worker;
};
#endif