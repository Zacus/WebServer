#ifndef SQL_CONNECTION_POOL
#define SQL_CONNECTION_POOL


//#include <stdio.h>
#include <mysql/mysql.h>
#include <iostream>
#include <string>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include "../lock/locker.h"

using namespace std;

class SqlConnectionPool{

	public:

		SqlConnectionPool();

		//初始化
		void init(string url, string user, string password, string db_name, int port, unsigned int max_conn);

		//获取数据库连接
		MYSQL *GetConnection();   

		//释放连接
		bool ReleaseConnection(MYSQL *conn);

		//获取连接
		int GetFreeConn();

		//销毁连接
		void DestroyPool();


		//单例模式
		static SqlConnectionPool *get_instance();


		~SqlConnectionPool();

	private:

		//最大连接数
		unsigned int max_conn; 

		//当前已使用的连接个数
		unsigned int cur_conn;

		//当前空闲的连接数
		unsigned int free_conn;

	private:
		
		Locker lock;
		//连接池
		list<MYSQL *> connList;
		Sem reserve;

	private:

		//主机地址
		string url;

		//数据库端口号
		string port;

		//用户名
		string user;

		//密码
		string password;

		//数据库名
		string database_name;


	
};

class ConnectionRAII{

	public:

		ConnectionRAII(MYSQL **conn, SqlConnectionPool * connPool);

		~ConnectionRAII();

	private:

		MYSQL *connRAII;

		SqlConnectionPool *poolRAII;
};



#endif

