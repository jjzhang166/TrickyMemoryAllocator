
#ifndef __GREEDY_MEM_MANAGER_H__
#define __GREEDY_MEM_MANAGER_H__



#include "BaseMemManager.h"

typedef unsigned char byte;

#define SafeDeleteObj(obj) \
{\
if (obj)\
{\
	delete obj;\
	obj = NULL;\
}\
}



#if WIN32
#include <windows.h>

typedef unsigned int tick_time;

struct CriticalSection
{
	CriticalSection()
	{
		InitializeCriticalSection(&cs);
	}
	~CriticalSection() { DeleteCriticalSection(&cs); }
	void lock() { EnterCriticalSection(&cs); }
	void unlock() { LeaveCriticalSection(&cs); }
	bool trylock() { return TryEnterCriticalSection(&cs) != 0; }

	CRITICAL_SECTION cs;
};


#elif linux

#include <pthread.h>

typedef unsigned long tick_time;

tick_time GetTickCount();


struct CriticalSection
{
	CriticalSection()
	{
		pthread_mutex_init(&m_mutex, NULL);
	}
	~CriticalSection() { pthread_mutex_destroy(&m_mutex); }
	void lock() { pthread_mutex_lock(&m_mutex); }
	void unlock() { pthread_mutex_unlock(&m_mutex); }
	//bool trylock() { return TryEnterCriticalSection(&cs) != 0; }

	pthread_mutex_t m_mutex;
};

#endif





#define GREEDY_MALLOC_ALIGN 16 //�ڴ����

const unsigned int GREEDY_MEM_MANAGE_MIN_BLOCK_SIZE = 64; //��С�����ڴ�飬��������С�򲻻��棬ֱ���ɲ���ϵͳ������ͷ�

const unsigned int GREEDY_MEM_MANAGE_MAX_BLOCK_SIZE = 1024 * 1024 * 1024 * 1; //��󻯵��ڴ�飬�����������򲻻��棬ֱ���ɲ���ϵͳ������ͷ�

//�н��л����ٽ費��
const unsigned int GREEDY_KEEP_IDLE_MEM_SECONDS = 15; //30��û�б����õ��ڴ潫�黹��ϵͳ

const unsigned int GREEDY_CLEAN_IDLE_MEM_TIMER_INTERVAL = 2000;//��λ����,��������ڴ涨ʱ����ʱ��

//#define MAX_TOTAL_REUSE_COUNT  2147438647l //ͳ�������ʵ�ʱ������������˴����ƣ���������롣

struct GreedyMemBlock;


//�Լ�ʵ�ֵļ�˫������ڵ㣨��stl��ࣩܶ
typedef struct SimpleListNode
{
	SimpleListNode(void * data)
	{
		this->next = NULL;
		this->pre = NULL;
		this->data = data;
	}

	SimpleListNode()
	{
		next = NULL;
		pre = NULL;
		data = NULL;
	}

	SimpleListNode * next;

	SimpleListNode * pre;

	void * data;

}SimpleListNode;


//�Լ�ʵ�ֵļ�˫��������stl��ࣩܶ
typedef struct SimpleList
{
	int len;

	SimpleListNode * front;

	SimpleListNode * end;

	SimpleList()
	{
		len = 0;
		front = NULL;
		end = NULL;
	};



	void erase(SimpleListNode * node)
	{
        SimpleListNode * nodePre = node->pre;
        
        
        SimpleListNode * nodeNext = node->next;

        if (node == NULL)
		{
			return;
		}

		
	

		node->pre = NULL;

		node->next = NULL;

		
		if (nodePre)
		{
			nodePre->next = nodeNext;
		}
		else
		{
			front = nodeNext;
		}


		if (nodeNext)
		{
			nodeNext->pre = nodePre;
		}
		else
		{
			end = nodePre;
		}



		len--;
	}

	SimpleListNode * push_back_data(void * data)
	{
		return push_back_node(new SimpleListNode(data));
	}


	SimpleListNode * push_back_node(SimpleListNode * node)
	{
		node->pre = NULL;
		node->next = NULL;


		if (len == 0)
		{
			front = node;
			end = node;
		}
		else
		{
			end->next = node;
			node->pre = end;
			end = node;
		}
		len++;

		return node;
	};

	SimpleListNode * push_front_data(void * data)
	{
		return push_front_node(new SimpleListNode(data));
	};

	SimpleListNode * push_front_node(SimpleListNode * node)
	{

		node->pre = NULL;
		node->next = NULL;

		if (len == 0)
		{
			front = node;
			end = node;
		}
		else
		{
			front->pre = node;
			node->next = front;
			front = node;
		}

		len++;

		return node;
    };


    SimpleListNode *pop_back_node()
    {
        
        SimpleListNode * node;
        
        if (len == 0)
        {
            return NULL;
        }
        else if (len == 1)
        {
            node = end;
            end = front = NULL;
        }
        else
        {
            node = end;
            end = end->pre;
            end->next = NULL;
        }
        len--;
        node->pre = NULL;
        
        node->next = NULL;
        
        return node;
    };
    
    void * pop_back_data()
	{
		SimpleListNode * node = pop_back_node();
		if (node == NULL)
		{
			return NULL;
		}
		else
		{
			void * data = node->data;
			SafeDeleteObj(node);
			return data;
		}
	};



    SimpleListNode * pop_front_node()
    {
        
        SimpleListNode * node;
        
        if (len == 0)
        {
            return NULL;
        }
        else if (len == 1)
        {
            node = front;
            
            front = end = NULL;
        }
        else
        {
            node = front;
            
            front = front->next;
            
            front->pre = NULL;
        }
        
        len--;
        
        node->pre = NULL;
        node->next = NULL;
        
        return node;
    };
    
    
    void * pop_front_data()
    {
        
		SimpleListNode * node = pop_front_node();
		if (node == NULL)
		{
			return NULL;
		}
		else
		{
			void * data = node->data;
			SafeDeleteObj(node);
			return data;
		}
	};

	
}SimpleList;


//�ڴ��
typedef struct GreedyMemBlock
{
	GreedyMemBlock(unsigned int lastReadyTime, void * p, int lenIndex) :lastReadyTime(lastReadyTime), p(p), lenIndex(lenIndex){
		nodeInReadyArray = new SimpleListNode(this);
	};

	~GreedyMemBlock() { 

		SafeDeleteObj(nodeInReadyArray);
		byte* udata = (byte *)p - 1 - sizeof(GreedyMemBlock *);
	    ::free(udata);
	};
	//���黹ʱ�䣬�����޶�û�б����ý��ᱻ����
	tick_time lastReadyTime;

	void * p;

	int lenIndex;

	SimpleListNode * nodeInReadyArray;

}GreedyMemBlock;

typedef struct LenAndBlocks
{
	unsigned int len;
	SimpleList  blockArray;

	LenAndBlocks()
	{
		len = -1;
	};

}LenAndBlocks;


#define MANAGER_MEM_SIZES_COUNT  3000

class GreedyMemManager : public BaseMemManager
{
public:
	GreedyMemManager();
	virtual ~GreedyMemManager();

	virtual void * malloc(int len);

	virtual void free(void * p);

	void onCleanTimerFired(); //�������ĳ�ʱ��û�б����õ��ڴ�

	int getLenIndex(unsigned int mallocSize);

	LenAndBlocks* getLenAndBLocks(unsigned int mallocSize);

	//double reuseBlockHitRate();

	//void cleanReuseBlockHitRateData();

	CriticalSection csLock;




	//long totalMallocCount;

	//long blockReuseCount;

#if WIN32
	MMRESULT timer_id;
#endif

#if linux
	pthread_t timer_id;

	void sleepAndFreeUnreusedMemory();
#endif

private:

	void initManageSizes();

	LenAndBlocks manageSizes[MANAGER_MEM_SIZES_COUNT];



};

extern GreedyMemManager gGreedyMemManager;

#endif
