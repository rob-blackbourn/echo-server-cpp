CXX = clang++
CXXFLAGS = -g -std=c++17

.PHONEY: default
default: all

.PHONEY: all
all: adress_for_name name_for_address names

adress_for_name: adress_for_name.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

name_for_address: name_for_address.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

names: names.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

.PHONY: clean
clean:
	rm -f adress_for_name.o adress_for_name
	rm -f name_for_address.o name_for_address
	rm -f names.o names
