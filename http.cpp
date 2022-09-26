#include"http.h"
using namespace std;

//定义Http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

unordered_map<string, string> users;

void Http::initmysql_result(connection_pool *connPool)
{
    //先从连接池中取一个连接
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool);

    //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}


//初始化连接,外部调用初始化套接字地址
void Http::init(int sockfd, const sockaddr_in &addr, char *root,
                     int close_log, string user, string passwd, string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;

    m_user_count++;

    doc_root = root;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    init();
}

//初始化新接受的连接
//check_state默认为分析请求行状态
void Http::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

//从状态机，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
Http::LINE_STATUS Http::parse_line()
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r')
        {
            if ((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
bool Http::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;
    while (true)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return false;
        }
        else if (bytes_read == 0)
        {
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}

//解析Http请求行，获得请求方法，目标url及Http版本号
Http::HTTP_CODE Http::parse_request_line(char *text)
{
    string line = text; 
    regex patten("^([^ ]*) ([^ ]*) Http/([^ ]*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, patten)){
        if(subMatch[1] == "GET"){
            m_method = GET;
        }else{
            m_method = POST;
            cgi = 1;
        }
        if(subMatch[2] == "/"){
            m_url = "/judge.html";
        }else{
            m_url = const_cast<char*>(static_cast<string>(subMatch[2]).c_str());
        }
        m_version = const_cast<char*>(static_cast<string>(subMatch[3]).c_str());
        m_check_state = CHECK_STATE_HEADER;
        return NO_REQUEST;
    }else{
        return BAD_REQUEST;
    }
    return NO_REQUEST;
}

//解析Http请求的一个头部信息
Http::HTTP_CODE Http::parse_headers(char *text)
{
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    string line = text;
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;

    if(regex_match(line,subMatch,patten)){
        switch (subMatch.str(1))
        {
        case "Connection":
            if(subMatch[2] == "keep-alive"){
                m_linger = true;
            }
            break;
        case "Content-length":
            m_content_length = subMatch[2];
            break;
        case "Host":
            m_host = subMatch[2];
            break;
        default:
            LOG_INFO("oop!unknow header: %s", text);
            break;
        }
        return NO_REQUEST;
    }
}

//判断Http请求是否被完整读入
Http::HTTP_CODE Http::parse_content(char *text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

Http::HTTP_CODE Http::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        m_start_line = m_checked_idx;
        LOG_INFO("%s", text);
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

/**
 * @brief 对请求进行解析（并执行业务逻辑？）
 * 
 * @return Http::HTTP_CODE 
 */
Http::HTTP_CODE Http::do_request()
{
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    //printf("m_url:%s\n", m_url);
    const char *p = strrchr(m_url, '/');

    //根据请求url执行业务
    //URL: http://IP address:port/page1.html
    if(*(p + 1) == 'p'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real,"/page1.html");
        //将网站目录和/register.html进行拼接，更新到m_real_file中
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real));
        free(m_url_real);

        //获取 /page1.html的内容并加入到m_write_buf中去
    }
    //URL: http://IP address:port/cgi-bin/calculator.pl 
    else if(*(p + 1) == 'c'){
        //执行计算操作并将计算结果的html文本形式放入m_write_buf
    }
    //URL: http://IP address:port/cgi-bin/query.pl (assuming it is in PERL)
    else if(*(p + 1) == 'q'){
        //执行插入操作
        //返回sql结果
        //将学生信息的html文本形式放入m_write_buf
    }

    // //处理cgi
    // if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    // {

    //     //根据标志判断是登录检测还是注册检测
    //     char flag = m_url[1];

    //     char *m_url_real = (char *)malloc(sizeof(char) * 200);
    //     strcpy(m_url_real, "/");
    //     strcat(m_url_real, m_url + 2);
    //     strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
    //     free(m_url_real);

    //     //将用户名和密码提取出来
    //     //user=123&passwd=123
    //     char name[100], password[100];
    //     int i;
    //     for (i = 5; m_string[i] != '&'; ++i)
    //         name[i - 5] = m_string[i];
    //     name[i - 5] = '\0';

    //     int j = 0;
    //     for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
    //         password[j] = m_string[i];
    //     password[j] = '\0';

    //     if (*(p + 1) == '3')
    //     {
    //         //如果是注册，先检测数据库中是否有重名的
    //         //没有重名的，进行增加数据
    //         char *sql_insert = (char *)malloc(sizeof(char) * 200);
    //         strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
    //         strcat(sql_insert, "'");
    //         strcat(sql_insert, name);
    //         strcat(sql_insert, "', '");
    //         strcat(sql_insert, password);
    //         strcat(sql_insert, "')");

    //         if (users.find(name) == users.end())
    //         {
    //             int res = 0;
    //             {
    //                 lock_guard<mutex>locker(m_mutex);
    //                 res = mysql_query(mysql, sql_insert);
    //                 users.insert(pair<string, string>(name, password));
    //             }
                
    //             if (!res)
    //                 strcpy(m_url, "/log.html");
    //             else
    //                 strcpy(m_url, "/registerError.html");
    //         }
    //         else
    //             strcpy(m_url, "/registerError.html");
    //     }
    //     //如果是登录，直接判断
    //     //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
    //     else if (*(p + 1) == '2')
    //     {
    //         if (users.find(name) != users.end() && users[name] == password)
    //             strcpy(m_url, "/welcome.html");
    //         else
    //             strcpy(m_url, "/logError.html");
    //     }
    // }

    // if (*(p + 1) == '0')
    // {
    //     char *m_url_real = (char *)malloc(sizeof(char) * 200);
    //     strcpy(m_url_real, "/register.html");
    //     strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    //     free(m_url_real);
    // }
    // else if (*(p + 1) == '1')
    // {
    //     char *m_url_real = (char *)malloc(sizeof(char) * 200);
    //     strcpy(m_url_real, "/log.html");
    //     strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    //     free(m_url_real);
    // }
    // else if (*(p + 1) == '5')
    // {
    //     char *m_url_real = (char *)malloc(sizeof(char) * 200);
    //     strcpy(m_url_real, "/picture.html");
    //     strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    //     free(m_url_real);
    // }
    // else if (*(p + 1) == '6')
    // {
    //     char *m_url_real = (char *)malloc(sizeof(char) * 200);
    //     strcpy(m_url_real, "/video.html");
    //     strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    //     free(m_url_real);
    // }
    // else if (*(p + 1) == '7')
    // {
    //     char *m_url_real = (char *)malloc(sizeof(char) * 200);
    //     strcpy(m_url_real, "/fans.html");
    //     strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    //     free(m_url_real);
    // }
    // else
    //     strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    // int fd = open(m_real_file, O_RDONLY);
    // m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    // close(fd);
    return FILE_REQUEST;
}
void Http::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}
bool Http::write(int* save_errno)
{
    int temp = 0;

    if (bytes_to_send == 0)
    {
        init();
        return true;
    }

    while (1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);

        if (temp < 0)
        {
            if (errno == EAGAIN)
            {
                *save_errno = EAGAIN;
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        if (bytes_to_send <= 0)
        {
            unmap();

            if (m_linger)
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
/**
 * @brief 添加响应信息
 * 
 * @param format 
 * @param ... 
 * @return true 
 * @return false 
 */
bool Http::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);

    LOG_INFO("request:%s", m_write_buf);

    return true;
}
bool Http::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "Http/1.1", status, title);
}
bool Http::add_headers(int content_len)
{
    return add_content_length(content_len) && add_linger() &&
           add_blank_line();
}
bool Http::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}
bool Http::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}
bool Http::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
bool Http::add_blank_line()
{
    return add_response("%s", "\r\n");
}
bool Http::add_content(const char *content)
{
    return add_response("%s", content);
}
/**
 * @brief 用于将返回数据写入写缓存段
 * 
 * @param ret 回复状态码
 * @return true 缓存段写入成功
 * @return false 缓存段写入失败
 */
bool Http::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    //请求正常的情况
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            //往缓冲区写入首部及内容
            add_headers(m_write_idx);
            add_content(m_write_buf);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv_count = 1;
            bytes_to_send = m_write_idx;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
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
bool Http::process()
{
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        return true;
    }
    bool write_ret = process_write(read_ret);
    return false;
}
