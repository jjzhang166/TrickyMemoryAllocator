#include "stdafx.h"
#include "GreedyMemManager.h"

GreedyMemManager gGreedyMemManager;

#if linux
#include <sys/time.h>

tick_time GetTickCount()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;

}

void * clearMemDataThread(void * arg)
{
    GreedyMemManager * manager = (GreedyMemManager *)arg;
    manager->sleepAndFreeUnreusedMemory();
    return NULL;
}

#endif


#if WIN32
void onGreadyCleanIdleMemTimerFired(UINT wTimerID, UINT msg, DWORD dwUser, DWORD dwl, DWORD dw2)
{

	gGreedyMemManager.onCleanTimerFired();

}
#endif


GreedyMemManager::GreedyMemManager()
{
    //totalMallocCount = 0;
    //
    //blockReuseCount = 0;
	initManageSizes();
#if WIN32
	timer_id = timeSetEvent(GREEDY_CLEAN_IDLE_MEM_TIMER_INTERVAL, 1, (LPTIMECALLBACK)onGreadyCleanIdleMemTimerFired, DWORD(this), TIME_PERIODIC);
#endif

#if linux

	if (pthread_create(&timer_id, NULL, clearMemDataThread, this) != 0)
	{
		printf("create  AnalysisDataThread failed\n");
	}

#endif
}


GreedyMemManager::~GreedyMemManager()
{
#if WIN32
	timeKillEvent(timer_id);
#endif

#if linux

	//todo 如何释放定时器线程
	pthread_cancel(timer_id);

#endif
}




void * GreedyMemManager::malloc(int len)
{


	//LenAndBlocks*  lenAndBLocks = getLenAndBLocks(len);
	int lenIndex = getLenIndex(len);
	if (lenIndex < 0)
	{
		byte* udata = (byte*)::malloc(len + 1);
		udata[0] = 0;
		return udata + 1;
	}
	else
	{

		//totalMallocCount += 1;

		//if (totalMallocCount > MAX_TOTAL_REUSE_COUNT)
		//{
		//	totalMallocCount = totalMallocCount / 2;
		//	blockReuseCount = (blockReuseCount - 1) / 2;

		//}

		LenAndBlocks*  lenAndBLocks = &(manageSizes[lenIndex]);
		csLock.lock();
		SimpleList * readyBlocks = &(lenAndBLocks->blockArray);
		//GREEDY_READY_BLOCKS * usingBlocks = usingSize2BlockMaps.find(blockLen)->second;


		if (readyBlocks->len > 0)
		{
	/*		blockReuseCount = blockReuseCount + 1;*/

			GreedyMemBlock * block = (GreedyMemBlock *)readyBlocks->pop_front_node()->data;
			//usingBlocks->push_back(block);
			//allReadyBlockes.erase(block->nodeInAllReady);
			csLock.unlock();
			//LOG_INFO(_T("get one block with len:%ld from idle, current idle array size:%d,using array size:%d, total idle array size:%d， oldest using mem:%hs"), block->len, readyBlocks->size(), usingBlocks->size(), allReadyBlockes.size(), pointer2Str(usingBlocks->front()->p).c_str());
			return block->p;
		}
		else
		{
			//LOG_INFO(_T("do malloc with len:%d"), blockLen);
			byte* udata = (byte*)::malloc(lenAndBLocks->len + 1 + sizeof(GreedyMemBlock *));
			byte* adata = udata + 1 + sizeof(GreedyMemBlock *);
			adata[-1] = 1;
			GreedyMemBlock *block = new GreedyMemBlock(-1, adata, lenIndex);

			//SimpleListNode * nodeInAllReady = new SimpleListNode(block);  block->nodeInAllReady = nodeInAllReady;
//csLock

			//usingBlocks->push_back(block);
			((GreedyMemBlock **)udata)[0] = block;
			csLock.unlock();
			//LOG_INFO(_T("create new block with len:%ld from idle, current idle array size:0, using array size:%d, total idle array size:%d, oldest using mem:%hs"), block->len, usingBlocks->size(), allReadyBlockes.size(), pointer2Str(usingBlocks->front()->p).c_str());
			return block->p;
		}
		
	}

	

}

void GreedyMemManager::free(void * p)
{
	byte *pTemp = (byte *)p;
	bool useSystemAllocator = (pTemp[-1] == 0);
	if (useSystemAllocator)
	{
		::free((pTemp -1));
	}
	else
	{
		byte* udata = pTemp - 1 - sizeof(GreedyMemBlock *);

		GreedyMemBlock *block = ((GreedyMemBlock **)udata)[0];
		block->lastReadyTime = GetTickCount() / 1000; //当前秒数。
		csLock.lock();


		LenAndBlocks*  lenAndBLocks = &(manageSizes[block->lenIndex]);
	
		lenAndBLocks->blockArray.push_back_node(block->nodeInReadyArray);

		csLock.unlock();

		//LOG_INFO(_T("save one block with len:%ld as idle, current idle array size:%d,using array size:%d, total idle array size:%d"), block->len, readyBlocks->size(), usingBlocks->size(), allReadyBlockes.size());
	}

}

#if linux
void GreedyMemManager::sleepAndFreeUnreusedMemory()
{

	while (true)
	{

		sleep(GREEDY_CLEAN_IDLE_MEM_TIMER_INTERVAL / 1000);
        this->onCleanTimerFired();
	}

};

#endif

void GreedyMemManager::onCleanTimerFired()//清理多余的长时间没有被重用的内存
{
	tick_time now = GetTickCount() / 1000;

		
	unsigned int cleanTimeSecond = now - GREEDY_KEEP_IDLE_MEM_SECONDS;
	csLock.lock();

	for (int i = 0; i < MANAGER_MEM_SIZES_COUNT; i++)
	{
		if (manageSizes[i].len <= 0)
		{
			break;
		}
	
		else
		{
			LenAndBlocks & lenAndBlocks = manageSizes[i];
			if (lenAndBlocks.blockArray.len > 0)
			{
				SimpleList & blockArrayTemp = lenAndBlocks.blockArray;

				SimpleListNode * nodeTemp = blockArrayTemp.end;
				while (nodeTemp != NULL)
				{
					SimpleListNode * nodePreTemp = nodeTemp->pre;
					GreedyMemBlock * block = (GreedyMemBlock *)nodeTemp->data;
					if (block->lastReadyTime <= cleanTimeSecond)
					{
						blockArrayTemp.erase(nodeTemp);
						SafeDeleteObj(block);
					}
					else
					{
						break;
					}

					nodeTemp = nodePreTemp;

				}
			}
		}
	




	}

	csLock.unlock();
}

int GreedyMemManager::getLenIndex(unsigned int mallocSize)
{
	if (mallocSize < GREEDY_MEM_MANAGE_MIN_BLOCK_SIZE || mallocSize > GREEDY_MEM_MANAGE_MAX_BLOCK_SIZE)
	{
		return -1;
	}

	int ret = -1;
	for (int i = 0; i < MANAGER_MEM_SIZES_COUNT; i++)
	{
		if (manageSizes[i].len <= 0)
		{
			break;
		}
		else if (manageSizes[i].len < mallocSize)
		{
			//coninue
		}
		else
		{

			ret = i;
			break;
		}
	}

	return ret;;
}

LenAndBlocks* GreedyMemManager::getLenAndBLocks(unsigned int mallocSize)
{
	LenAndBlocks * ret = NULL;

	if (mallocSize < GREEDY_MEM_MANAGE_MIN_BLOCK_SIZE || mallocSize > GREEDY_MEM_MANAGE_MAX_BLOCK_SIZE)
	{
		return ret;
	}

	for (int i = 0; i < MANAGER_MEM_SIZES_COUNT; i++)
	{
		if (manageSizes[i].len <= 0)
		{
			break;
		}
		else if (manageSizes[i].len < mallocSize)
		{
			//coninue
		}
		else
		{
			ret = &manageSizes[i];
			break;
		}
	}


	return ret;;
}

//
//double GreedyMemManager::reuseBlockHitRate()
//{
//	long totalMallocCountTemp = totalMallocCount;
//
//	long blockReuseCountTemp = blockReuseCount;
//
//	if (totalMallocCount == 0)
//	{
//		return 0.0;
//	}
//	else
//	{
//		return blockReuseCountTemp*1.0 / totalMallocCountTemp;
//	}
//}
//
//
//void GreedyMemManager::cleanReuseBlockHitRateData()
//{
//	totalMallocCount = 0;
//
//	blockReuseCount = 0;
//}

void GreedyMemManager::initManageSizes()
{
	for (int i = 0; i < MANAGER_MEM_SIZES_COUNT; i++)
	{
		manageSizes[i].len = -1;
	}

	//readySize2BlockMaps.clear();
	int iSiseIndex = 0;
	unsigned int size = GREEDY_MEM_MANAGE_MIN_BLOCK_SIZE;
	while (true)
	{
		manageSizes[iSiseIndex].len = size; iSiseIndex++;
		//readySize2BlockMaps.insert(GREEDY_READY_SIZE_BLOCK_MAP_VALUE_TYPE(size, new GREEDY_READY_BLOCKS()));
		//usingSize2BlockMaps.insert(GREEDY_READY_SIZE_BLOCK_MAP_VALUE_TYPE(size, new GREEDY_READY_BLOCKS()));

		unsigned int size2 = (size / 4) * 7;
		if (size2 > GREEDY_MEM_MANAGE_MAX_BLOCK_SIZE)
		{
			break;
		}
		else
		{
			//readySize2BlockMaps.insert(GREEDY_READY_SIZE_BLOCK_MAP_VALUE_TYPE(size2, new GREEDY_READY_BLOCKS()));
			//usingSize2BlockMaps.insert(GREEDY_READY_SIZE_BLOCK_MAP_VALUE_TYPE(size2, new GREEDY_READY_BLOCKS()));
			manageSizes[iSiseIndex].len = size; iSiseIndex++;
		}

		size = size * 2;
		if (size > GREEDY_MEM_MANAGE_MAX_BLOCK_SIZE)
		{
			break;
		}
		else
		{
			//continue
		}
	}




}
