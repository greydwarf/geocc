CXXFLAGS=-isystem /home/bsellers/code/s2geometry/include -I../../include -I. -Wall -Wextra -O2 -g
LDFLAGS=-lzstd -L/home/bsellers/code/s2geometry -Wl,-rpath=/home/bsellers/code/s2geometry -ls2

../../build/%.o: %.cc Makefile
	$(CXX) -o $@ $< -c $(CXXFLAGS)
../../build/geocc: ../../build/geocc.o
	$(CXX) -o $@ $< $(LDFLAGS)
