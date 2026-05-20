CXX = g++
CXXFLAGS = -std=c++20 -Wall
TARGETS = ludo board players

all: $(TARGETS)

run: all
	./ludo 4


clean:
	rm -f $(TARGETS)
	ipcrm -a

.PHONY: all run clean
