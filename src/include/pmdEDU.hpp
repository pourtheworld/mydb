#ifndef PMDEDU_HPP_
#define PMDEDU_HPP_

#include "core.hpp"
#include "pmdEDUEvent.hpp"
#include "ossQueue.hpp"
#include "ossSocket.hpp"

#define PMD_INVALID_EDUID 0//我们规定EDU的ID从1开始
#define PMD_IS_EDU_CREATING(x)	(PMD_EDU_CREATING==x)//是否正在创建
#define PMD_IS_EDU_RUNNING(x)	(PMD_EDU_RUNNING==x)//是否正在运行
#define PMD_IS_EDU_WAITING(x)	(PMD_EDU_WAITING==x)//是否正在等待
#define PMD_IS_EDU_IDLE(x)	(PMD_EDU_IDLE==x)//是否正在空闲
#define PMD_IS_EDU_DESTROY(x)	(PMD_EDU_DESTROY==x)//是否正在销毁

typedef unsigned long long EDUID;

//EDU类型，实际上只有2种：TCP监听以及Agent代理
enum EDU_TYPES
{
	EDU_TYPE_TCPLISTENER=0,
	EDU_TYPE_AGENT,

	EDU_TYPE_UNKNOWN,
	EDU_TYPE_MAXIMUM=EDU_TYPE_UNKNOWN
};

enum EDU_STATUS
{
	PMD_EDU_CREATING=0,
	PMD_EDU_RUNNING,
	PMD_EDU_WAITING,
	PMD_EDU_IDLE,
	PMD_EDU_DESTROY,
	PMD_EDU_UNKNOWN,
	PMD_EDU_STATUS_MAXIMUM=PMD_EDU_UNKNOWN
};

class pmdEDUMgr;

class pmdEDUCB
{
public:
	pmdEDUCB(pmdEDUMgr *mgr,EDU_TYPES type);
	inline EDUID getID(){	return _id;	}
	//将待处理事件push到队列中，等待EDU处理
	inline void postEvent(pmdEDUEvent const &data){	_queue.push(data);	}
	bool waitEvent(pmdEDUEvent &data,long long millsec)
	{
		bool waitMsg=false;//是否得到了队列中pop出的事件
		//如果当前EDU状态不为空闲，将其状态设置为等待
		if(PMD_EDU_IDLE!=_status)	_status=PMD_EDU_WAITING;
		if(0>millsec)
		{//如果当前设置时间小于0，则无限等待，从事件队列里pop出一个事件给data
			_queue.wait_and_pop(data);
			waitMsg=true;
		}//给定超时时间，在超时时间内是否pop出事件
		else	waitMsg=_queue.timed_wait_and_pop(data,millsec);
		
		if(waitMsg)//如果pop出了事件
		{	//事件为终止，则断开连接
			if(data._eventType==PMD_EDU_EVENT_TERM) _isDisconnected=true;
			else _status=PMD_EDU_RUNNING;//否则，状态调整为运行
		}
		return waitMsg;
	}
	inline void force(){	_isForced=true;	}//一个线程迫使其它线程退出的标志
	inline void disconnect(){	_isDisconnected=true;}
	inline EDU_TYPES getType(){	return _type;	}
	inline EDU_STATUS getStatus(){	return _status;	}
	inline void setType(EDU_TYPES type){	_type=type;}
	inline void setID(EDUID id){	_id=id;}
	inline void setStatus(EDU_STATUS status){	_status=status;}
	inline bool isForced(){	return _isForced;}
	inline pmdEDUMgr* getEDUMgr(){	return _mgr;}
private:
	EDU_TYPES _type;
	pmdEDUMgr *_mgr;
	EDU_STATUS _status;
	EDUID _id;
	bool _isForced;
	bool _isDisconnected;
	ossQueue<pmdEDUEvent> _queue;
};

//返回值为int，形参为EDU,传入参数的函数指针
typedef int (*pmdEntryPoint)(pmdEDUCB*,void*);
//返回值为上面的函数指针，传入值为EDU的类型，根据类型来定义函数指针
pmdEntryPoint getEntryFuncByType(EDU_TYPES type);

int pmdAgentEntryPoint(pmdEDUCB *cb,void *arg);//Agent入口
int pmdTcpListenerEntryPoint(pmdEDUCB *cb,void *arg);
int pmdEDUEntryPoint(EDU_TYPES type,pmdEDUCB *cb,void *arg);

int pmdRecv(char *pBuffer,int recvSize,ossSocket *sock,pmdEDUCB *cb);
int pmdSend(const char *pBuffer,int sendSize,ossSocket *sock,pmdEDUCB *cb);

#endif
