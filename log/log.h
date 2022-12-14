#ifndef LOG_H
#define LOG_H

#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include<string>
#include<queue>
#include<mutex>

using namespace std;


class Log{
    private:

        static Log* m_log;
        char *buf_txt;
        int now_min;
        int buf_max_size;
        int buf_flag;
        string file_name;
        queue<string> *buf_point;
        FILE *logfile;

    public:
    
        mutex mutex_log_buf1;
        mutex mutex_log_buf2;
        mutex mutex_log_bufpoint;
        queue<string> buf1;
        queue<string> buf2;
        static Log* getInstance();
        Log(int bufmaxsize,string filename);
        
        ~Log();
        void writelog(int logtype,string ipaddr,string logtxt);
};




#endif