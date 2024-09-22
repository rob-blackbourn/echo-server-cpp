CXX = clang++
CXXFLAGS = -g -std=c++23 -Wall

HPP_FILES = \
	tcp.hpp \
	tcp_socket.hpp \
	tcp_listener.hpp \
	tcp_client.hpp \
	tcp_stream.hpp \
	tcp_buffered_stream.hpp \
	tcp_server.hpp

.PHONEY: default
default: all

.PHONEY: all
all: chat-server echo-server

chat-server: chat-server.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

chat-server.o: chat-server.cpp $(HPP_FILES)

echo-server: echo-server.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

echo-server.o: echo-server.cpp $(HPP_FILES)

.PHONY: clean
clean:
	rm -f chat-server.o chat-server
	rm -f echo-server.o echo-server
	