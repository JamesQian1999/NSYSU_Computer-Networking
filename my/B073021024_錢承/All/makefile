
all: client.cpp server.cpp
	clear ; g++ client.cpp -o client -std=c++11 -pthread
	clear ; g++ server.cpp -o server -std=c++11 -pthread

client: client.cpp
	clear ; g++ client.cpp -o client -std=c++11 -pthread

server: server.cpp
	clear ; g++ server.cpp -o server -std=c++11 -pthread

run_server: server
	clear ; ./server

clean:
	rm client server received\(*\)*

