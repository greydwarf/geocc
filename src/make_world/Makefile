CXXFLAGS=-I../../include -I. -Wall -Wextra -O2 -g
LDFLAGS=-lflatbuffers -lzstd -lpqxx -lpq -lgeos

../../build/make_world: ../../build/make_world.o
	$(CXX) -o $@ $< $(LDFLAGS)
../../build/%.o: %.cc Makefile
	$(CXX) -o $@ $< -c $(CXXFLAGS)
