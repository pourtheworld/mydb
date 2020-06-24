#ifndef RTN_HPP_
#define RTN_HPP_
#include    "bson.h"
#include    "dms.hpp"
#include    "ixmBucket.hpp"

#define RTN_FILE_NAME   "data.1"    //定义存储文件名字

class rtn
{
private:
    dmsFile *_dmsFile;
    ixmBucketManager *_ixmBucketMgr; 
public:
    rtn();
    ~rtn();
    int rtnInitialize();
    int rtnInsert(bson::BSONObj &record);
    int rtnFind(bson::BSONObj &inRecord,bson::BSONObj &outRecord);
    int rtnRemove(bson::BSONObj &record);
};


#endif