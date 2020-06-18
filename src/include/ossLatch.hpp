#ifndef OSSLATCH_HPP_
#define OSSLATCH_HPP_


#include<pthread.h>

//将锁封装成共享以及互斥
enum OSS_LATCH_MODE
{
	SHARED,
	EXCLUSIVE
};

//互斥锁
class ossXLatch
{
private:
	pthread_mutex_t _lock;//底层就是pthread的mutex
public:
	ossXLatch(){	pthread_mutex_init(&_lock,0);	}
	~ossXLatch(){	pthread_mutex_destroy(&_lock);	}
	void get(){	pthread_mutex_lock(&_lock);	}
	void release(){	pthread_mutex_unlock(&_lock);	}
	bool try_get(){	return (pthread_mutex_trylock(&_lock)==0);}
};

class ossSLatch
{
private:
	pthread_rwlock_t _lock;
public:
	ossSLatch(){	pthread_rwlock_init(&_lock,0);	}
	~ossSLatch(){	pthread_rwlock_destroy(&_lock);	}
	//写锁
	void get(){	pthread_rwlock_wrlock(&_lock);	}
	void release(){	pthread_rwlock_unlock(&_lock);	}
	bool try_get(){	return (pthread_rwlock_trywrlock(&_lock)==0);}
	//读锁
	void get_shared(){	pthread_rwlock_rdlock(&_lock);}
	void release_shared(){	pthread_rwlock_unlock(&_lock);}
	bool try_get_shared(){	return (pthread_rwlock_tryrdlock(&_lock)==0);}
};

#endif
