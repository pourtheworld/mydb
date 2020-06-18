#include "pd.hpp"
#include "pmdEDUMgr.hpp"
#include "pmdEDU.hpp"
#include "pmd.hpp"

//EDU类型与其名字的对应
static std::map<EDU_TYPES,std::string> mapEDUName;
//EDU类型是否是系统类型的判定
static std::map<EDU_TYPES,EDU_TYPES> mapEDUTypesSys;

//EDU类型的注册函数
int registerEDUName(EDU_TYPES type,const char *name,bool system)
{
	int rc=EDB_OK;
	//先通过EDU的类型找其迭代器，判断map中是否已经存在
	std::map<EDU_TYPES,std::string>::iterator it=mapEDUName.find(type);
	if(it!=mapEDUName.end())
	{
		PD_LOG(PDERROR,"EDU Type conflict[type:%d,%s<->%s",(int)type,it->second.c_str(),name);
		rc=EDB_SYS;
		goto error;
	}
	//将名字对应起来
	mapEDUName[type]=std::string(name);
	//如果是系统型的，一并插入到系统map中。
	if(system)	mapEDUTypesSys[type]=type;
done:
	return rc;
error:
	goto done;
}

const char *getEDUName(EDU_TYPES type)
{
	std::map<EDU_TYPES,std::string>::iterator it=mapEDUName.find(type);
	if(it!=mapEDUName.end())	return it->second.c_str();
	return "Unknown";
}

bool isSystemEDU(EDU_TYPES type)
{
	std::map<EDU_TYPES,EDU_TYPES>::iterator it=mapEDUTypesSys.find(type);
	return it==mapEDUTypesSys.end()?false:true;

}

//EDU构造函数
pmdEDUCB::pmdEDUCB(pmdEDUMgr *mgr,EDU_TYPES type):
_type(type),
_mgr(mgr),
_status(PMD_EDU_CREATING),
_id(0),
_isForced(false),
_isDisconnected(false)
{}

//EDU的入口信息结构
struct _eduEntryInfo
{
	EDU_TYPES type;	//EDU类型
	int	regResult;	//注册结果
	pmdEntryPoint entryFunc;//入口函数
};

//用于getEntryFuncByType，对_eduEntryInfo的赋值
//其中type,desp,system用于注册EDU，entry用于EDU对应入口函数
#define ON_EDUTYPE_TO_ENTRY1(type,system,entry,desp)	\
{type,registerEDUName(type,desp,system),entry}

//根据输入的EDU类型寻找对应入口函数
pmdEntryPoint getEntryFuncByType(EDU_TYPES type)
{
	pmdEntryPoint rt=NULL;
	//静态，第一次加载这个函数时生成entry入口函数对应数组
	static const _eduEntryInfo entry[]={
		ON_EDUTYPE_TO_ENTRY1(EDU_TYPE_AGENT,false,pmdAgentEntryPoint,"Agent"),
		ON_EDUTYPE_TO_ENTRY1(EDU_TYPE_TCPLISTENER,true,pmdTcpListenerEntryPoint,"TCPListener"),
		ON_EDUTYPE_TO_ENTRY1(EDU_TYPE_MAXIMUM,false,NULL,"Unknown")
	};

	static const unsigned int number=sizeof(entry)/sizeof(_eduEntryInfo);

	unsigned int index=0;

	for(;index<number;++index)
	{	//entry数组中寻找对应于type的入口函数
		if(entry[index].type==type)
		{	//获取到了该入口
			rt=entry[index].entryFunc;
			goto done;
		}
	}
done:
	return rt;
}

//EDU中接受数据
int pmdRecv(char *pBuffer,int recvSize,ossSocket *sock,pmdEDUCB *cb)
{
	int rc=EDB_OK;
	EDB_ASSERT(sock,"Socket is NULL");
	EDB_ASSERT(cb,"cb is NULL");

	while(true)
	{	
		//如果EDB被强制关闭
		if(cb->isForced())
		{
			rc=EDB_APP_FORCED;
			goto done;
		}
		rc=sock->recv(pBuffer,recvSize);
		if(EDB_TIMEOUT==rc)	continue;
		goto done;
	}
done:
	return rc;
}

//EDU中发送数据
int pmdSend ( const char *pBuffer, int sendSize,ossSocket *sock, pmdEDUCB *cb )
{
   int rc = EDB_OK ;
   EDB_ASSERT ( sock, "Socket is NULL" ) ;
   EDB_ASSERT ( cb, "cb is NULL" ) ;
   while ( true )
   {
      if ( cb->isForced () )
      {
         rc = EDB_APP_FORCED ;
         goto done ;
      }
      rc = sock->send ( pBuffer, sendSize ) ;
      if ( EDB_TIMEOUT == rc )
         continue ;
      goto done ;
   }
done :
   return rc ;
}

//EDU的入口函数
int pmdEDUEntryPoint(EDU_TYPES type,pmdEDUCB *cb,void *arg)
{
	//通过EDU对数据进行初始化
	int rc=EDB_TIMEOUT;
	EDUID myEDUID=cb->getID();
	pmdEDUMgr *eduMgr=cb->getEDUMgr();
	pmdEDUEvent event;
	bool eduDestroyed=false;
	bool isForced=false;

	while(!eduDestroyed)
	{	//获取EDU类型
		type=cb->getType();

		//等待事件,如果没有等待到事件：
		if(!cb->waitEvent(event,100))
		{	//如果时因为被强迫shutdown，打印日志出该EDUID
			if(cb->isForced())
			{
				PD_LOG(PDEVENT,"EDU %lld is forced",myEDUID);
				isForced=true;
			}
			else continue;//不然继续等待
		}

		//说明已经获取了事件，现在开始判断事件类型
		//注意我们在这个阶段，事件只能是RESUME和TERMINATE


		//如果事件为恢复
		if(!isForced&&PMD_EDU_EVENT_RESUME==event._eventType)
		{		//说明有一个事件过来了，让这个EDU等待一下
				eduMgr->waitEDU(myEDUID);

				//根据EDUtype类型给予entry函数
				pmdEntryPoint entryFunc=getEntryFuncByType(type);
				if(!entryFunc)//获取entry失败
				{
					PD_LOG(PDERROR,"EDU %lld type %d entry point func is NULL",myEDUID,type);
					EDB_SHUTDOWN_DB	//相当于严重错误，直接关闭DB
					rc=EDB_SYS;
				}
				else
				{	//获取成功，跳转到入口函数，并赋值给事件的数据
					rc=entryFunc(cb,event._Data);
				}

				//合法性检查，是否现在数据库正在启动
				if(EDB_IS_DB_UP)
				{	//一个系统性的EDU永远不应该退出，而现在事件为恢复
					//且数据库正在启动，因此需要关闭数据库
					if(isSystemEDU(cb->getType()))
					{
						PD_LOG(PDSEVERE,"System EDU: %lld,type %s exits with %d",myEDUID,getEDUName(type),rc);
						EDB_SHUTDOWN_DB
					}//非系统性的EDU，可以退出
					else if(rc)	PD_LOG(PDWARNING,"EDU %lld,type %s,exits with %d",myEDUID,getEDUName(type),rc);
				}
				eduMgr->waitEDU(myEDUID);
		}
		else if(!isForced&&PMD_EDU_EVENT_TERM!=event._eventType)
		{//如果不是Terminate事件，也不是resume事件，那么事件类型出错。
			PD_LOG(PDERROR,"Receive the wrong event %d in EDU %lld,type %s",event._eventType,myEDUID,getEDUName(type));
			rc=EDB_SYS;
		}//如果被强制shutdown了，且事件也是terminate，则报告到日志里。
		else if(isForced&&PMD_EDU_EVENT_TERM==event._eventType&&cb->isForced())
		{
			PD_LOG ( PDEVENT, "EDU %lld, type %s is forced", myEDUID, type ) ;
        	isForced = true ;
		}

		//如果事件被置为释放，那么释放事件数据
		if(!isForced&&event._Data&&event._release)
		{
			free(event._Data);
			event.reset();
		}

		//此时事件数据被释放已经结束，因此return EDU
		rc=eduMgr->returnEDU(myEDUID,isForced,&eduDestroyed);
		if(rc)	PD_LOG(PDERROR,"Invalid EDU Status for EDU: %lld,type %s",myEDUID,getEDUName(type));
		PD_LOG(PDDEBUG,"Terminating thread for EDU: %lld,type %s",myEDUID,getEDUName(type));
	}
	return 0;
}

