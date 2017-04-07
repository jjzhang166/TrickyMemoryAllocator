
#ifndef __BASE_MEM_MANAGER_H__
#define __BASE_MEM_MANAGER_H__

class BaseMemManager
{
public:
	BaseMemManager();
	virtual ~BaseMemManager();

	virtual void * malloc(int len) = 0;
	virtual void free(void * p) = 0;
};

#endif

