
all: client.cpp server.cpp
	g++ client.cpp -o client -std=c++11 -pthread
	 g++ server.cpp -o server -std=c++11 -pthread

client: client.cpp
	g++ client.cpp -o client -std=c++11 -pthread

server: server.cpp
	g++ server.cpp -o server -std=c++11 -pthread

run_server: server
	./server

clean:
	rm client server received_*

