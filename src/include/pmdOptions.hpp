#ifndef PMDOPTIONS_HPP_
#define PMDOPTIONS_HPP_

#include "core.hpp"
#include <boost/program_options.hpp>
#include <boost/program_options/parsers.hpp>

using namespace std;

namespace po=boost::program_options;

//定义选项宏 帮助 数据库路径 服务名 最大线程池 日志路径 配置路径
#define PMD_OPTION_HELP                  "help"
#define PMD_OPTION_DBPATH                "dbpath"
#define PMD_OPTION_SVCNAME               "svcname"
#define PMD_OPTION_MAXPOOL               "maxpool"
#define PMD_OPTION_LOGPATH               "logpath"
#define PMD_OPTION_CONFPATH              "confpath"

//添加options_description
#define PMD_ADD_PARAM_OPTIONS_BEGIN( desc )\
        desc.add_options()
//添加结束
#define PMD_ADD_PARAM_OPTIONS_END ;

//command的格式为string a+b
#define PMD_COMMANDS_STRING(a,b) (string(a) + string(b)).c_str()

//我们自定义的options_description的内容，也是HELP输出的内容
#define PMD_COMMANDS_OPTIONS \
        ( PMD_COMMANDS_STRING ( PMD_OPTION_HELP, ",h"), "help" ) \
        ( PMD_COMMANDS_STRING ( PMD_OPTION_DBPATH, ",d"), boost::program_options::value<string>(), "database file full path" ) \
        ( PMD_COMMANDS_STRING ( PMD_OPTION_SVCNAME, ",s"), boost::program_options::value<string>(), "local service name" ) \
        ( PMD_COMMANDS_STRING ( PMD_OPTION_MAXPOOL, ",m"), boost::program_options::value<unsigned int>(), "max pooled agent" ) \
        ( PMD_COMMANDS_STRING ( PMD_OPTION_LOGPATH, ",l"), boost::program_options::value<string>(), "diagnostic log file full path" ) \
        ( PMD_COMMANDS_STRING ( PMD_OPTION_CONFPATH, ",c"), boost::program_options::value<string>(), "configuration file full path" ) \

//配置文件名 日志文件名 数据库文件名 服务名 线程池
#define CONFFILENAME "edb.conf"
#define LOGFILENAME  "diag.log"
#define DBFILENAME   "edb.data"
#define SVCNAME      "48127"
#define NUMPOOL      20

class pmdOptions
{
public:
	pmdOptions();
	~pmdOptions();
public:
	//用户在cmd中输入的配置选项
	int readCmd(int argc,char **argv,po::options_description &desc,po::variables_map &vm);
	//variables_map是选项变量对应值的一个map
	//importVM将从CMD或者配置文件中取出vm，并将里面的内容读出
	int importVM(const po::variables_map &vm,bool isDefault=true);
	//从配置文件中读取配置选项，在用户cmd之前执行
	int readConfigureFile(const char *path,po::options_description &desc,po::variables_map &vm);
	//初始化配置：设置description，先import配置文件中的vm，再import CMD中的vm
	int init(int argc,char **argv);
public:
	inline char* getDBPath(){return _dbPath;}
	inline char* getLogPath(){return _logPath;}
	inline char* getConfPath(){return _confPath;}
	inline char* getServiceName(){return _svcName;}
	inline	int getMaxPool(){return _maxPool;}
private:
	char _dbPath[OSS_MAX_PATHSIZE+1];
	char _logPath[OSS_MAX_PATHSIZE+1];
	char _confPath[OSS_MAX_PATHSIZE+1];
	char _svcName[NI_MAXSERV+1];
	int _maxPool;
};
#endif
