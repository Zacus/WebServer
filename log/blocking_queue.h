#ifndef BLOCKING_QUEUE_H
#define BLOCKING_QUEUE_H


#include "../lock/locker.h"
#include<stdlib.h>
#include<pthread.h>
#include<sys/time.h>

template <class T>

class BlockQueue{

	public:
		BlockQueue(int max_size = 1000){

			if(max_size <= 0){

				exit(-1);
			}

			m_max_size = max_size;
			m_array = new T[max_size];
			m_size = 0;
			m_front = -1;
			m_back = -1;
		}

		void clear(){

			m_mutex.lock();
			m_size = 0;
			m_front =-1;
			m_back = -1;
			m_mutex.unlock();
		}


		//队满
		bool full(){

			m_mutex.lock();
			if(m_size>=m_max_size){

				m_mutex.unlock();
				return true;
			}
			m_mutex.unlock();
			return false;
		}

		//队空
		bool empty(){

			m_mutex.lock();
			if(m_size==0){

				m_mutex.unlock();
				return true;

			}
			m_mutex.unlock();
			return false;

		}

		//返回队首元素
		bool front(T &value){

			m_mutex.lock();
			if(m_size ==0){

				m_mutex.unlock();
				return false;
			}
			value = m_array[m_front];
			m_mutex.unlock();
			return true;
		}

		//返回队尾指针
		bool back(T &value){

			m_mutex.lock();
			if(m_size == 0){

				m_mutex.unlock();
				return false;
			}
			value = m_array[m_back];
			m_mutex.unlock();
			return true;

		}

		int size(){

			int tmp =0;
			m_mutex.lock();
			tmp = m_size;

			m_mutex.unlock();

			return tmp;
		}

		int max_size(){

			int tmp = 0;
			m_mutex.lock();
			tmp = m_max_size;
			m_mutex.unlock();
			return tmp;
		}


		bool push(const T &item){

			m_mutex.lock();
			if(m_size >= m_max_size){

				m_cond.broadcast();
				m_mutex.unlock();
				return false;
			}

			m_back = (m_back+1)%m_max_size;
			m_array[m_back] = item;

			m_size++;

			m_cond.broadcast();
			m_mutex.unlock();
			return true;
		}


		bool pop(T &item){

			m_mutex.lock();
			while(m_size <=0){

				if(!m_cond.wait(m_mutex.get())){

					m_mutex.unlock();
					return false;
				}
			}

			m_front = (m_front +1)%m_max_size;
			item = m_array[m_front];
			m_size--;
			m_mutex.unlock();
			return true;


		}


		bool pop(T &item, int ms_timeout){

			struct timespec t ={ 0, 0 };
			struct timeval now = {0,0};
			gettimeofday(&now, NULL); //获取系统时间
			m_mutex.lock();
			if(m_size<=0){

				t.tv_sec= now.tv_sec+ms_timeout/1000;
				t.tv_nsec=(ms_timeout %1000)*1000;
				if(!m_cond.timewait(m_mutex.get(),t)){

					m_mutex.unlock();
					return false;
				}
			}

			if(m_size <=0){

				m_mutex.unlock();
				return false;
			}

			m_front = (m_front+1)%m_max_size;
			item = m_array[m_front];
			m_size--;
			m_mutex.unlock();
			return true;
		}

		~BlockQueue(){

			m_mutex.lock();
			if(m_array != NULL)
				delete [] m_array;

			m_mutex.unlock();
		}


	private:

		Locker m_mutex;
		Cond m_cond;


		T *m_array;
		int m_size;
		int m_max_size;
		int m_front;
		int m_back;

		
		
};

#endif
