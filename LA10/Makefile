CXX := g++
CXXFLAGS := -std=c++17 -O2

all: diskmanager foosh

diskmanager: diskmanager.cpp diskutils.h
	$(CXX) $(CXXFLAGS) -o diskmanager diskmanager.cpp

foosh: foosh.cpp diskutils.cpp diskutils.h
	$(CXX) $(CXXFLAGS) -o foosh foosh.cpp diskutils.cpp

clean:
	rm -f diskmanager foosh
