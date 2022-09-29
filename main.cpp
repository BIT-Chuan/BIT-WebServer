#include<iostream>
#include"webserver/webserver.h"

int main()
{
    WebServer* web = new WebServer(12345, 8, 0, "", "", "");
    web->run();
    return 0;
}