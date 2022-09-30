#ifndef HTTP_H
#define HTTP_H

#include<regex>
#include<cstring>
#include<unistd.h>
#include<signal.h>
#include<sys/types.h>
#include<stdlib.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <unordered_map>
#include<mutex>
#include<fstream>
#include<cstring>
#include<iostream>

#include"../log/log.h"
#include"../sqlconnpool/sqlconnpool.h"

class Http{
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 4096;
    enum METHOD
    {
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
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    Http() {}
    ~Http() {}

public:
    void init(int sockfd, const sockaddr_in &addr, char *, std::string user, std::string passwd, std::string sqlname);
    bool process();
    bool read_once();
    bool write(int*);
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    //void initmysql_result(connection_pool *connPool);
    int timer_flag;
    int improv;
    int getFd(){
        return m_sockfd;
    }

private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() { return m_read_buf + m_start_line; };
    LINE_STATUS parse_line();
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

    //根据路径读入html
    bool read_html(std::string url);
public:
    static int m_epollfd;
    static int m_user_count;
    //MYSQL *mysql;
    int m_state;  //读为0, 写为1

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
    char *m_string;
    int bytes_to_send;
    int bytes_have_send;
    char *doc_root;

    std::unordered_map<std::string, std::string> m_users;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
    std::mutex m_mutex;

    //传输内容的缓存,用来在业务部分将需要响应的content填入，在process_write中再写入实际的传输响应缓存m_write_buf中
    char content_buf[WRITE_BUFFER_SIZE];
    int content_idx = 0;
};
#endif