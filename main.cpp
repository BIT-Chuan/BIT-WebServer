#include<iostream>
#include"webserver/webserver.h"

int main()
{
    WebServer* web = new WebServer(6789, 8, "root", "637210", "studentDB", 3306);

    web->run();
    return 0;
}