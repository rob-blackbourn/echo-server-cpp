CXX = clang++
CXXFLAGS = -g -std=c++23

.PHONEY: default
default: all

.PHONEY: all
all: chat-server echo-server

chat-server: chat-server.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

chat-server.o: chat-server.cpp tcp.hpp tcp_listener.hpp tcp_client.hpp tcp_socket.hpp tcp_server.hpp

echo-server: echo-server.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

echo-server.o: echo-server.cpp tcp.hpp tcp_listener.hpp tcp_client.hpp tcp_socket.hpp tcp_server.hpp

.PHONY: clean
clean:
	rm -f chat-server.o chat-server
	rm -f echo-server.o echo-server
	