#ifndef IXM_HPP_
#define IXM_HPP_

#include    "ossLatch.hpp"
#include    "bson.h"
#include    <map>
#include    "dmsRecord.hpp"

using namespace bson;

#define IXM_KEY_FIELDNAME    "_id"
#define IXM_HASH_MAP_SIZE   1000

//索引元素 包含记录数据+记录id
struct ixmEleHash
{
    const char *data;
    dmsRecordID recordID;
};


//首先有一个桶管理器，保存了1000个桶
//每个桶包含一个map。可以通过map的first 索引值，得到map的second 索引记录。
//索引值通过记录数据+记录长度通过索引函数得到。

class ixmBucketManager
{
private:
    class ixmBucket
    {
    private:
        std::multimap<unsigned int,ixmEleHash> _bucketMap;
        ossSLatch _mutex;
    public:
        //给定索引值，寻找当前桶map中对应的索引记录。与给定索引记录比较是否存在。
        int isIDExist(unsigned int hashNum,ixmEleHash &eleHash);
        int createIndex ( unsigned int hashNum, ixmEleHash &eleHash ) ;
        int findIndex ( unsigned int hashNum, ixmEleHash &eleHash ) ;
        int removeIndex ( unsigned int hashNum, ixmEleHash &eleHash ) ;
    };
    //通过给定的record得到索引值。通过索引值%桶数得到对应桶。在对应桶的map中建立索引。
        int _processData(BSONObj &record,dmsRecordID &recordID,
        unsigned int &hashNum,ixmEleHash &eleHash,unsigned int &random);
private:
    std::vector<ixmBucket*> _bucket;
public:
    ixmBucketManager ()
   {
   }
   ~ixmBucketManager ()
   {
      ixmBucket *pIxmBucket = NULL ;
      for ( int i = 0; i < IXM_HASH_MAP_SIZE; ++i )
      {
         pIxmBucket = _bucket[i] ;
         if ( pIxmBucket )
            delete pIxmBucket ;
      }
   }
   int initialize () ;
   int isIDExist ( BSONObj &record ) ;
   int createIndex ( BSONObj &record, dmsRecordID &recordID ) ;
   int findIndex ( BSONObj &record, dmsRecordID &recordID ) ;
   int removeIndex ( BSONObj &record, dmsRecordID &recordID ) ;
};

#endif