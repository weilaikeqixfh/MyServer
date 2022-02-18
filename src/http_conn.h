#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include "epoll.h"
#include "locker.h"

class http_conn
{
public:
    /*文件名长度*/
    static const int FILENAME_LEN = 200;
    /*读缓冲区的大小*/
    static const int READ_BUFFER_SIZE = 2048;
    /*写缓冲区的大小*/
    static const int WRITE_BUFFER_SIZE = 1024;
    /*HTTP请求方法，仅支持GET*/
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
    /*解析客户请求是，主状态机所处的状态*/
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    /*服务器处理HTTP请求可能的结果*/
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
    /*行的读取状态*/
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    //初始化新接收的连接
    void init(int sockfd, const sockaddr_in &addr);
    //关闭连接
    void close_conn();
    // 处理客户请求
    void process();
    // 非阻塞读操作
    bool read_once();
    //非阻塞写操作
    bool write();
private:
    void init();//初始化其余连接的信息
    HTTP_CODE process_read();//解析http请求
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char *text);//解析请求首行
    HTTP_CODE parse_headers(char *text);//解析头
    HTTP_CODE parse_content(char *text);//解析请求体
    HTTP_CODE do_request();//具体的处理
    LINE_STATUS parse_line();//从状态机解析一行，
    char *get_line() { return m_read_buf + m_start_line;  };//获取一行
    void unmap();	

    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();


public:
    //所有socket上的事件都被注册到同一个epoll内核事件表中，所以将epoll文件描述符设置为静态的,共享
    static int m_epollfd;
    static int m_user_count;//统计用户数量

private:
    //该HTTP连接的socket和对方的socket地址
    int m_sockfd;
    sockaddr_in m_address;//socket通信地址
    char m_read_buf[READ_BUFFER_SIZE];//读缓冲大小
    int m_read_idx;//标志读缓冲区以及读入的客户端数据的最后一个字节下一个位置

    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;

    int m_checked_idx;//当前正在分析的字符在读缓冲区的位置
    int m_start_line;//当前正在解析行的起始位置
    char *m_url;
    char *m_version;
    METHOD m_method;
    char *m_host;
    bool m_linger;//是否保持连接
    int m_content_length;//请求体字节数

    char m_real_file[FILENAME_LEN];
    CHECK_STATE m_check_state;//主状态机当前所处的状态
    char *m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int bytes_to_send;
    int bytes_have_send;
};

#endif
