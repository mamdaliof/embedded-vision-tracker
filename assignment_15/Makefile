vision_tracker: vision_tracker.cpp lut_gen.hpp yuv_process.hpp
	g++ -O3 vision_tracker.cpp -o vision_tracker `pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0 opencv4`

test_lut: test_lut.cpp lut_gen.hpp
	g++ -O3 test_lut.cpp -o test_lut

test_yuv: test_yuv.cpp yuv_process.hpp
	g++ -O3 test_yuv.cpp -o test_yuv `pkg-config --cflags --libs opencv4`

run_test: test_lut test_yuv
	./test_lut
	./test_yuv

clean:
	rm -f test_lut test_yuv vision_tracker
