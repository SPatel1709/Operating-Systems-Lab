CC = g++
CFLAGS = -Wall -g

all: manager timer process launcher

manager: manager.cpp
	$(CC) $(CFLAGS) -o manager manager.cpp

timer: timer.cpp
	$(CC) $(CFLAGS) -o timer timer.cpp

process: process.cpp
	$(CC) $(CFLAGS) -o process process.cpp

launcher: launcher.cpp
	$(CC) $(CFLAGS) -o launcher launcher.cpp

run: all
	./manager &
	sleep 0.5
	./timer &
	sleep 0.5
	./launcher

clean:
	rm -f manager timer process launcher

.PHONY: all run clean
