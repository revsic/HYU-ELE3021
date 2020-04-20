CXX = g++
CXXFLAGS = -O2 -Wall -Werror -std=c++14 -I .

SRCS = \
	main.cpp\
	mlfq.cpp

OBJS = $(SRCS:.cpp=.o)

all: unittest

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $^

unittest: $(OBJS)
	$(CXX) $(CXXFLAGS) -o unittest $^

clean:
	rm unittest *.o
