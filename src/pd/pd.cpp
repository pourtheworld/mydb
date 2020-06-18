#include"pd.hpp"
#include"core.hpp"
#include"ossLatch.hpp"	//多线程可能会同时写文件，需要互斥锁
#include"ossPrimitiveFileOp.hpp"	

//根据输入的等级打印给用户
const static char *PDLEVELSTRING[]=
{
	"SEVERE",
	"ERROR",
	"EVENT",
	"WARNING",
	"INFO",
	"DEBUG"
};

const char* getPDLevelDesp(PDLEVEL level)
{
	if((unsigned int)level>(unsigned int)PDDEBUG)	return "Unknow Level";
	return PDLEVELSTRING[(unsigned int)level];
}

//日志标准头：年月日时分秒微秒 权限等级 新行 PID TID 新行 当前函数 当前行
//空行 文件 消息 空行 空行
const static char *PD_LOG_HEADER_FORMAT="%04d-%02d-%02d-%02d.%02d.%02d.%06d\
	\
Level:%s"OSS_NEWLINE"PID:%-37dTID:%d"OSS_NEWLINE"Function:%-32sLine:%d"\
OSS_NEWLINE"File:%s"OSS_NEWLINE"Message:"OSS_NEWLINE"%s"OSS_NEWLINE OSS_NEWLINE;

//默认日志等级为warning
PDLEVEL _curPDLevel=PD_DFT_DIAGLEVEL;
//日志路径
char _pdDiagLogPath[OSS_MAX_PATHSIZE+1]={0};
//互斥锁
ossXLatch _pdLogMutex;
//日志文件句柄
ossPrimitiveFileOp _pdLogFile;

//打开日志文件
static int _pdLogFileReopen()
{
	int rc=EDB_OK;
	_pdLogFile.Close();
	//根据当前日志路径打开文件
	rc=_pdLogFile.Open(_pdDiagLogPath);
	if(rc)
	{
		printf("Failed to open log file,errno=%d"OSS_NEWLINE,rc);
		goto error;
	}
	//指针移动到末尾
	_pdLogFile.seekToEnd();
done:
	return rc;
error:
	goto done;
}

//写入日志
static int _pdLogFileWrite(const char *pData)
{
	int rc=EDB_OK;
	size_t dataSize=strlen(pData);
	_pdLogMutex.get();//在进行写之前加上写锁
	if(!_pdLogFile.isValid())//先检查日志文件是否打开
	{
		rc=_pdLogFileReopen();
		if(rc)
		{
			printf("Failed to open log file,errno=%d"OSS_NEWLINE,rc);
			goto error;
		}
	}
	rc=_pdLogFile.Write(pData,dataSize);
	if(rc)
	{
		printf("Failed to write into log file,errno=%d"OSS_NEWLINE,rc);
		goto error;
	}
done:
	_pdLogMutex.release();//发生错误或者结束写入都跳转到这释放锁
	return rc;
error:
	goto done;
}

void pdLog(PDLEVEL level,const char* func,const char *file,unsigned int line,const char *format,...)
{
	int rc=EDB_OK;
	if(_curPDLevel<level)	return;//当前缺省为waring，假设给定的权限大于waring，比如debug，则跳过。
	
	va_list ap;	//可变参数list
	char userInfo[PD_LOG_STRINGMAX];	//用户传入信息
	char sysInfo[PD_LOG_STRINGMAX];		//系统待写入日志消息
	struct tm otm;	//time object
	struct timeval tv;//用于记录微妙
	struct timezone tz;
	time_t tt;
	
	gettimeofday(&tv,&tz);	//获得当前系统时间
	tt=tv.tv_sec;//获得年月日时分秒
	localtime_r(&tt,&otm);//把tt的内容给otm
		
	//把用户信息放入userInfo
	va_start(ap,format);
	vsnprintf(userInfo,PD_LOG_STRINGMAX,format,ap);
	va_end(ap);
	
	//根据format以及长度，把内容放到sysInfo里
	snprintf(sysInfo,PD_LOG_STRINGMAX,PD_LOG_HEADER_FORMAT,
		otm.tm_year+1900,
		otm.tm_mon+1,
		otm.tm_mday,
		otm.tm_hour,
		otm.tm_min,
		otm.tm_sec,
		tv.tv_usec,
		PDLEVELSTRING[level],
		getpid(),
		syscall(SYS_gettid),
		func,
		line,
		file,
		userInfo
	);
	
	//将系统信息打印在屏幕上
	printf("%s"OSS_NEWLINE,sysInfo);
	//将系统信息写入日志文件
	if(_pdDiagLogPath[0]!='\0')
	{
		rc=_pdLogFileWrite(sysInfo);
		if(rc)
		{
			printf("Failed to write into log file,error=%d"OSS_NEWLINE,rc);
			printf("%s"OSS_NEWLINE,sysInfo);
		}
	}
	return	;
}
