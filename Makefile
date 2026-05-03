CXX      = clang++
CXXFLAGS = -std=c++11 -march=native -O3 \
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

# FP exception trap builds (no -ffast-math so traps fire correctly)
debug:
	$(MAKE) clean
	$(MAKE) CXXFLAGS="-std=c++11 -march=native -O0 -g -DDEBUG_FP $(shell pkg-config --cflags libpng)"

scalar-debug:
	$(MAKE) clean
	$(MAKE) CXXFLAGS="-std=c++11 -O0 -g -DDEBUG_FP -DHIGHPRECISION $(shell pkg-config --cflags libpng)"

.PHONY: clean debug scalar-debug
