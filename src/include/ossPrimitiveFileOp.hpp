#ifndef OSSPRIMITIVEFILEOP_HPP_
#define OSSPRIMITIVEFILEOP_HPP_

#include "core.hpp"

#ifdef _WINDOWS

#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define OSS_F_GETLK        F_GETLK64
#define OSS_F_SETLK        F_SETLK64
#define OSS_F_SETLKW       F_SETLKW64

#define oss_struct_statfs  struct statfs64
#define oss_statfs         statfs64
#define oss_fstatfs        fstatfs64
#define oss_struct_statvfs struct statvfs64
#define oss_statvfs        statvfs64
#define oss_fstatvfs       fstatvfs64
#define oss_struct_stat    struct _stati64
#define oss_struct_flock   struct flock64
#define oss_stat           stat64
#define oss_lstat          lstat64
#define oss_fstat          _fstati64
#define oss_open           _open
#define oss_lseek          _lseeki64
#define oss_ftruncate      ftruncate64
#define oss_off_t          __int64
#define oss_close          _close
#define oss_access         access
#define oss_chmod          
#define oss_read           read
#define oss_write          write

#define O_RDWR				_O_RDWR
#define O_RDONLY			_O_RDONLY
#define O_WRONLY			_O_WRONLY
#define O_CREAT				_O_CREAT
#define O_TRUNC				_O_TRUNC

#define OSS_HANDLE			int
#define OSS_INVALID_HANDLE_FD_VALUE (OSS_HANDLE(-1))
#else

#define OSS_HANDLE		   int
#define OSS_F_GETLK        F_GETLK64
#define OSS_F_SETLK        F_SETLK64
#define OSS_F_SETLKW       F_SETLKW64

#define oss_struct_statfs  struct statfs64
#define oss_statfs         statfs64
#define oss_fstatfs        fstatfs64
#define oss_struct_statvfs struct statvfs64
#define oss_statvfs        statvfs64
#define oss_fstatvfs       fstatvfs64
#define oss_struct_stat    struct stat64
#define oss_struct_flock   struct flock64
#define oss_stat           stat64
#define oss_lstat          lstat64
#define oss_fstat          fstat64
#define oss_open           open64
#define oss_lseek          lseek64
#define oss_ftruncate      ftruncate64
#define oss_off_t          off64_t
#define oss_close          close
#define oss_access         access
#define oss_chmod          chmod
#define oss_read           read
#define oss_write          write

#define OSS_INVALID_HANDLE_FD_VALUE (-1)

#endif // _WINDOWS

#define OSS_PRIMITIVE_FILE_OP_FWRITE_BUF_SIZE 2048
#define OSS_PRIMITIVE_FILE_OP_READ_ONLY     (((unsigned int)1) << 1)
#define OSS_PRIMITIVE_FILE_OP_WRITE_ONLY    (((unsigned int)1) << 2)
#define OSS_PRIMITIVE_FILE_OP_OPEN_EXISTING (((unsigned int)1) << 3)
#define OSS_PRIMITIVE_FILE_OP_OPEN_ALWAYS   (((unsigned int)1) << 4)
#define OSS_PRIMITIVE_FILE_OP_OPEN_TRUNC    (((unsigned int)1) << 5)


typedef oss_off_t offsetType;	//偏移类型

class ossPrimitiveFileOp
{
public:
	typedef OSS_HANDLE handleType;	//文件句柄的类型，就是文件描述符的类型，int
private:
	handleType _fileHandle;
	//定义私有化的拷贝复制函数
	ossPrimitiveFileOp(const ossPrimitiveFileOp&){}
	//重载=拷贝操作符函数，目的都是防止隐式的文件拷贝
	const ossPrimitiveFileOp &operator=(const ossPrimitiveFileOp &);
	bool _bIsStdout;//是一个标准输出的流还是一个文件流，用于下面的openStdout

protected:
	void setFileHandle(handleType handle);	//通过一个已知的文件，生成这样一个对象

public:
	ossPrimitiveFileOp();//构造的操作对象为空
	int Open(const char *pFilePath,unsigned int options=OSS_PRIMITIVE_FILE_OP_OPEN_ALWAYS);
	void openStdout();//将屏幕作为一个文件输出打开，向这个文件输入直接会到屏幕上，与_bIsStdout对应
	void Close();
	bool isValid(void);
	int Read(const size_t size,void* const pBuf,int* const pBytedRead);//向pBuf读取一定size，最后返回已读长度
	int Write(const void* pBuf,size_t len=0);
	int fWrite(const char* fmt,...);//给定一个格式，向文件写入
	offsetType getCurrentOffset (void) const;//得到当前偏移
	void seekToOffset(offsetType offset);	//寻找偏移
	void seekToEnd(void);//寻找到最后
	int getSize(offsetType* const pFileSize);
	handleType getHandle(void) const{	return _fileHandle;	}
};
#endif
