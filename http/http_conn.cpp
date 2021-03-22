
#include "http_conn.h"
#include "../log/log.h"
#include <map>


//#define connfdET
#define connfdLT 

//#define listenfdET
#define listenfdLT


int HttpConn::m_user_count=0;
int HttpConn::m_epollfd = -1;




const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";
//用户名和密码
map<string,string> users;
Locker m_lock;

const char *doc_root = "/home/zs/WebServer/root";


void HttpConn::initMysqlResult(SqlConnectionPool *connPool){


	//先从连接池中取出一个连接
	MYSQL *mysql = NULL;
	ConnectionRAII mysqlcon(&mysql, connPool);

	//在user表中检索username,password数据,浏览器端输入
	if(mysql_query(mysql,"SELECT username,password FROM user")){

		LOG_ERROR("SELECT error:%s\n",mysql_error(mysql));
	}

	//从表中检索完整的结果集合
	MYSQL_RES *result = mysql_store_result(mysql);

	//返回结果集中的列数
	int num_fields = mysql_num_fields(result);

	//返回所有字段结构的数组
	MYSQL_FIELD *fields = mysql_fetch_fields(result);

	//从结果集中获取下一行,将对应的用户名和密码存入map中
	while(MYSQL_ROW row = mysql_fetch_row(result)){

		string tmp1(row[0]);
		string tmp2(row[1]);
		users[tmp1]=tmp2;
	}
}

// 对文件描述符设置 非阻塞
int setnonblocking(int fd){

	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

//将内核事件表注册读事件,ET模式,选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot)
{

	epoll_event ev;
	ev.data.fd = fd;

#ifdef connfdET
	ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef connfdLT
	ev.events = EPOLLIN | EPOLLRDHUP;
#endif

#ifdef listenfdET
	ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef listenfdLT
	ev.events = EPOLLIN | EPOLLRDHUP;
#endif

	if(one_shot){

		ev.events |= EPOLLONESHOT;
	}
	epoll_ctl(epollfd,  EPOLL_CTL_ADD, fd, &ev);
	setnonblocking(fd);
}

//从内核事件表删除描述符
void removefd(int epollfd, int fd){

	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd ,0);
	close(fd);
}

//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev){

	epoll_event event;
	event.data.fd = fd;

#ifdef connfdET
	event.events = ev | EPOLLET | EPOLLRDHUP;
#endif

#ifdef connfdLT
	event.events = ev | EPOLLRDHUP;
#endif

	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

//关闭连接,关闭一个连接,客户总量减一
void HttpConn::closeConn(bool real_close){

	if(real_close && (m_sockfd != -1)){

		removefd(m_epollfd, m_sockfd);
		m_sockfd =-1;
		m_user_count--;
	}
}


//初始化连接,外部调用初始化套接字
void HttpConn::init(int sockfd, const sockaddr_in &addr){

	m_sockfd = sockfd;
	m_address = addr;

	addfd(m_epollfd, sockfd, true);

	m_user_count++;
	init();
}

//初始化新接受的连接
void HttpConn::init(){

	mysql == NULL;
	bytes_to_send = 0;
	bytes_have_send = 0;
	m_check_state = CHECK_STATE_REQUESTLINE;
	m_linger = false;
	m_method = GET;
	m_url = 0;
	m_version = 0;
	m_content_length =0;
	m_host =0;
	m_start_line = 0;
	m_checked_idx = 0;
	m_read_idx = 0;
	m_write_idx =0;
	cgi =0;
	memset(m_read_buf, '\0',READ_BUFFER_SIZE);
	memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
	memset(m_real_file, '\0', FILENAME_LEN);
}

//从状态机
HttpConn::LINE_STATUS HttpConn::parse_line(){

	char temp;
	for(;m_checked_idx < m_read_idx; ++m_checked_idx){

		temp = m_read_buf[m_checked_idx];
		if(temp == '\r'){

			if((m_checked_idx+1) == m_read_idx){
				return LINE_OPEN;
			}
			else if(m_read_buf[m_checked_idx+1] =='\n'){

				m_read_buf[m_checked_idx++] = '\0';
				m_read_buf[m_checked_idx++] = '\0';

				return LINE_OK;
			}
			return LINE_BAD;
		}
		else if(temp == '\n'){

			if(m_checked_idx >1 && m_read_buf[m_checked_idx -1]=='\r'){

				m_read_buf[m_checked_idx -1] = '\0';
				m_read_buf[m_checked_idx++]='\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
	}
	return LINE_OPEN;

}
//循环读取用户数据,直到无数据可读或对方关闭连接
//非阻塞ET工作模式下,需要一次性将数据读完
bool HttpConn::readOnce(){

	if(m_read_idx >= READ_BUFFER_SIZE){

		return false;
	}

	int bytes_read = 0;
	
#ifdef connfdLT

	bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);

	m_read_idx += bytes_read;

	if(bytes_read <= 0){

		return false;
	}

	return true;
#endif

#ifdef connfdET

	while(true){

		bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE-m_read_idx, 0);

		if(bytes_read == -1){

			if(errno == EAGAIN || errno == EWOULDBLOCK){
				break;
			}
			return false;
		}
		else if(bytes_read==0){

			return false;
		}

		m_read_idx += bytes_read;
	}

	return false;
#endif

}


//解析http请求行,获取请求方法,目标url及http版本号
HttpConn::HTTP_CODE HttpConn::parseRequestLine(char *text){

	m_url = strpbrk(text, " \t");  //m_url=\t/Zacus/WebServer  HTTP/1.1
	if(!m_url){

		return BAD_REQUEST;	
	}
	*m_url++='\0'; // m_url=/Zacus/WebServer HTTP/1.1
	char *method = text;
	if(strcasecmp(method,"GET")==0){

		m_method=GET;
	}else if(strcasecmp(method,"POST")==0){

		m_method = POST;
		cgi = 1;
	}else{
		return BAD_REQUEST;
	}

	m_url+=strspn(m_url," \t");
	m_version = strpbrk(m_url," \t");
	if(!m_version){
		return BAD_REQUEST;
	}
	*m_version++ = '\0';
	m_version +=strspn(m_version,"\t");
	if(strcasecmp(m_version,"HTTP/1.1")!=0){

		return BAD_REQUEST;
	}
	if(strncasecmp(m_version,"http://",7)==0 ){

		m_url +=7;
		m_url = strchr(m_url,'/');
	}

	if(strncasecmp(m_version,"https://",8)==0 ){

		m_url += 8;
		m_url = strchr(m_url,'/');
	}

	if(!m_url || m_url[0]!= '/')

		return BAD_REQUEST;

	if(strlen(m_url) ==1)
		strcat(m_url, "judge.html");

	m_check_state = CHECK_STATE_HEADER;

	return NO_REQUEST;
}

//
HttpConn::HTTP_CODE HttpConn::parseHeaders(char *text){

	
	if(text[0]=='\0'){

		if(m_content_length != 0){

			m_check_state = CHECK_STATE_CONTENT;
			return NO_REQUEST;
		}
		return GET_REQUEST;
	}
	else if(strncasecmp(text, "Connection:", 11) == 0){

		text += 11;
		text += strspn(text, " \t");
		if(strncasecmp(text, "keep-alive", 11)==0)
		{
			m_linger = true;
		}
	}
	else if(strncasecmp(text, "Content-length:", 15)==0)
	{
		text += 15;
		text += strspn(text, " \t");
		m_content_length = atol(text);
	}
	else if(strncasecmp(text, "Host:", 5)==0)
	{
		text +=5;
		text += strspn(text, "\t");
		m_host = text;
	}
	else
	{
		LOG_INFO("oop! unkonw header:%s", text);
		Log::get_instance()->flush();

	}

	return NO_REQUEST;
}



HttpConn::HTTP_CODE HttpConn::parseContent(char *text){

	if(m_read_idx >= (m_content_length + m_checked_idx))
	{
		text[m_content_length] = '\0';

		m_string = text;
		return GET_REQUEST;
	}

	return NO_REQUEST;
}


HttpConn::HTTP_CODE HttpConn::processRead(){

	LINE_STATUS line_status = LINE_OK;
	HTTP_CODE ret = NO_REQUEST;

	char *text = 0;

	while((m_check_state == CHECK_STATE_CONTENT && line_status ==LINE_OK) ||
		       	((line_status = parse_line()) == LINE_OK)){

		text = getLine();
		m_start_line = m_checked_idx;
		LOG_INFO("%s", text);
		Log::get_instance()->flush();
		switch(m_check_state){

			case CHECK_STATE_REQUESTLINE:
				{
					ret = parseRequestLine(text);
					if(ret == BAD_REQUEST)
						return BAD_REQUEST;
					break;
				}
			case CHECK_STATE_HEADER:
				{
					ret = parseHeaders(text);
					if(ret == BAD_REQUEST)
						return BAD_REQUEST;
					else if(ret == GET_REQUEST)
					{
						return doRequest();
					}
					break;
				}

			case CHECK_STATE_CONTENT:
				{

					ret = parseContent(text);
					if(ret == GET_REQUEST)
						return doRequest();
					line_status = LINE_OPEN;

					break;
				}
			default:
				return INTERNAL_ERROR;

		}
	}
	return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::doRequest()
{
	strcpy(m_real_file, doc_root);
	int len = strlen(doc_root);

	const char *p = strrchr(m_url, '/');

	//处理cgi
	if(cgi ==1 && (*(p+1)=='2' || *(p+1) == '3'))
	{
		//根据标志判断是登录检测还是注册检测
		char flag = m_url[1];

		char *m_url_real = (char *)malloc(sizeof(char)* 200);
		strcpy(m_url_real, "/");
		strcat(m_url_real, m_url + 2);
		strncpy(m_real_file +len, m_url_real, FILENAME_LEN - len -1);
		free(m_url_real);

		//将用户名和密码 提取出来
		//user=xxx&password=123
		char name[100];
		char password[100];
		int i=0;
		for(i=5;m_string[i]!='&';++i)
			name[i-5] = m_string[i];
		name[i-5] = '\0';

		int j = 0;
		for(i=i+10;m_string[i]!='\0';++i,++j)
			password[j] = m_string[i];
		password[j]='\0';


		//同步线程登录校验
		if(*(p+1) == '3')
		{
			//如果是注册,先检测数据库中是否有重名
			char *sql_insert = (char *)malloc(sizeof(char)* 200);
			strcpy(sql_insert,"INSERT INTO user(username,password) VALUES(");
			strcat(sql_insert, "'");
			strcat(sql_insert, name);
			strcat(sql_insert, "', '");
			strcat(sql_insert, password);
			strcat(sql_insert, "')");

			//判断map中能否找到重名的用户名
			if(users.find(name) == users.end()){

				//向数据库中插入数据时,需要通过锁来同步数据
				m_lock.lock();
				int ret = mysql_query(mysql,sql_insert);
				users.insert(pair<string,string>(name,password));
				m_lock.unlock();

				if(!ret)
					strcpy(m_url, "/log.html");
				else
					strcpy(m_url,"/registerError.html");


			}
			else
				strcpy(m_url, "/registerError.html");
		}

		//登录
		else if(*(p+1) == '2')
		{
			if(users.find(name)!= users.end() && users[name] == password )
				
				strcpy(m_url, "/welcome.html");
			else
				strcpy(m_url,"/logError.html");
		}

	}

	//注册页面
	if(*(p+1) == '0')
	{
		char *m_url_real = (char *)malloc(sizeof(char)*200);
		strcpy(m_url_real,"/register.html");
		strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}

	//登录页面
        else if(*(p+1) == '1')
	{
		char *m_url_real = (char *)malloc(sizeof(char)*200);
		strcpy(m_url_real,"/log.html");
		strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}

	
	//图片页面
        else if(*(p+1) == '5')
	{
		char *m_url_real = (char *)malloc(sizeof(char)*200);
		strcpy(m_url_real,"/picture.html");
		strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}

	//视频页面
        else if(*(p+1) == '6')
	{
		char *m_url_real = (char *)malloc(sizeof(char)*200);
		strcpy(m_url_real,"/videos.html");
		strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}

	//关注页面
        else if(*(p+1) == '7')
	{
		char *m_url_real = (char *)malloc(sizeof(char)*200);
		strcpy(m_url_real,"/fans.html");
		strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}


	//发送url实际请求的文件
        else
	{
		strncpy(m_real_file+len,m_url, FILENAME_LEN-len-1);
	}

	//获取文件信息,成功返回0,失败返回-1
	if(stat(m_real_file, &m_file_stat) <0)
		
		return NO_RESOURCE;

	if(!(m_file_stat.st_mode & S_IROTH) )

		return FORBIDDEN_REQUEST;

	//判断是否为目录	
	if(S_ISDIR(m_file_stat.st_mode))

		return BAD_REQUEST;

	int fd = open(m_real_file, O_RDONLY);

	//将文件映射进内存
	m_file_address = (char *)mmap(0,m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);

	return FILE_REQUEST;
}


void HttpConn::unmap(){

	if(m_file_address){

		munmap(m_file_address, m_file_stat.st_size);
		m_file_address = 0;
	}
}

bool HttpConn::write(){

	int temp = 0;
	if(bytes_to_send == 0){

		modfd(m_epollfd, m_sockfd, EPOLLIN);
		init();

		return true;
	}

	while(1){

		temp = writev(m_sockfd, m_iv, m_iv_count);

		if(temp <0)
		{
			if(errno == EAGAIN)
			{
				modfd(m_epollfd, m_sockfd, EPOLLOUT);
				return true;
			}
			unmap();

			return false;
		}

		bytes_have_send += temp;
		bytes_to_send -= temp;
		if(bytes_have_send >= m_iv[0].iov_len)
		{
			m_iv[0].iov_len = 0;
			m_iv[0].iov_base = m_file_address + (bytes_have_send - m_write_idx);
			m_iv[1].iov_len = bytes_to_send;
		}
		else
		{
			m_iv[0].iov_base = m_write_buf + bytes_have_send;
			m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
		}

		if(bytes_to_send <= 0 )
		{
			unmap();
			modfd(m_epollfd, m_sockfd, EPOLLIN);

			if(m_linger)
			{
				init();
				return true;
			}
			else
			{
				return false;
			}

		}
	}
}

bool HttpConn::addResponse(const char *format, ...)
{

	if(m_write_idx >= WRITE_BUFFER_SIZE)

		return false;

	va_list arg_list;
	va_start(arg_list, format);
	int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE -1-m_write_idx, format, arg_list);
	if(len >= (WRITE_BUFFER_SIZE-1-m_write_idx))
	{
		va_end(arg_list);
		return false;
	}

	m_write_idx += len;
	va_end(arg_list);

	LOG_INFO("request:%s", m_write_buf);
	Log::get_instance()->flush();

	return true;

}

bool HttpConn::addStatusLine(int status, const char *title){

	return addResponse("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HttpConn::addHeaders(int content_len)
{
	return addContentLength(content_len)&& addLinger() &&
		addBlankLine();
}

bool HttpConn::addContentLength(int content_len)
{
	return addResponse("Content-Length:%d\r\n", content_len);
}

bool HttpConn::addContentType()
{
	return addResponse("Content-Type:%s\r\n","text/html");
}

bool HttpConn::addLinger()
{
	return addResponse("Connection:%s\r\n", (m_linger == true) ? "keep-alive":"close");
}

bool HttpConn::addBlankLine()
{
	return addResponse("%s", "\r\n");
}

bool HttpConn::addContent(const char *content)
{
	return addResponse("%s", content);
}



bool HttpConn::processWrite(HTTP_CODE ret){

	switch(ret){

		case INTERNAL_ERROR:
			{
				addStatusLine(500, error_500_title);
				addHeaders(strlen(error_500_form));
				if(!addContent(error_500_form))
					return false;
				break;		
			}
		case BAD_REQUEST:
			{
				addStatusLine(404, error_404_title);
				addHeaders(strlen(error_404_form));
				if(!addContent(error_404_form))
					return false;
				break;		
			}

		case FORBIDDEN_REQUEST:

			{
				addStatusLine(403, error_403_title);
				addHeaders(strlen(error_403_form));
				if(!addContent(error_403_form))
					return false;
				break;		
			}

		case FILE_REQUEST:
			{
				addStatusLine(200,ok_200_title);
				if(m_file_stat.st_size != 0){

					addHeaders(m_file_stat.st_size);
					m_iv[0].iov_base = m_write_buf;
					m_iv[0].iov_len = m_write_idx;
					m_iv[1].iov_base = m_file_address;
					m_iv[1].iov_len = m_file_stat.st_size;
					m_iv_count = 2;
					bytes_to_send = m_write_idx + m_file_stat.st_size;

					return true;
				}
				else
				{
					const char *ok_string = "<html><body></body></html>";
					addHeaders(strlen(ok_string));
					if(!addContent(ok_string))
						return false;

				}

			}
			
		default:
			return false;
	}

	m_iv[0].iov_base = m_write_buf;
	m_iv[0].iov_len = m_write_idx;
	m_iv_count = 1;
	bytes_to_send = m_write_idx;
	return true;

}



void HttpConn::process(){

	HTTP_CODE read_ret = processRead();
	if(read_ret == NO_REQUEST){

		modfd(m_epollfd, m_sockfd, EPOLLIN);
		return;
	}

	bool write_ret = processWrite(read_ret);
	if(!write_ret){

		closeConn();
	}

	modfd(m_epollfd, m_sockfd, EPOLLOUT);

}







