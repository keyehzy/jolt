CXX=g++
CXXFLAGS=-std=c++17 -g -Wall -Wextra -pedantic

main : main.o
	$(CXX) $(CXXFLAGS) -o main main.o

main.o : main.cpp
	$(CXX) $(CXXFLAGS) -c main.cpp

clean :
	rm -f main.o main
