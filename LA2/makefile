CXX = g++
CXXFLAGS = -std=c++20 -Wall

all: agent customer run

agent: agent.cpp
	$(CXX) $(CXXFLAGS) agent.cpp -o agent

customer: customer.cpp
	$(CXX) $(CXXFLAGS) customer.cpp -o customer

clean:
	rm -f agent customer

.PHONY: all run clean
