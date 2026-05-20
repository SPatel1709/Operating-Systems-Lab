CXX      := g++
CC       := gcc
CXXFLAGS := -Wall -Wextra -std=c++17 -pthread
CFLAGS   := -Wall
TARGET   := manager

compile: $(TARGET)

$(TARGET): manager.cpp worker.cpp dlhandler.cpp genvector.o
	$(CXX) $(CXXFLAGS) -o $@ manager.cpp worker.cpp dlhandler.cpp genvector.o

genvector.o: genvector.c
	$(CC) $(CFLAGS) -c genvector.c -o genvector.o

run: $(TARGET)
	./$(TARGET) 4 8 16

clean:
	rm -f $(TARGET) genvector.o
