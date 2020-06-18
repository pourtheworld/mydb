#ifndef PMDEDUMGR_HPP_
#define PMDEDUMGR_HPP_

#include "core.hpp"
#include "pmdEDU.hpp"
#include "ossLatch.hpp"
#include "ossUtil.hpp"  //包含一些睡眠函数 返回tid pid等

#define EDU_SYSTEM 0x01
#define EDU_USER 0x02
#define EDU_ALL (EDU_SYSTEM|EDU_USER)

class pmdEDUMgr
{
private:
    std::map<EDUID,pmdEDUCB*>   _runQueue;  //正在执行的EDU队列
    std::map<EDUID,pmdEDUCB*>   _idleQueue; //处于idle的EDU队列
    std::map<unsigned int,EDUID>    _tid_eduid_map; //tid与EDUID的映射

    ossSLatch _mutex;//虽然它叫mutex,但是我们定义的是读写锁

    EDUID _EDUID;    //64bit 递增

    std::map<unsigned int,EDUID> _mapSystemEDUS;    //系统类型EDU的map

    bool _isQuiesced;   //在关闭数据库时，数据库应该挂起将线程池锁定
    bool _isDestroyed;
public:
    pmdEDUMgr():_EDUID(1),_isQuiesced(false),_isDestroyed(false){}
    ~pmdEDUMgr(){reset();}
    void reset(){_destroyAll();}
    
    //从run和idle队列中获取EDU的总量
    unsigned int size()
    {
        unsigned int num=0;
        _mutex.get_shared();//只读
        num=(unsigned int)_runQueue.size()+(unsigned int)_idleQueue.size();
        _mutex.release_shared();
        return num;
    }

    unsigned int sizeRun ()
   {
      unsigned int num = 0 ;
      _mutex.get_shared () ;
      num = ( unsigned int ) _runQueue.size () ;
      _mutex.release_shared () ;
      return num ;
   }

   unsigned int sizeIdle ()
   {
      unsigned int num = 0 ;
      _mutex.get_shared () ;
      num = ( unsigned int ) _idleQueue.size () ;
      _mutex.release_shared () ;
      return num ;
   }

    unsigned int sizeSystem ()
   {
      unsigned int num = 0 ;
      _mutex.get_shared () ;
      num = _mapSystemEDUS.size() ;
      _mutex.release_shared () ;
      return num ;
   }

   EDUID getSystemEDU(EDU_TYPES edu)
   {
       EDUID eduID=PMD_INVALID_EDUID;
       _mutex.get_shared();
       std::map<unsigned int,EDUID>::iterator it=_mapSystemEDUS.find(edu);
       if(it!=_mapSystemEDUS.end()) eduID=it->second;
       _mutex.get_shared();
       return eduID;
   }

   bool isSystemEDU ( EDUID eduID )
   {
      bool isSys = false ;
      _mutex.get_shared () ;
      isSys = _isSystemEDU ( eduID ) ;
      _mutex.release_shared () ;
      return isSys ;
   }

    //根据类型将EDU注册进map
   void regSystemEDU ( EDU_TYPES edu, EDUID eduid )
   {
      _mutex.get() ;
      _mapSystemEDUS[ edu ] = eduid ;
      _mutex.release () ;
   }

    bool isQuiesced ()
   {
      return _isQuiesced ;
   }
   void setQuiesced ( bool b )
   {
      _isQuiesced = b ;
   }
   bool isDestroyed ()
   {
      return _isDestroyed ;
   }

   //设置该EDU类型是否可以返回线程池
   //显然系统类型的EDU是不可以返回的，只有AGENT可以返回，因为TCPLISTENER
   //是一个死循环，不能返回线程池
   static bool isPoolable ( EDU_TYPES type )
   {
      return ( EDU_TYPE_AGENT == type ) ;
   }

private:
    int _createNewEDU(EDU_TYPES type,void *arg,EDUID *eduid);
    int _destroyAll();
    int _forceEDUs(int property=EDU_ALL);//强迫某些EDU终止
    unsigned int _getEDUCount(int property=EDU_ALL);

    void _setDestroyed(bool b)  {_isDestroyed =b;}
    bool _isSystemEDU ( EDUID eduID )
   {
      std::map<unsigned int, EDUID>::iterator it = _mapSystemEDUS.begin() ;
      while ( it != _mapSystemEDUS.end() )
      {
         if ( eduID == it->second )
         {
            return true ;
         }
         ++it ;
      }
      return false ;
   }
        
     /* 关于EDU的状态转换图，大写字母为当前状态，小写字幕为转换函数
    * EDU Status Transition Table
    * C: CREATING
    * R: RUNNING
    * W: WAITING
    * I: IDLE
    * D: DESTROY
    * c: createNewEDU
    * a: activateEDU
    * d: destroyEDU
    * w: waitEDU
    * t: deactivateEDU
    *   C   R   W   I   D  <--- from
    * C c
    * R a   -   a   a   -  <--- Creating/Idle/Waiting status can move to Running status
    * W -   w   -   -   -  <--- Running status move to Waiting
    * I t   -   t   -   -  <--- Creating/Waiting status move to Idle
    * D d   -   d   d   -  <--- Creating / Waiting / Idle can be destroyed
    * ^ To
    */


   /*   处于创建 等待 空闲的EDU可以通过该函数转为destory
    * This function must be called against a thread that either in
    * PMD_EDU_WAITING or PMD_EDU_IDLE or PMD_EDU_CREATING status
    * This function set the status to PMD_EDU_DESTROY and remove
    * the control block from manager
    * Parameter:
    *   EDU ID (UINt64)
    * Return:
    *   EDB_OK (success)
    *   EDB_SYS (the given eduid can't be found)
    *   EDB_EDU_INVAL_STATUS (EDU is found but not with expected status)
    */
   int _destroyEDU ( EDUID eduID ) ;
  

  /*    处于创建 等待的EDU，可以通过该函数放回线程池的idle队列，注意只发生在
  // agent类型的EDU。
    * This function must be called against a thread that either in creating
    * or waiting status, it will return without any change if the agent is
    * already in pool
    * This function will change the status to PMD_EDU_IDLE and put to
    * idlequeue, representing the EDU is pooled (no longer associate with
    * any user activities)
    * deactivateEDU supposed only happened to AGENT EDUs
    * Any EDUs other than AGENT will be forwarded to destroyEDU and return
    * EDB_SYS
    * Parameter:
    *   EDU ID (UINt64)
    * Return:
    *   EDB_OK (success)
    *   EDB_SYS (the given eduid can't be found, or it's not AGENT)
    *           (note that deactivate an non-AGENT EDU will cause the EDU
    *            destroyed and EDB_SYS return)
    *   EDB_EDU_INVAL_STATUS (EDU is found but not with expected status)
    */
   int _deactivateEDU ( EDUID eduID ) ;
public:
     /* 处于创建 等待 空闲的EDU可以通过该函数启动
     //另外处于创建和等待状态的EDU应该在run队列，空闲的EDU应该在idle队列
     //该函数可以将空闲的EDU从id队列放到run队列中。
    * This function must be called against a thread that either in
    * creating/idle or waiting status
    * Threads in creating/waiting status should be sit in runqueue
    * Threads in idle status should be sit in idlequeue
    * This function set the status to PMD_END_RUNNING and bring
    * the control block to runqueue if it was idle status
    * Parameter:
    *   EDU ID (UINt64)
    * Return:
    *   EDB_OK (success)
    *   EDB_SYS (the given eduid can't be found)
    *   EDB_EDU_INVAL_STATUS (EDU is found but not with expected status)
    */
   int activateEDU ( EDUID eduID ) ;

    /*  处于运行的EDU可以通过该函数转为wait状态，但依旧在run队列
    * This function must be called against a thread that in running
    * status
    * Threads in running status will be put in PMD_EDU_WAITING and
    * remain in runqueue
    * Parameter:
    *   EDU ID (UINt64)
    * Return:
    *   EDB_OK (success)
    *   EDB_SYS (the given eduid can't be found)
    *   EDB_EDU_INVAL_STATUS (EDU is found but not with expected status)
    */
   int waitEDU ( EDUID eduID ) ;

    /* 该函数会从idle队列中获取一个thread或者创建一个thread给EDU
    * This function is called to get an EDU run the given function
    * Depends on if there's any idle EDU, manager may choose an existing
    * idle thread or creating a new threads to serve the request
    * Parmaeter:
    *   pmdEntryPoint ( void (*entryfunc) (pmdEDUCB*, void*) )
    *   type (EDU type, PMD_TYPE_AGENT for example )
    *   arg ( void*, pass to pmdEntryPoint )
    * Output:
    *   eduid ( UINt64, the edu id for the assigned EDU )
    * Return:
    *   EDB_OK (success)
    *   EDB_SYS (internal error (edu id is reused) or creating thread fail)
    *   EDB_OOM (failed to allocate memory)
    *   EDB_INVALIDARG (the type is not valid )
    */
   int startEDU ( EDU_TYPES type, void* arg, EDUID *eduid ) ;


    /* 该函数将一个消息推给EDU的消息队列
    * This function should post a message to EDU
    * In each EDU control block there is a queue for message
    * Posting EDU means adding an element to the queue
    * If the EDU is doing some other activity at the moment, it may
    * not able to consume the event right away
    * There can be more than one event sitting in the queue
    * The order is first in first out
    * Parameter:
    *   EDU Id ( EDUID )
    *   enum pmdEDUEventTypes, in pmdEDUEvent.hpp
    *   pointer for data
    * Return:
    *   EDB_OK ( success )
    *   EDB_SYS ( given EDU ID can't be found )
    */
   int postEDUPost ( EDUID eduID, pmdEDUEventTypes type,
                     bool release = false, void *pData = NULL ) ;

     /* EDU在给定时间内等待事件
    * This function should wait an event for EDU
    * If there are more than one event sitting in the queue
    * waitEDU function should pick up the earliest event
    * This function will wait forever if the input is less than 0
    * Parameter:
    *    EDU ID ( EDUID )
    *    millisecond for the period of waiting ( -1 by default )
    * Output:
    *    Reference for event
    * Return:
    *   EDB_OK ( success )
    *   EDB_SYS ( given EDU ID can't be found )
    *   EDB_TIMEOUT ( timeout )
    */
   int waitEDUPost ( EDUID eduID, pmdEDUEvent& event,
                     long long millsecond ) ;

    /*  将处于等待或者创建状态的EDU返回给线程池
    //线程池会决定是回收还是销毁
    * This function should return an waiting/creating EDU to pool
    * (cannot be running)
    * Pool will decide whether to destroy it or pool the EDU
    * Any thread main function should detect the destroyed output
    * deactivateEDU supposed only happened to AGENT EDUs
    * Parameter:
    *   EDU ID ( EDUID )
    * Output:
    *   Pointer for whether the EDU is destroyed
    * Return:
    *   EDB_OK ( success )
    *   EDB_SYS ( given EDU ID can't be found )
    *   EDB_EDU_INVAL_STATUS (EDU is found but not with expected status)
    */
   int returnEDU ( EDUID eduID, bool force, bool* destroyed ) ;

   int forceUserEDU(EDUID eduID);

   pmdEDUCB *getEDU(unsigned int tid);  //给定tid，获取EDU
   pmdEDUCB *getEDU();//根据当前tid，获取EDU
   pmdEDUCB *getEDUByID(EDUID eduID);//根据EDUID获取EDUCB
   void setEDU(unsigned int tid,EDUID eduid);
};



#endif
