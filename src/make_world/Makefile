CXXFLAGS=-isystem /home/bsellers/code/s2geometry/include -I../../include -I. -Wall -Wextra -O2 -g
LDFLAGS=-lflatbuffers -lzstd -lpqxx -lpq -L/home/bsellers/code/s2geometry -Wl,-rpath=/home/bsellers/code/s2geometry -ls2

../../build/make_world: ../../build/make_world.o
	$(CXX) -o $@ $< $(LDFLAGS)
../../build/%.o: %.cc Makefile
	$(CXX) -o $@ $< -c $(CXXFLAGS)
