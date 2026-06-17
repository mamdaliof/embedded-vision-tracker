CXX = g++
CXXFLAGS = -O3 -Wall $(shell pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0 opencv4)
LDFLAGS = $(shell pkg-config --libs gstreamer-1.0 gstreamer-app-1.0 opencv4)

PAN_DIR = controllers/PositionControllerPan
TILT_DIR = controllers/PositionControllerTilt

CONTROLLER_SRCS = \
	$(PAN_DIR)/pan_submod.c \
	$(PAN_DIR)/pan_model.c \
	$(PAN_DIR)/pan_funcs.c \
	$(PAN_DIR)/pan_integ.c \
	$(PAN_DIR)/pan_matrix.c \
	$(PAN_DIR)/EulerAngles.c \
	$(PAN_DIR)/motionprofiles.c \
	$(TILT_DIR)/tilt_submod.c \
	$(TILT_DIR)/tilt_model.c \
	$(TILT_DIR)/tilt_funcs.c \
	$(TILT_DIR)/tilt_integ.c \
	$(TILT_DIR)/tilt_matrix.c \
	$(TILT_DIR)/EulerAngles.c \
	$(TILT_DIR)/motionprofiles.c

TARGETS = vision yuv_tracker lut_bench naive_bench vision_control_finaldemo vision_control_finaldemo_save_output

all: $(TARGETS)

vision: vision.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

yuv_tracker: yuv.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

lut_bench: lut_time.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

naive_bench: naive_time.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

vision_control_finaldemo: vision_control_finaldemo.cpp $(CONTROLLER_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -pthread -lm

vision_control_finaldemo_save_output: vision_control_finaldemo_save_output.cpp $(CONTROLLER_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -pthread -lm

clean:
	rm -f $(TARGETS)

.PHONY: all clean
