#ifndef HTTP_CONN_H_
#define HTTP_CONN_H_

#include<unistd.h>
#include<sys/types.h>
#include<sys/epoll.h>
#include<sys/types.h>
#include<string.h>
#include<sys/socket.h>
#include<fcntl.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<pthread.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<stdlib.h>
#include<stdio.h>
#include<stdarg.h>
#include<errno.h>
#include<sys/wait.h>
#include<sys/uio.h>
#include"../lock/locker.h"
#include<mysql/mysql.h>
#include"../CGIMysql/sql_connection_pool.h"

class HttpConn{

	public:

		static const int FILENAME_LEN = 200;
		static const int READ_BUFFER_SIZE = 2048;
		static const int WRITE_BUFFER_SIZE = 1024;

		//http请求方式
		enum METHOD{

			GET = 0,
			POST,
			HEAD,
			PUT,
			DELETE,
			TRACE,
			OPTIONS,
			CONNECT,
			PATH
		};

		//主状态机
		enum CHECK_STATE{

			CHECK_STATE_REQUESTLINE = 0,  //解析请求行
			CHECK_STATE_HEADER,	//解析请求报文头部
			CHECK_STATE_CONTENT	//解析消息体,仅用于解析post请求
		};

		//http请求结果
		enum HTTP_CODE{

			NO_REQUEST,	//请求不完整,需要继续读取请求报文数据
			GET_REQUEST,	//获得完整的http请求,调用doRequest完成请求资源映射
			BAD_REQUEST,	//http请求报文语法有误或请求资源为目录
			NO_RESOURCE,	//请求资源不存在
			FORBIDDEN_REQUEST,	//客户对服务没有足够的访问权限,跳转processWrite完成响应报文
			FILE_REQUEST,	//请求资源可以正常访问,跳转processWrite完成响应报文
			INTERNAL_ERROR,	//服务器内部错误
			CLOSED_CONNECTION	//客户端服务器已关闭
		};

		//从状态机,行的读取状态
		enum LINE_STATUS{

			LINE_OK =0, 	//完整读取一行
			LINE_BAD,	//报文语法有错误
			LINE_OPEN	//读取的行不完整
		};


	public:

		HttpConn(){}
		~HttpConn() {}

	public:

		void init(int sockfd, const sockaddr_in &addr);
		void closeConn(bool real_close=true);
		//处理请求
		void process();
		//读入客户一次请求到读缓冲区
		bool readOnce();
		bool write();
		sockaddr_in *getAddress(){

			return &m_address;
		}

		void initMysqlResult(SqlConnectionPool *connPool);


	private:

		void init();
		//解析报文
		HTTP_CODE processRead();
		bool processWrite(HTTP_CODE ret);
		HTTP_CODE parseHeaders(char *text);
		HTTP_CODE parseRequestLine(char *text);
		HTTP_CODE parseContent(char *text);
	
		//响应请求文件
		HTTP_CODE doRequest();	
		char *getLine(){

			return m_read_buf+m_start_line;
		}

		LINE_STATUS parse_line();
		void unmap();
		bool addResponse(const char *format, ...);
		bool addContent(const char *content);;
		bool addStatusLine(int status, const char *title);
		bool addHeaders(int content_length);
		bool addContentType();
		bool addContentLength(int content_length);
		bool addLinger();
		bool addBlankLine();

	public:

		static int m_epollfd;
		static int m_user_count;
		MYSQL *mysql;

	private:

		int m_sockfd;
		sockaddr_in m_address;
		char m_read_buf[READ_BUFFER_SIZE];
		int m_read_idx;
		int m_checked_idx;
		int m_start_line;
		char m_write_buf[WRITE_BUFFER_SIZE];
		int m_write_idx;

		CHECK_STATE m_check_state;
		METHOD m_method;
		char m_real_file[FILENAME_LEN];
		char *m_url;
		char *m_version;
		char *m_host;
		int m_content_length;
		bool m_linger;
		char *m_file_address;
		struct stat m_file_stat;
		struct iovec m_iv[2];
		int m_iv_count;
		int cgi;
		char *m_string; //储存请求头数据
		int bytes_to_send;
		int bytes_have_send;

};

#endif

