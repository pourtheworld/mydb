#include "dms.hpp"
#include "pd.hpp"

using namespace bson;

const char *gKetyFieldName=DMS_KEY_FIELDNAME;


dmsFile::dmsFile():_header(NULL),_pFileName(NULL){}

dmsFile::~dmsFile()
{
    if(_pFileName)  free(_pFileName);
    close();    //mmap的各种段解映射
}

int dmsFile::insert(BSONObj &record,BSONObj &outRecord,dmsRecordID &rid)
{
    return EDB_OK;
}

int dmsFile::remove(dmsRecordID &rid)
{
    return EDB_OK;
}

int dmsFile::find(dmsRecordID &rid,BSONObj &result)
{
    return EDB_OK;
}

//将changeSize指定大小的页更新到freespace里（仅仅是映射更新）
void dmsFile::_updateFreeSpace(dmsPageHeader *header,
                               int changeSize,PAGEID pageID)
{   //首先根据给定的header获取需要page的freespace
    unsigned int freeSpace=header->_freeSpace;
    //pair保存了_freespacemap类型的迭代器
    std::pair<std::multimap<unsigned int,PAGEID>::iterator,
              std::multimap<unsigned int,PAGEID>::iterator> ret;
    //根据freeSpace的大小，在_freeSpaceMap中寻找符合key值的迭代器范围
    ret=_freeSpaceMap.equal_range(freeSpace);

    //起点为pair中PAGEID最小的迭代器first，终点为PAGEID最大的迭代器second
    for(std::multimap<unsigned int,PAGEID>::iterator it=ret.first;
        it!=ret.second;++it)
    {
        if(it->second==pageID)  //如果PAGEID和形参给定一致
        {   //我们先将这个映射抹去
            _freeSpaceMap.erase(it);
            break;
        }
    }

    freeSpace+=changeSize;
    header->_freeSpace=freeSpace;
    //将修改过的freespace重新加入映射
    _freeSpaceMap.insert(pair<unsigned int,PAGEID>(freeSpace,pageID));

}

int dmsFile::initialize(const char *pFileName)
{
    offsetType offset=0;
    int rc=EDB_OK;

    _pFileName=strdup(pFileName);   //复制文件名
    if ( !_pFileName )
    {
      rc = EDB_OOM ;
      PD_LOG ( PDERROR, "Failed to duplicate file name" ) ;
      goto error ;
    }

    //打开文件
    rc=open(_pFileName,OSS_PRIMITIVE_FILE_OP_OPEN_ALWAYS);
    PD_RC_CHECK ( rc, PDERROR, "Failed to open file %s, rc = %d",
                 _pFileName, rc ) ;
        
getfilesize:
    rc=_fileOp.getSize(&offset);//获得文件大小
    PD_RC_CHECK ( rc, PDERROR, "Failed to get file size, rc = %d",
                 rc ); 

    if(!offset) //获得文件大小失败
    {
        rc=_initNew();  //重新初始化文件
        PD_RC_CHECK ( rc, PDERROR, "Failed to initialize file, rc = %d",
                    rc ) ;
        goto getfilesize ;  //再去获得文件大小
    }

    rc=_loadData(); //装载数据
    rc = _loadData () ;
    PD_RC_CHECK ( rc, PDERROR, "Failed to load data, rc = %d", rc ) ;
done :
   return rc ;
error :
   goto done ;
}

int dmsFile::_extendSegment()
{
    int rc=EDB_OK;
    char *data=NULL;
    int freeMapSize=0;
    dmsPageHeader pageHeader;
    offsetType offset=0;

    //让我们先获得文件已经扩张的大小
    rc=_fileOp.getSize(&offset);
    PD_RC_CHECK ( rc, PDERROR, "Failed to get file size, rc = %d", rc ) ;

    rc=_extendFile(DMS_FILE_SEGMENT_SIZE);//增加128M
    PD_RC_CHECK ( rc, PDERROR, "Failed to extend segment rc = %d", rc ) ;

    //从原来的end到新的end这一段映射过去
    //调用mmap进行内存映射，data为内存映射的指针
    rc=map(offset,DMS_FILE_SEGMENT_SIZE,(void**)&data);
    PD_RC_CHECK ( rc, PDERROR, "Failed to map file, rc = %d", rc ) ;

    //接下来我们要将段中每一个页初始化
    //自然我们先把页的头给定义好
    strcpy(pageHeader._eyeCatcher,DMS_PAGE_EYECATCHER);
    pageHeader._size = DMS_PAGESIZE ;
    pageHeader._flag = DMS_PAGE_FLAG_NORMAL ;
    pageHeader._numSlots = 0 ;
    pageHeader._slotOffset = sizeof ( dmsPageHeader ) ;
    pageHeader._freeSpace = DMS_PAGESIZE - sizeof(dmsPageHeader) ;
    pageHeader._freeOffset = DMS_PAGESIZE ;

    //把页头给每一页补上
    for(int i=0;i<DMS_FILE_SEGMENT_SIZE;i+=DMS_PAGESIZE)//一共128M，每次增加4M
    {
        memcpy(data+i,(char*)&pageHeader,sizeof(dmsPageHeader));
    }

    //页的空闲空间和PAGEID的映射更新
    _mutex.get();
    freeMapSize=_freeSpaceMap.size();//为了获取映射PAGEID现在到哪了
    for(int i=0;i<DMS_PAGES_PER_SEGMENT;++i)
    {
        _freeSpaceMap.insert(pair<unsigned int,PAGEID>(pageHeader._freeSpace,i+freeMapSize));
    }

    //将新的segment放到Body中
    _body.push_back(data);
    _header->_size+=DMS_PAGES_PER_SEGMENT;
    _mutex.release();
done:
    return rc;
error:
    goto done;
}

//initialize发现size为0时进行
int dmsFile::_initNew()
{
    int rc=EDB_OK;
    rc=_extendFile(DMS_FILE_HEADER_SIZE);       //先扩展文件头部
    PD_RC_CHECK ( rc, PDERROR, "Failed to extend file, rc = %d", rc ) ;
    rc = map ( 0, DMS_FILE_HEADER_SIZE, ( void **)&_header ) ;  //头部映射到内存
    PD_RC_CHECK ( rc, PDERROR, "Failed to map, rc = %d", rc ) ;

    //初始头部信息
    strcpy ( _header->_eyeCatcher, DMS_HEADER_EYECATCHER ) ;
   _header->_size = 0 ;
   _header->_flag = DMS_HEADER_FLAG_NORMAL ;
   _header->_version = DMS_HEADER_VERSION_CURRENT ;

done :
   return rc ;
error :
   goto done ;
}

//根据需要的空间大小，在空闲空间的map中找到最合适的PAGE
PAGEID dmsFile::_findPage(size_t requiredSize)
{
    std::multimap<unsigned int,PAGEID>::iterator findIter;
    findIter=_freeSpaceMap.upper_bound(requiredSize);   //找到Map中比requiredSize刚好大一点的迭代器

    if ( findIter != _freeSpaceMap.end() )
    {
      return findIter->second ;
    }
    return DMS_INVALID_PAGEID ;
}

//先将文件分配的区域置空
int dmsFile::_extendFile(int size)
{
    int rc=EDB_OK;
    char temp[DMS_EXTEND_SIZE]={0}; //先定义64K的文件头大小
    memset(temp,0,DMS_EXTEND_SIZE);

    //如果size不是64K的倍数，那么是无效的输入
    if ( size % DMS_EXTEND_SIZE != 0 )
    {
      rc = EDB_SYS ;
      PD_LOG ( PDERROR, "Invalid extend size, must be multiple of %d",
               DMS_EXTEND_SIZE ) ;
      goto error ;
    }

    //将分配的空间置空，每次置空64K
    for ( int i = 0; i < size; i += DMS_EXTEND_SIZE )
    {
      _fileOp.seekToEnd () ;
      rc = _fileOp.Write ( temp, DMS_EXTEND_SIZE ) ;
      PD_RC_CHECK ( rc, PDERROR, "Failed to write to file, rc = %d", rc ) ;
    }
done :
   return rc ;
error :
   goto done ;
}

//根据给定的page地址和记录ID(pageid slotid)找到slot的偏移
int dmsFile::_searchSlot(char *page,dmsRecordID &rid,SLOTOFF &slot)
{
   int rc = EDB_OK ;
   dmsPageHeader *pageHeader = NULL ;
   //检查page地址
   if ( !page )
   {
      rc = EDB_SYS ;
      PD_LOG ( PDERROR, "page is NULL" ) ;
      goto 
      
      error ;
   }

    //检查RID是否合法
   if ( 0 > rid._pageID || 0 > rid._slotID )
   {
      rc = EDB_SYS ;
      PD_LOG ( PDERROR, "Invalid RID: %d.%d",
               rid._pageID, rid._slotID ) ;
      goto error ;
   }

   pageHeader = (dmsPageHeader *)page ; //得到page地址
   //检查slotID是否合法
   if ( rid._slotID > pageHeader->_numSlots )
   {
      rc = EDB_SYS ;
      PD_LOG ( PDERROR, "Slot is out of range, provided: %d, max: %d",
               rid._slotID, pageHeader->_numSlots ) ;
      goto error ;
   }

    //slot的偏移为 当前页地址+页头+slotID*slot大小
   slot=*(SLOTOFF*)(page+sizeof(dmsPageHeader)+rid._slotID*sizeof(SLOTOFF));
done :
   return rc ;
error :
   goto done ;
}

//装载数据，将文件头，各个sements映射到内存，并将_body,_freeSpaceMap，index更新
//注意文件头，页头等内容已经存在了
int dmsFile::_loadData()
{
    int rc=EDB_OK;
    int numPage=0;
    int numSegments=0;
    dmsPageHeader *pageHeader=NULL;
    char *data=NULL;
    BSONObj bson;
    SLOTID slotID=0;
    SLOTOFF slotOffset=0;
    dmsRecordID recordID;

    //检查文件头是否存在
    if(!_header)
    {
        rc = map ( 0, DMS_FILE_HEADER_SIZE, ( void **)&_header ) ;
        PD_RC_CHECK ( rc, PDERROR, "Failed to map file header, rc = %d", rc ) ;
    }

    numPage=_header->_size;
    if ( numPage % DMS_PAGES_PER_SEGMENT )//检查页数是否为segment倍数
    {
         rc = EDB_SYS ;
         PD_LOG ( PDERROR, "Failed to load data, partial segments detected" ) ;
         goto error ;
    }

    numSegments=numPage/DMS_PAGES_PER_SEGMENT;  //获得段数
    if ( numSegments > 0 )
    {
      for ( int i = 0; i < numSegments; ++i )   //
      {
         // 将已经存在的段映射到内存中
         rc = map ( DMS_FILE_HEADER_SIZE + DMS_FILE_SEGMENT_SIZE * i,
                    DMS_FILE_SEGMENT_SIZE,
                    (void **)&data ) ;
         PD_RC_CHECK ( rc, PDERROR, "Failed to map segment %d, rc = %d",
                       i, rc ) ;
         _body.push_back ( data ) ;//更新body
         // 将每页映射到freeSpaceMap
         for ( unsigned int k = 0; k < DMS_PAGES_PER_SEGMENT; ++k )
         {//获得每页的头
            pageHeader = ( dmsPageHeader * ) ( data + k * DMS_PAGESIZE ) ;
            _freeSpaceMap.insert (
                  pair<unsigned int, PAGEID>(pageHeader->_freeSpace, k ) ) ;
            slotID = ( SLOTID ) pageHeader->_numSlots ;
            recordID._pageID = (PAGEID) k ;
            /*
            for ( unsigned int s = 0; s < slotID; ++s )
            {
               slotOffset = *(SLOTOFF*)(data+k*DMS_PAGESIZE +
                            sizeof(dmsPageHeader ) + s*sizeof(SLOTID) ) ;
               if ( DMS_SLOT_EMPTY == slotOffset )
               {
                  continue ;
               }
               bson = BSONObj ( data + k*DMS_PAGESIZE +
                                slotOffset + sizeof(dmsRecord) ) ;
               recordID._slotID = (SLOTID)s ;
               rc = _ixmBucketMgr->isIDExist ( bson ) ;
               PD_RC_CHECK ( rc, PDERROR, "Failed to call isIDExist, rc = %d", rc ) ;
               rc = _ixmBucketMgr->createIndex ( bson, recordID ) ;
               PD_RC_CHECK ( rc, PDERROR, "Failed to call ixm createIndex, rc = %d", rc ) ;
            }
            */ //更新index
         }
      } // for ( int i = 0; i < numSegments; ++i )
   } // if ( numSegments > 0 )
done :
   return rc ;
error :
   goto done ;

}

//根据每个slot标记，对一个页里的无用记录进行整理
void dmsFile::_recoverSpace(char *page)
{
   char *pLeft               = NULL ;
   char *pRight              = NULL ;
   SLOTOFF slot              = 0 ;
   int recordSize            = 0 ;
   bool isRecover            = false ;  //是否需要重组
   dmsRecord *recordHeader   = NULL ;
   dmsPageHeader *pageHeader = NULL ;

   //pLeft先到该页的头之后，也就是第一个slot的位置
   pLeft=page+sizeof(dmsPageHeader);
   //pRight直接到该页的尾部
   pRight=page+DMS_PAGESIZE;
   pageHeader=(dmsPageHeader*)page;//获得该页的头位置

   for ( unsigned int i = 0; i < pageHeader->_numSlots; ++i )
   {    //获得该slot对应的记录偏移
      slot = *((SLOTOFF*)(pLeft + sizeof(SLOTOFF) * i ) ) ;
      if ( DMS_SLOT_EMPTY != slot ) //slot标记不为-1，数据有用
      {
         recordHeader = (dmsRecord *)(page + slot ) ;
         recordSize = recordHeader->_size ;//获取记录长度
         pRight -= recordSize ;
         if ( isRecover )   //注意如果没有过标记为-1的slot，是不需要进行move的，只需要移动pRight指针
         {//只要出现了标记为-1的slot，后续所有的有效记录都要开始move
            memmove ( pRight, page + slot, recordSize ) ;
            //更新slot对应的记录偏移
            *((SLOTOFF*)(pLeft + sizeof(SLOTOFF) * i ) ) = (SLOTOFF)(pRight-page) ;
         }
      }
      else//slot标记为-1，该记录需要使得该页进行重组，后续都要开始move
      {
         isRecover = true ;
      }
   }
   //将页头的空闲偏移更新
   pageHeader->_freeOffset = pRight - page ;


}