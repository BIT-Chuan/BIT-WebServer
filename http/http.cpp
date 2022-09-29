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

// void Http::initmysql_result(connection_pool *connPool)
// {
//     //先从连接池中取一个连接
    
// }


//初始化连接,外部调用初始化套接字地址
void Http::init(int sockfd, const sockaddr_in &addr, char *root,
                     int close_log, string user, string passwd, string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;

    //m_user_count++;

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
    //mysql = NULL;
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
            //char str[INET_ADDRSTRLEN];
            //inet_ntop(AF_INET, &m_address.sin_addr, str, sizeof(str));
            //Log::getInstance()->writelog(3, static_cast<string>(str), "Request format error!\n");
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
            //char str[INET_ADDRSTRLEN];
            //inet_ntop(AF_INET, &m_address.sin_addr, str, sizeof(str));
            //Log::getInstance()->writelog(3, static_cast<string>(str), "Request format error!\n");
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

string aaaline;
char* aaaa;

//解析Http请求行，获得请求方法，目标url及Http版本号
Http::HTTP_CODE Http::parse_request_line(char *text)
{
    string line = text; 
    aaaline = line;
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch subMatch;
    {
        lock_guard<mutex>locker(m_mutex);
        //cout << "111" << endl;
        cout << "line:" << aaaline << endl;
        //cout << "text:" << text << endl;
    }
    if(regex_match(line, subMatch, patten)){
        {
            lock_guard<mutex>locker(m_mutex);
            cout << "2222" << endl;
        }
        //cout << "method:" << subMatch[1] << endl;
        if(subMatch[1] == "GET"){
            m_method = GET;
        }else{
            m_method = POST;
            cgi = 1;
        }
        if(subMatch[2] == "/"){
            m_url = "/index.html";

        }else if(subMatch[2] == "/post.html"){
            m_url = "/post.html";
        }
        else if(subMatch[2] == "/postresponse.html"){
            m_url = "/postresponse.html";
        }
        else if(subMatch[2] == "/stuid.html"){
            m_url = "/stuid.html";
        }
        else if(subMatch[2] == "/page1.html"){
            m_url = "/page1.html";
        }
        else{
            m_url = const_cast<char*>(static_cast<string>(subMatch[2]).c_str());
        }
        //m_url = const_cast<char*>(static_cast<string>(subMatch[2]).c_str());
        aaaa = m_url;
        {
            lock_guard<mutex>locker(m_mutex);
            cout << "m_url" << aaaa << endl;
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
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &m_address.sin_addr, str, sizeof(str));
        Log::getInstance()->writelog(3, static_cast<string>(str), "parse_headers return GET_REQUEST\n");
        return GET_REQUEST;
    }
    string line = text;
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;


    if(regex_match(line,subMatch,patten)){
        if(subMatch[1] == "Connection"){
            if(subMatch[2] == "keep-alive"){
                m_linger = true;
            }
        }
        else if(subMatch[1] == "Content-Length"){
            m_content_length = stoi(static_cast<string>(subMatch[2]));
        }
        else if(subMatch[1] == "Host"){
            m_host = const_cast<char*>(static_cast<string>(subMatch[2]).c_str());
        }
        else{
            char str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &m_address.sin_addr, str, sizeof(str));
            //Log::getInstance()->writelog(3, static_cast<string>(str), "Unkown header: " + static_cast<string>(subMatch[1]) + "\n");
        }
        return NO_REQUEST;
    }
}

//判断Http请求是否被完整读入
Http::HTTP_CODE Http::parse_content(char *text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &m_address.sin_addr, str, sizeof(str));
        Log::getInstance()->writelog(3, static_cast<string>(str), "parse_contetn return GET_REQUEST\n");
        text[m_content_length] = '\0';
        m_string = text;
        return GET_REQUEST;
    }
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &m_address.sin_addr, str, sizeof(str));
    Log::getInstance()->writelog(3, static_cast<string>(str), "parse_contetn return NO_REQUEST\n");
    return NO_REQUEST;
}

Http::HTTP_CODE Http::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        cout << "process_read::m_check_state:" << m_check_state << endl;
        text = get_line();
        cout << "process_read::text:" << text << endl;
        m_start_line = m_checked_idx;
        //char str[INET_ADDRSTRLEN];
        //inet_ntop(AF_INET, &m_address.sin_addr, str, sizeof(str));
        //Log::getInstance()->writelog(3, static_cast<string>(str), static_cast<string>(text) + "\n");
        
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            cout << "CHECK_STATE_REQUESTLINE" << endl;
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST){
                char str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &m_address.sin_addr, str, sizeof(str));
                Log::getInstance()->writelog(3, static_cast<string>(str), "BAD_REQUEST\n");
                return BAD_REQUEST;
            }
            else if (ret == GET_REQUEST)
            {
                char str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &m_address.sin_addr, str, sizeof(str));
                Log::getInstance()->writelog(3, static_cast<string>(str), "GET_REQUEST->do_request\n");

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
            char str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &m_address.sin_addr, str, sizeof(str));
            Log::getInstance()->writelog(3, static_cast<string>(str), "INTERNAL_ERROR\n");
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
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &m_address.sin_addr, str, sizeof(str));
    Log::getInstance()->writelog(3, static_cast<string>(str), "do_request\n");

    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);

    //printf("m_url:%s\n", m_url);

    string s = m_url;
    int index = s.find_last_of('/') + 1;

    cout << "m_url:" << m_url << endl;


    s = s.substr(index, s.size() - index);
    //char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &m_address.sin_addr, str, sizeof(str));
    Log::getInstance()->writelog(3, static_cast<string>(str), "s:" + s + "\n");

    //根据请求url执行业务
    //URL: http://IP address:port/page1.html
    if(s == "page1.html"){
        //获取 /page1.html的内容并加入到content_buf中去
        cout << "s:" << s << endl;
        string path = doc_root + s;
        read_html(path);
    }
    //http://IP address:port/
    else if(s == "index.html"){
        cout << "s:" << s << endl;
        string path = doc_root + s;
        read_html(path);
    }
    else if(s == "post.html" && m_method == GET){
        cout << "s:" << s << endl;
        string path = doc_root + s;
        read_html(path);
    }
    // else if(s == "postresponse.html" && m_method == POST){
    //     char str[INET_ADDRSTRLEN];
    //     inet_ntop(AF_INET, &m_address.sin_addr, str, sizeof(str));
    //     Log::getInstance()->writelog(3, static_cast<string>(str), m_string);
    //     string path = doc_root + s;
    //     read_html(path);
    // }
    //URL: http://IP address:port/cgi-bin/calculator.pl 且请求为post
    else if(s == "postresponse.html" && m_method == POST){
        string line = m_string;
        regex patten("^int1=([1-9]*)&opt=([^&]*)&int2=([1-9]]*)$");
        smatch subMatch;
        if(regex_match(line,subMatch,patten)){
            int a = stoi(subMatch[1]);
            int b = stoi(subMatch[3]);
            int ans = 0;
            string opt = subMatch[2];
            if(opt == "add"){
                ans = a + b;
            }
            else if(opt == "sub"){
                ans = a - b;
            }
            else if(opt == "mul"){
                ans = a * b;
            }
            else if(opt == "div"){
                ans = a / b;
            }
            else{}
            snprintf(content_buf, sizeof(content_buf), "%d", ans);
            content_idx += strlen(content_buf);
        }
        else{

        }
    }
    //URL: http://IP address:port/cgi-bin/query.pl 请求为GET,传递html页面
    else if(s == "stuid.html" && m_method == GET){
        cout << "s:" << s << endl;
        string path = doc_root + s;
        read_html(path);
    }
    //URL: http://IP address:port/cgi-bin/query.pl 请求为POST,返回查询内容
    else if(s == "stuid.html" && m_method == POST){
        //请求体内容为：  ID=${id}
        char id[100];
        for (int i = 3; m_string[i] != '\0'; ++i)id[i-3] = m_string[i];
        //todo:根据id返回sql结果
        
        //todo:将学生信息的html文本形式放入m_write_buf
        
    }


    // if (stat(m_real_file, &m_file_stat) < 0){
    //     char str[INET_ADDRSTRLEN];
    //     inet_ntop(AF_INET, &m_address.sin_addr, str, sizeof(str));
    //     //Log::getInstance()->writelog(3, static_cast<string>(str), "No resource: " + static_cast<string>(m_real_file) + "\n");
    //     cout << "no resource" << endl;
    //     return NO_RESOURCE;
    // }

        

    // if (!(m_file_stat.st_mode & S_IROTH)){
    //     char str[INET_ADDRSTRLEN];
    //     inet_ntop(AF_INET, &m_address.sin_addr, str, sizeof(str));
    //     //Log::getInstance()->writelog(3, static_cast<string>(str), "No priority: " + static_cast<string>(m_real_file) + "\n");
    //     return FORBIDDEN_REQUEST;
    // }
        

    // if (S_ISDIR(m_file_stat.st_mode)){
    //     char str[INET_ADDRSTRLEN];
    //     inet_ntop(AF_INET, &m_address.sin_addr, str, sizeof(str));
    //     //Log::getInstance()->writelog(3, static_cast<string>(str), "Invalid path: " + static_cast<string>(m_real_file) + "\n");
    //     return BAD_REQUEST;
    // }
        

    // int fd = open(m_real_file, O_RDONLY);
    // m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    // close(fd);
    return FILE_REQUEST;
}


bool Http::write(int* save_errno)
{
    int temp = 0;
    cout << "write::m_write_buf:" << m_write_buf << endl;

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
            cout << "case temp < 0" << endl;
            if (errno == EAGAIN)
            {
                cout << "errno == EAGAIN" << endl;
                *save_errno = EAGAIN;
                return true;
            }
            cout << "errno != EAGAIN" << endl;
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

            if (m_linger)
            {
                init();
                cout << "reinit success" << endl;
                return true;
            }
            else
            {
                cout << "http1 close conn" << endl;
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
        if (content_idx != 0)
        {
            //往缓冲区写入首部及内容
            add_headers(content_idx);
            add_content(content_buf);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv_count = 1;
            bytes_to_send = m_write_idx;
            bzero(&content_buf, sizeof(content_buf));
            content_idx = 0;
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
    cout << "process_write::m_write_buf:" << m_write_buf << endl;
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



bool Http::read_html(string url){
    ifstream infile;
	infile.open(url);
    
	if (!infile.is_open())
	{
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &m_address.sin_addr, str, sizeof(str));
        Log::getInstance()->writelog(3, static_cast<string>(str), "Open html file failed!\n");
		return false;
	}
	string buf;
	while (getline(infile,buf))
	{
		strcat(content_buf,buf.c_str());
        content_idx += strlen(buf.c_str());
	}
    cout << "content_buf:" << content_buf << endl;
	return true;
}