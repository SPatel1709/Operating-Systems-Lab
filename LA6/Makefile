CXX = g++
CXXFLAGS = -Wall

compile: manager demon nomad

manager: manager.cpp cond.c
	$(CXX) $(CXXFLAGS) -o manager manager.cpp

demon: demon.cpp cond.c
	$(CXX) $(CXXFLAGS) -o demon demon.cpp

nomad: nomad.cpp cond.c
	$(CXX) $(CXXFLAGS) -o nomad nomad.cpp

demorun:
	./demon 4 & ./nomad 5


clean:
	rm -f manager demon nomad
