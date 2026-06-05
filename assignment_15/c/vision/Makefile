CXX = g++
CXXFLAGS = -O3 -Wall $(shell pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0 opencv4)
LDFLAGS = $(shell pkg-config --libs gstreamer-1.0 gstreamer-app-1.0 opencv4)

TARGETS = vision yuv_tracker lut_bench naive_bench

all: $(TARGETS)

vision: vision.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

yuv_tracker: yuv.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

lut_bench: lut_time.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

naive_bench: naive_time.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
