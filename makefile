
server: main.cpp  ./http/http.cpp  ./epoll/epoller.cpp  ./webserver/webserver.cpp ./log/log.cpp
	g++ -o server  $^ -lpthread

clean:
	rm  -r server