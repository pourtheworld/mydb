#include"core.hpp"
#include"ossPrimitiveFileOp.hpp"

ossPrimitiveFileOp::ossPrimitiveFileOp()
{	//构造函数中我们把句柄先设置为非法值，在open中打开
	_fileHandle=OSS_INVALID_HANDLE_FD_VALUE;
	//把是否为输出流也设置为false
	_bIsStdout=false;
}

//句柄是否为非法值
bool ossPrimitiveFileOp::isValid(){	return (OSS_INVALID_HANDLE_FD_VALUE!=_fileHandle);}

void ossPrimitiveFileOp::Close()
{
	//如果当前文件正常且不是输出流，则正常关闭并把句柄设置为非法值
	if(isValid()&&(!_bIsStdout))
	{
		oss_close(_fileHandle);
		_fileHandle=OSS_INVALID_HANDLE_FD_VALUE;
	}
	//如果为输出流或者不正常句柄，则不操作
}

int ossPrimitiveFileOp::Open(const char* pFilePath,unsigned int options)
{
	int rc=0;
	//默认文件模式为读写状态
	int mode=O_RDWR;

	if(options&OSS_PRIMITIVE_FILE_OP_READ_ONLY)	mode=O_RDONLY;//只读
	else if(options&OSS_PRIMITIVE_FILE_OP_WRITE_ONLY)	mode=O_WRONLY;//只写

	if(options&OSS_PRIMITIVE_FILE_OP_OPEN_EXISTING)	{}//已存在保持原样
	else if(options&OSS_PRIMITIVE_FILE_OP_OPEN_ALWAYS)	mode|=O_CREAT;//不存在的话创建
	
	if(options&OSS_PRIMITIVE_FILE_OP_OPEN_TRUNC)	mode|=O_TRUNC;	//截断的话每次从0开始

	do
	{
		_fileHandle=oss_open(pFilePath,mode,0644);//设置为0644，用户可读写，组等只读
	}while((-1==_fileHandle)&&(EINTR==errno));//中断继续打开

	if(_fileHandle<=OSS_INVALID_HANDLE_FD_VALUE)
	{
		rc=errno;
		goto exit;
	}

exit:
	return rc;
}

//打开输出流文件
void ossPrimitiveFileOp::openStdout()
{
	setFileHandle(STDOUT_FILENO);//将文件描述符通过stdout方式打开
	_bIsStdout=true;//是否为输出流置为是
}

//获取当前文件偏移
offsetType ossPrimitiveFileOp::getCurrentOffset() const
{
	return oss_lseek(_fileHandle,0,SEEK_CUR);//SEEK_CUR
}

void ossPrimitiveFileOp::seekToOffset(offsetType offset)
{	//给定一个offset，通过SEEK_SET宏定位。
	if((oss_off_t)-1!=offset)	oss_lseek(_fileHandle,offset,SEEK_SET);
}

void ossPrimitiveFileOp::seekToEnd(void)
{
	oss_lseek(_fileHandle,0,SEEK_END);
}

//读取总长度，待读取缓存，当前读到的位置
int ossPrimitiveFileOp::Read(const size_t size,void* const pBuffer,int* const pBytesRead)
{
	int retval=0;
	ssize_t bytesRead=0;
	if(isValid())
	{
		do
		{
			bytesRead=oss_read(_fileHandle,pBuffer,size);
		}while((-1==bytesRead)&&(EINTR==errno));//当中断而没读到则循环
		if(-1==bytesRead)	goto err_read;//读完了文件描述符还是-1则出错
	}
	else	goto err_read;
	
	if(pBytesRead)	*pBytesRead=bytesRead;//当前读到的位置
exit:
	return retval;
err_read:
	*pBytesRead=0;
	retval=errno;
	goto exit;
}

int ossPrimitiveFileOp::Write(const void* pBuffer,size_t size)
{
	int rc=0;
	size_t currentSize=0;
	if(0==size)	size=strlen((char*)pBuffer);//如果没有传进bBuffer的长度，则自己读取
	
	if(isValid())
	{
		do
		{	//当前写到哪了，就继续读size-当前位置的长度
			rc=oss_write(_fileHandle,&((char*)pBuffer)[currentSize],size-currentSize);
			if(rc>=0)	currentSize+=rc;
		}while(((-1==rc)&&(EINTR==errno))||((-1!=rc)&&(currentSize!=size)));//一种情况是中断没写进
		//还有一种情况是写进了但没写完
		if(-1==rc)
		{
			rc=errno;
			goto exit;
		}
		rc=0;
	}
exit:
	return rc;
}

//标准格式写入
int ossPrimitiveFileOp::fWrite(const char* format,...)
{
	int rc=0;
	va_list ap;//可变参数list
	char buf[OSS_PRIMITIVE_FILE_OP_FWRITE_BUF_SIZE]={0};//默认2048字节

	va_start(ap,format);//ap从format开始
	vsnprintf(buf,sizeof(buf),format,ap);//待写入的地址，地址空间长度，待写入内容，可能有的待写入内容
	va_end(ap);//ap结束
	
	rc=Write(buf);

	return rc;
}

//设置文件句柄
void ossPrimitiveFileOp::setFileHandle(handleType handle){	_fileHandle=handle;	}

//得到文件长度：先设置一个_stati64的结构体，再通过_fstati64函数，根据提供的文件描述符，定位到文件并放入结构体中。
//再通过结构体的st_size获得文件长度。
int ossPrimitiveFileOp::getSize(offsetType* const pFileSize)
{
	int rc=0;
	//回到ossPrimitiveFileOp.hpp,oss_struct_stat->struct _stati64,文件信息结构体，由文件描述符+buffer组成
	oss_struct_stat buf={0};
	
	if(-1==oss_fstat(_fileHandle,&buf))
	//_fstati64获取文件信息，参数为文件描述符+struct _stati64，长度保存在结构体的st_size中
	{
		rc=errno;
		goto err_exit;
	}

	*pFileSize=buf.st_size;

exit:
	return rc;

err_exit:
	*pFileSize=0;
	goto exit;
}
