#include    "core.hpp"
#include    "rtn.hpp"
#include    "pd.hpp"
#include    "pmd.hpp"

using namespace bson;

rtn::rtn():_dmsFile(NULL),_ixmBucketMgr(NULL){}

rtn::~rtn()
{
    if(_dmsFile)    delete _dmsFile;
    if(_ixmBucketMgr)   delete _ixmBucketMgr;
}

//通过调用内核的文件路径，给文件初始化
int rtn::rtnInitialize()
{
   int rc=EDB_OK;
    
   _ixmBucketMgr=new(std::nothrow) ixmBucketManager();
   if ( !_ixmBucketMgr )
   {
      rc = EDB_OOM ;
      PD_LOG ( PDERROR, "Failed to new ixm bucketManager" ) ;
      goto error ;
   }

   _dmsFile=new(std::nothrow) dmsFile(_ixmBucketMgr);
    if ( !_dmsFile )
   {
      rc = EDB_OOM ;
      PD_LOG ( PDERROR, "Failed to new dms file" ) ; 
      goto error ;
   }

   rc=_ixmBucketMgr->initialize();
   if ( rc )
   {
      PD_LOG ( PDERROR, "Failed to call bucketMgr initialize, rc = %d", rc ) ;
      goto error ;
   }

   rc=_dmsFile->initialize(pmdGetKRCB()->getDataFilePath());
   if ( rc )
   {
      PD_LOG ( PDERROR, "Failed to call dms initialize, rc = %d", rc ) ;
      goto error ;
   }

done :
   return rc ;
error :
   goto done ; 
}

//实际上就是调用文件的insert
int rtn::rtnInsert(BSONObj &record)
{
    int rc=EDB_OK;
    dmsRecordID recordID;
    BSONObj outRecord;
    
    //检查索引是否已经存在该记录了。
    rc=_ixmBucketMgr->isIDExist(record);
    PD_RC_CHECK ( rc, PDERROR, "Failed to call isIDExist, rc = %d", rc ) ;

    rc=_dmsFile->insert(record,outRecord,recordID);
    if ( rc )
    {
      PD_LOG ( PDERROR, "Failed to call dms insert, rc = %d", rc ) ;
      goto error ;
    }
    //插入完以后更新索引
    rc=_ixmBucketMgr->createIndex(outRecord,recordID);
    PD_RC_CHECK ( rc, PDERROR, "Failed to call ixmCreateIndex, rc = %d", rc ) ;
done :
   return rc ;
error :
   goto done ;
}

//给定一个记录，先通过索引查找是否存在，如果存在获得record id
//通过record id去对应页找记录并输出
int rtn::rtnFind ( BSONObj &inRecord, BSONObj &outRecord )
{
   int rc = EDB_OK ;
   dmsRecordID recordID ;
   //先通过给定记录去索引里找记录的ID
   rc = _ixmBucketMgr->findIndex ( inRecord, recordID ) ;
   PD_RC_CHECK ( rc, PDERROR, "Failed to call ixm findIndex, rc = %d", rc ) ;
   //再通过记录ID去文件里找记录
   rc = _dmsFile->find ( recordID, outRecord ) ;
   PD_RC_CHECK ( rc, PDERROR, "Failed to call dms find, rc = %d", rc ) ;
done :
   return rc ;
error :
   goto done ;
}

//先清除index(过程中会检查该记录是否存在)
//再在文件中清除
int rtn::rtnRemove ( BSONObj &record )
{
   int rc = EDB_OK ;
   dmsRecordID recordID ;
   rc = _ixmBucketMgr->removeIndex ( record, recordID ) ;
   PD_RC_CHECK ( rc, PDERROR, "Failed to call ixm removeIndex, rc = %d", rc ) ;
   rc = _dmsFile->remove ( recordID ) ;
   PD_RC_CHECK ( rc, PDERROR, "Failed to call dms remove, rc = %d", rc ) ;
done :
   return rc ;
error :
   goto done ;
}