CXX      = clang++
CXXFLAGS = -std=c++11 -march=native -O3 -ffast-math \
           $(shell pkg-config --cflags libpng)
LDFLAGS  = $(shell pkg-config --libs libpng) -lz

SRCS = trig.cpp scene.cpp images.cpp raytrace.cpp tests.cpp main.cpp
OBJS = $(SRCS:.cpp=.o)

atrace: $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o atrace

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f atrace $(OBJS) scene.bmp

.PHONY: clean
