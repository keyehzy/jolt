CXX=g++
CXXFLAGS=-std=c++20 -g3 -ggdb -Wall -Wextra -pedantic

SRCS= main.cpp x86_64.cpp
OBJS= $(SRCS:.cpp=.o)

main: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(OBJS) main
