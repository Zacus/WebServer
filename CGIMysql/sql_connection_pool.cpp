
#include "sql_connection_pool.h"


SqlConnectionPool::SqlConnectionPool(){

	this->cur_conn =0;
	this->free_conn =0;
}

//初始化
void SqlConnectionPool::init(string url, string user, string password, string db_name, int port, unsigned int max_conn){

	this->url = url;
	this->port = port;
	this->user = user;
	this->password = password;
	this->database_name = db_name;

	lock.lock();

	for(int i=0;i<max_conn;++i){

		MYSQL *conn = NULL;
		conn = mysql_init(conn);

		if(!conn){

			cout << "Error:"<< mysql_init(conn);
			exit(1);
		}

		conn = mysql_real_connect(conn, url.c_str(), user.c_str(), password.c_str(), db_name.c_str(), port, NULL , 0);

		
		if(!conn){

			cout<<"Error:"<< mysql_error(conn);
			exit(1);
		}

		connList.push_back(conn);
		++free_conn;
	}

	reserve = Sem(free_conn);

	this->max_conn = free_conn;

	lock.unlock();

	
}

//获取数据库连接
MYSQL *SqlConnectionPool::GetConnection(){

	MYSQL *conn =NULL;

	if(!connList.size()) 
		return NULL;


	//p操作
	reserve.wait();

	lock.lock();

	//从连接池中取出一个连接
	conn = connList.front();
	connList.pop_front();

	--free_conn;
	++cur_conn;

	lock.unlock();
	return conn;


}

//释放连接
bool SqlConnectionPool::ReleaseConnection(MYSQL *conn){

	if(!conn)
		return false;

	lock.lock();

	//回收
	connList.push_back(conn);
	++free_conn;
	--cur_conn;

	lock.unlock();

	//V操作
	reserve.post();

	return true;
}

//获取连接个数
int SqlConnectionPool::GetFreeConn(){

	return this->free_conn;
}

//销毁连接
void SqlConnectionPool::DestroyPool(){

	lock.lock();
	if(connList.size()>0){

		list<MYSQL *>::iterator it;
		for(it = connList.begin(); it != connList.end();++it){

			MYSQL *conn=*it;
			mysql_close(conn);
		}

		cur_conn =0;
		free_conn =0;
		connList.clear();

		lock.unlock();
	}
	lock.unlock();
}


//单例模式
SqlConnectionPool *SqlConnectionPool::get_instance(){

	static SqlConnectionPool connPool;
	return &connPool;
}

SqlConnectionPool::~SqlConnectionPool(){

	DestroyPool();
}

ConnectionRAII::ConnectionRAII(MYSQL **SQL, SqlConnectionPool *connPool){


	*SQL = connPool->GetConnection();
	
	connRAII = *SQL;

	poolRAII = connPool;
}

ConnectionRAII::~ConnectionRAII(){

	poolRAII->ReleaseConnection(connRAII);
}

