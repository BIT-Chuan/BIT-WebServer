#include<iostream>
#include<stdlib.h>
#include<string.h>
#include<string>
#include<mutex>
#include <sys/time.h>
#include"log.h"
using namespace std;
Log::Log(int bufmaxsize=5,string filename="log.txt")
{
    buf_max_size=bufmaxsize;
    file_name=filename;
    now_min=0;
    buf_txt = new char[256];
    mutex_log_bufpoint.lock();
    buf_point=&buf1;
    buf_flag=1;
    mutex_log_bufpoint.unlock();
}

Log::~Log()
{
    if (logfile != NULL)
    {
        fclose(logfile);
    }
}

void Log::writelog(int logtype,string ipaddr,string logtxt)
{
    
    char s[10] = {0};
    switch (logtype)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    mutex_log_bufpoint.lock();
    int n = snprintf(buf_txt, 999, "%s %s-- [%d-%02d-%02d %02d:%02d:%02d] : %s",
             s, ipaddr.c_str(), my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
             my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, logtxt.c_str());
             cout<<"!"<<endl;
    buf_txt[n] = '\n';
    buf_txt[n+1] = '\0';
    string log_str = buf_txt;
    mutex_log_bufpoint.unlock();
    if(buf_point->size() >= buf_max_size||now_min!=my_tm.tm_min){
        if(buf_flag==1){
            mutex_log_bufpoint.lock();
            buf_point=&buf2;
            buf_flag=0;
            mutex_log_buf2.lock();
            buf_point->push(log_str);
            mutex_log_buf2.unlock();
            logfile = fopen(file_name.c_str(),"a");
            while(!buf1.empty()){
                fputs(buf1.front().c_str(),logfile);
                buf1.pop();
            }
            fclose(logfile);
            mutex_log_bufpoint.unlock();
        }
        else{
            mutex_log_bufpoint.lock();
            buf_point=&buf1;
            buf_flag=1;
            mutex_log_buf1.lock();
            buf_point->push(log_str);
            mutex_log_buf1.unlock();
            logfile = fopen(file_name.c_str(),"a");
            while(!buf2.empty()){
                fputs(buf2.front().c_str(),logfile);
                buf2.pop();
            }
            fclose(logfile);
            mutex_log_bufpoint.unlock();
        }
        if(now_min!=my_tm.tm_min){
            now_min=my_tm.tm_min;
        }
    }
    else{
        if(buf_flag==1){
            mutex_log_buf1.lock();
            buf_point->push(log_str);
            mutex_log_buf1.unlock();
        }
        else{
            mutex_log_buf2.lock();
            buf_point->push(log_str);
            mutex_log_buf2.unlock();
        }
    }

}