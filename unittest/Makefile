CXX = g++
CXXFLAGS = -O2 -Wall -Werror -std=c++14 -I .

SRCS = \
	main.cpp\
	mlfq.cpp

OBJS = $(addprefix build/, $(SRCS:.cpp=.o))

all: unittest

build:
	mkdir build

build/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $^

unittest: build $(OBJS)
	$(CXX) $(CXXFLAGS) -o unittest $(OBJS)

clean:
	rm unittest build/*.o
