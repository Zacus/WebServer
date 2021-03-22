server:main.c ./threadpool/threadpool.h ./http/http_conn.cpp ./http/http_conn.h ./lock/locker.h ./log/log.cpp ./log/log.h ./log/blocking_queue.h ./CGIMysql/sql_connection_pool.cpp ./CGIMysql/sql_connection_pool.h 
	g++ -o server main.c  ./threadpool/threadpool.h  ./http/http_conn.cpp ./http/http_conn.h ./lock/locker.h ./log/log.cpp ./CGIMysql/sql_connection_pool.cpp ./CGIMysql/sql_connection_pool.h -lpthread -lmysqlclient 


clean:
	rm -r server
