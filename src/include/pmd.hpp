#ifndef PMD_HPP_
#define PMD_HPP_

#include "core.hpp"
#include "pmdEDUMgr.hpp"
//数据库当前状态
enum EDB_DB_STATUS
{
	EDB_DB_NORMAL=0,
	EDB_DB_SHUTDOWN,
	EDB_DB_PANIC
};

//数据库是否处于正常状态
#define EDB_IS_DB_NORMAL	(EDB_DB_NORMAL==pmdGetKRCB()->getDBStatus())

//数据库是否处于关闭状态
#define EDB_IS_DB_DOWN		(EDB_DB_SHUTDOWN==pmdGetKRCB()->getDBStatus()||\
				 EDB_DB_PANIC==pmdGetKRCB()->getDBStatus())

//数据库是否处于启动状态
#define EDB_IS_DB_UP		(!EDB_IS_DB_DOWN)


#define EDB_SHUTDOWN_DB  { pmdGetKRCB()->setDBStatus(EDB_DB_SHUTDOWN); }

class pmdOptions;
//内核模块
class EDB_KRCB
{
private:
	//底层为配置选项+数据库状态
	char _dataFilePath[OSS_MAX_PATHSIZE+1];
	char _logFilePath[OSS_MAX_PATHSIZE+1];
	int _maxPool;
	char _svcName[NI_MAXSERV+1];
	EDB_DB_STATUS _dbStatus;
private :
    pmdEDUMgr     _eduMgr ;
public:
	EDB_KRCB()
	{
		_dbStatus=EDB_DB_NORMAL;
		memset(_dataFilePath,0,sizeof(_dataFilePath));
		memset(_logFilePath,0,sizeof(_logFilePath));
		_maxPool=0;
		memset(_svcName,0,sizeof(_svcName));
	}
	~EDB_KRCB(){}
	inline EDB_DB_STATUS getDBStatus(){	return _dbStatus;	}
	inline const char* getDataFilePath(){	return _dataFilePath;	}
	inline const char* getLogFilePath(){	return _logFilePath;	}
	inline const char* getSvcName(){	return _svcName;	}
	inline int getMaxPool(){	return _maxPool;	}
	inline void setDBStatus(EDB_DB_STATUS status){	_dbStatus=status;	}
	void setDataFilePath(const char *pPath)
	{
		strncpy(_dataFilePath,pPath,sizeof(_dataFilePath));
	}
	void setLogFilePath(const char *pPath)
	{
		strncpy(_logFilePath,pPath,sizeof(_logFilePath));
	}
	void setSvcName(const char *pName)
	{
		strncpy(_svcName,pName,sizeof(_svcName));
	}

	void setMaxPool(int maxPool)
	{
		_maxPool=maxPool;
	}
	int init(pmdOptions *options);
	pmdEDUMgr *getEDUMgr ()
    {
      return &_eduMgr ;
    }
};

//在本文件外定义了pmd_krcb，实际在pmd.cpp中
extern EDB_KRCB pmd_krcb;

//一个全局函数，用来返回pmd_krcb真正的地址
inline EDB_KRCB *pmdGetKRCB(){	return &pmd_krcb;	}

#endif
