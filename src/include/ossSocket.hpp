#ifndef OSSSOCKET_HPP_
#define OSSSOCKET_HPP_

#include "core.hpp"

//替换errno number
#define SOCKET_GETLASTERROR errno


#define OSS_SOCKET_DFT_TIMEOUT 10000

//最大 hostname
#define OSS_MAX_HOSTNAME NI_MAXHOST
#define OSS_MAX_SERVICENAME NI_MAXSERV

class _ossSocket
{
private:
	int _fd;				//文件描述符
	socklen_t _addressLen;			//本地地址长度
	socklen_t _peerAddressLen;		//对方地址长度
	struct sockaddr_in _sockAddress;	//本地地址
	struct sockaddr_in _peerAddress;	//地方地址
	bool _init;				//是否初始化
	int _timeout;				//超时

protected:
	unsigned int _getPort(sockaddr_in *addr);	//获取端口
	int _getAddress(sockaddr_in *addr,char *pAddress,unsigned int length);//获取地址

public:
	int setSocketLi(int lOnOff,int linger);
	void setAddress(const char *pHostName,unsigned int port);	//设置地址

	//listening socket
	_ossSocket();
	_ossSocket(unsigned int port,int timeout=0);	
	
	//connecting socket
	_ossSocket(const char *pHostname,unsigned int port,int timeout=0);
	
	_ossSocket(int *sock,int timeout=0);//已存在的socket
	~_ossSocket()
	{
		close();
	}
	int initSocket();	//初始化
	int bind_listen();
	bool isConnected();
	int send(const char *pMsg,int len,int timeout=OSS_SOCKET_DFT_TIMEOUT,int flags=0);
	int recv(char *pMsg,int len,int timeout=OSS_SOCKET_DFT_TIMEOUT,int flags=0);	//满足长度的receive
	int recvNF(char *pMsg,int len,int timeout=OSS_SOCKET_DFT_TIMEOUT);	//传多少接受多少
	int connect();
	void close();
	int accept(int *sock,struct sockaddr *addr,socklen_t *addrlen,int timeout=OSS_SOCKET_DFT_TIMEOUT);

	//TCP协议中往往把多个小包打包成打包防止多次封装解封，但数据库的实时性要求我们关闭这个功能
	int disableNagle();

	unsigned int getPeerPort();	//得到对方端口
	int getPeerAddress(char *pAddress,unsigned int length);	//得到对方地址
	unsigned int getLocalPort();
	int getLocalAddress(char *pAddress,unsigned int length);
	int setTimeout(int seconds);
	static int getHostName(char *pName,int nameLen);
	//将服务名转换为端口号  vim /etc/services 端口号是2字节的
	static int getPort(const char *pServiceName,unsigned short &port);	
};
typedef class _ossSocket ossSocket;


#endif
