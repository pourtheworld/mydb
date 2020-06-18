#include "ossSocket.hpp"
#include "pd.hpp"

//listening socket
_ossSocket::_ossSocket(unsigned int port,int timeout)
{
        //初始化信息
        _init=false;
        _fd=0;
        _timeout=timeout;

        //清空本机及对方地址
        memset(&_sockAddress,0,sizeof(sockaddr_in));
        memset(&_peerAddress,0,sizeof(sockaddr_in));

        //设置对方地址长度，本机选择IPV4,接受任意地址
        _peerAddressLen=sizeof(_peerAddress);
        _sockAddress.sin_family=AF_INET;

        //htonl 将主机的长整型转换成网络字节顺序
        //htons 将主机的整形转换成网络字节顺序
        _sockAddress.sin_addr.s_addr=htonl(INADDR_ANY);
        _sockAddress.sin_port=htons(port);
        _addressLen=sizeof(_sockAddress);
}

//创建socket
_ossSocket::_ossSocket()
{
        _init=false;
        _fd=0;
        _timeout=0;

        memset(&_sockAddress,0,sizeof(sockaddr_in));
        memset(&_peerAddress,0,sizeof(sockaddr_in));

        _peerAddressLen=sizeof(_peerAddress);
        _addressLen=sizeof(_sockAddress);

}


//connecting socket
_ossSocket::_ossSocket(const char *pHostname,unsigned int port,int timeout)
{
        struct hostent *hp;
        _init=false;
        _fd=0;
        _timeout=timeout;

        memset(&_sockAddress,0,sizeof(sockaddr_in));
        memset(&_peerAddress,0,sizeof(sockaddr_in));

        _peerAddressLen=sizeof(_peerAddress);
        _sockAddress.sin_family=AF_INET;

        //如果对方地址符合ipv4，那么直接通过hostent实例的方法接受该地址
        if((hp=gethostbyname(pHostname)))
                _sockAddress.sin_addr.s_addr=*((int*)hp->h_addr_list[0]);
        else    //将字符串形式的IP地址转换为网络字节顺序整型值
                _sockAddress.sin_addr.s_addr=inet_addr(pHostname);
        _sockAddress.sin_port=htons(port);
        _addressLen=sizeof(_sockAddress);

}

//existing socket
_ossSocket::_ossSocket(int *sock,int timeout)
{
        //可能的错误编码
        int rc=EDB_OK;

        //已经存在的socket
        _fd=*sock;
        _init =true;
        _timeout=timeout;
        _addressLen=sizeof(_sockAddress);

        //只需要清空对方的地址
        memset(&_peerAddress,0,sizeof(sockaddr_in));
        _peerAddressLen=sizeof(_peerAddress);

        //获取自己的地址
        rc=getsockname(_fd,(sockaddr*)&_sockAddress,&_addressLen);
        if(rc)
        {
		PD_LOG(PDERROR,"Failed to get sock name,error=%d",SOCKET_GETLASTERROR);
                _init=false;//如果错误初始化socket失败
        }
        else
        {       //获取对方的地址
                rc=getpeername(_fd,(sockaddr*)&_peerAddress,&_peerAddressLen);
		PD_RC_CHECK(rc,PDERROR,"Failed to get peer name,error=%d",SOCKET_GETLASTERROR);
        }
done:
	return;
error:
	goto done;

}


//初始化socket
//需要注意的是我们将创建socket的准备工作放到了前面去做
//具体的socket创建放到了init中
int _ossSocket::initSocket()
{
        int rc=EDB_OK;
        if(_init) goto done;

        memset(&_peerAddress,0,sizeof(sockaddr_in));
        _peerAddressLen=sizeof(_peerAddress);
        //创建socket SOCK_STREAM为有保障的,SOCK_DGRAM为无保障的
        _fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        if(-1==_fd)
        {
		PD_RC_CHECK(EDB_NETWORK,PDERROR,"Failed to initialize socket,error=%d",SOCKET_GETLASTERROR);
        }
        _init=true;
        setTimeout(_timeout);
done:
        return rc;
error:
        goto done;
}

//socket的linger，为close以后等待缓冲区发送晚的驻留时间
//l_onoff为是否发送完，l_linger为驻留时间
int _ossSocket::setSocketLi(int lOnOff,int linger)
{
        int rc=EDB_OK;
        struct linger _linger;
        _linger.l_onoff=lOnOff;
        _linger.l_linger=linger;

        //setsockopt，SOL_SOCKET代表需要在socket层面上进行设置，后面跟需要设置的内容
        rc=setsockopt(_fd,SOL_SOCKET,SO_LINGER,(const char*)&_linger,sizeof(_linger));
        return rc;
}

//与connecting socket类似
void _ossSocket::setAddress(const char *pHostname,unsigned int port)
{
        struct hostent *hp;

        memset(&_sockAddress,0,sizeof(sockaddr_in));
        memset(&_peerAddress,0,sizeof(sockaddr_in));

        _addressLen=sizeof(_sockAddress);
        _peerAddressLen=sizeof(_peerAddress);

        _sockAddress.sin_family=AF_INET;
        if((hp=gethostbyname(pHostname)))
                _sockAddress.sin_addr.s_addr=*((int*)hp->h_addr_list[0]);
        else
                _sockAddress.sin_addr.s_addr=inet_addr(pHostname);
        _sockAddress.sin_port=htons(port);

}


int _ossSocket::bind_listen()
{
        int rc=EDB_OK;
        int temp=1;

        //设置地址可重用
        rc=setsockopt(_fd,SOL_SOCKET,SO_REUSEADDR,(char*)&temp,sizeof(int));

        if(rc)	PD_LOG(PDWARNING,"Failed to setsocktopt SO_REUSEADDR,rc=%d",SOCKET_GETLASTERROR); 


        //设置linger时间
        rc=setSocketLi(1,30);
        if(rc)	PD_LOG(PDWARNING,"Failed to setsocktopt SO_LINGER,rc=%d",SOCKET_GETLASTERROR); 

        //进行bind,需要fd,本机地址，地址长度
        rc=::bind(_fd,(struct sockaddr*)&_sockAddress,_addressLen);
        if(rc)
        {
		PD_RC_CHECK(EDB_NETWORK,PDERROR,"Failed to bind socket,rc=%d",SOCKET_GETLASTERROR);
        }

        //进行listen,需要fd，最大连接数
        rc=listen(_fd,SOMAXCONN);
        if(rc)
        {
		PD_RC_CHECK(EDB_NETWORK,PDERROR,"Failed to listen socket,rc=%d",SOCKET_GETLASTERROR);
        }
done:
        return rc;
error:
        close();
        goto done;
}

//send  
int _ossSocket::send(const char *pMsg,int len,int timeout,int flags)
{
        int rc=EDB_OK;
        int maxFD=_fd;
        struct timeval maxSelectTime;
        //fd_set用于select，实际上是一个数组，检查哪个句柄可读
        fd_set fds;

        maxSelectTime.tv_sec=timeout/1000000;
        maxSelectTime.tv_usec=timeout%1000000;

        //len为0则返回
        if(0==len)      return EDB_OK;

        //给一个循环用于等待socket就绪
        while(true)
        {
                FD_ZERO(&fds);  //将set中清零，无fd
                FD_SET(_fd,&fds);//将_fd加入set

                //select非阻塞轮询函数，根据对应的poll判断是否有资源可用(可读或者可写)
                //需要检查的文件描述字个数，用于检查可读性的一组文件描述字，可写性...，异常...，
                //超时，NULL为阻塞，0为非阻塞，其他为超时时间
                rc=select(maxFD+1,NULL,&fds,NULL,timeout>=0?&maxSelectTime:NULL);

                //select超时
                if(0==rc)
                {
                        rc=EDB_TIMEOUT;
                        goto done;
                }

                if(0>rc)
                {
                        //错误是中断则继续select
                        if(EINTR==rc)
                                continue;
			PD_RC_CHECK(EDB_NETWORK,PDERROR,"Failed to select from socket,rc=%d",rc);
                }
                //检查fd是否已经在set中
                if(FD_ISSET(_fd,&fds))
                        break;
        }

        while(len>0)
        {       //连接断开时，系统会返回一个SIGPIPE。我们设置成不要返回。
                rc=::send(_fd,pMsg,len,MSG_NOSIGNAL|flags);
                if(-1==rc)
                {
			PD_RC_CHECK(EDB_NETWORK,PDERROR,"Failed to send,rc=%d",SOCKET_GETLASTERROR);
                }
                //每次发送后，待发送的长度减去rc；
                len-=rc;
                //在发送缓冲区后加上已发送长度。
                pMsg+=rc;
        }
        rc=EDB_OK;
done:
        return rc;
error:
        goto done;
}

//检查是否已连接，其实就是send一个空字符串看返回值。
bool _ossSocket::isConnected()
{
        int rc=EDB_OK;
        rc=::send(_fd,"",0,MSG_NOSIGNAL);
        if(0>rc)
                return false;
        return true;
}


#define MAX_RECV_RETRIES 5      //中断重连的次数
int _ossSocket::recv(char *pMsg,int len,int timeout,int flags)
{
        int rc=EDB_OK;
        int retries=0;
        int maxFD=_fd;
        struct timeval maxSelectTime;
        fd_set fds;

        maxSelectTime.tv_sec=timeout/1000000;
        maxSelectTime.tv_usec=timeout%1000000;

        if(0==len)      return EDB_OK;

        while(true)
        {
                FD_ZERO(&fds);  //将set中清零，无fd
                FD_SET(_fd,&fds);//将_fd加入set

                rc=select(maxFD+1,NULL,&fds,NULL,timeout>=0?&maxSelectTime:NULL);

                if(0==rc)
                {
                        rc=EDB_TIMEOUT;
                        goto done;
                }

                if(0>rc)
                {
                        rc=SOCKET_GETLASTERROR;
                        if(EINTR==rc)
                                continue;
			PD_RC_CHECK(EDB_NETWORK,PDERROR,"Failed to select from socket,rc=%d",rc);
                }
                if(FD_ISSET(_fd,&fds))
                        break;
        }

        while(len>0)
        {
                rc=::recv(_fd,pMsg,len,MSG_NOSIGNAL|flags);
                if(rc>0)
                {       //MSG_PEEK,读取数据包的时候，不会把该数据包从缓存队列中删除
                        if(flags & MSG_PEEK)    goto done;
                        len-=rc;
                        pMsg+=rc;
                }
                else if(rc==0)
                {
			PD_RC_CHECK(EDB_NETWORK_CLOSE,PDWARNING,"Peer unexpected shutdown");
                }
                else
                {       //EAGAIN代表在非阻塞模式下调用了阻塞操作，EWOULDBLOCK是在windows下的EAGAIN
                        rc=SOCKET_GETLASTERROR;
                        if((EAGAIN==rc||EWOULDBLOCK==rc)&&_timeout>0)
                        {
				PD_RC_CHECK(EDB_NETWORK,PDERROR,"Recv() timeout: rc=%d",rc);
                        }
                        //EINTR操作被中断唤醒
                        if((EINTR==rc)&&(retries<MAX_RECV_RETRIES))
                        {
                                retries++;
                                continue;
                        }
                        printf("Recv() Failed: rc=%d",rc);
                        rc=EDB_NETWORK;
                        goto error;
                }
        }
        rc=EDB_OK;
done:
        return rc;
error:
        goto done;

}

int _ossSocket::recvNF(char *pMsg,int len,int timeout)
{
        int rc=EDB_OK;
        int retries=0;
        int maxFD=_fd;
        struct timeval maxSelectTime;
        fd_set fds;

        maxSelectTime.tv_sec=timeout/1000000;
        maxSelectTime.tv_usec=timeout%1000000;

        if(0==len)      return EDB_OK;

        while(true)
        {
                FD_ZERO(&fds);  //将set中清零，无fd
                FD_SET(_fd,&fds);//将_fd加入set

                rc=select(maxFD+1,NULL,&fds,NULL,timeout>=0?&maxSelectTime:NULL);

                if(0==rc)
                {
                        rc=EDB_TIMEOUT;
                        goto done;
                }

                if(0>rc)
                {
                        rc=SOCKET_GETLASTERROR;
                        if(EINTR==rc)
                                continue;
			PD_RC_CHECK(EDB_NETWORK,PDERROR,"Failed to select from socket,rc=%d",rc);
                }
                if(FD_ISSET(_fd,&fds))
                        break;
        }
        //前面与recv是一样的


        rc=::recv(_fd,pMsg,len,MSG_NOSIGNAL);
        if(rc>0)        len=rc;
        else if(rc==0)
        {
		PD_RC_CHECK(EDB_NETWORK_CLOSE,PDWARNING,"Peer unexpected shutdown");
        }
        else
        {
                rc=SOCKET_GETLASTERROR;
                if((EAGAIN==rc||EWOULDBLOCK==rc)&&_timeout>0)
                {
			PD_RC_CHECK(EDB_NETWORK,PDERROR,"Recv() timeout: rc=%d",rc);
                }

                //EINTR操作被中断唤醒
                if((EINTR==rc)&&(retries<MAX_RECV_RETRIES))
                {
                        retries++;
                }
		PD_RC_CHECK(EDB_NETWORK,PDERROR,"Recv() Failed: rc=%d",rc);
        }
        rc=EDB_OK;
done:
        return rc;
error:
        goto done;
}

int _ossSocket::connect()
{
        int rc=EDB_OK;

        //建立连接
        rc=::connect(_fd,(struct sockaddr *)&_sockAddress,_addressLen);
        if(rc)
        {
		PD_RC_CHECK(EDB_NETWORK,PDERROR,"Failed to connect,rc=%d",SOCKET_GETLASTERROR);
        }

        //获取本机地址
        rc=getsockname(_fd,(sockaddr*)&_sockAddress,&_addressLen);
        if(rc)
        {
		PD_RC_CHECK(EDB_NETWORK,PDERROR,"Failed to get local address,rc=%d",rc);
        }

        //获取对方地址
        rc=getpeername(_fd,(sockaddr*)&_peerAddress,&_peerAddressLen);
        if(rc)
        {
		PD_RC_CHECK(EDB_NETWORK,PDERROR,"Failed to get peer address,rc=%d",rc);
        }
done:
        return rc;
error:
        goto done;

}

void _ossSocket::close()
{
        if(_init)
        {
                int i=0;
                i=::close(_fd);
                if(i<0) i=-1;
                _init=false;
        }
}

int _ossSocket::accept(int *sock,struct sockaddr *addr,socklen_t *addrlen,int timeout)
{
        int rc=EDB_OK;
        int maxFD=_fd;
        struct timeval maxSelectTime;
        fd_set fds;

        maxSelectTime.tv_sec=timeout/1000000;
        maxSelectTime.tv_usec=timeout%1000000;


        while(true)
        {
                FD_ZERO(&fds);  //将set中清零，无fd
                FD_SET(_fd,&fds);//将_fd加入set

                rc=select(maxFD+1,&fds,NULL,NULL,timeout>=0?&maxSelectTime:NULL);

                if(0==rc)
                {
			*sock=0;
                        rc=EDB_TIMEOUT;
                        goto done;
                }

                if(0>rc)
                {
                        rc=SOCKET_GETLASTERROR;
                        if(EINTR==rc)
                                continue;
			PD_RC_CHECK(EDB_NETWORK,PDERROR,"Failed to select from socket,rc=%d",SOCKET_GETLASTERROR);
                }
                if(FD_ISSET(_fd,&fds))
                        break;
        }

        //上面与send的检查一致
        rc=EDB_OK;
        *sock=::accept(_fd,addr,addrlen);
        if(-1==*sock)
        {
		 PD_RC_CHECK(EDB_NETWORK,PDERROR,"Failed to accept socket,rc=%d",SOCKET_GETLASTERROR);
        }
done:
        return rc;
error:
        close();
        goto done;
}

//防止多个小包打包成大包
int _ossSocket::disableNagle()
{
        int rc=EDB_OK;
        int temp=1;
        rc=setsockopt(_fd,IPPROTO_TCP,TCP_NODELAY,(char*)&temp,sizeof(int));

        if(rc) PD_LOG(PDWARNING,"Failed to setsockopt,rc=%d",SOCKET_GETLASTERROR);

        rc=setsockopt(_fd,SOL_SOCKET,SO_KEEPALIVE,(char*)&temp,sizeof(int));

        if(rc)	PD_LOG(PDWARNING,"Failed to setsockopt,rc=%d",SOCKET_GETLASTERROR);
       
 	 return rc;
}

unsigned int _ossSocket::_getPort(sockaddr_in *addr)
{
        return ntohs(addr->sin_port);
}

int _ossSocket::_getAddress(sockaddr_in *addr,char *pAddress,unsigned int length)
{
        int rc=EDB_OK;
        length=length<NI_MAXHOST?length:NI_MAXHOST;
        //getnameinfo以一个socket地址为参数，返回描述主机和服务的字符串
        //sockaddr,socklen,host,hostlen,serv,servlen,flag
        //NI_NUMERICHOST表示返回包含地址的梳子形式而不是名称
        rc=getnameinfo((struct sockaddr*)addr,sizeof(sockaddr),pAddress,length,NULL,0,NI_NUMERICHOST);
        if(rc)
        {
		PD_RC_CHECK(EDB_NETWORK,PDERROR,"Failed to getnameinfo,rc=%d",SOCKET_GETLASTERROR);
        }
done:
        return rc;
error:
        goto done;
}

unsigned int _ossSocket::getLocalPort()
{       return _getPort(&_sockAddress); }

unsigned int _ossSocket::getPeerPort()
{       return _getPort(&_peerAddress); }

int _ossSocket::getLocalAddress(char *pAddress,unsigned int length)
{       return _getAddress(&_sockAddress,pAddress,length); }

int _ossSocket::getPeerAddress(char *pAddress,unsigned int length)
{       return _getAddress(&_peerAddress,pAddress,length);}

int _ossSocket::setTimeout(int seconds)
{
        //windows把毫秒作为参数，linux把timeval作为参数
        int rc=EDB_OK;
        struct timeval tv;
        tv.tv_sec=seconds;
        tv.tv_usec=0;

        rc=setsockopt(_fd,SOL_SOCKET,SO_RCVTIMEO,(char*)&tv,sizeof(tv));
               
        if(rc) PD_LOG(PDWARNING,"Failed to setsockopt,rc=%d",SOCKET_GETLASTERROR);

        rc=setsockopt(_fd,SOL_SOCKET,SO_SNDTIMEO,(char*)&tv,sizeof(tv));

        if(rc)	PD_LOG(PDWARNING,"Failed to setsockopt,rc=%d",SOCKET_GETLASTERROR);

        return rc;
}

int _ossSocket::getHostName(char *pName,int nameLen)
{
        return gethostname(pName,nameLen);
}

int _ossSocket::getPort(const char *pServiceName,unsigned short &port)
{
        int rc=EDB_OK;
        struct servent *servinfo;
        servinfo=getservbyname(pServiceName,"tcp");
        //如果servinfo存在的话，从s_port拿出端口并通过ntohs转成数字。
        //不存在则尝试atoi转成整数
        if(!servinfo)   port=atoi(pServiceName);
        else    port=(unsigned short)ntohs(servinfo->s_port);
        return rc;
}

