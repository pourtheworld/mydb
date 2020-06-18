#ifndef PD_HPP_
#define PD_HPP_

#include"core.hpp"

#define PD_LOG_STRINGMAX 4096	//日志最长长度

//输入需要的日志等级、格式、其它参数
//如果当前设置的等级大于输入等级，则pdLog
#define PD_LOG(level,fmt,...)				\
	do{						\
		if(_curPDLevel>=level)			\
		{ 					\
			pdLog(level,__func__,__FILE__,__LINE__,fmt,##__VA_ARGS__);	\
		}					\
	}while(0)					\

//检查条件是否成立
//如果不成立会返回一个retCode,并跳转到一个Label,出现一条日志
#define PD_CHECK(cond,retCode,gotoLabel,level,fmt,...)	\
	do{						\
		if(!(cond))				\
		{					\
			rc=(retCode);			\
			PD_LOG((level),fmt,##__VA_ARGS__);	\
			goto gotoLabel;			\
		}					\
	}while(0)					\

//检查retCode是否为OK,如果不是则打印retCode和它的日志
#define PD_RC_CHECK(rc,level,fmt,...)			\
	do{						\
		PD_CHECK((EDB_OK==(rc)),(rc),error,(level),fmt,##__VA_ARGS__);	\
	}while(0)					\

//检查条件，如果不满足则打印日志，并跳转到error
#define EDB_VALIDATE_GOTOERROR(cond,ret,str)		\
	{if(!(cond))	{pdLog(PDERROR,__func__,__FILE__,__LINE__,str);	\
		rc=ret;goto error;}}			\

#define EDB_ASSERT(cond,str)  {if(cond){}}
#define EDB_CHECK(cond,str)   {if(cond){}}

enum PDLEVEL
{
   PDSEVERE = 0,
   PDERROR,
   PDEVENT,
   PDWARNING,
   PDINFO,
   PDDEBUG
} ;

extern PDLEVEL _curPDLevel;
const char *getPDLevelDesp ( PDLEVEL level ) ;

#define PD_DFT_DIAGLEVEL PDWARNING
//一个是format+...,一个是string
void pdLog(PDLEVEL level,const char *func,const char *file,unsigned int line,const char *format,...);
void pdLog(PDLEVEL level,const char *func,const char *file,unsigned int line,std::string message);

#endif
