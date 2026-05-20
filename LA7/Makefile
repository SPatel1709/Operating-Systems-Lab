CXX = g++
CC = gcc
CXXFLAGS = -Wall -std=c++23 -pthread
CFLAGS = -Wall

all: genschedule foobar

genschedule: genschedule.c
	$(CC) $(CFLAGS) -o genschedule genschedule.c
	./genschedule 16 20 3 5 demofoo.txt demobar.txt

foobar: foobar.cpp global.hpp manager.cpp work.cpp
	$(CXX) $(CXXFLAGS) -o foobar foobar.cpp	

rundemo: foobar genschedule
	./foobar 2 1 demofoo.txt demobar.txt

clean:
	rm -f genschedule foobar *.txt
