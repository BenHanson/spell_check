CXX = g++
CXXFLAGS = -O3 -std=c++20 -Wall -I $(BOOST_ROOT) -I ../lexertl17/include \
-I ../parsertl17/include -I ../wildcardtl/include

LDFLAGS = -O3

all: spell_check

spell_check: filter.o main.o output.o pathnames.o types.o utils.o
	$(CXX) $(LDFLAGS) -o spell_check filter.o main.o output.o \
pathnames.o types.o utils.o $(LIBS)

filter.o: filter.cpp
	$(CXX) $(CXXFLAGS) -o filter.o -c filter.cpp

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -o main.o -c main.cpp

output.o: output.cpp
	$(CXX) $(CXXFLAGS) -o output.o -c output.cpp

pathnames.o: pathnames.cpp
	$(CXX) $(CXXFLAGS) -o pathnames.o -c pathnames.cpp

types.o: types.cpp
	$(CXX) $(CXXFLAGS) -o types.o -c types.cpp

utils.o: utils.cpp
	$(CXX) $(CXXFLAGS) -o utils.o -c utils.cpp

clean:
	- rm *.o
	- rm spell_check
