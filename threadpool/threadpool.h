
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGIMysql/sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(SqlConnectionPool *connPool, int thread_num = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T *request);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //请求队列
    Locker m_queuelocker;       //保护请求队列的互斥锁
    Sem m_queuestat;            //是否有任务需要处理
    bool m_stop;		//是否结束线程
    SqlConnectionPool *m_connPool;  //数据库
};
//线程池的初始化
template <typename T>

threadpool<T>::threadpool(SqlConnectionPool *connPool, int thread_num,int max_requests ):
	m_thread_number(thread_num),m_max_requests(max_requests),m_stop(false),m_threads(NULL),m_connPool(connPool)
{
	if(m_thread_number <=0 || max_requests <=0)
		throw std::exception();

	m_threads = new pthread_t[m_thread_number];

	if(!m_threads)
		throw std::exception();

	int i=0;
	for(i=0;i<thread_num ;++i){

		if(pthread_create(m_threads + i, NULL, worker, this) != 0)
		{
			delete[] m_threads;
			throw std::exception();
		}
		
		if(pthread_detach(m_threads[i]))
		{
			delete[] m_threads;
			throw std::exception();
		}
	}

}

//销毁线程池
template <typename T>
threadpool<T>::~threadpool()
{
	delete[] m_threads;
	m_stop = true;
}


//添加请求
template <typename T>
bool threadpool<T>::append(T *request){

	m_queuelocker.lock();

	//如果请求队列的大小大于允许的最大请求数量
	if(m_workqueue.size() > m_max_requests)
	{
		m_queuelocker.unlock();
		return false;
	}

	m_workqueue.push_back(request);
	m_queuelocker.unlock();
	m_queuestat.post();
	return true;
}

template <typename T>
void *threadpool<T>::worker(void *arg)
{

	threadpool *pool = (threadpool *)arg;

	pool->run();

	return pool;
}


template <typename T>
void threadpool<T>::run()
{

	while(!m_stop)
	{
		m_queuestat.wait();
		m_queuelocker.lock();
		if(m_workqueue.empty()){

			m_queuelocker.unlock();
			continue;
		}

		T *request = m_workqueue.front();
		m_workqueue.pop_front();
		m_queuelocker.unlock();
		if(!request)
			continue;

		ConnectionRAII mysqlcon(&request->mysql, m_connPool);

		request->process();
	}
}

#endif






































