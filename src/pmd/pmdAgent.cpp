#include "pd.hpp"
#include "pmdEDUMgr.hpp"
#include "pmdEDU.hpp"
#include "ossSocket.hpp"
#include "../bson/src/bson.h"
#include "pmd.hpp"
#include "msg.hpp"


using namespace bson;
using namespace std;

//当y-10,x=19,（19+ 10-1） - （（19+ 10-1）%10）=20 
//实际作用为获得比x稍大的 y的整数倍。
//用于一会重新分配缓冲区时，大小刚好为数据包的整数倍
#define ossRoundUpToMultipleX(x,y) (  ((x)+((y)-1))-(((x)+((y)-1)) %(y)) )  
//我们暂时把agent的接受缓冲区设置和一页的大小一样
#define PMD_AGENT_RECIEVE_BUFFER_SZ 4096
#define EDB_PAGE_SIZE 4096

static int pmdProcessAgentRequest(char *pReceiveBuffer,
                                  int packetSize,
                                  char **ppResultBuffer,
                                  int *pResultBufferSize,
                                  bool *disconnect,
                                  pmdEDUCB *cb)
{
    EDB_ASSERT(disconnect,"disconnect can't be NULL");
    EDB_ASSERT(pReceiveBuffer,"pReceiveBuffer is NULL");
    
    int rc=EDB_OK;
    unsigned int probe=0;
    const char *pInsertorBuffer=NULL;
    BSONObj recordID;   //用于Query Delete消息解封时获取BSONObj,因为它们需要的是ID(key)
    BSONObj retObj;  //用于Snapshot和Query消息返回BSONObj

   //实际上是一个解封装的过程，不过只是将消息头信息get
    MsgHeader *header=(MsgHeader*)pReceiveBuffer;
    int messageLength=header->messageLen;
    int opCode=header->opCode;
    EDB_KRCB *krcb=pmdGetKRCB();

   //先将断开标志置为false
   *disconnect=false;

   //消息是否小于消息头长度
   if(messageLength<(int)sizeof(MsgHeader))
   {
      probe=10;
      rc=EDB_INVALIDARG;
      goto error;
   }
   try
   {  //处理消息
      if(OP_INSERT==opCode)
      {
         int recordNum=0;  //插入记录数量
         PD_LOG(PDDEBUG,"Insert request received");
         //insert消息是由消息头+recordNum+BSONoBJ组成的
         //通过这一步解封我们获得的pInsertorBuffer已经指向了BSONObj的首字母
         rc=msgExtractInsert(pReceiveBuffer,recordNum,&pInsertorBuffer);

         if(rc)
         {
            PD_LOG(PDERROR,"Failed to read insert packet");
            probe=20;
            rc=EDB_INVALIDARG;
            goto error;
         }
         try
         {  
            //我们将它打印出来
            BSONObj insertor(pInsertorBuffer);
            PD_LOG(PDEVENT,"Insert: insertor: %s",insertor.toString().c_str());
         }
         catch(const std::exception& e)
         {
            PD_LOG(PDERROR,"Failed to create insertor for insert: %s",e.what());
            probe=30;
            rc=EDB_INVALIDARG;
            goto error;
         }
      }
      else if(OP_QUERY==opCode)
      {
         PD_LOG(PDDEBUG,"Query request received");
         //同样是解封，不过直接用BSONObj获取key了
         rc=msgExtractQuery(pReceiveBuffer,recordID);
         if(rc)
         {
            PD_LOG(PDERROR,"Failed to read query packet");
            probe=40;
            rc=EDB_INVALIDARG;
            goto error;
         }
         //暂时用简单的打印
         PD_LOG(PDEVENT,"Query condition: %s",recordID.toString().c_str());
         try
         {//假的BSONobj查询返回对象，测试效果用
            BSONObjBuilder b;
            b.append("query","test");
            b.append("result",1);
            retObj=b.obj();
         }
         catch(const std::exception& e)
         {
            PD_LOG(PDERROR,"Failed to create return BSONObj: %s",e.what());
            probe=55;
            rc=EDB_INVALIDARG;
            goto error;
         }
      }
      else if(OP_DELETE==opCode)
      {
         PD_LOG(PDDEBUG,"Delete request received");
         //同样是解封，不过直接用BSONObj获取key了
         rc=msgExtractQuery(pReceiveBuffer,recordID);
         if(rc)
         {
            PD_LOG(PDERROR,"Failed to read delete packet");
            probe=40;
            rc=EDB_INVALIDARG;
            goto error;
         }
         //暂时用简单的打印
         PD_LOG(PDEVENT,"Delete condition: %s",recordID.toString().c_str());
      }
      else if(OP_SNAPSHOT==opCode)
      {
         PD_LOG(PDDEBUG,"Snapshot request received");
         try
         {//假的BSONobj快照返回对象，测试效果用
            BSONObjBuilder b;
            b.append("insertTimes",100);
            b.append("delTimes",1000);
            b.append("queryTimes",100);
            b.append("serverRunTime",100);
            retObj=b.obj();
         }
         catch(const std::exception& e)
         {
            PD_LOG(PDERROR,"Failed to create return BSONObj: %s",e.what());
            probe=55;
            rc=EDB_INVALIDARG;
            goto error;
         }
         
      }
      else if(OP_COMMAND==opCode)
      {

      }
      else if(OP_DISCONNECT==opCode)
      {
         PD_LOG(PDEVENT,"Receive disconnect msg");
         *disconnect=true;
      }
      else
      {
         probe=60;
         rc=EDB_INVALIDARG;
         goto error;
      }
   }
   catch(std::exception &e)
   {
         PD_LOG(PDERROR,"Error happened during performing operation: %s",e.what());
         probe=70;
         rc=EDB_INVALIDARG;
         goto error;
   }
done:
   //如果不是断开连接的消息，则封装回复信息
   if(!*disconnect)
   {
      switch(opCode)
      {//只有Snapshot和Query消息需要返回BSONObj
         case OP_SNAPSHOT:
         case OP_QUERY:
            msgBuildReply(ppResultBuffer,pResultBufferSize,rc,&retObj);
            break;
         default:
            msgBuildReply(ppResultBuffer,pResultBufferSize,rc,NULL);
      }
   }
   return rc;
error:
   switch(rc)
   {
      case EDB_INVALIDARG:
         PD_LOG(PDERROR,"Invalid argument is received,probe: %d",probe);
         break;
      case EDB_IXM_ID_NOT_EXIST:
         PD_LOG(PDERROR,"Record does not exist");
      default:
         PD_LOG(PDERROR,"System error,probe: %d,rc=%d",probe,rc);
         break;
   }
   goto done;
}


//agent EDU的入口 形参为EDUCB和自带的函数
//首先通过自带的函数获取到ossSocket
//分配好接受和发送的缓冲区
//接受前四个字节（包长度）到缓冲区，如果包长度小于4个字节出错；
//如果数据长度大于接受缓冲区长度，根据宏重新分配缓冲区。
//正式接受后续的包，并在缓冲区最后加上0，将EDU状态转变成run
//分配发送区长度
//执行agent处理函数
//发送回复消息，将EDU状态转变成wait

int pmdAgentEntryPoint(pmdEDUCB *cb,void *arg)
{
   int rc                = EDB_OK ;
   unsigned int probe    = 0 ;
   bool disconnect       = false ;
   char *pReceiveBuffer  = NULL ;   //接受缓冲区
   char *pResultBuffer   = NULL ;   //发送缓冲区
   //接受缓冲区长度先设置为一页
   int receiveBufferSize = ossRoundUpToMultipleX (
                           PMD_AGENT_RECIEVE_BUFFER_SZ,
                           EDB_PAGE_SIZE ) ;
   //接受缓冲区大小先设置为一个回复信息结构体的长度
   int resultBufferSize  = sizeof( MsgReply ) ;
   int packetLength      = 0 ;
   EDUID myEDUID         = cb->getID () ;
   pmdEDUMgr *eduMgr     = cb->getEDUMgr() ;
   
   //传入的形参为ossSocket的句柄
   int s                 = *(( int *) &arg ) ;
   ossSocket sock ( &s ) ;
   sock.disableNagle () ;
   
   //分配接受缓冲区的大小
   pReceiveBuffer = (char*)malloc( sizeof(char) *
                                   receiveBufferSize ) ;
   if ( !pReceiveBuffer )
   {
      rc = EDB_OOM ;
      probe = 10 ;
      goto error ;
   }
   
   //分配发送缓冲区的大小
   pResultBuffer = (char*)malloc( sizeof(char) *
                                  resultBufferSize ) ;
   if ( !pResultBuffer )
   {
      rc = EDB_OOM ;
      probe = 20 ;
      goto error ;
   }
   
   //当连接没有断开
   while ( !disconnect )
   {
      // 先接受下个包的前四个字节(数据包的长度获取)
      rc = pmdRecv ( pReceiveBuffer, sizeof (int), &sock, cb ) ;
      if ( rc )
      {
        if ( EDB_APP_FORCED == rc ) //突然被强制终止了
        {
           disconnect = true ;
           continue ;
        }
        probe = 30 ;
        goto error ;
      }
      //获取到了包长度
      packetLength = *(int*)(pReceiveBuffer) ;
      PD_LOG ( PDDEBUG,
              "Received packet size = %d", packetLength ) ;
      if ( packetLength < (int)sizeof (int) )
      {//如果包长度小于4个字节
         probe = 40 ;
         rc = EDB_INVALIDARG ;
         goto error ;
      }
      // 如果包长度大于已分配的缓冲区长度，重新分配
      if ( receiveBufferSize < packetLength+1 )
      {
         PD_LOG ( PDDEBUG,
                 "Receive buffer size is too small: %d vs %d, increasing...",
                  receiveBufferSize, packetLength ) ;
         //新分配的缓冲区大小为比包长度大的，页的最小倍数
         int newSize = ossRoundUpToMultipleX ( packetLength+1, EDB_PAGE_SIZE ) ;
         if ( newSize < 0 )
         {
            probe = 50 ;
            rc = EDB_INVALIDARG ;
            goto error ;
         }
         //先释放原有缓冲区
         free ( pReceiveBuffer ) ;
         pReceiveBuffer = (char*)malloc ( sizeof(char) * (newSize) ) ;
         if ( !pReceiveBuffer )
         {
            rc = EDB_OOM ;
            probe = 60 ;
            goto error ;
         }
         //前四个字节我们设置为包的长度
         *(int*)(pReceiveBuffer) = packetLength ;
         //但总体缓冲区长度为页倍数
         receiveBufferSize = newSize ;
      }
      //重新分配好以后，确认了包长度，我们接受后续的包内容
      rc = pmdRecv ( &pReceiveBuffer[sizeof(int)],
                     packetLength-sizeof(int),
                     &sock, cb ) ;
      if ( rc )
      {
         if ( EDB_APP_FORCED == rc )
         {
            disconnect = true ;
            continue ;
         }
         probe = 70 ;
         goto error ;
      }
      // 在缓冲区包的最后面加上0
      pReceiveBuffer[packetLength] = 0 ;
      
      //接受完数据我们将该EDU run起来
      if ( EDB_OK != ( rc = eduMgr->activateEDU ( myEDUID )) )
      {
         goto error ;
      }

      //重新分配发送缓冲区
      if( resultBufferSize >(int)sizeof( MsgReply ) )
      {
          resultBufferSize =  (int)sizeof( MsgReply ) ;
          free ( pResultBuffer ) ;
          pResultBuffer = (char*)malloc( sizeof(char) *
                                         resultBufferSize ) ;
          if ( !pResultBuffer )
          {
            rc = EDB_OOM ;
            probe = 20 ;
            goto error ;
          }
      }

      //正式对数据包进行处理
      rc = pmdProcessAgentRequest ( pReceiveBuffer,
                                    packetLength,
                                    &pResultBuffer,
                                    &resultBufferSize,
                                    &disconnect,
                                    cb ) ;
      if ( rc )
      {
         PD_LOG ( PDERROR, "Error processing Agent request, rc=%d", rc ) ;
      }
      // 如果该信息不是关闭信息，那么对其进行回复
      if ( !disconnect )
      {
         rc = pmdSend ( pResultBuffer, *(int*)pResultBuffer, &sock, cb ) ;
         if ( rc )
         {
            if ( EDB_APP_FORCED == rc )
            {
            }
            probe = 80 ;
            goto error ;
         }
      }
      //把EDU置为等待状态
      if ( EDB_OK != ( rc = eduMgr->waitEDU ( myEDUID )) )
      {
         goto error ;
      }
   }
done :
    //不管怎么样两个缓冲区都要释放，sock都要关闭
   if ( pReceiveBuffer )
      free ( pReceiveBuffer )  ;
   if ( pResultBuffer )
      free ( pResultBuffer )  ;
   sock.close () ;
   return rc;
error :
   switch ( rc )
   {
   case EDB_SYS :
      PD_LOG ( PDSEVERE,
              "EDU id %d cannot be found, probe %d", myEDUID, probe ) ;
      break ;
   case EDB_EDU_INVAL_STATUS :
      PD_LOG ( PDSEVERE,
              "EDU status is not valid, probe %d", probe ) ;
      break ;
   case EDB_INVALIDARG :
      PD_LOG ( PDSEVERE,
              "Invalid argument receieved by agent, probe %d", probe ) ;
      break ;
   case EDB_OOM :
      PD_LOG ( PDSEVERE,
              "Failed to allocate memory by agent, probe %d", probe ) ;
      break ;
   case EDB_NETWORK :
      PD_LOG ( PDSEVERE,
              "Network error occured, probe %d", probe ) ;
      break ;
   case EDB_NETWORK_CLOSE :
      PD_LOG ( PDDEBUG,
              "Remote connection closed" ) ;
      rc = EDB_OK ;
      break ;
   default :
      PD_LOG ( PDSEVERE,
              "Internal error, probe %d", probe ) ;
   }
   goto done ;
}