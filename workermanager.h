#ifndef __workermanager_h__
#define __workermanager_h__
#include <vector>
#include "worker.h"

class CWorkerManager
{
public:
	CWorkerManager();
	~CWorkerManager();
	/**ip--监听ip listenPort--监听端口 workerNum----worker 数量 threadNum--每个worker的处理线程数*/
	int Init(unsigned int ip,int listenPort,int workerNum,int threadNum);
	/**启动*/
	int Start();
	/**将accept到的socket*/
	int PutSocket(int idx,int* fd, int n);
private:
	/**sockpair 保存的对象*/
	int **_fd;
	int _workerCount;
	int _listenFd;
	std::vector<CWorker *> _workers;
	bool _stop;
	int _listenPort;
	unsigned int _listenIp;
};
#endif