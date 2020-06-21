#ifndef DMS_HPP_
#define DMS_HPP_

#include    "ossLatch.hpp"
#include    "ossMmapFile.hpp"
#include    "bson.h"
#include    "dmsRecord.hpp"
//include    "ixmBucket.hpp"
#include    <vector>

#define DMS_EXTEND_SIZE 65536//文件每次扩张的最小长度
#define DMS_PAGESIZE    4194304//数据页的大小为4M
//记录最大SIZE=页大小-文件对应每个页头-记录头-slot大小
#define DMS_MAX_RECORD (DMS_PAGESIZE-sizeof(dmsHeader)-sizeof(dmsRecord)-sizeof(SLOTOFF))
#define DMS_MAX_PAGES   262144//页个数 256K 
//256K *   4M=1T
typedef unsigned int SLOTOFF;   //slot类型
#define DMS_INVALID_SLOTID  0xFFFFFFFF
#define DMS_INVALID_PAGEID  0xFFFFFFFF

#define DMS_KEY_FIELDNAME   "_id"   //类似于Mongo的key，唯一_id

extern const char* gKeyFieldName;

//记录头部flag状态宏
#define DMS_RECORD_FLAG_NORMAL  0
#define DMS_RECORD_FLAG_DROPPED 1

//记录
struct dmsRecord
{   //记录头
    unsigned int _size;
    unsigned int _flag;
    //记录开始
    char _data[0];  //message里曾经用过，标识了记录真正开始的位置
};

//当前文件标识
#define DMS_HEADER_EYECATCHER   "DMSH"
#define DMS_HEADER_EYECATCHER_LEN   4
//当前数据库状态
#define DMS_HEADER_FLAG_NORMAL  0
#define DMS_HEADER_FLAG_DROPPED 1
//当前数据库版本
#define DMS_HEADER_VERSION_0 0
#define DMS_HEADER_VERSION_CURRENT DMS_HEADER_VERSION_0

//当前文件头
struct dmsHeader
{
    char _eyeCatcher[DMS_HEADER_EYECATCHER_LEN];
    unsigned int _size; //数据页数量
    unsigned int _flag;
    unsigned int _version;
};

/*********************************************************
页结构
-------------------------
| PAGE HEADER           |
-------------------------
| Slot List             |
-------------------------
| Free Space            |
-------------------------
| Data                  |
-------------------------
**********************************************************/

//页的表示
#define DMS_PAGE_EYECATCHER "PAGH"
#define DMS_PAGE_EYECATCHER_LEN 4
//当前页状态
#define DMS_PAGE_FLAG_NORMAL    0
#define DMS_PAGE_FLAG_UNALLOC   1
#define DMS_SLOT_EMPTY  0xFFFFFFFF

struct dmsPageHeader
{
    char _eyeCatcher[DMS_PAGE_EYECATCHER_LEN];
    unsigned int _size; //页大小
    unsigned int _flag; //页状态
    unsigned int _numSlots; //slot个数
    unsigned int _slotOffset; //slot开始偏移
    unsigned int _freeSpace;    //空闲空间的大小
    unsigned int _freeOffset;   //空闲空间的偏移
    char _data[0];
};

#define DMS_FILE_SEGMENT_SIZE 134217728 //数据段默认128M
//实际上我们的文件头只有16字节，但是后续文件头部可能会由更多信息，我们定义成64K
#define DMS_FILE_HEADER_SIZE    65536  
//每个数据段的数据页数 128M/4M=32
#define DMS_PAGES_PER_SEGMENT   (DMS_FILE_SEGMENT_SIZE/DMS_PAGESIZE)
//最大的数据段数 256K/32=8192
#define DMS_MAX_SEGMENTS    (DMS_MAX_PAGES/DMS_PAGES_PER_SEGMENT)


class dmsFile:public ossMmapFile
{
private:
    dmsHeader   *_header;   //文件头
    std::vector<char *> _body;  //文件多个段的起始地址

    //空闲空间的映射 由需要页的大小映射到PAGEID
    std::multimap<unsigned int,PAGEID>  _freeSpaceMap;

    ossSLatch   _mutex;
    ossXLatch   _extendMutex;
    char *_pFileName;
    //ixmBucketManager *_ixmBucketMgr;
public:
    dmsFile();
        ~dmsFile();

    //包括打开文件+装载文件 initNew和LoadData
    int initialize(const char *pFileName);

    //插入记录，第一个形参为record在磁盘中的地址
    //第二个形参为内存映射后再内存中的地址，用于索引。它们内容一致
    //第三个为RID，记录插入的页和slot
    int insert(bson::BSONObj &record,bson::BSONObj &outRecord,dmsRecordID &rid);
    int remove(dmsRecordID &rid);
    int find(dmsRecordID &rid,bson::BSONObj &result);

private:
    int _extendSegment();   //增加段，映射到内存，页头加入到页，更新freeSpaceMap
    int _initNew();//初始化空文件，增加文件头，映射到内存
    int _extendFile(int size);  //扩展文件，给定扩展的大小
    //将segment映射到内存，更新_body，_freeSpaceMap,index
    int _loadData();
    //根据给定的RID，传出对应Page和slot的偏移
    int _searchSlot(char *page,dmsRecordID &recordID,SLOTOFF &slot);

    //对页的某些无用记录进行重组
    void _recoverSpace(char *page); 
    //给定changeSize，找到对应大小的page，更新freeSpaceMap
    void _updateFreeSpace(dmsPageHeader *header,int changeSize,PAGEID pageID);
    PAGEID _findPage(size_t requiredSize);

public:
    inline unsigned int getNumSegments(){return _body.size();}
    inline unsigned int getNumPages ()
    {
      return getNumSegments() * DMS_PAGES_PER_SEGMENT ;
    }
    //根据pageID把页映射到内存地址上
    inline char *pageToOffset(PAGEID pageID)
    {
        if(pageID>=getNumPages())   return NULL;
        //首先获得第几个段的起始地址；
        //取模得到该页是此段的第几个页。乘上每页的长度。得到在该段上的偏移。
        //最后相加得到该页所需的内存地址
        return _body[pageID/DMS_PAGES_PER_SEGMENT]+DMS_PAGESIZE*(pageID%DMS_PAGES_PER_SEGMENT);

    }

    //文件的大小是否有效
    inline bool validSize ( size_t size )
    {
      if ( size < DMS_FILE_HEADER_SIZE )
      {
         return false ;
      }
      size = size - DMS_FILE_HEADER_SIZE ;
      //去头以后是否是数据段的整数倍
      if ( size % DMS_FILE_SEGMENT_SIZE != 0 )
      {
         return false ;
      }
      return true ;
    }


};





#endif