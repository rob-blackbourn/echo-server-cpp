CXX = clang++
CXXFLAGS = -g -std=c++17

.PHONEY: default
default: all

.PHONEY: all
all: server client

server: server.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

client: client.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

.PHONY: clean
clean:
	rm -f server.o server
	rm -f client.o client
