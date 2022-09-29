
server: main.cpp  ./http/http.cpp  ./epoll/epoller.cpp  ./webserver/webserver.cpp ./log/log.cpp ./sqlconnpool/sqlconnpool.cpp
	g++ -o server  $^ -lpthread -lmysqlclient

clean:
	rm  -r server