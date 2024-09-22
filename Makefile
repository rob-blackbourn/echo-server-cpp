CXX = clang++
CXXFLAGS = -g -std=c++23

.PHONEY: default
default: all

.PHONEY: all
all: server

server: server.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

server.o: server.cpp tcp.hpp tcp_listener.hpp tcp_client.hpp tcp_socket.hpp tcp_server.hpp

.PHONY: clean
clean:
	rm -f server.o server
	