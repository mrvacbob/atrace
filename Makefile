CXX      = clang++
CXXFLAGS = -std=c++14 -march=native -O3 -MMD -MP \
           $(shell pkg-config --cflags libpng)
LDFLAGS  = $(shell pkg-config --libs libpng) -lz

SRCS = trig.cpp texture.cpp scene.cpp images.cpp raytrace.cpp tests.cpp main.cpp
OBJS = $(SRCS:.cpp=.o)
DEPS = $(OBJS:.o=.d)

atrace: $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o atrace

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -f atrace $(OBJS) $(DEPS) scene.bmp scene.hdr

# Smoke-test render: run atrace and verify exposure is in a sane range.
# exposure_scale should be between 0.05 and 5.0 for this scene.
# A broken render (all-black, NaN, wrong lighting) lands outside that range.
check: atrace
	@scale=$$(./atrace 2>&1 | grep -o 'exposure_scale=[0-9.]*' | cut -d= -f2); \
	echo "exposure_scale=$$scale"; \
	python3 -c "s=float('$$scale'); assert 0.1 < s < 0.5, f'exposure {s} out of expected range [0.1, 0.5]'" \
	  && echo "PASS" || (echo "FAIL: render may be broken"; exit 1)

# FP exception trap build (no -ffast-math so traps fire correctly)
debug:
	$(MAKE) clean
	$(MAKE) CXXFLAGS="-std=c++14 -march=native -O0 -g -DDEBUG_FP $(shell pkg-config --cflags libpng)"

# POV-Ray render (output: scene_pov.png / scene_pov.exr for HDR inspection)
pov:
	povray +W1280 +H960 +A0.3 +AM2 +Oscene_pov.png scene.pov -D
	povray +W1280 +H960 +A0.3 +AM2 +FE +Oscene_pov.exr scene.pov -D

pov-preview:
	povray +W640 +H480 +A0.3 +AM2 +Oscene_pov_preview.png scene.pov -D

reference.png: atrace
	./atrace
	sips -s format png scene.bmp --out reference.png

.PHONY: clean debug pov pov-preview reference.png check
