CXXFLAGS=-isystem /home/bsellers/code/s2geometry/include -I../../include -I. -Wall -Wextra -x c++ -O0 -g
LDFLAGS=-lflatbuffers -lzstd -lpqxx -lpq -L/home/bsellers/code/s2geometry -Wl,-rpath=/home/bsellers/code/s2geometry -ls2

../../build/%.o: %.cc
	$(CXX) -o $@ $< $(CXXFLAGS)
../../build/make_world: make_world.o
	$(CXX) -o $@ $< $(LDFLAGS)
