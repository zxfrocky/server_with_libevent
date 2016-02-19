#include "workermanager.h"

int main()
{
	CWorkerManager *manager = new CWorkerManager();
	manager->Init(INADDR_ANY,10000,2,3);
	manager->Start();
	return 0;
}