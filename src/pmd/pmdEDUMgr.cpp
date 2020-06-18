/* EDU的状态转换图
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
 * R a   -   a   a   -  <--- Create/Idle/Wait status can move to Running status
 * W -   w   -   -   -  <--- Running status move to Waiting
 * I t   -   t   -   -  <--- Creating/Waiting status move to Idle
 * D d   -   d   d   -  <--- Creating / Waiting / Idle can be destroyed
 * ^ To
 */

#include "pd.hpp"
#include "pmd.hpp"
#include "pmdEDUMgr.hpp"

//销毁USER和SYSTEM的EDU，通过force
int pmdEDUMgr::_destroyAll()
{
    _setDestroyed(true);
    setQuiesced(true);

    unsigned int timeCounter=0;

    //先销毁USER
    unsigned int eduCount=_getEDUCount(EDU_USER);

    while(eduCount!=0)
    {   //第一次先通知所有USER EDU
        if(0==timeCounter%50)   _forceEDUs(EDU_USER);
        ++timeCounter;  //一共循环50次，也就是5秒
        ossSleepmillis(100);//一次等待0.1秒
        eduCount=_getEDUCount(EDU_USER);//每次检查还剩多少个USER
    }

    timeCounter=0;

    //再销毁SYSTEM
    eduCount=_getEDUCount(EDU_ALL);
    while(eduCount!=0)
    {
        if(0==timeCounter%50)   _forceEDUs(EDU_ALL);
        ++timeCounter;
        ossSleepmillis(100);
        eduCount=_getEDUCount(EDU_ALL);
    }

    return EDB_OK;

    
}

//迫使一个特定USER EDU销毁
int pmdEDUMgr::forceUserEDU(EDUID eduID)
{
    int rc=EDB_OK;
    std::map<EDUID,pmdEDUCB*>::iterator it;
    _mutex.get();
    if(isSystemEDU(eduID))  //系统EDU则error
    {
        PD_LOG(PDERROR,"System EDU %d can't be forced",eduID);
        rc=EDB_PMD_FORCE_SYSTEM_EDU;
        goto error;
    }
    {   //分别在run和idle队列里学着有无该EDU，有的话通知force
        for(it=_runQueue.begin();it!=_runQueue.end();++it)
        {
            if((*it).second->getID()==eduID)
            {
                (*it).second->force();
                goto done;
            }
        }
        for(it=_idleQueue.begin();it!=_idleQueue.end();++it)
        {
            if((*it).second->getID()==eduID)
            {
                (*it).second->force();
                goto done;
            }
        }
    }
done:
    _mutex.release();
    return rc;
error:
    goto done;
}

// 阻止新的request，并且让某种类型的所有EDU销毁
int pmdEDUMgr::_forceEDUs ( int property )
{
   std::map<EDUID, pmdEDUCB*>::iterator it ;

   
   _mutex.get() ;
   
   for ( it = _runQueue.begin () ; it != _runQueue.end () ; ++it )
   {
      if ( ((EDU_SYSTEM & property) && _isSystemEDU( it->first ))
         || ((EDU_USER & property) && !_isSystemEDU( it->first )) )
      {
         ( *it ).second->force () ;
         PD_LOG ( PDDEBUG, "force edu[ID:%lld]", it->first ) ;
      }
   }

   for ( it = _idleQueue.begin () ; it != _idleQueue.end () ; ++it )
   {
      if ( EDU_USER & property )
      {
         ( *it ).second->force () ;
      }
   }
   _mutex.release() ;
   
   return EDB_OK ;
}

//获得EDU的个数，property指EDU为USER还是SYSTEM
unsigned int pmdEDUMgr::_getEDUCount(int property)
{
    unsigned int eduCount=0;
    std::map<EDUID,pmdEDUCB*>::iterator it;

    _mutex.get_shared();
    
    //先从run队列中寻找
    for(it=_runQueue.begin();it!=_runQueue.end();++it)
    {//如果给定类型为系统类型且run中成员也为系统或者给定类型为USER且run中成员也为USER
        if((EDU_SYSTEM&property)&&_isSystemEDU(it->first)||((EDU_USER&property)&&!_isSystemEDU(it->first)))
        {//那么计数+1
            ++eduCount;
        }
    }
    //再从idle队列中寻找
    for ( it = _idleQueue.begin () ; it != _idleQueue.end () ; ++it )
   {
      if ( EDU_USER & property )
      {
         ++eduCount ;
      }
   }

   _mutex.release_shared();
   return eduCount;
}

//给定EDUID,先去队列中获取EDUCB，再推一个事件进它的事件队列
//POST事件不论是run还是idle队列的EDU都可以实行
int pmdEDUMgr::postEDUPost(EDUID eduID,pmdEDUEventTypes type,bool release,void *pData)
{
    int rc=EDB_OK;
    pmdEDUCB *eduCB=NULL;
    std::map<EDUID,pmdEDUCB*>::iterator it;

    _mutex.get_shared();
    //首先去run队列中找有无这个EDU
    if(_runQueue.end()==(it=_runQueue.find(eduID)))
    {   //找不到再去idle队列中找
        if(_idleQueue.end()==(it=_idleQueue.find(eduID)))
        {
            rc=EDB_SYS;
            goto error;
        }
    }
    eduCB=(*it).second;
    eduCB->postEvent(pmdEDUEvent(type,release,pData));
done:
    _mutex.release_shared();
    return rc;
error:
    goto done;
}

//给定EDUID,找到EDUCB,再让它等待事件
int pmdEDUMgr::waitEDUPost ( EDUID eduID, pmdEDUEvent& event,
                             long long millsecond = -1 )
{
   int rc = EDB_OK ;
   pmdEDUCB* eduCB = NULL ;
   std::map<EDUID, pmdEDUCB*>::iterator it ;
   _mutex.get_shared () ;
   if ( _runQueue.end () == ( it = _runQueue.find ( eduID )) )
   {
      if ( _idleQueue.end () == ( it = _idleQueue.find ( eduID )) )
      {
         rc = EDB_SYS ;
         goto error ;
      }
   }
   eduCB = ( *it ).second ;

   if ( !eduCB->waitEvent( event, millsecond ) )
   {
      rc = EDB_TIMEOUT ;
      goto error ;
   }
done :
   _mutex.release_shared () ;
   return rc ;
error :
   goto done ;
}

//将一个给定的EDU释放控制
//由线程池决定是销毁EDU还是返回线程池
//它和disactivate一样，只有creating和wating可以执行
int pmdEDUMgr::returnEDU(EDUID eduID,bool force,bool *destroyed)
{
    int rc=EDB_OK;
    EDU_TYPES type=EDU_TYPE_UNKNOWN;
    pmdEDUCB *educb=NULL;
    std::map<EDUID,pmdEDUCB*>::iterator it;

    _mutex.get_shared();
    //先通过EDUID找到EDUCB，找不到说明已经被销毁了
     if ( _runQueue.end() == ( it = _runQueue.find ( eduID ) ) )
   {
      if ( _idleQueue.end() == ( it = _idleQueue.find ( eduID ) ) )
      {
         rc = EDB_SYS ;
         *destroyed = false ;
         _mutex.release_shared () ;
         goto error ;
      }
   }

   educb=(*it).second;

   if(educb)    type=educb->getType();  //得到类型是为了等会判断是否可回收到线程池
   _mutex.release_shared();

    //当类型为不可回收类型或者我们指定它被强迫销毁了或者已经被销毁了或者当前线程大于线程池容量了
    //我们手动将其销毁
    if(!isPoolable(type)||force||isDestroyed()||size()>(unsigned int)pmdGetKRCB()->getMaxPool())
    {
        rc=_destroyEDU(eduID);
        if(destroyed)//注意destroyed是我们传进来的bool*指针
        {//当_destroyEDU返回EDB_OK或者EDB_SYS(EDB找不到)则成功
            if(EDB_OK==rc||EDB_SYS==rc) *destroyed=true;
            else *destroyed=false;
        }
    }
    else
    {   //其它情况一律回收
        rc=_deactivateEDU(eduID);
        if(destroyed)
        {//如果EDB找不到，说明销毁成功
            if(EDB_SYS==rc) *destroyed=true;
            else *destroyed=false;
        }
    }
done:
    return rc;
error:
    goto done;
    
}

//从idle队列中获取一个EDU，并将其放入run队列中，并将idle状态转换为waiting状态
//需要注意的是，idle里只有可能是agent类型的EDU
//新创建的EDU是直接被放到run队列中的
//如果idle中没有EDU了，那么创建一个新的
//当EDU启动了，需要post一个resume事件
int pmdEDUMgr::startEDU(EDU_TYPES type,void *arg,EDUID *eduid)
{
    int rc=EDB_OK;
    EDUID eduID=0;
    pmdEDUCB* eduCB=NULL;
    std::map<EDUID,pmdEDUCB*>::iterator it;

    //数据库当前是否为挂起状态
    if(isQuiesced())
    {
        rc=EDB_QUIESCED;
        goto done;
    }

    _mutex.get();
    //如果为空，或者该EDU类型不是agent(idle队列里只能是agent)
    if(true==_idleQueue.empty()||!isPoolable(type))
    {
        _mutex.release();
        rc=_createNewEDU(type,arg,eduid);
        if(EDB_OK==rc)  goto done;
        goto error;
 
    }

    //这句循环其实是在判断：所有idle队列里EDU都是销毁状态吗?
    for(it=_idleQueue.begin();(_idleQueue.end()!=it)&&(PMD_EDU_IDLE!=(*it).second->getStatus());++it);
    //确实都是销毁状态，那么新创一个EDU
    if(_idleQueue.end()==it)
    {
        _mutex.release();
        rc=_createNewEDU(type,arg,eduid);
        if(EDB_OK==rc)  goto done;
        goto error;
    }

    //此时说明至少有一个idle状态的EDU
    eduID=(*it).first;
    eduCB=(*it).second;
    //将它从idle队列中取出，放到run队列，并且状态置为waiting
    _idleQueue.erase(eduID);
    EDB_ASSERT(isPoolable(type),"must be agent");
    eduCB->setType(type);
    eduCB->setStatus(PMD_EDU_WAITING);
    _runQueue[eduID]=eduCB;
    *eduid=eduID;
    _mutex.release();

    eduCB->postEvent(pmdEDUEvent(PMD_EDU_EVENT_RESUME,false,arg));
done:
    return rc;
error:
    goto done;
}

//创建一个新的EDU 
//首先根据类型得到其入口函数
//检查id在当前队列中是否重复
//将id放入run队列
//创建新的thread并传入CB等参数
//和startEDU一样post一个RESUME事件
int pmdEDUMgr::_createNewEDU(EDU_TYPES type,void *arg,EDUID *eduid)
{
    int rc=EDB_OK;
    unsigned int probe=0;
    pmdEDUCB *cb=NULL;
    EDUID myEDUID=0;
    if(isQuiesced())
    {
        rc=EDB_QUIESCED;
        goto done;
    }

    //根据type类型得到入口函数
    if(!getEntryFuncByType(type))
    {
        PD_LOG ( PDERROR, "The edu[type:%d] not exist or function is null", type ) ;
        rc = EDB_INVALIDARG ;
        probe = 30 ;
        goto error ;
    }

    cb=new(std::nothrow) pmdEDUCB(this,type);
    EDB_VALIDATE_GOTOERROR(cb,EDB_OOM,"Out of memory to create agent control block")
    cb->setStatus(PMD_EDU_CREATING);

    //检查run和idle队列里是否存在这个id
    _mutex.get();
    if ( _runQueue.end() != _runQueue.find ( _EDUID )  )
   {
      _mutex.release () ;
      rc = EDB_SYS ;
      probe = 10 ;
      goto error ;
   }
   
   if ( _idleQueue.end() != _idleQueue.find ( _EDUID )  )
   {
      _mutex.release () ;
      rc = EDB_SYS ;
      probe = 15 ;
      goto error ;
   }

    //没有的话则递增EDUID
   cb->setID(_EDUID);
   if(eduid)    *eduid=_EDUID;
   _runQueue[_EDUID]=(pmdEDUCB*)cb;
   myEDUID=_EDUID;
   ++_EDUID;
   _mutex.release();

   try
   {//创建thread，将CB和其它参数传进去
       boost::thread agentThread(pmdEDUEntryPoint,type,cb,arg);
       agentThread.detach();
   }
   catch(const std::exception& e)
   {
       _runQueue.erase(myEDUID);
       rc=EDB_SYS;
       probe=20;
       goto error;
   }

   cb->postEvent(pmdEDUEvent(PMD_EDU_EVENT_RESUME,false,arg));
done:
    return rc;
error:
    delete cb;
    PD_LOG ( PDERROR, "Failed to create new agent, probe = %d", probe ) ;
    goto done ; 
}

//销毁EDU 处于创造状态 等待状态 空闲状态
//前面两种EDU均在run队列，后者在idle队列
//我们依次在idle和run队列中查找该EDU,找到后改状态为destroy
//最后在tid与EDUID的map上去除映射关系

int pmdEDUMgr::_destroyEDU ( EDUID eduID )
{
   int rc        = EDB_OK ;
   pmdEDUCB* eduCB = NULL ;
   //先默认其状态为creating
   unsigned int eduStatus = PMD_EDU_CREATING ;
   std::map<EDUID, pmdEDUCB*>::iterator it ;
   std::map<unsigned int, EDUID>::iterator it1 ;

    //先看看有没有这个ID的EDU
   _mutex.get() ;
   if ( _runQueue.end () == ( it = _runQueue.find ( eduID )) )
   { 
      if ( _idleQueue.end () == ( it = _idleQueue.find ( eduID )) )
      {//哪都没有返回错误
         rc = EDB_SYS ;
         goto error ;
      }


      eduCB = ( *it ).second ;
      //如果在idle队列中找到，我们希望它的状态是idle
      if ( !PMD_IS_EDU_IDLE ( eduCB->getStatus ()) )
      {
         rc = EDB_EDU_INVAL_STATUS ;
         goto error ;
      }
      //没问题的话我们把它从idle队列中抹去，并置为destroy状态
      eduCB->setStatus ( PMD_EDU_DESTROY ) ;
      _idleQueue.erase ( eduID ) ;
   }
   
   else
   {//如果它在run队列中被找到，我们希望它是waiting或者creating状态
      eduCB = ( *it ).second ;
      eduStatus = eduCB->getStatus () ;
      if ( !PMD_IS_EDU_WAITING ( eduStatus ) &&
           !PMD_IS_EDU_CREATING ( eduStatus ) )
      {
         rc = EDB_EDU_INVAL_STATUS ;
         goto error ;
      }
      //没问题的话把它从run队列中去除，状态设置为销毁
      eduCB->setStatus ( PMD_EDU_DESTROY ) ;
      _runQueue.erase ( eduID ) ;
   }

   // 我们将tid和EDUID的映射给去除
   for ( it1 = _tid_eduid_map.begin(); it1 != _tid_eduid_map.end();
         ++it1 )
   {
      if ( (*it1).second == eduID )
      {
         _tid_eduid_map.erase ( it1 ) ;
         break ;
      }
   }
   //与creating相反，我们delete掉该线程以及CB
   if ( eduCB )
      delete ( eduCB ) ;
done :
   _mutex.release () ;
   return rc ;
error :
   goto done ;
}

//将run->wait，但依旧还在run队列上
int pmdEDUMgr::waitEDU ( EDUID eduID )
{
   int rc                 = EDB_OK ;
   pmdEDUCB* eduCB        = NULL ;
   unsigned int eduStatus = PMD_EDU_CREATING ;
   std::map<EDUID, pmdEDUCB*>::iterator it ;


   _mutex.get_shared () ;
   if ( _runQueue.end () == ( it = _runQueue.find ( eduID )) )
   {
      //不在run队列，直接报错
      rc = EDB_SYS ;
      goto error ;
   }
   eduCB = ( *it ).second ;

   eduStatus = eduCB->getStatus () ;

   //如果已经是等待状态，无需多言
   if ( PMD_IS_EDU_WAITING ( eduStatus ) )
      goto done ;
   //如果不是run状态，也报错
   if ( !PMD_IS_EDU_RUNNING ( eduStatus ) )
   {
      
      rc = EDB_EDU_INVAL_STATUS ;
      goto error ;
   }
   eduCB->setStatus ( PMD_EDU_WAITING ) ;
done :
   _mutex.release_shared () ;
   return rc ;
error :
   goto done ;
}

//deactivate 当处于create或者wait，可以申请返回线程池的idle队列
//注意run状态的EDU需要先转为wait
//只有agent类型的可以deactivate

int pmdEDUMgr::_deactivateEDU ( EDUID eduID )
{
   int rc         = EDB_OK ;
   unsigned int eduStatus = PMD_EDU_CREATING ;
   pmdEDUCB* eduCB  = NULL ;
   std::map<EDUID, pmdEDUCB*>::iterator it ;
   
   _mutex.get() ;
   if ( _runQueue.end () == ( it = _runQueue.find ( eduID )) )
   {
      //如果EDU不在run队列中，我们先判断它是否在idle队列中
      //如果已经在的话，那无需多言
      if ( _idleQueue.end() != _idleQueue.find ( eduID )  )
      {
         goto done ;
      }
      // 如果都不在，出错
      rc = EDB_SYS ;
      goto error ;
   }
   eduCB = ( *it ).second ;

   eduStatus = eduCB->getStatus () ;

   //如果已经是idle了，无需多言
   if ( PMD_IS_EDU_IDLE ( eduStatus ) )
      goto done ;

   if ( !PMD_IS_EDU_WAITING ( eduStatus ) &&
        !PMD_IS_EDU_CREATING ( eduStatus ) )
   {
      rc = EDB_EDU_INVAL_STATUS ;
      goto error ;
   }

   //必须是agent类型
   EDB_ASSERT ( isPoolable ( eduCB->getType() ),
                "Only agent can be pooled" )
   _runQueue.erase ( eduID ) ;
   eduCB->setStatus ( PMD_EDU_IDLE ) ;
   _idleQueue [ eduID ] = eduCB ;
done :
   _mutex.release () ;
   return rc ;
error :
   goto done ;
}


//activate 可以从createing waiting idling状态转入

int pmdEDUMgr::activateEDU ( EDUID eduID )
{
   int   rc        = EDB_OK ;
   unsigned int  eduStatus = PMD_EDU_CREATING ;
   pmdEDUCB* eduCB   = NULL;
   std::map<EDUID, pmdEDUCB*>::iterator it ;
   
   _mutex.get() ;
   if ( _idleQueue.end () == ( it = _idleQueue.find ( eduID )) )
   {    //由于两个队列都有可能有，所以都找一下
      if ( _runQueue.end () == ( it = _runQueue.find ( eduID )) )
      {
         rc = EDB_SYS ;
         goto error ;
      }
      eduCB = ( *it ).second ;

      
      eduStatus = eduCB->getStatus () ;
    //如果已经是run了，无需多言
      if ( PMD_IS_EDU_RUNNING ( eduStatus ) )
         goto done ;
    //如果run队列中都不是wait和create，报错
      if ( !PMD_IS_EDU_WAITING ( eduStatus ) &&
           !PMD_IS_EDU_CREATING ( eduStatus ) )
      {
         rc = EDB_EDU_INVAL_STATUS ;
         goto error ;
      }
      eduCB->setStatus ( PMD_EDU_RUNNING ) ;
      goto done ;
   }
   eduCB = ( *it ).second ;
   eduStatus = eduCB->getStatus () ;
   if ( PMD_IS_EDU_RUNNING ( eduStatus ) )
      goto done ;
   //在idle里检查是不是idle状态
   if ( !PMD_IS_EDU_IDLE ( eduStatus ) )
   {
      rc = EDB_EDU_INVAL_STATUS ;
      goto error ;
   }
   
   _idleQueue.erase ( eduID ) ;
   eduCB->setStatus ( PMD_EDU_RUNNING ) ;
   _runQueue [ eduID ] = eduCB ;
done :
   _mutex.release () ;
   return rc ;
error :
   goto done ;
}

// 根据tid从map中获取EDUID
// 再靠EDUID从队列中找到EDUCB
pmdEDUCB *pmdEDUMgr::getEDU ( unsigned int tid )
{
   std::map<unsigned int, EDUID>::iterator it ;
   std::map<EDUID, pmdEDUCB*>::iterator it1 ;
   EDUID eduid ;
   pmdEDUCB *pResult = NULL ;
   _mutex.get_shared () ;
   it = _tid_eduid_map.find ( tid ) ;
   if ( _tid_eduid_map.end() == it )
   {
      pResult = NULL ;
      goto done ;
   }
   eduid = (*it).second ;
   it1 = _runQueue.find ( eduid ) ;
   if ( _runQueue.end() != it1 )
   {
      pResult = (*it1).second ;
      goto done ;
   }
   it1 = _idleQueue.find ( eduid ) ;
   if ( _idleQueue.end() != it1 )
   {
      pResult = (*it1).second ;
      goto done ;
   }
done :
   _mutex.release_shared () ;
   return pResult ;
}


void pmdEDUMgr::setEDU ( unsigned int tid, EDUID eduid )
{
   _mutex.get() ;
   _tid_eduid_map [ tid ] = eduid ;
   _mutex.release () ;
}

// 从当前的线程中获取pmdEDUCB
pmdEDUCB *pmdEDUMgr::getEDU ()
{
   return getEDU ( ossGetCurrentThreadID() ) ;
}

//根据EDUID获取EDUCB
pmdEDUCB *pmdEDUMgr::getEDUByID ( EDUID eduID )
{
   std::map<EDUID, pmdEDUCB*>::iterator it ;
   pmdEDUCB *pResult = NULL ;

   _mutex.get_shared () ;
   if ( _runQueue.end () == ( it = _runQueue.find ( eduID )) )
   {
      
      if ( _idleQueue.end () == ( it = _idleQueue.find ( eduID )) )
      {
         goto done ;
      }
   }
   pResult = it->second ;
done :
   _mutex.release_shared () ;
   return pResult ;
}