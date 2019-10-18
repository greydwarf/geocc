CXXFLAGS=-I../../include -I. -Wall -Wextra -O2 -g
LDFLAGS=-lzstd -lgeos_c -lgeos

all: ../../build/geocc ../../build/MapIndex_test
../../build/geocc: ../../build/MapIndex.o ../../build/geocc.o
	$(CXX) -o $@ $^ $(LDFLAGS)
../../build/MapIndex_test: ../../build/MapIndex.o ../../build/MapIndex_test.o
	$(CXX) -o $@ $^ $(LDFLAGS)
../../build/%.o: %.cc Makefile
	$(CXX) -o $@ $< -c $(CXXFLAGS)
