#include    "core.hpp"
#include    "pd.hpp"
#include    "ossHash.hpp"
#include    "ixmBucket.hpp"

//manager的函数是类似的，都需要先通过处理函数得到对应桶号random
//然后交给对应桶处理


//manager级别
//首先处理record
//然后交给对应的桶去判断
int ixmBucketManager::isIDExist(BSONObj &record)
{
   int rc               = EDB_OK ;
   unsigned int hashNum = 0 ;
   unsigned int random  = 0 ;
   ixmEleHash eleHash ;
   dmsRecordID recordID ;

    //处理记录是由Manager完成的
   rc = _processData ( record, recordID, hashNum, eleHash, random ) ;
   if ( rc )
   {
      PD_LOG ( PDERROR, "Failed to process data, rc = %d", rc ) ;
      goto error ;
   }

   //交给对应桶处理
   rc = _bucket[random]->isIDExist ( hashNum, eleHash ) ;
   if ( rc )
   {
      PD_LOG ( PDERROR, "Failed to create index, rc = %d", rc ) ;
      goto error ;
   }
done :
   return rc ;
error :
   goto done ;
}

//manager级别
//首先处理record
//然后交给对应的桶去创建
int ixmBucketManager::createIndex(BSONObj &record,dmsRecordID &recordID)
{
   int rc                = EDB_OK ;
   unsigned int hashNum  = 0 ;
   unsigned int random   = 0 ;
   ixmEleHash eleHash ;
   rc = _processData ( record, recordID, hashNum, eleHash, random ) ;
   PD_RC_CHECK ( rc, PDERROR, "Failed to process data, rc = %d", rc ) ;
   rc = _bucket[random]->createIndex ( hashNum, eleHash ) ;
   PD_RC_CHECK ( rc, PDERROR, "Failed to create index, rc = %d", rc ) ;
   recordID = eleHash.recordID ;
done :
   return rc ;
error :
   goto done ;
}

//manager级别
//首先处理record
//然后交给对应的桶去寻找
int ixmBucketManager::findIndex ( BSONObj &record, dmsRecordID &recordID )
{
   int rc                = EDB_OK ;
   unsigned int hashNum  = 0 ;
   unsigned int random   = 0 ;
   ixmEleHash eleHash ;
   rc = _processData ( record, recordID, hashNum, eleHash, random ) ;
   PD_RC_CHECK ( rc, PDERROR, "Failed to process data, rc = %d", rc ) ;
   rc = _bucket[random]->findIndex ( hashNum, eleHash ) ;
   PD_RC_CHECK ( rc, PDERROR, "Failed to find index, rc = %d", rc ) ;
   recordID = eleHash.recordID ;
done :
   return rc ;
error :
   goto done ;
}

//manager级别
//首先处理record
//然后交给对应的桶去移除
int ixmBucketManager::removeIndex ( BSONObj &record, dmsRecordID &recordID )
{
   int rc                = EDB_OK ;
   unsigned int hashNum  = 0 ;
   unsigned int random   = 0 ;
   ixmEleHash eleHash ;
   rc = _processData ( record, recordID, hashNum, eleHash, random ) ;
   PD_RC_CHECK ( rc, PDERROR, "Failed to process data, rc = %d", rc ) ;
   rc = _bucket[random]->removeIndex ( hashNum, eleHash ) ;
   PD_RC_CHECK ( rc, PDERROR, "Failed to remove index, rc = %d", rc ) ;
   recordID._pageID = eleHash.recordID._pageID ;
   recordID._slotID = eleHash.recordID._slotID ;
done :
   return rc ;
error :
   goto done ;
}

//处理数据记录函数
//先检验记录是否有_id字段打头
//根据散列值获得桶号random
//把散列元素的data和id附上值

int ixmBucketManager::_processData ( BSONObj &record,
                                     dmsRecordID &recordID,
                                     unsigned int &hashNum,
                                     ixmEleHash &eleHash,
                                     unsigned int &random )
{
   int rc               = EDB_OK ;
   //得到字段为i_id的BSON元素部分
   BSONElement element  = record.getField ( IXM_KEY_FIELDNAME ) ;
   //如果没有_id字段，或者_id的类型不是int或者string 则出错
   if ( element.eoo() ||
        ( element.type() != NumberInt && element.type() != String ) )
   {
      rc = EDB_INVALIDARG ;
      PD_LOG ( PDERROR, "record must be with _id" ) ;
      goto error ;
   }

   // 根据_id的值和长度，经过散列函数获得散列值
   hashNum = ossHash ( element.value(), element.valuesize() ) ;
   // 根据散列值通过取模得到桶号
   random = hashNum % IXM_HASH_MAP_SIZE ;

   //将散列元素赋值
   eleHash.data = element.rawdata () ;
   eleHash.recordID = recordID ;
done :
   return rc ;
error :
   goto done ;
}

//桶manager的初始化，就是将vector中的桶一个个插入
int ixmBucketManager::initialize ()
{
   int rc = EDB_OK ;
   ixmBucket *temp = NULL ;
   for ( int i = 0; i < IXM_HASH_MAP_SIZE; ++i )
   {
      temp = new (std::nothrow) ixmBucket () ;
      if ( !temp )
      {
         rc = EDB_OOM ;
         PD_LOG ( PDERROR, "Failed to allocate new ixmBucket" ) ;
         goto error ;
      }
      _bucket.push_back ( temp ) ;
      temp = NULL ;
   }
done:
   return rc ;
error :
   goto done ;
}

//具体的记录检查函数：
//首先已经经过Manager的记录处理函数，得到了对应的散列值和散列元素
//去对应的桶里寻找相同散列值的迭代器范围
//在这个范围内判断是否与散列元素相同：先判断数据类型->数据长度->数据本身
//我们对该桶使用共享锁

int ixmBucketManager::ixmBucket::isIDExist ( unsigned int hashNum,
                                             ixmEleHash &eleHash )
{
   int rc = EDB_OK ;
   BSONElement destEle ;
   BSONElement sourEle ;
   ixmEleHash existEle ;
   std::pair<std::multimap<unsigned int, ixmEleHash>::iterator,
             std::multimap<unsigned int, ixmEleHash>::iterator> ret ;
   _mutex.get_shared () ;
   ret = _bucketMap.equal_range ( hashNum ) ;   //相同散列值的迭代器范围
   sourEle = BSONElement ( eleHash.data ) ;
   for ( std::multimap<unsigned int, ixmEleHash>::iterator it = ret.first ;
         it != ret.second; ++it )
   {
      existEle = it->second ;
      destEle = BSONElement ( existEle.data ) ;
      if ( sourEle.type() == destEle.type() )   //先判断值的类型是否相同
      {
         if ( sourEle.valuesize() == destEle.valuesize() )  //再判断长度
         {
            if ( !memcmp ( sourEle.value(), destEle.value(),    //最后判断内容
                           destEle.valuesize() ) )
            {
               rc = EDB_IXM_ID_EXIST ;
               PD_LOG ( PDERROR, "record _id does exist" ) ;
               goto error ;
            }
         }
      }
   }
done :
   _mutex.release_shared () ;
   return rc ;
error :
   goto done ;
}

//具体的记索引创建函数：
//将经过数据处理得到的散列值和散列元素插入到map中即可。
//使用写锁
int ixmBucketManager::ixmBucket::createIndex ( unsigned int hashNum,
                                               ixmEleHash &eleHash )
{
   int rc = EDB_OK ;
   _mutex.get() ;
   _bucketMap.insert (
      pair<unsigned int, ixmEleHash> ( hashNum, eleHash ) ) ;
   _mutex.release () ;
   return rc ;
}

//具体的记录查找函数：
//过程与记录检查函数记录相同，区别在于对比完记录完全相同以后，返回记录ID
//我们对该桶使用共享锁


int ixmBucketManager::ixmBucket::findIndex ( unsigned int hashNum,
                                             ixmEleHash &eleHash )
{
   int rc = EDB_OK ;
   BSONElement destEle ;
   BSONElement sourEle ;
   ixmEleHash existEle ;
   std::pair<std::multimap<unsigned int, ixmEleHash>::iterator,
             std::multimap<unsigned int, ixmEleHash>::iterator> ret ;
   _mutex.get_shared () ;
   ret = _bucketMap.equal_range ( hashNum ) ;
   sourEle = BSONElement ( eleHash.data ) ;
   for ( std::multimap<unsigned int, ixmEleHash>::iterator it = ret.first ;
         it != ret.second; ++it )
   {
      existEle = it->second ;
      destEle = BSONElement ( existEle.data ) ;
      if ( sourEle.type() == destEle.type() )
      {
         if ( sourEle.valuesize() == destEle.valuesize() )
         {
            if ( !memcmp ( sourEle.value(), destEle.value(),
                           destEle.valuesize() ) )
            {
               eleHash.recordID = existEle.recordID ;
               goto done ;//只要找到相等的就返回
            }
         }
      }
   }
   rc = EDB_IXM_ID_NOT_EXIST ;
   PD_LOG ( PDERROR, "record _id does not exist, hashNum = %d", hashNum ) ;
   goto error ;
done :
   _mutex.release_shared () ;
   return rc ;
error :
   goto done ;
}

//具体的记录查找函数：
//过程与记录检查函数记录相同，区别在于对比完记录完全相同以后，map中erase这条
//我们对该桶使用写锁

int ixmBucketManager::ixmBucket::removeIndex ( unsigned int hashNum,
                                             ixmEleHash &eleHash )
{
   int rc = EDB_OK ;
   BSONElement destEle ;
   BSONElement sourEle ;
   ixmEleHash existEle ;
   std::pair<std::multimap<unsigned int, ixmEleHash>::iterator,
             std::multimap<unsigned int, ixmEleHash>::iterator> ret ;
   _mutex.get () ;
   ret = _bucketMap.equal_range ( hashNum ) ;
   sourEle = BSONElement ( eleHash.data ) ;
   for ( std::multimap<unsigned int, ixmEleHash>::iterator it = ret.first ;
         it != ret.second; ++it )
   {
      existEle = it->second ;
      destEle = BSONElement ( existEle.data ) ;
      if ( sourEle.type() == destEle.type() )
      {
         if ( sourEle.valuesize() == destEle.valuesize() )
         {
            if ( !memcmp ( sourEle.value(), destEle.value(),
                           destEle.valuesize() ) )
            {
               eleHash.recordID = existEle.recordID ;
               _bucketMap.erase ( it ) ;    //完全相等后，map中的对erase
               goto done ;
            }
         }
      }
   }
   rc = EDB_INVALIDARG ;
   PD_LOG ( PDERROR, "record _id does not exist" ) ;
   goto error ;
done :
   _mutex.release () ;
   return rc ;
error :
   goto done ;
}