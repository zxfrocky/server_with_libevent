/**
	author 445297005@qq.com
*/
#ifndef __COMMON_H__
#define __COMMON_H__

typedef struct SBuffer
{
	int id;
	int socket;
	int len;
	char *buf;
}SBuffer;
#endif