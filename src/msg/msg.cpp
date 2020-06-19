#include "core.hpp"
#include "msg.hpp"
#include "pd.hpp"

using namespace bson;

//用于检查当前缓冲区ppBuffer其pBufferSize能否满足length的大小
static int msgCheckBuffer(char **ppBuffer,int *pBufferSize,int length)
{
    int rc=EDB_OK;
    if(length>*pBufferSize)
    {   //如果不够大，我们需要先将ppBuffer保存起来
        //因为等会我们需要realloc,当realloc失败我们返回空指针
        //这样就不能将其释放了，会造成内存泄漏
        char *pOldBuf=*ppBuffer;
        if(length<0)
        {
            PD_LOG(PDERROR,"invalid length: %d",length);
            rc=EDB_INVALIDARG;
            goto error;
        }
        //根据我们给定的length进行重分配
        *ppBuffer=(char*)realloc(*ppBuffer,sizeof(char)*length);

        //当我们realloc失败了需要PD_LOG，此时ppBuffer为NULL
        if(!*ppBuffer)
        {
            PD_LOG(PDERROR,"Failed to allocate %d bytes buffer",length);
            rc=EDB_OOM;
            //将预先保存的指针重新赋给ppBuffer
            *ppBuffer=pOldBuf;
            goto error;
        }
        *pBufferSize=length;
    }
done:
    return rc;
error:
    goto done;
}

//接下来会对MsgReply的封装和拆封进行详细分析，其它类型类似不再赘述

//需要返回的当前缓冲区、缓冲区当前长度、给定的返回值、需要返回的数据
int msgBuildReply(char **ppBuffer,int *pBufferSize,int returnCode,BSONObj *objReturn)
{
    int rc=EDB_OK;
    int size=sizeof(MsgReply);  //当前包的长度实际为MsgReply的Header
    MsgReply *pReply=NULL;

    //如果有需要返回的内容，长度+数据长度
    if(objReturn)   size+=objReturn->objsize();
    
    //接下来需要重新分配ppBuffer，首先查看当前缓冲区是否够大
    rc=msgCheckBuffer(ppBuffer,pBufferSize,size);
    PD_RC_CHECK ( rc, PDERROR, "Failed to realloc buffer for %d bytes, rc = %d",
                 size, rc ) ;

    //缓冲区分配好了，我们先把它的指针类型强转成MsgReply
    //即-> Header:messengeLen+opCode 、returnCode、numReturn、data[0]
    pReply=(MsgReply*)(*ppBuffer);
    pReply->header.messageLen=size;
    pReply->header.opCode=OP_REPLY;

    pReply->returnCode=returnCode;
    pReply->numReturn=(objReturn)?1:0;

    //从pReply结构体中成员data所占内存的首地址开始，复制data的内容
    if(objReturn)   memcpy(&pReply->data[0],objReturn->objdata(),objReturn->objsize());

done:
    return rc;
error:
    goto done;

}

//MsgReply的解封
//获得的缓冲区 待接受的返回值 待接受的记录数 待接受的data内容
int msgExtractReply(char *pBuffer,int &returnCode,int &numReturn,const char **ppObjStart)
{
    int rc=EDB_OK;

    //我们用一个pReply的指针去指向获得的缓冲区
    MsgReply *pReply=(MsgReply*)pBuffer;

    //如果整个pReply长度小于其头部的，没有意义。
    if ( pReply->header.messageLen < (int)sizeof(MsgReply) )
   {
      PD_LOG ( PDERROR, "Invalid length of reply message" ) ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }


    //如果消息类型不是Reply，同样没有意义。
   if ( pReply->header.opCode != OP_REPLY )
   {
      PD_LOG ( PDERROR, "non-reply code is received: %d, expected %d",
               pReply->header.opCode, OP_REPLY ) ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }

   returnCode=pReply->returnCode;
   numReturn=pReply->numReturn;

   if(0==numReturn) *ppObjStart=NULL;
   else
   {
       *ppObjStart=&pReply->data[0];
   }
done:
    return rc;
error:
    goto done;
}

//Insert类型消息的封装1
int msgBuildInsert ( char **ppBuffer, int *pBufferSize, BSONObj &obj )
{
   int rc             = EDB_OK ;
   int size           = sizeof(MsgInsert) + obj.objsize() ;
   MsgInsert *pInsert = NULL ;
   rc = msgCheckBuffer ( ppBuffer, pBufferSize, size ) ;
   if ( rc )
   {
      PD_LOG ( PDERROR, "Failed to realloc buffer for %d bytes, rc = %d",
               size, rc ) ;
      goto error ;
   }
   
   pInsert                    = (MsgInsert*)(*ppBuffer) ;
   
   pInsert->header.messageLen = size ;
   pInsert->header.opCode     = OP_INSERT ;
   
   pInsert->numInsert         = 1 ;
   
   memcpy ( &pInsert->data[0], obj.objdata(), obj.objsize() ) ;
done :
   return rc ;
error :
   goto done ;
}

//Insert类型消息的封装2，就是数据变为vector了
int msgBuildInsert ( char **ppBuffer, int *pBufferSize, vector<BSONObj*> &obj )
{
   int rc             = EDB_OK ;
   int size           = sizeof(MsgInsert) ;
   MsgInsert *pInsert = NULL ;
   vector<BSONObj*>::iterator it ;
   char *p            = NULL ;
   for ( it = obj.begin(); it != obj.end(); ++it )
   {
      size += (*it)->objsize() ;
   }
   rc = msgCheckBuffer ( ppBuffer, pBufferSize, size ) ;
   if ( rc )
   {
      PD_LOG ( PDERROR, "Failed to realloc buffer for %d bytes, rc = %d",
               size, rc ) ;
      goto error ;
   }
   
   pInsert                    = (MsgInsert*)(*ppBuffer) ;
   
   pInsert->header.messageLen = size ;
   pInsert->header.opCode     = OP_INSERT ;
   
   pInsert->numInsert         = obj.size() ;
   
   p = &pInsert->data[0] ;
   for ( it = obj.begin(); it != obj.end(); ++it )
   {
      memcpy ( p, (*it)->objdata(), (*it)->objsize() ) ;
      p += (*it)->objsize() ;
   }
done :
   return rc ;
error :
   goto done ;
}

//Insert类型消息的解封
int msgExtractInsert ( char *pBuffer, int &numInsert, const char **ppObjStart )
{
   int rc              = EDB_OK ;
   MsgInsert *pInsert  = (MsgInsert*)pBuffer ;
   
   if ( pInsert->header.messageLen < (int)sizeof(MsgInsert) )
   {
      PD_LOG ( PDERROR, "Invalid length of insert message" ) ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }
   
   if ( pInsert->header.opCode != OP_INSERT )
   {
      PD_LOG ( PDERROR, "non-insert code is received: %d, expected %d",
               pInsert->header.opCode, OP_INSERT ) ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }
   
   numInsert  = pInsert->numInsert ;
   
   if ( 0 == numInsert )
   {
      *ppObjStart = NULL ;
   }
   else
   {
      *ppObjStart = &pInsert->data[0] ;
   }
done :
   return rc ;
error :
   goto done ;
}

//Delete类型消息的封装
int msgBuildDelete ( char **ppBuffer, int *pBufferSize, BSONObj &key )
{
   int rc             = EDB_OK ;
   int size           = sizeof(MsgDelete) + key.objsize() ;
   MsgDelete *pDelete = NULL ;
   rc = msgCheckBuffer ( ppBuffer, pBufferSize, size ) ;
   if ( rc )
   {
      PD_LOG ( PDERROR, "Failed to realloc buffer for %d bytes, rc = %d",
               size, rc ) ;
      goto error ;
   }
   
   pDelete                    = (MsgDelete*)(*ppBuffer) ;
   
   pDelete->header.messageLen = size ;
   pDelete->header.opCode     = OP_DELETE ;
   
   memcpy ( &pDelete->key[0], key.objdata(), key.objsize() ) ;
done :
   return rc ;
error :
   goto done ;
}

//Delete类型消息的解封
int msgExtractDelete ( char *pBuffer, BSONObj &key )
{
   int rc              = EDB_OK ;
   MsgDelete *pDelete  = (MsgDelete*)pBuffer ;
   
   if ( pDelete->header.messageLen < (int)sizeof(MsgDelete) )
   {
      PD_LOG ( PDERROR, "Invalid length of delete message" ) ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }
   
   if ( pDelete->header.opCode != OP_DELETE )
   {
      PD_LOG ( PDERROR, "non-delete code is received: %d, expected %d",
               pDelete->header.opCode, OP_DELETE ) ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }
   
   key = BSONObj ( &pDelete->key[0] ) ;
done :
   return rc ;
error :
   goto done ;
}

//Query类型消息的封装
int msgBuildQuery ( char **ppBuffer, int *pBufferSize, BSONObj &key )
{
   int rc             = EDB_OK ;
   int size           = sizeof(MsgQuery) + key.objsize() ;
   MsgQuery *pQuery   = NULL ;
   rc = msgCheckBuffer ( ppBuffer, pBufferSize, size ) ;
   if ( rc )
   {
      PD_LOG ( PDERROR, "Failed to realloc buffer for %d bytes, rc = %d",
               size, rc ) ;
      goto error ;
   }
   
   pQuery                    = (MsgQuery*)(*ppBuffer) ;
   
   pQuery->header.messageLen = size ;
   pQuery->header.opCode     = OP_QUERY ;
   
   memcpy ( &pQuery->key[0], key.objdata(), key.objsize() ) ;
done :
   return rc ;
error :
   goto done ;
}

//Query类型消息的解封
int msgExtractQuery ( char *pBuffer, BSONObj &key )
{
   int rc              = EDB_OK ;
   MsgQuery *pQuery    = (MsgQuery*)pBuffer ;
   
   if ( pQuery->header.messageLen < (int)sizeof(MsgQuery) )
   {
      PD_LOG ( PDERROR, "Invalid length of query message" ) ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }
   
   if ( pQuery->header.opCode != OP_QUERY )
   {
      PD_LOG ( PDERROR, "non-query code is received: %d, expected %d",
               pQuery->header.opCode, OP_QUERY ) ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }
   
   key = BSONObj ( &pQuery->key[0] ) ;
done :
   return rc ;
error :
   goto done ;
}

//Commnad类型消息的封装1
int msgBuildCommand ( char **ppBuffer, int *pBufferSize, BSONObj &obj )
{
   int rc               = EDB_OK ;
   int size             = sizeof(MsgCommand) + obj.objsize() ;
   MsgCommand *pCommand = NULL ;
   rc = msgCheckBuffer ( ppBuffer, pBufferSize, size ) ;
   if ( rc )
   {
      PD_LOG ( PDERROR, "Failed to realloc buffer for %d bytes, rc = %d",
               size, rc ) ;
      goto error ;
   }
   
   pCommand                    = (MsgCommand*)(*ppBuffer) ;
   
   pCommand->header.messageLen = size ;
   pCommand->header.opCode     = OP_COMMAND ;
   
   pCommand->numArgs           = 1 ;
   
   memcpy ( &pCommand->data[0], obj.objdata(), obj.objsize() ) ;
done :
   return rc ;
error :
   goto done ;
}

//Commnad类型消息的封装2 多个vector
int msgBuildCommand ( char **ppBuffer, int *pBufferSize, vector<BSONObj*>&obj )
{
   int rc               = EDB_OK ;
   int size             = sizeof(MsgCommand) ;
   MsgCommand *pCommand = NULL ;
   vector<BSONObj*>::iterator it ;
   char *p            = NULL ;
   for ( it = obj.begin(); it != obj.end(); ++it )
   {
      size += (*it)->objsize() ;
   }
   rc = msgCheckBuffer ( ppBuffer, pBufferSize, size ) ;
   if ( rc )
   {
      PD_LOG ( PDERROR, "Failed to realloc buffer for %d bytes, rc = %d",
               size, rc ) ;
      goto error ;
   }
   
   pCommand                    = (MsgCommand*)(*ppBuffer) ;
   
   pCommand->header.messageLen = size ;
   pCommand->header.opCode     = OP_COMMAND ;
   
   pCommand->numArgs           = obj.size() ;
   
   p = &pCommand->data[0] ;
   for ( it = obj.begin(); it != obj.end(); ++it )
   {
      memcpy ( p, (*it)->objdata(), (*it)->objsize() ) ;
      p += (*it)->objsize() ;
   }
done :
   return rc ;
error :
   goto done ;
}

//Command类型消息的解封
int msgExtractCommand ( char *pBuffer, int &numArgs, const char **ppObjStart )
{
   int rc                = EDB_OK ;
   MsgCommand *pCommand  = (MsgCommand*)pBuffer ;
   
   if ( pCommand->header.messageLen < (int)sizeof(MsgCommand) )
   {
      PD_LOG ( PDERROR, "Invalid length of command message" ) ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }
   
   if ( pCommand->header.opCode != OP_COMMAND )
   {
      PD_LOG ( PDERROR, "non-command code is received: %d, expected %d",
               pCommand->header.opCode, OP_COMMAND ) ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }
   
   numArgs  = pCommand->numArgs ;
   
   if ( 0 == numArgs )
   {
      *ppObjStart = NULL ;
   }
   else
   {
      *ppObjStart = &pCommand->data[0] ;
   }
done :
   return rc ;
error :
   goto done ;
}