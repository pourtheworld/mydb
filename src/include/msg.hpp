#ifndef MSG_HPP_
#define MSG_HPP_

#include "bson.h"

#define OP_REPLY        1
#define OP_INSERT       2
#define OP_DELETE       3
#define OP_QUERY        4
#define OP_COMMAND      5
//由于在网络层的disconnect需要PD_LOG,我们从通信层定义正常的关闭与连接
#define OP_DISCONNECT   6
#define OP_CONNECT      7
#define OP_SNAPSHOT     8

#define RETURN_CODE_STATE_OK 1

//各类消息的头结构是一样 4bytes消息长度+4bytes消息类型（为了内存对齐）
struct MsgHeader
{
    int messageLen;
    int opCode;
};

//回复类消息的主体为 返回值  返回的记录数
//data[0]为可变长度数组，本身可以不占内存
//在这里用于记录消息主体开始的位置
struct MsgReply
{
    MsgHeader header;
    int returnCode;
    int numReturn;
    char data[0];
};

//插入类消息的主体为 插入的记录数
struct MsgInsert
{
    MsgHeader header;
    int numInsert;
    char data[0];
};

//删除类消息没啥主体，不过key对应了待删除消息的键
struct MsgDelete
{
    MsgHeader header;
    char key[0];
};

struct MsgQuery
{
    MsgHeader header;
    char key[0];
};

//命令类消息要包含命令参数
struct MsgCommand
{
    MsgHeader header;
    int numArgs;;
    char data[0];
};

//接下来是各个消息类型的封装和拆封

//回复消息封装
//缓冲区，缓冲区长度，指定返回值，指定返回内容
int msgBuildReply(char **ppBuffer,int *pBufferSize,int returnCode,bson::BSONObj *objReturn);

//回复消息解封
//缓冲区，接受返回值，接受返回的记录数，获得内容
int msgExtractReply(char *pBuffer,int &returnCode,int &numReturn,const char **ppObjStart);

//插入消息的封装我们定义两个，区别在于内容上第二个为vector，可以指定多个BSON对象
int msgBuildInsert(char **ppBuffer,int *pBufferSize,bson::BSONObj &obj);
int msgBuildInsert(char **ppBuffer,int *pBufferSize,vector<bson::BSONObj*> &obj);

int msgExtractInsert ( char *pBuffer, int &numInsert, const char **ppObjStart ) ;

int msgBuildDelete ( char **ppBuffer, int *pBufferSize, bson::BSONObj &key ) ;

int msgExtractDelete  ( char *pBuffer, bson::BSONObj &key ) ;

int msgBuildQuery ( char **ppBuffer, int *pBufferSize, bson::BSONObj &key ) ;

int msgExtractQuery ( char *pBuffer, bson::BSONObj &key ) ;

int msgBuildCommand ( char **ppBuffer, int *pBufferSize, bson::BSONObj &obj ) ;

int msgBuildCommand ( char **ppBuffer, int *pBufferSize, vector<bson::BSONObj*>&obj ) ;

int msgExtractCommand ( char *pBuffer, int &numArgs, const char **ppObjStart ) ;

#endif
