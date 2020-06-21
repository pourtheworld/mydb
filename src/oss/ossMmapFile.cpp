#include "ossMmapFile.hpp"
#include "pd.hpp"

using namespace std;

int _ossMmapFile::open(const char *pFilename,unsigned int options)
{
    int rc=EDB_OK;

    _mutex.get();
    rc=_fileOp.Open(pFilename,options);
    if(EDB_OK==rc)  _opened=true;
    else
    {
        PD_LOG ( PDERROR, "Failed to open file, rc = %d",
               rc ) ;
      goto error ;
    }

    strncpy(_fileName,pFilename,sizeof(_fileName));
done:
    _mutex.release();
    return rc;
error:
    goto done;
}

void _ossMmapFile::close()
{
    _mutex.get();
    for(vector<_ossMmapSegment>::iterator i=_segments.begin();i!=_segments.end();++i)
    {   //segment i强转成void*，并得到其指向内存的指针；将后续length解映射
        munmap((void*)(*i)._ptr,(*i)._length);
    }

    _segments.clear();  //将segment vector清空

    if(_opened)
    {
        _fileOp.Close();
        _opened=false;
    }
    _mutex.release();
}

int _ossMmapFile::map(unsigned long long offset,unsigned int length,void **pAddress)
{
    _mutex.get();
    int rc=EDB_OK;
    ossMmapSegment seg(0,0,0);//指针 长度 偏移
    unsigned long long fileSize=0;
    void *segment=NULL;
    if(0==length)   goto done;
    rc=_fileOp.getSize((off_t*)&fileSize);   //获得该文件长度
    if(rc)
    {
        PD_LOG ( PDERROR,
               "Failed to get file size, rc = %d", rc ) ;
      goto error ;
    }
    if ( offset + length > fileSize )   //偏移加上长度大于文件长度
    {
      PD_LOG ( PDERROR,
               "Offset is greater than file size" ) ;
      rc = EDB_INVALIDARG ;
      goto error ;
    }

    //NULL 映射内存的位置随意
    //length 需要映射多长
    //可读可写
    //shared读写部分全公开，private只有读公开
    //文件句柄
    //文件偏移
    segment=mmap(NULL,length,PROT_READ|PROT_WRITE,MAP_SHARED,_fileOp.getHandle(),offset);

    if ( MAP_FAILED == segment )
    {
      PD_LOG ( PDERROR,
               "Failed to map offset %ld length %d, errno = %d",
               offset, length, errno ) ;
      if ( ENOMEM == errno )
         rc = EDB_OOM ;
      else if ( EACCES == errno )
         rc = EDB_PERM ;
      else
         rc = EDB_SYS ;
      goto error ;
    }

    seg._ptr=segment;
    seg._length = length ;
    seg._offset = offset ;
    _segments.push_back ( seg ) ;
    if ( pAddress )
      *pAddress = segment ; //最后这个内存映射的指针通过pAddress传出。
done :
   _mutex.release () ;
   return rc ;
error :
   goto done ;


}