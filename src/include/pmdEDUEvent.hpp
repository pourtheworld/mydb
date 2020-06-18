#ifndef PMDEDUEVENT_HPP_
#define PMDEDUEVENT_HPP_

#include"core.hpp"
//engine dispatchable unit
enum pmdEDUEventTypes
{
	PMD_EDU_EVENT_NONE=0,	//置空
	PMD_EDU_EVENT_TERM,	//终止EDU
	PMD_EDU_EVENT_RESUME,	//恢复一个EDU,数据为开启EDU的参数
	PMD_EDU_EVENT_ACTIVE,
	PMD_EDU_EVENT_DEACTIVE,
	PMD_EDU_EVENT_MSG,
	PMD_EDU_EVENT_TIMEOUT,
	PMD_EDU_EVENT_LOCKWAKEUP
};


class pmdEDUEvent
{
public :
  //不传类型的构造，则置空
   pmdEDUEvent () :
   _eventType(PMD_EDU_EVENT_NONE),
   _release(false),
   _Data(NULL)
   {
   }
   //传类型的构造
   pmdEDUEvent ( pmdEDUEventTypes type ) :
   _eventType(type),
   _release(false),
   _Data(NULL)
   {
   }
   //传类型，是否释放，数据
   pmdEDUEvent ( pmdEDUEventTypes type, bool release, void *data ) :
   _eventType(type),
   _release(release),
   _Data(data)
   {
   }
   //重置事件类型
   void reset ()
   {
      _eventType = PMD_EDU_EVENT_NONE ;
      _release = false ;
      _Data = NULL ;
   }

   pmdEDUEventTypes _eventType ;
   bool             _release ;
   void            *_Data ;


};

#endif
