CXX = clang++
CXXFLAGS = -g -std=c++23 -Wall
LDLIBS = -lspdlog -lfmt

SERVER_HPP = \
	tcp.hpp \
	tcp_socket.hpp \
	tcp_listener_socket.hpp \
	tcp_server_socket.hpp \
	tcp_stream.hpp \
	tcp_buffered_stream.hpp \
	tcp_server.hpp \
	poll_handler.hpp \
	poller.hpp \
	tcp_server_socket_poll_handler.hpp \
	tcp_listener_socket_poll_handler.hpp
CLIENT_HPP = \
	tcp_socket.hpp \
	tcp_client_socket.hpp

.PHONEY: default
default: all

.PHONEY: all
all: chat-server echo-server client

chat-server: chat-server.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

chat-server.o: chat-server.cpp $(SERVER_HPP)

echo-server: echo-server.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

echo-server.o: echo-server.cpp $(SERVER_HPP)

client: client.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

client.o: client.cpp $(CLIENT_HPP)

.PHONY: clean
clean:
	rm -f chat-server.o chat-server
	rm -f echo-server.o echo-server
	rm -f client.o client
	