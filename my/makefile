
all: client.cpp server.cpp
	g++ client.cpp -o client
	g++ server.cpp -o server

client: client.cpp
	g++ client.cpp -o client

server: server.cpp
	g++ server.cpp -o server

clean:
	rm client server received_* &> /dev/null

