#include"core.hpp"
#include"ossSocket.hpp"
#include"pmdEDU.hpp"
#include"pmd.hpp"
#include"pmdEDUMgr.hpp"
#include"pd.hpp"

#define PMD_TCPLISTENER_RETRY 5
#define OSS_MAX_SERVICENAME NI_MAXSERV

//TCP监听入口
//先从内核获取服务名，并转换成端口号
//用该端口号给当前EDU的sock，作为Bind_listen，将当前EDU设置为run
//再开一个循环用于accept，每听到一个,start一个新agentEDU,把sock赋给它
//如果从accept出来了，说明出错，将当前EDU设置为wait
int pmdTcpListenerEntryPoint(pmdEDUCB *cb,void *arg)
{
   int          rc        = EDB_OK ;
   pmdEDUMgr *  eduMgr    = cb->getEDUMgr() ;
   EDUID        myEDUID   = cb->getID() ;
   unsigned int retry     = 0 ;
   EDUID        agentEDU  = PMD_INVALID_EDUID ;
   char         svcName[OSS_MAX_SERVICENAME+1] ;//服务名

//每次循环检查 重试次数是否到达上限；数据库是否已经关闭
   while ( retry <= PMD_TCPLISTENER_RETRY && !EDB_IS_DB_DOWN )
   {
      retry ++ ;
	  //从内核获得服务名
      strcpy( svcName, pmdGetKRCB()->getSvcName() ) ;
      PD_LOG ( PDEVENT, "Listening on port_test %s\n", svcName ) ;

      int port = 0 ;
      int len = strlen ( svcName ) ;
	  //将服务名转换成端口
      for ( int i = 0; i<len; ++i )
      {
         if ( svcName[i] >= '0' && svcName[i] <= '9' )
         {
            port = port * 10 ;
            port += int( svcName[i] - '0' ) ;
         }
         else
         {
            PD_LOG ( PDERROR, "service name error!\n" ) ;
         }
      }

	  //将端口赋予sock，此sock用于bind_listen
      ossSocket sock ( port ) ;
      rc = sock.initSocket () ;
      EDB_VALIDATE_GOTOERROR ( EDB_OK==rc, rc, "Failed initialize socket" )

      rc = sock.bind_listen () ;
      EDB_VALIDATE_GOTOERROR ( EDB_OK==rc, rc,
                               "Failed to bind/listen socket");
      // 监听成功后，该EDU转换成run
      if ( EDB_OK != ( rc = eduMgr->activateEDU ( myEDUID )) )
      {
         goto error ;
      }

     //这个循环用于监听
      while ( !EDB_IS_DB_DOWN )
      {
         int s ;
         rc = sock.accept ( &s, NULL, NULL ) ;
         //每次10ms，超时continue
         if ( EDB_TIMEOUT == rc )
         {
            rc = EDB_OK ;
            continue ;
         }
         //有错误并且数据库关闭则退出循环
         if ( rc && EDB_IS_DB_DOWN )
         {
            rc = EDB_OK ;
            goto done ;
         }
         else if ( rc )
         {
            
            PD_LOG ( PDERROR, "Failed to accept socket in TcpListener" ) ;
            PD_LOG ( PDEVENT, "Restarting socket to listen" ) ;
            break ;
         }

         // 把接收到的句柄赋给pData
         void *pData = NULL ;
         *((int *) &pData) = s ;

		//让线程池再启动一个agent的EDU，赋予它刚才的sock
         rc = eduMgr->startEDU ( EDU_TYPE_AGENT, pData, &agentEDU ) ;
         if ( rc )
         {
            if ( rc == EDB_QUIESCED )
            {
               
               PD_LOG ( PDWARNING, "Reject new connection due to quiesced database" ) ;
            }
            else
            {
               PD_LOG ( PDERROR, "Failed to start EDU agent" ) ;
            }
            //如果刚才分配新的thread失败了，只能将这个新的sock关闭
            ossSocket newsock ( &s ) ;
            newsock.close () ;
            continue ;
         }
      }
	  //讲道理accept是一直循环的，到这里说明出问题了，那么将状态设置成wait
      if ( EDB_OK != ( rc = eduMgr->waitEDU ( myEDUID )) )
      {
         goto error ;
      }
   } 
done :
   return rc;
error :
   switch ( rc )
   {
   case EDB_SYS :
      PD_LOG ( PDSEVERE, "System error occured" ) ;
      break ;
   default :
      PD_LOG ( PDSEVERE, "Internal error" ) ;
   }
   goto done ;
}
