CXXFLAGS=-I../../include -I. -Wall -Wextra -O2 -g
LDFLAGS=-lzstd -lgeos_c -lgeos

../../build/%.o: %.cc Makefile
	$(CXX) -o $@ $< -c $(CXXFLAGS)
../../build/geocc: ../../build/geocc.o
	$(CXX) -o $@ $< $(LDFLAGS)
