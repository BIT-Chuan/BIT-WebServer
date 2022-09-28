#ifndef LOG_H
#define LOG_H

#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include<string>
#include<queue>
#include<mutex>

using namespace std;

mutex mutex_log_buf1;
mutex mutex_log_buf2;
mutex mutex_log_bufpoint;
queue<string> buf1;
queue<string> buf2;

class Log{
    private:
        char *buf_txt;
        int now_min;
        int buf_max_size;
        int buf_flag;
        string file_name;
        queue<string> *buf_point;
        FILE *logfile;

    public:
        Log(int bufmaxsize,string filename);
        ~Log();
        void writelog(int logtype,string ipaddr,string logtxt);
};
#endif