#include "pmd.hpp"
#include "pmdOptions.hpp"

EDB_KRCB pmd_krcb;
extern char _pdDiagLogPath[OSS_MAX_PATHSIZE+1];	//在pd文件中定义的

int EDB_KRCB::init(pmdOptions *options)
{
	setDBStatus(EDB_DB_NORMAL);
	setDataFilePath(options->getDBPath());
	setLogFilePath(options->getLogPath());
	//从配置文件中得到Log文件的地址，并copy给pd的日志文件地址
	strncpy(_pdDiagLogPath,getLogFilePath(),sizeof(_pdDiagLogPath));
	setSvcName(options->getServiceName());
	setMaxPool(options->getMaxPool());
	return EDB_OK;
}
