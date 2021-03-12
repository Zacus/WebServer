#ifndef LOCKER_H
#define LOCKER_H

#include<exception>
#include<pthread.h>
#include<semaphore.h>


//信号量类
class sem{

	public:
		sem(){
			if(sem_init(&m_sem,0,0)!=0){

				throw std::exception();
			}
		}

		sem(int num){

			if(sem_init(&m_sem, 0, num)!=0){

				throw std::exception();
			}
		}

		~sem(){

			sem_destroy(&m_sem);
		}


		//信号量等待P操作
		bool wait(){

			return sem_wait(&m_sem) == 0;
		}

		//信号量释放V操作
		bool post(){

			return sem_post(&m_sem) == 0;
		}

	private:
		sem_t m_sem;
};

//互斥锁类
class locker{

	public:
		locker(){

			if(pthread_mutex_init(&m_mutex, NULL) != 0){

				throw std::exception();
			}
		}

		~locker(){

			pthread_mutex_destroy(&m_mutex);
		}

		//加锁
		bool lock(){

			return pthread_mutex_lock(&m_mutex)==0;
		}

		//解锁
		bool unlock(){

			return pthread_mutex_unlock(&m_mutex)==0;
		}

		pthread_mutex_t *get(){

			return &m_mutex;
		}

	private:

		pthread_mutex_t m_mutex;


};

//条件变量类
class cond{

	public:
		cond(){

			if(pthread_cond_init(&m_cond, NULL) !=0){

				throw std::exception();
			}
		}

		~cond(){

			pthread_cond_destroy(&m_cond);
		}

		bool wait(pthread_mutex_t *m_mutex){

			int ret = 0;
			ret = pthread_cond_wait(&m_cond,m_mutex);

			return ret==0;
		}

		bool timewait(pthread_mutex_t * m_mutex, struct timespec t){

			int ret = 0;

			ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);

			return ret==0;
		}

		bool signal(){

			return pthread_cond_signal(&m_cond) == 0;
		}

		//唤醒所有被阻塞的线程
		bool broadcast(){

			return pthread_cond_broadcast(&m_cond) == 0;
		}


	private:

		pthread_cond_t m_cond;

};


#endif





















