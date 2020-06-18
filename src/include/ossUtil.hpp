#ifndef OSSUTIL_HPP_
#define OSSUTIL_HPP_

#include "core.hpp"
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/xtime.hpp>
//nanosleep( )---------以纳秒为单位
//#include<time.h>
// struct timespec
//{
//  time_t  tv_sec;         /* 秒seconds */
//  long    tv_nsec;        /* 纳秒nanoseconds */
//};
//int nanosleep(const struct timespec *req, struct timespec *rem);
//return: 若进程暂停到参数*req所指定的时间，成功则返回0，若有信号中断则返回-1，并且将剩余微秒数记录在*rem中。
//req->tv_sec是以秒为单位，而tv_nsec以毫微秒为单位（10的-9次方秒）。
//由于调用nanosleep是是进程进入TASK_INTERRUPTIBLE,这种状态是会相应信号而进入TASK_RUNNING状态的。

inline void ossSleepmicros(unsigned int s)
{
	struct timespec t;
	t.tv_sec=(time_t)(s/1000000);//秒
	t.tv_nsec=1000*(s%1000000);//微妙
	while(nanosleep(&t,&t)==-1&&errno==EINTR);//当因为中断没睡够，还剩多少会记录到*rem中，恢复以后继续睡
}

inline void ossSleepmillis(unsigned int s){	return ossSleepmicros(s*1000);	}

typedef pid_t OSSPID;
typedef pthread_t OSSTID;

inline OSSPID ossGetParentProcessID(){	return getppid();	}
inline OSSPID ossGetCurrentProcessID(){	return getpid();	}
inline OSSTID ossGetCurrentThreadID(){	return syscall(SYS_gettid);	}




#endif
