#ifndef __STD_MEM_MANAGER_H__
#define __STD_MEM_MANAGER_H__

#include "BaseMemManager.h"
class StdMemManager :
	public BaseMemManager
{
public:
	StdMemManager();
	virtual ~StdMemManager();

	virtual void * malloc(int len);
	virtual void free(void * p);

};

#endif




