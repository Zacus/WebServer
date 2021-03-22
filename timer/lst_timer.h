#ifndef LST_TIMER_H
#define LST_TIMER_H

#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include "../log/log.h"

//声明定时器类
class util_timer;

//连接资源
struct client_data
{
	//客户端socket地址
	sockaddr_in address;

	//socket文件描述符
	int sockfd;

	//定时器
	util_timer *timer;
};

//定时器类
class util_timer
{
	public:
		util_timer() : prev(NULL),next(NULL) {}

	public:

		//超时时间
		time_t expire; 

		//回调函数
		void (*cb_func)(client_data *);

		//连接资源
		client_data *user_data;

		//前驱定时器
		util_timer *prev;

		//后继定时器
		util_timer *next;
};


class sort_timer_lst
{

	public:
		sort_timer_lst() : head(NULL), tail(NULL) {}

		~sort_timer_lst()
		{
			util_timer *tmp = head;
			while(tmp){

				head = tmp->next;
				delete tmp;
				tmp = head;
			}
		}

		//添加定时器
		void add_timer(util_timer *timer)
		{

			if(!timer) return ;
			if(!head){

				head=tail=timer;
				return;
			}
		

			// 如果新的定时器超时时间小于当前头部结点
			//直接将当前定时器结点作为头节点
			if(timer->expire < head->expire){

				timer->next = head;
				head->prev = timer;
				head = timer;
				return;
			}

			//否则调用私有成员函数调整节点链接位置
			add_timer(timer, head);
		}

		//调整定时器,任务发生变化时,调整定时器在链表中的位置
		void adjust_timer(util_timer *timer){

			if(!timer) return;

			util_timer *tmp = timer->next;

			//被调整的定时器在链表尾部定时器超时值,不调整
			//定时器超时仍然小于下一个
			if(!tmp || (timer->expire < tmp->expire))
				return;

			//被调整的定时器是链表头节点,将定时器取出,重新插入
			if(timer == head){

				head = head->next;
				head->prev = NULL;
				timer->next = NULL;
				add_timer(timer, head);
			}
			else                    //被调整定时器在链表中,将定时器取出,重新插入
			{
				timer->prev->next = timer->next;
				timer->next->prev = timer->prev;
				add_timer(timer,timer->next);
			}

		}

		//删除定时器
		void del_timer(util_timer * timer)
		{
			if(!timer) return ;

			if((timer == head) && (timer == tail))
			{
				delete timer;
				head =NULL;
				tail =NULL;
				return ;
			}

			if(timer == head)
			{
				head =head->next;
				head->prev = NULL;
				delete timer;
				return ;
			}

			if(timer == tail){

				tail = tail->prev;
				tail->next = NULL;
				delete timer;
				return ;
			}

			timer->prev->next = timer->next;
			timer->next->prev = timer->prev;
			delete timer;


		}

		//定时任务处理函数
		void tick()
		{
			if(!head) return ;

			LOG_INFO("%s", "timer tick");
			Log::get_instance()->flush();

			//获取当前时间
			time_t cur= time(NULL);
			util_timer * tmp = head;

			//遍历定时器链表
			while(tmp){

				//链表为升序排列
				//当前时间小于定时器的超时时间,后面的定时器也没有到期

				if(cur < tmp->expire)
				{
					break;
				}

				//定时器到期,调用回调函数,执行定时事件
				tmp->cb_func(tmp->user_data);

				//将处理后的定时器从链表中删除,并重置头节点
				head = tmp ->next;
				if(head){
					head->prev = NULL;
				}

				delete tmp;
				tmp = head;
			}
		}


	private:

		void add_timer(util_timer *timer, util_timer * lst_head)
		{
			util_timer *prev = lst_head;
			util_timer *tmp = prev->next;

			while(tmp){

				if(timer->expire < tmp->expire)
				{
					prev->next = timer;
					timer ->next = tmp;
					tmp->prev = timer;
					timer->prev = prev;
					break;
				}

				prev = tmp;
				tmp = tmp ->next;
			}

			if(!tmp){

				prev->next = timer;
				timer->prev = prev;
				timer->next = NULL;
				tail = timer;
			}
		}

	private:
		util_timer *head;
		util_timer *tail;
};

#endif
