#ifndef OSSMMAPFILE_HPP_
#define OSSMMAPFILE_HPP_


#include    "core.hpp"
#include    "ossLatch.hpp"
#include    "ossPrimitiveFileOp.hpp"

class _ossMmapFile
{
protected:
    //一个文件可能需要映射到内存多次，因为当文件大小改变时
    //不能直接修改映射到内存的大小，需要重新映射一次
    class _ossMmapSegment
    {
    public:
        //文件段指向内存的指针
        void *_ptr;
        //文件需要映射的长度
        unsigned int _length;
        //文件所在位置的偏移
        unsigned long long _offset;
        _ossMmapSegment(void *ptr,unsigned int length,unsigned long long offset)
        {
            _ptr=ptr;
            _length=length;
            _offset=offset;
        }
    };
    typedef _ossMmapSegment ossMmapSegment;
    
    ossPrimitiveFileOp _fileOp;
    ossXLatch _mutex;
    bool _opened;   //文件是否开着
    std::vector<ossMmapSegment> _segments;
    char _fileName[OSS_MAX_PATHSIZE];

public:
    //定义一个映射段vector的迭代器
    typedef std::vector<ossMmapSegment>::const_iterator CONST_ITR;

    inline CONST_ITR begin()
    {
        return _segments.begin();
    }

    inline CONST_ITR end()
    {
        return _segments.end();
    }

    inline unsigned int segmentSize()
    {  
        return _segments.size();
    }
public:
    _ossMmapFile()
    {
        _opened=false;
        memset(_fileName,0,sizeof(_fileName));
    }
    ~_ossMmapFile()
    {
        close();
    }
    int open(const char *pFilename,unsigned int options);
    void close();
    int map(unsigned long long offset,unsigned int length,void **pAddress);
};
typedef class _ossMmapFile ossMmapFile;

#endif