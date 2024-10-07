CXX = clang++
# CXX = g++
CXXFLAGS = -g -std=c++23 -Wall -I../external -I/opt/homebrew/include
LDLIBS = -L/opt/homebrew/lib -lspdlog -lfmt -lssl -lcrypto

COMMON_HPP = \
	file.hpp \
	tcp_socket.hpp \
	bio.hpp \
	tcp_stream.hpp \
	ssl_ctx.hpp \
	ssl.hpp \
	utils.hpp

SERVER_HPP = \
	tcp.hpp \
	tcp_listener_socket.hpp \
	tcp_server_socket.hpp \
	poll_handler.hpp \
	poller.hpp \
	tcp_socket_poll_handler.hpp \
	tcp_listener_socket_poll_handler.hpp
CLIENT_HPP = \
	tcp_client_socket.hpp

.PHONEY: default
default: all

.PHONEY: all
all: chat-server echo-server client

chat-server: chat-server.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

chat-server.o: chat-server.cpp $(SERVER_HPP) $(COMMON_HPP)

echo-server: echo-server.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

echo-server.o: echo-server.cpp $(SERVER_HPP)

client: client.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

client.o: client.cpp $(CLIENT_HPP) $(COMMON_HPP)

.PHONY: clean
clean:
	rm -f chat-server.o chat-server
	rm -f echo-server.o echo-server
	rm -f client.o client
	
