#include"core.hpp"
#include"command.hpp"
#include"commandFactory.hpp"
#include "pd.hpp"

COMMAND_BEGIN
COMMAND_ADD(COMMAND_INSERT,InsertCommand)
COMMAND_ADD(COMMAND_CONNECT,ConnectCommand)
COMMAND_ADD(COMMAND_QUIT,QuitCommand)
COMMAND_ADD(COMMAND_HELP,HelpCommand)
COMMAND_END

extern int gQuit;//引用了edb.cpp的gQuit

int ICommand::execute(ossSocket &sock,std::vector<std::string> &argVec)
{
	return EDB_OK;
}

int ICommand::getError(int code)
{
  switch(code)
   {
      case EDB_OK:
         break;
      case EDB_IO:
         std::cout << "io error is occurred" << std::endl;
         break;
      case EDB_INVALIDARG:
         std::cout << "invalid argument" << std::endl;
         break;
      case EDB_PERM:
         std::cout << "edb_perm" << std::endl;
         break;
      case EDB_OOM:
         std::cout << "edb_oom" << std::endl;
         break;
      case EDB_SYS:
         std::cout << "system error is occurred." << std::endl;
         break;
      case EDB_QUIESCED:
         std::cout << "EDB_QUIESCED" << std::endl;
         break;
      case EDB_NETWORK_CLOSE:
         std::cout << "net work is closed." << std::endl;
         break;
      case EDB_HEADER_INVALID:
         std::cout << "record header is not right." << std::endl;
         break;
      case EDB_IXM_ID_EXIST:
         std::cout << "record key is exist." << std::endl;
         break;
      case EDB_IXM_ID_NOT_EXIST:
         std::cout << "record is not exist" << std::endl;
         break;
      case EDB_NO_ID:
         std::cout << "_id is needed" << std::endl;
         break;
      case EDB_QUERY_INVALID_ARGUMENT:
         std::cout << "invalid query argument" << std::endl;
         break;
      case EDB_INSERT_INVALID_ARGUMENT:
         std::cout <<  "invalid insert argument" << std::endl;
         break;
      case EDB_DELETE_INVALID_ARGUMENT:
         std::cout << "invalid delete argument" << std::endl;
         break;
      case EDB_INVALID_RECORD:
         std::cout << "invalid record string" << std::endl;
         break;
      case EDB_SOCK_NOT_CONNECT:
         std::cout << "sock connection does not exist" << std::endl;
         break;
      case EDB_SOCK_REMOTE_CLOSED:
         std::cout << "remote sock connection is closed" << std::endl;
         break;
      case EDB_MSG_BUILD_FAILED:
         std::cout << "msg build failed" << std::endl;
         break;
      case EDB_SOCK_SEND_FAILED:
         std::cout << "sock send msg failed" << std::endl;
         break;
      case EDB_SOCK_INIT_FAILED:
         std::cout << "sock init failed" << std::endl;
         break;
      case EDB_SOCK_CONNECT_FAILED:
         std::cout << "sock connect remote server failed" << std::endl;
         break;
      default :
         break;
   }
   return code;
}

int ICommand::recvReply(ossSocket &sock)
{
	int length=0;	//消息数据长度
	int ret=EDB_OK;

	memset(_recvBuf,0,RECV_BUF_SIZE);	//先将接受缓存填为0
	if(!sock.isConnected())	return getError(EDB_SOCK_NOT_CONNECT);	
	while(1)
	{	//开始从server接受数据了，首先接受数据的长度，占用4个字节
		ret=sock.recv(_recvBuf,sizeof(int));
		if(EDB_TIMEOUT==ret)	continue;
		else if(EDB_NETWORK_CLOSE==ret)	return getError(EDB_SOCK_REMOTE_CLOSED);
		else break;
		
	}
        //接受到了数据的长度
        length=*(int*)_recvBuf;
        //判断长度是否有效
        if(length>RECV_BUF_SIZE)        return getError(EDB_RECV_DATA_LENGTH_ERROR);

        //正式接受数据
	while(1)
	{
		ret=sock.recv(&_recvBuf[sizeof(int)],length-sizeof(int));
		if(ret==EDB_TIMEOUT)	continue;
		else if(EDB_NETWORK_CLOSE==ret)	return getError(EDB_SOCK_REMOTE_CLOSED);
		else break;
	}
	return ret;
}

//OnMsgBuild是一个函数指针，返回int,形参为char**,int&,BSONObj&
//在这个版本的发送消息中，发送完消息会执行一个用户输入的回调函数
int ICommand::sendOrder(ossSocket &sock,OnMsgBuild onMsgBuild)
{
	int ret=EDB_OK;
	bson::BSONObj bsonData;
	try{
		bsonData=bson::fromjson(_jsonString);
	}catch(std::exception &e){
	return getError(EDB_INVALID_RECORD);
	}

	memset(_sendBuf,0,SEND_BUF_SIZE);
	int size=SEND_BUF_SIZE;
	char *pSendBuf=_sendBuf;
	ret=onMsgBuild(&pSendBuf,&size,bsonData);
	if(ret)	return getError(EDB_MSG_BUILD_FAILED);
	if(ret) return getError(EDB_SOCK_SEND_FAILED);
	return ret;
}

int ICommand::sendOrder(ossSocket &sock,int opCode)
{
	int ret=EDB_OK;
	memset(_sendBuf,0,SEND_BUF_SIZE);

	char *pSendBuf=_sendBuf;
	const char *pStr="hello world";
	//总长度包括pStr的长度，和表示长度的4个字节。
	//pSendBuf最前面4个字节放入数据长度。
	*(int*)pSendBuf=strlen(pStr)+1+sizeof(int);
	//在头部4个字节后面把具体的数据放入。
	memcpy(&pSendBuf[4],pStr,strlen(pStr)+1);
	/* MsgHeader *header=(MsgHeader*)pSendBuf;
	header->messageLen=sizeof(MsgHeader);
	header->opCode=opCode;*/
	ret=sock.send(pSendBuf,*(int*)pSendBuf);
	return ret;
}

int InsertCommand::handleReply()
{  /*
   MsgReply * msg = (MsgReply*)_recvBuf;
   int returnCode = msg->returnCode;
   int ret = getError(returnCode);
   
   return ret;
   */
  return EDB_OK;
}

int InsertCommand::execute( ossSocket & sock, std::vector<std::string> & argVec )
{
   int rc = EDB_OK;
   if( argVec.size() <1 )
   {
      return getError(EDB_INSERT_INVALID_ARGUMENT);
   }
   _jsonString = argVec[0];
     if( !sock.isConnected() )
   {
      return getError(EDB_SOCK_NOT_CONNECT);
   }

   rc = sendOrder( sock, 0);
   PD_RC_CHECK ( rc, PDERROR, "Failed to send order, rc = %d", rc ) ;

   rc = recvReply( sock );
   PD_RC_CHECK ( rc, PDERROR, "Failed to receive reply, rc = %d", rc ) ;
   rc = handleReply();
   PD_RC_CHECK ( rc, PDERROR, "Failed to receive reply, rc = %d", rc ) ;
done :
   return rc;
error :
   goto done ;
}

int ConnectCommand::execute(ossSocket &sock,std::vector<std::string> &argVec)
{
        int ret=EDB_OK;
        //读取argVec传过来的地址和端口
        _address=argVec[0];
        _port=atoi(argVec[1].c_str());
        sock.close();
        sock.setAddress(_address.c_str(),_port);
        ret=sock.initSocket();
        if(ret) return getError(EDB_SOCK_INIT_FAILED);  //初始化socket失败
        ret=sock.connect();
        if(ret) return getError(EDB_SOCK_CONNECT_FAILED);       //连接失败
        sock.disableNagle();//将TCP设置为无阻碍
        return ret;
}

int QuitCommand::handleReply()
{
   int ret = EDB_OK;
   gQuit = 1;
   return ret;
}

int QuitCommand::execute(ossSocket &sock,std::vector<std::string> &argVec)
{
        int ret=EDB_OK;
        if(!sock.isConnected()) return getError(EDB_SOCK_NOT_CONNECT);
        ret=sendOrder(sock,0);
        sock.close();
        ret=handleReply();
        return ret;
}

int HelpCommand::execute( ossSocket & sock, std::vector<std::string> & argVec )
{
   int ret = EDB_OK;
   printf("List of classes of commands:\n\n");
   printf("%s [server] [port]-- connecting mydb server\n", COMMAND_CONNECT);
   printf("%s -- sending a insert command to mydb server\n", COMMAND_INSERT);
   printf("%s -- sending a query command to mydb server\n", COMMAND_QUERY);
   printf("%s -- sending a delete command to mydb server\n", COMMAND_DELETE);
   printf("%s [number]-- sending a test command to mydb server\n", COMMAND_TEST);
   printf("%s -- providing current number of record inserting\n", COMMAND_SNAPSHOT);
   printf("%s -- quitting command\n\n", COMMAND_QUIT);
   printf("Type \"help\" command for help\n");
   return ret;
}

