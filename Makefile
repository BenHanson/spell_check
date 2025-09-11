CXX = g++
CXXFLAGS = -O -std=c++20 -Wall -I $(BOOST_ROOT) -I ../lexertl17/include \
-I ../parsertl17/include

LDFLAGS = -O

LIBS = 

all: spell_check

spell_check: filter.o main.o types.o
	$(CXX) $(LDFLAGS) -o spell_check filter.o main.o types.o $(LIBS)

filter.o: filter.cpp
	$(CXX) $(CXXFLAGS) -o filter.o -c filter.cpp

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -o main.o -c main.cpp

types.o: types.cpp
	$(CXX) $(CXXFLAGS) -o types.o -c types.cpp

library:

binary:

clean:
	- rm *.o
	- rm spell_check
