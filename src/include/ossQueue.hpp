#ifndef OSSQUEUE_HPP_
#define OSSQUEUE_HPP_

#include<queue>
#include<boost/thread.hpp>
#include<boost/thread/thread_time.hpp>

template<typename Data>
class ossQueue
{
private:
//ossQueue由队列，互斥锁，条件变量组成
	std::queue<Data> _queue;
	boost::mutex _mutex;
	boost::condition_variable _cond;
public:
	unsigned int size()//得到队列长度
	{	//设置函数级的锁
		boost::mutex::scoped_lock lock(_mutex);
		//在返回长度之前不解锁，函数结束自动解锁
		return (unsigned int)_queue.ssize();
	}
	
	void push(Data const &data)
	{
		boost::mutex::scoped_lock lock(_mutex);
		_queue.push(data);
		//push以后则解锁
		lock.unlock();
		//注意在解锁后条件变量会通知其它线程可以抢锁了，具体看wait_and_pop
		_cond.notify_one();
	}

	bool empty() const
	{
		boost::mutex::scoped_lock lock(_mutex);
		return _queue.empty();
	}

	bool try_pop(Data &value)
	{
		boost::mutex::scoped_lock lock(_mutex);
		//尝试弹出数据，如果为空则返回false
		if(_queue.empty())	return false;
		value=_queue.front();
		_queue.pop();
		return true;
	}

	void wait_and_pop(Data &value)
	{
		boost::mutex::scoped_lock lock(_mutex);
		//如果当前队列为空，条件变量会获取当前锁，并且先释放
		//其它线程可以获取这个锁进行例如push的操作
		//push以后解锁会通知wait_and_pop的线程你可以获取锁了
		//重新进行while循环，发现不为空则跳出循环
		while(_queue.empty())	_cond.wait(lock);
		value=_queue.front();
		_queue.pop();
	}
	
	//不是无限等待，给while加上一个超时
	bool timed_wait_and_pop(Data &value,long long millsec)
	{	//当前的系统时间+超时=等待到的最晚时间
		boost::system_time const timeout=boost::get_system_time()+boost::posix_time::milliseconds(millsec);
		boost::mutex::scoped_lock lock(_mutex);
		while(_queue.empty())
		{	//用_timed_wait代替wait，超时直接return false
			if(!_cond.timed_wait(lock,timeout))	return false;
		}
		value=_queue.front();
		_queue.pop();
		return true;
	}
};
#endif
